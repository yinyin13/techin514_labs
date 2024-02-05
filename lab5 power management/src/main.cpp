#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

const char* ssid = "UW MPSK";
const char* password = "6A^UdR7S#M";
#define DATABASE_URL "https://lab5-group2-d545b-default-rtdb.firebaseio.com/"
#define API_KEY "AIzaSyCwFXS2PNr1d4PHxcumLpxzFhHLMmrjrZo"
#define STAGE_INTERVAL 12000
#define MAX_WIFI_RETRIES 5
#define ULTRASONIC_THRESHOLD 60
#define DETECTION_TIME_THRESHOLD 15000 // 15 seconds
#define DEEP_SLEEP_DURATION 12000 // 12 seconds

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

const int trigPin = D9;
const int echoPin = D8;
const float soundSpeed = 0.034;

float measureDistance();
void connectToWiFi();
void initFirebase();
void sendDataToFirebase(float distance);

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void loop() {
  // First, measure distance
  float currentDistance = measureDistance();

  // Check if an object is detected
  if (currentDistance > ULTRASONIC_THRESHOLD) {
    // Object detected
    if (millis() - sendDataPrevMillis > DETECTION_TIME_THRESHOLD || sendDataPrevMillis == 0) {
      connectToWiFi();
      initFirebase();
      sendDataToFirebase(currentDistance);

      // Disconnect WiFi before going to deep sleep
      WiFi.disconnect();
      esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION * 1000); // in microseconds
      esp_deep_sleep_start();
    }
  }

  // If no object detected, go to deep sleep for STAGE_INTERVAL
  Serial.println("Going to deep sleep...");
  WiFi.disconnect();
  esp_sleep_enable_timer_wakeup(STAGE_INTERVAL * 1000); // in microseconds
  esp_deep_sleep_start();
}

float measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * soundSpeed / 2;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  return distance;
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int wifiCnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    wifiCnt++;
    if (wifiCnt > MAX_WIFI_RETRIES) {
      Serial.println("WiFi connection failed");
      ESP.restart();
    }
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase authentication successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase authentication failed: %s\n", config.signer.signupError.message.c_str());
    ESP.restart();
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

void sendDataToFirebase(float distance) {
  if (Firebase.ready() && signupOK) {
    if (Firebase.RTDB.pushFloat(&fbdo, "test/distance", distance)) {
      Serial.println("Data sent to Firebase");
      sendDataPrevMillis = millis();
    } else {
      Serial.println("Failed to send data to Firebase");
      Serial.printf("Error: %s\n", fbdo.errorReason());
    }
    count++;
  }
}
