// ===================== hikesafe_config.h (Simplified & Fixed) =====================
#ifndef HIKESAFE_CONFIG_H
#define HIKESAFE_CONFIG_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include <algorithm>
#include "FS.h"
#include "SPIFFS.h"

// Hardware Constants - Basic functionality only
#define MAX_TTL 5

// Display settings - Force specific I2C pins for OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define OLED_SDA 21  // Explicit SDA pin
#define OLED_SCL 22  // Explicit SCL pin

// LoRa settings - Try these pin configurations if default doesn't work
#define LORA_SS 18      // Default: GPIO 18
#define LORA_RST 14     // Default: GPIO 14  
#define LORA_DIO0 26    // Default: GPIO 26
#define LORA_BAND 915E6 // 915MHz for US, change to 868E6 for EU

// Alternative pin configurations (uncomment if default doesn't work):
// Option 1: TTGO LoRa32 V1
// #define LORA_SS 18
// #define LORA_RST 14
// #define LORA_DIO0 26

// Option 2: TTGO LoRa32 V2
// #define LORA_SS 18
// #define LORA_RST 12
// #define LORA_DIO0 26

// Option 3: Heltec WiFi LoRa 32
// #define LORA_SS 18
// #define LORA_RST 14
// #define LORA_DIO0 26

// Option 4: Generic ESP32 with LoRa module
// #define LORA_SS 5
// #define LORA_RST 2
// #define LORA_DIO0 4

// SD card settings
#define SD_CS 5

// TTL settings
#define TTL_BROADCAST_INTERVAL 15000
#define TTL_CLEANUP_INTERVAL 30000
#define TTL_NODE_TIMEOUT 60000

// Display update intervals
#define DISPLAY_UPDATE_INTERVAL 1000   // Slower for stability
#define STATUS_LOG_INTERVAL 60000

// Buffer sizes
#define LORA_BUFFER_SIZE 256
#define MESSAGE_BUFFER_SIZE 128

// Safety timeouts
#define WATCHDOG_TIMEOUT 30000
#define BLE_RECONNECT_DELAY 2000
#define STORAGE_RETRY_DELAY 5000

// Power management
#define BATTERY_VOLTAGE_PIN 35
#define LOW_BATTERY_THRESHOLD 3.3
#define CRITICAL_BATTERY_THRESHOLD 3.0

struct TTLMessage {
    String id;
    int ttl;
    int rssi;
    unsigned long timestamp;
    float latitude;
    float longitude;
    
    TTLMessage() : id(""), ttl(0), rssi(0), timestamp(0), latitude(0.0), longitude(0.0) {}
    TTLMessage(String _id, int _ttl, int _rssi = 0, float lat = 0.0, float lon = 0.0) 
        : id(_id), ttl(_ttl), rssi(_rssi), timestamp(millis()), latitude(lat), longitude(lon) {}
};

struct NodeInfo {
    String nodeId;
    int rssi;
    unsigned long lastSeen;
    float batteryLevel;
    String status;
    
    NodeInfo() : nodeId(""), rssi(0), lastSeen(0), batteryLevel(0.0), status("") {}
};

// Storage management structure
struct StorageStatus {
    bool sdMounted;
    bool spiffsMounted;
    bool sdWritable;
    bool spiffsWritable;
    unsigned long lastSDCheck;
    unsigned long lastSPIFFSCheck;
    int sdRetryCount;
    int spiffsRetryCount;
    
    StorageStatus() : sdMounted(false), spiffsMounted(false), sdWritable(false), 
                     spiffsWritable(false), lastSDCheck(0), lastSPIFFSCheck(0),
                     sdRetryCount(0), spiffsRetryCount(0) {}
};

// Forward declarations for global variables
#ifndef MAIN_FILE
extern std::vector<TTLMessage> nearbyNodes;
extern std::vector<NodeInfo> nodeInfoList;
extern BLEServer* pServer;
extern BLECharacteristic* pCharacteristic;
extern bool deviceConnected;
extern bool oldDeviceConnected;
extern bool bleConnected;
extern bool loraInitialized;
extern String currentStatus;
extern String lastMessage;
extern int connectedNodes;
extern unsigned long lastWatchdogReset;
extern unsigned long systemStartTime;
extern Adafruit_SSD1306* display_ptr;
extern StorageStatus storage;
extern String serviceUUID;
extern String characteristicUUID;
extern String nodeId;
extern float batteryVoltage;
extern bool lowBatteryWarning;
extern bool systemReady;
extern bool sdCardAvailable;
extern bool spiffsAvailable;
extern bool displayInitialized;
#endif

// Function declarations - organized by module

// Core system functions
void initializeHardware();
void resetWatchdog();
bool checkSystemHealth();
String generateNodeId();
String generateUUID();
float getBatteryLevel();
void checkBatteryStatus();
void enterDeepSleep();

// Display functions
bool setupOLED();
void loopOLED();
void updateDisplay();
void showMessage(String title, String message, int duration = 3000);
void showError(String error);
void displaySplashScreen();
bool testOLEDConnection();

// Storage functions
bool initializeStorage();
bool mountSD();
bool mountSPIFFS();
void loopStorage();
bool testStorageWrite(String filename, String storageType);
bool isSDAvailable();
bool isSPIFFSAvailable();

// BLE functions
bool setupBLE();
void loopBLE();
void sendBLEMessage(String msg);
void handleBLECommand(String command);
void updateBLEStatus(bool connected);
void sendNodeListToBLE();
void sendSystemStatusToBLE();

// TTL/LoRa mesh functions
bool setupTTL();
void loopTTL();
void handleTTLMessage(String incoming, int rssi);
void broadcastTTL();
void forwardTTL(String msg, int currentTTL);
void cleanupExpiredNodes();

// LoRa communication functions
bool setupLoRaMesh();
void loopLoRaMesh();
void handleLoRaMessage(String message);
void sendLoRaMessage(String msg);

// Logging functions
void logEvent(String tag, String data);
bool reopenSDCard();

// Utility functions
void updateNodeCount(int count);
void updateDisplayStatus(String status);
void updateLastMessage(String message);
void safeDelay(unsigned long ms);
bool isValidNodeId(String nodeId);

// BLE Callback classes
class MyServerCallbacks: public BLEServerCallbacks {
public:
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
};

class MyCallbacks: public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic *pCharacteristic) override;
};

#endif // HIKESAFE_CONFIG_H