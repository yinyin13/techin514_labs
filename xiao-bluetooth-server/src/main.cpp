#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;

int trig = D9;
int echo = D8;
long duration;
float distance;
float meter;

// Constants for low-pass filter
const float alpha = 0.2; // Smoothing factor for low-pass filter

// Variables for denoising
float filteredDistance = 0.0;

#define SERVICE_UUID        "80fb769d-6100-4dc9-ade6-4a437e425ce6"
#define CHARACTERISTIC_UUID "bb1fdaa8-bac5-4f04-a1a4-094edbabdf51"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    pinMode(trig, OUTPUT);
    digitalWrite(trig, LOW);
    delayMicroseconds(2);
    pinMode(echo, INPUT);
    delay(6000);
    Serial.println("Distance:");

    BLEDevice::init("XIAO_ESP32S3_matilda");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Connected to Matilda's esp32. BLE Device name: 'XIAO_ESP32S3_matilda'");
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined! Now you can read it on your phone!");
}

void loop() {
    // Measure raw distance
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);
    duration = pulseIn(echo, HIGH);

    if (duration >= 38000) {
        Serial.println("Out of range");
    } else {
        // Calculate raw distance
        distance = duration / 58;

        // Low-pass filter to denoise the distance data
        filteredDistance = alpha * distance + (1.0 - alpha) * filteredDistance;

        // Print both raw and denoised distance on the serial monitor
        Serial.print("Raw Distance: ");
        Serial.print(distance);
        Serial.print("cm\tDenoised Distance: ");
        Serial.print(filteredDistance);
        Serial.println("cm");

        meter = filteredDistance / 100;

        // Transmit data over BLE only if denoised distance is less than 30cm
        if (filteredDistance < 30.0 && deviceConnected) {
            // Convert the filteredDistance to a string
            std::string distanceStr = std::to_string(filteredDistance);

            // Set the value of the BLECharacteristic
            pCharacteristic->setValue(distanceStr.c_str());
            
            // Notify the connected device
            pCharacteristic->notify();
            
            Serial.print("Transmitted Denoised Distance: ");
            Serial.print(filteredDistance);
            Serial.println("cm");
        }
    }

    // Check for device connection
    if (deviceConnected) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            pCharacteristic->setValue("Connected to Matilda's esp32. BLE Device name: 'XIAO_ESP32S3_matilda'");
            pCharacteristic->notify();
            Serial.println("Connected to Matilda's esp32. BLE Device name: 'XIAO_ESP32S3_matilda'");
            previousMillis = currentMillis; // Update previousMillis
        }
    }

    // Handle BLE advertising when device disconnects
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }

    // Do something when connecting
    if (deviceConnected && !oldDeviceConnected) {
        // Do something when connecting
        oldDeviceConnected = deviceConnected;
    }

    delay(1000);
}
