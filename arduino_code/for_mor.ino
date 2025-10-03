#define MAIN_FILE
#include "hikesafe_config.h"

// DISABLE BLE FOR DEBUGGING
#define ENABLE_BLE false  // Set to true when BLE is working

// Define global variables (only in main file)
std::vector<TTLMessage> nearbyNodes;
std::vector<NodeInfo> nodeInfoList;
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool bleConnected = false;
bool loraInitialized = false;
String currentStatus = "Initializing...";
String lastMessage = "";
int connectedNodes = 0;
unsigned long lastWatchdogReset = 0;
unsigned long systemStartTime = 0;

// UUIDs - will be set during initialization with better uniqueness
String serviceUUID = "";
String characteristicUUID = "";
String nodeId = "";

// Battery monitoring
float batteryVoltage = 0.0f;
bool lowBatteryWarning = false;

// Storage management
StorageStatus storage;

// Hardware object pointers - will be dynamically allocated
Adafruit_SSD1306* display_ptr = nullptr;

// System status
bool systemReady = false;
bool sdCardAvailable = false;
bool spiffsAvailable = false;
bool displayInitialized = false;

// ----------------- UUID Generation Functions (IMPROVED) -----------------
String generateUUID() {
    // Use MAC address + millis + random for better uniqueness
    uint64_t chipid = ESP.getEfuseMac();
    unsigned long timestamp = millis();
    
    String uuid = "";
    uuid += String((uint32_t)(chipid >> 32), HEX);
    uuid += "-";
    uuid += String((uint32_t)chipid, HEX);
    uuid += "-";
    uuid += String(timestamp, HEX);
    uuid += "-";
    uuid += String(random(0x1000, 0x9FFF), HEX);
    uuid += "-";
    uuid += String(random(0x100000, 0x9FFFFF), HEX);
    
    uuid.toUpperCase();
    return uuid;
}

String generateNodeId() {
    uint64_t chipid = ESP.getEfuseMac();
    // Use only the chip ID for consistency - same device = same ID
    String id = String((uint32_t)chipid, HEX);
    id.toUpperCase();
    return id.substring(0, 8); // Keep it short and consistent
}

// ----------------- Core System Functions -----------------
void initializeHardware() {
    Serial.println("[SYSTEM] Initializing hardware objects...");
    
    // Seed random number generator properly
    randomSeed(ESP.getEfuseMac() + millis());
    
    // Initialize I2C with explicit pins for better OLED reliability
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(100000); // Slower I2C for stability
    
    // Dynamically allocate hardware objects for memory safety
    display_ptr = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    
    if (!display_ptr) {
        Serial.println("[SYSTEM] CRITICAL: Memory allocation failed!");
        ESP.restart();
    }
    
    // Generate UUIDs and node ID with better randomness
    nodeId = generateNodeId();
    serviceUUID = generateUUID();
    characteristicUUID = generateUUID();
    
    Serial.println("[SYSTEM] Node ID: " + nodeId);
    Serial.println("[SYSTEM] Service UUID: " + serviceUUID);
    Serial.println("[SYSTEM] Characteristic UUID: " + characteristicUUID);
    Serial.println("[SYSTEM] Hardware initialization complete");
}

void resetWatchdog() {
    lastWatchdogReset = millis();
}

bool checkSystemHealth() {
    if (millis() - lastWatchdogReset > WATCHDOG_TIMEOUT) {
        Serial.println("[SYSTEM] Watchdog timeout - system may be hung");
        return false;
    }
    return true;
}

String getNodeId() {
    return nodeId;
}

float getBatteryLevel() {
    int rawValue = analogRead(BATTERY_VOLTAGE_PIN);
    batteryVoltage = (rawValue / 4095.0) * 3.3 * 2;
    float percentage = ((batteryVoltage - 3.0) / (4.2 - 3.0)) * 100.0;
    return constrain(percentage, 0.0, 100.0);
}

void checkBatteryStatus() {
    float batteryLevel = getBatteryLevel();
    
    if (batteryVoltage < CRITICAL_BATTERY_THRESHOLD) {
        Serial.println("[POWER] CRITICAL battery level - entering deep sleep");
        showError("Critical Battery");
        logEvent("POWER", "Critical battery - entering deep sleep");
        safeDelay(5000);
        enterDeepSleep();
    } else if (batteryVoltage < LOW_BATTERY_THRESHOLD && !lowBatteryWarning) {
        lowBatteryWarning = true;
        Serial.println("[POWER] Low battery warning");
        showMessage("Low Battery", String(batteryLevel, 1) + "%", 3000);
        logEvent("POWER", "Low battery warning: " + String(batteryLevel, 1) + "%");
    }
}

void enterDeepSleep() {
    Serial.println("[POWER] Entering deep sleep mode...");
    
    if (display_ptr) {
        display_ptr->clearDisplay();
        display_ptr->display();
    }
    
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
    esp_deep_sleep_start();
}

void safeDelay(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        resetWatchdog();
        yield();
        delay(1);
    }
}

bool isValidNodeId(String nodeId) {
    return nodeId.length() >= 4 && nodeId.length() <= 16;
}

// ----------------- Storage Management -----------------
bool initializeStorage() {
    Serial.println("[STORAGE] Initializing storage systems...");
    storage = StorageStatus();
    
    bool spiffsSuccess = mountSPIFFS();
    bool sdSuccess = mountSD();
    
    if (!spiffsSuccess && !sdSuccess) {
        Serial.println("[STORAGE] WARNING: No storage available!");
        return false;
    }
    
    return true;
}

bool mountSPIFFS() {
    if (spiffsAvailable) {
        Serial.println("[STORAGE] SPIFFS already mounted");
        return true;
    }
    
    Serial.println("[STORAGE] Mounting SPIFFS...");
    spiffsAvailable = SPIFFS.begin(true);
    
    if (spiffsAvailable) {
        storage.spiffsMounted = true;
        storage.spiffsWritable = testStorageWrite("/test_spiffs.txt", "SPIFFS");
        storage.lastSPIFFSCheck = millis();
        storage.spiffsRetryCount = 0;
        Serial.println("[STORAGE] SPIFFS mounted successfully");
    } else {
        storage.spiffsRetryCount++;
        Serial.println("[STORAGE] SPIFFS mount failed (attempt " + String(storage.spiffsRetryCount) + ")");
    }
    
    return spiffsAvailable;
}

bool mountSD() {
    if (sdCardAvailable) {
        Serial.println("[STORAGE] SD card already mounted");
        return true;
    }
    
    Serial.println("[STORAGE] Mounting SD card...");
    sdCardAvailable = SD.begin(SD_CS);
    
    if (sdCardAvailable) {
        storage.sdMounted = true;
        storage.sdWritable = testStorageWrite("/test_sd.txt", "SD");
        storage.lastSDCheck = millis();
        storage.sdRetryCount = 0;
        Serial.println("[STORAGE] SD card mounted successfully");
    } else {
        storage.sdRetryCount++;
        Serial.println("[STORAGE] SD card mount failed (attempt " + String(storage.sdRetryCount) + ")");
    }
    
    return sdCardAvailable;
}

bool testStorageWrite(String filename, String storageType) {
    bool success = false;
    
    if (storageType == "SPIFFS" && spiffsAvailable) {
        File testFile = SPIFFS.open(filename, FILE_WRITE);
        if (testFile) {
            testFile.println("Test write");
            testFile.close();
            SPIFFS.remove(filename);
            success = true;
        }
    } else if (storageType == "SD" && sdCardAvailable) {
        File testFile = SD.open(filename, FILE_WRITE);
        if (testFile) {
            testFile.println("Test write");
            testFile.close();
            SD.remove(filename);
            success = true;
        }
    }
    
    Serial.println("[STORAGE] " + storageType + " write test: " + (success ? "PASSED" : "FAILED"));
    return success;
}

void loopStorage() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < STORAGE_RETRY_DELAY) return;
    lastCheck = millis();
    
    if (!spiffsAvailable && storage.spiffsRetryCount < 5) {
        mountSPIFFS();
    }
    
    if (!sdCardAvailable && storage.sdRetryCount < 5) {
        mountSD();
    }
}

bool isSDAvailable() {
    return sdCardAvailable && storage.sdWritable;
}

bool isSPIFFSAvailable() {
    return spiffsAvailable && storage.spiffsWritable;
}

// ----------------- OLED Display -----------------
bool setupOLED() {
    Serial.println("[OLED] Initializing display...");
    
    if (!display_ptr) {
        Serial.println("[OLED] CRITICAL: Display pointer is null!");
        return false;
    }
    
    // Test I2C communication first
    Wire.beginTransmission(SCREEN_ADDRESS);
    byte error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.println("[OLED] I2C communication failed - error code: " + String(error));
        Serial.println("[OLED] Check wiring: SDA=" + String(OLED_SDA) + ", SCL=" + String(OLED_SCL));
        return false;
    }
    
    Serial.println("[OLED] I2C communication OK");
    
    if(!display_ptr->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("[OLED] SSD1306 allocation/initialization failed");
        return false;
    }
    
    displayInitialized = true;
    
    // Initial display
    display_ptr->clearDisplay();
    display_ptr->setTextSize(2);
    display_ptr->setTextColor(SSD1306_WHITE);
    display_ptr->setCursor(0, 0);
    display_ptr->println("HikeSafe");
    
    display_ptr->setTextSize(1);
    display_ptr->setCursor(0, 20);
    display_ptr->println("Node: " + nodeId.substring(0, 8));
    display_ptr->setCursor(0, 35);
    display_ptr->println("Display: OK");
    display_ptr->setCursor(0, 50);
    display_ptr->println("Starting up...");
    display_ptr->display();
    
    Serial.println("[OLED] Display initialized successfully");
    return true;
}

bool testOLEDConnection() {
    if (!displayInitialized) return false;
    
    Wire.beginTransmission(SCREEN_ADDRESS);
    return (Wire.endTransmission() == 0);
}

void loopOLED() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > DISPLAY_UPDATE_INTERVAL) {
        if (displayInitialized && testOLEDConnection()) {
            updateDisplay();
        } else if (displayInitialized) {
            Serial.println("[OLED] Display connection lost - attempting recovery");
            displayInitialized = false;
            safeDelay(1000);
            setupOLED();
        }
        lastUpdate = millis();
    }
}

void updateDisplay() {
    if (!displayInitialized || !display_ptr) {
        return;
    }
    
    try {
        display_ptr->clearDisplay();
        display_ptr->setTextColor(SSD1306_WHITE);
        display_ptr->setTextSize(1);
        
        // Line 1: Header
        display_ptr->setCursor(0, 0);
        display_ptr->println("=== HIKESAFE ===");
        
        // Line 2: Node ID (shortened)
        display_ptr->setCursor(0, 10);
        display_ptr->println("ID: " + nodeId.substring(0, 8));
        
        // Line 3: Status
        display_ptr->setCursor(0, 20);
        display_ptr->println("St: " + currentStatus.substring(0, 13));
        
        // Line 4: BLE and Node count
        display_ptr->setCursor(0, 30);
        display_ptr->print("BLE:");
        display_ptr->print(ENABLE_BLE ? (bleConnected ? "ON" : "OFF") : "DIS");
        display_ptr->print(" N:");
        display_ptr->println(nearbyNodes.size());
        
        // Line 5: Battery and uptime
        display_ptr->setCursor(0, 40);
        display_ptr->print("Bat:");
        display_ptr->print((int)getBatteryLevel());
        display_ptr->print("% Up:");
        display_ptr->println((millis() - systemStartTime) / 1000);
        
        // Line 6: Storage status
        display_ptr->setCursor(0, 50);
        display_ptr->print("LoRa:");
        display_ptr->print(loraInitialized ? "OK" : "FAIL");
        display_ptr->print(" SD:");
        display_ptr->print(sdCardAvailable ? "OK" : "X");
        
        // Line 7: Last message (shortened)
        display_ptr->setCursor(0, 57);
        display_ptr->println(lastMessage.substring(0, 21));
        
        display_ptr->display();
        
    } catch (...) {
        Serial.println("[OLED] Exception during display update");
        displayInitialized = false;
    }
}

void showMessage(String title, String message, int duration) {
    if (!displayInitialized || !display_ptr) {
        Serial.println("[OLED] Cannot show message - display not initialized");
        return;
    }
    
    try {
        display_ptr->clearDisplay();
        display_ptr->setTextSize(1);
        display_ptr->setTextColor(SSD1306_WHITE);
        
        display_ptr->setCursor(0, 0);
        display_ptr->println(title);
        display_ptr->println("----------------");
        
        int yPos = 20;
        int lineHeight = 10;
        int maxCharsPerLine = 21;
        
        for (int i = 0; i < message.length() && yPos < 60; i += maxCharsPerLine) {
            String line = message.substring(i, min(i + maxCharsPerLine, (int)message.length()));
            display_ptr->setCursor(0, yPos);
            display_ptr->println(line);
            yPos += lineHeight;
        }
        
        display_ptr->display();
        
        if (duration > 0) {
            safeDelay(duration);
            updateDisplay();
        }
        
    } catch (...) {
        Serial.println("[OLED] Exception during showMessage");
        displayInitialized = false;
    }
}

void showError(String error) {
    Serial.println("[ERROR] " + error);
    showMessage("ERROR", error, 3000);
    logEvent("ERROR", error);
}

void displaySplashScreen() {
    if (!displayInitialized) return;
    
    display_ptr->clearDisplay();
    display_ptr->setTextSize(2);
    display_ptr->setTextColor(SSD1306_WHITE);
    display_ptr->setCursor(10, 10);
    display_ptr->println("HikeSafe");
    
    display_ptr->setTextSize(1);
    display_ptr->setCursor(0, 35);
    display_ptr->println("Emergency Mesh System");
    display_ptr->setCursor(0, 50);
    display_ptr->println("Node: " + nodeId.substring(0, 12));
    display_ptr->display();
    
    safeDelay(3000);
}

// ----------------- BLE Implementation (OPTIONAL) ----------------
void MyServerCallbacks::onConnect(BLEServer* pServer) {
    deviceConnected = true;
    updateBLEStatus(true);
    Serial.println("[BLE] Client connected");
    
    safeDelay(100);
    sendSystemStatusToBLE();
}

void MyServerCallbacks::onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    updateBLEStatus(false);
    Serial.println("[BLE] Client disconnected");
}

void MyCallbacks::onWrite(BLECharacteristic *pCharacteristic) {
    String val = pCharacteristic->getValue();
    if (val.length() == 0) return;

    Serial.println("[BLE RX] " + val);
    
    // Handle various BLE commands here
    if (val.startsWith("CMD:")) {
        String command = val.substring(4);
        handleBLECommand(command);
    }
}

bool setupBLE() {
    if (!ENABLE_BLE) {
        Serial.println("[BLE] BLE is DISABLED for debugging");
        return false;
    }
    
    Serial.println("[BLE] Starting BLE initialization...");
    
    try {
        Serial.println("[BLE] Step 1: Initializing BLE Device");
        BLEDevice::init("HikeSafe_" + nodeId);
        safeDelay(500);
        
        Serial.println("[BLE] Step 2: Creating BLE server");
        pServer = BLEDevice::createServer();
        
        if (!pServer) {
            Serial.println("[BLE] FAILED: Could not create BLE server");
            return false;
        }
        
        Serial.println("[BLE] Step 3: Setting server callbacks");
        pServer->setCallbacks(new MyServerCallbacks());
        
        Serial.println("[BLE] Step 4: Creating BLE service");
        BLEService *pService = pServer->createService(serviceUUID);
        if (!pService) {
            Serial.println("[BLE] FAILED: Could not create BLE service");
            return false;
        }
        
        Serial.println("[BLE] Step 5: Creating BLE characteristic");
        pCharacteristic = pService->createCharacteristic(
                            characteristicUUID,
                            BLECharacteristic::PROPERTY_READ   |
                            BLECharacteristic::PROPERTY_WRITE  |
                            BLECharacteristic::PROPERTY_NOTIFY
                          );
                          
        if (!pCharacteristic) {
            Serial.println("[BLE] FAILED: Could not create BLE characteristic");
            return false;
        }
        
        Serial.println("[BLE] Step 6: Adding descriptor and callbacks");
        pCharacteristic->addDescriptor(new BLE2902());
        pCharacteristic->setCallbacks(new MyCallbacks());
        
        Serial.println("[BLE] Step 7: Starting service");
        pService->start();
        
        Serial.println("[BLE] Step 8: Starting advertising");
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(serviceUUID);
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);
        pAdvertising->setMaxPreferred(0x12);
        BLEDevice::startAdvertising();
        
        Serial.println("[BLE] ✓ BLE Server initialization COMPLETED successfully");
        Serial.println("[BLE] Device name: HikeSafe_" + nodeId);
        deviceConnected = false;
        return true;
        
    } catch (...) {
        Serial.println("[BLE] Exception during BLE initialization");
        return false;
    }
}

void loopBLE() {
    if (!ENABLE_BLE) return;
    
    if (!deviceConnected && oldDeviceConnected) {
        safeDelay(BLE_RECONNECT_DELAY);
        if (pServer) {
            pServer->startAdvertising();
            Serial.println("[BLE] Restarting advertising");
        }
        oldDeviceConnected = deviceConnected;
    }
    
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("[BLE] New device connected");
    }
}

void sendBLEMessage(String msg) {
    if (!ENABLE_BLE) return;
    
    if (deviceConnected && pCharacteristic) {
        try {
            pCharacteristic->setValue(msg.c_str());
            pCharacteristic->notify();
            Serial.println("[BLE TX] " + msg);
        } catch (...) {
            Serial.println("[BLE] Failed to send message: " + msg);
        }
    }
}

void handleBLECommand(String command) {
    if (!ENABLE_BLE) return;
    
    Serial.println("[BLE] Processing command: " + command);
    
    if (command == "STATUS") {
        sendSystemStatusToBLE();
    } 
    else if (command == "PING") {
        sendBLEMessage("PONG:" + getNodeId() + ",Battery:" + String(getBatteryLevel(), 1));
    } 
    else if (command == "RESET") {
        sendBLEMessage("ACK:RESET_IN_3_SECONDS");
        logEvent("SYSTEM", "Reset requested via BLE");
        safeDelay(3000);
        ESP.restart();
    }
    else {
        sendBLEMessage("ERROR:UNKNOWN_COMMAND:" + command);
    }
}

void sendSystemStatusToBLE() {
    if (!ENABLE_BLE) return;
    
    String status = "STATUS:";
    status += "Nodes=" + String(nearbyNodes.size());
    status += ",BLE=" + String(deviceConnected ? "1" : "0");
    status += ",LoRa=" + String(loraInitialized ? "1" : "0");
    status += ",Uptime=" + String((millis() - systemStartTime)/1000) + "s";
    status += ",SD=" + String(sdCardAvailable ? "1" : "0");
    status += ",SPIFFS=" + String(spiffsAvailable ? "1" : "0");
    status += ",Display=" + String(displayInitialized ? "1" : "0");
    status += ",Battery=" + String(getBatteryLevel(), 1) + "%";
    status += ",NodeID=" + nodeId;
    sendBLEMessage(status);
}

void sendNodeListToBLE() {
    if (!ENABLE_BLE) return;
    
    if (nearbyNodes.size() == 0) {
        sendBLEMessage("NODES:NONE");
        return;
    }
    
    sendBLEMessage("NODES:COUNT=" + String(nearbyNodes.size()));
    safeDelay(50);
    
    for (size_t i = 0; i < nearbyNodes.size(); i++) {
        const auto& node = nearbyNodes[i];
        String nodeInfo = "NODE:" + node.id + 
                         ",TTL=" + String(node.ttl) + 
                         ",RSSI=" + String(node.rssi) +
                         ",Age=" + String((millis() - node.timestamp)/1000) + "s";
        sendBLEMessage(nodeInfo);
        safeDelay(50);
    }
}

// ----------------- LoRa Implementation ----------------
bool setupLoRaMesh() {
    Serial.println("[LoRa] Initializing LoRa...");
    Serial.println("[LoRa] Using pins - SS:" + String(LORA_SS) + " RST:" + String(LORA_RST) + " DIO0:" + String(LORA_DIO0));
    
    // Reset LoRa module first
    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(100);
    
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    
    Serial.println("[LoRa] Attempting to begin with frequency: " + String(LORA_BAND));
    
    // Try multiple times with different approaches
    for (int attempt = 1; attempt <= 3; attempt++) {
        Serial.println("[LoRa] Attempt " + String(attempt) + " of 3");
        
        if (LoRa.begin(LORA_BAND)) {
            Serial.println("[LoRa] LoRa.begin() successful!");
            
            // Configure LoRa settings
            LoRa.setTxPower(20);
            Serial.println("[LoRa] TX Power set to 20");
            
            LoRa.setSpreadingFactor(8);
            Serial.println("[LoRa] Spreading Factor set to 8");
            
            LoRa.setSignalBandwidth(125E3);
            Serial.println("[LoRa] Bandwidth set to 125kHz");
            
            LoRa.setCodingRate4(5);
            Serial.println("[LoRa] Coding Rate set to 4/5");
            
            LoRa.enableCrc();
            Serial.println("[LoRa] CRC enabled");
            
            LoRa.setSyncWord(0x34);
            Serial.println("[LoRa] Sync word set to 0x34");
            
            loraInitialized = true;
            Serial.println("[LoRa] ✓ LoRa mesh initialized successfully!");
            return true;
        }
        
        Serial.println("[LoRa] Attempt " + String(attempt) + " failed");
        delay(1000);
    }
    
    Serial.println("[LoRa] ✗ Failed to initialize LoRa radio after 3 attempts!");
    Serial.println("[LoRa] Check your wiring:");
    Serial.println("[LoRa]   SS (NSS) -> GPIO " + String(LORA_SS));
    Serial.println("[LoRa]   RST      -> GPIO " + String(LORA_RST)); 
    Serial.println("[LoRa]   DIO0     -> GPIO " + String(LORA_DIO0));
    Serial.println("[LoRa]   SCK      -> GPIO 5");
    Serial.println("[LoRa]   MISO     -> GPIO 19");
    Serial.println("[LoRa]   MOSI     -> GPIO 27");
    Serial.println("[LoRa]   VCC      -> 3.3V");
    Serial.println("[LoRa]   GND      -> GND");
    
    updateDisplayStatus("LoRa FAILED");
    return false;
}

void loopLoRaMesh() {
    if (!loraInitialized) return;
    
    // Check for incoming packets
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        String incoming = "";
        while (LoRa.available()) {
            incoming += (char)LoRa.read();
        }
        int rssi = LoRa.packetRssi();
        
        Serial.println("[LoRa RX] " + incoming + " RSSI: " + String(rssi));
        handleLoRaMessage(incoming);
        updateLastMessage("RX: " + incoming.substring(0, 12));
    }
    
    // Periodic broadcast
    static unsigned long lastBroadcast = 0;
    if (millis() - lastBroadcast > TTL_BROADCAST_INTERVAL) {
        broadcastTTL();
        lastBroadcast = millis();
    }
}

void sendLoRaMessage(String msg) {
    if (!loraInitialized) return;
    
    Serial.println("[LoRa TX] " + msg);
    LoRa.beginPacket();
    LoRa.print(msg);
    LoRa.endPacket();
}

void handleLoRaMessage(String message) {
    Serial.println("[LoRa] Processing message: " + message);
    // Add message processing logic here
}

// ----------------- TTL System ----------------
bool setupTTL() {
    Serial.println("[TTL] TTL mesh system initialized");
    nearbyNodes.clear();
    return true;
}

void loopTTL() {
    // Clean up expired nodes
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup > TTL_CLEANUP_INTERVAL) {
        cleanupExpiredNodes();
        lastCleanup = millis();
    }
}

void broadcastTTL() {
    if (!loraInitialized) return;
    
    String message = "TTL:" + nodeId + ":" + String(MAX_TTL) + ":" + String(getBatteryLevel(), 1);
    sendLoRaMessage(message);
    Serial.println("[TTL] Broadcasting: " + message);
}

void cleanupExpiredNodes() {
    size_t originalSize = nearbyNodes.size();
    nearbyNodes.erase(
        std::remove_if(nearbyNodes.begin(), nearbyNodes.end(),
            [](const TTLMessage& node) {
                return (millis() - node.timestamp) > TTL_NODE_TIMEOUT;
            }),
        nearbyNodes.end()
    );
    
    if (nearbyNodes.size() != originalSize) {
        Serial.println("[TTL] Cleaned up " + String(originalSize - nearbyNodes.size()) + " expired nodes");
        updateNodeCount(nearbyNodes.size());
    }
}

// ----------------- Logging ----------------
void logEvent(String tag, String data) {
    String logEntry = String(millis()) + " [" + tag + "] " + data;
    Serial.println("[LOG] " + logEntry);
    
    // Try to save to storage if available
    if (isSPIFFSAvailable()) {
        File logFile = SPIFFS.open("/system.log", FILE_APPEND);
        if (logFile) {
            logFile.println(logEntry);
            logFile.close();
        }
    } else if (isSDAvailable()) {
        File logFile = SD.open("/system.log", FILE_APPEND);
        if (logFile) {
            logFile.println(logEntry);
            logFile.close();
        }
    }
}

// ----------------- Setup / Loop (NO BLE DEBUG VERSION) -------------------------
void setup() {
    Serial.begin(115200);
    safeDelay(3000); // Give more time
    Serial.println("\n\n=== HikeSafe System Starting (NO BLE DEBUG VERSION) ===");
    Serial.print("ESP32 Chip ID: ");
    Serial.println(ESP.getEfuseMac(), HEX);
    Serial.print("Free heap at start: ");
    Serial.println(ESP.getFreeHeap());
    
    systemStartTime = millis();
    resetWatchdog();

    Serial.println("[SETUP] Step 1: Hardware initialization");
    initializeHardware();
    resetWatchdog();
    
    Serial.println("[SETUP] Step 2: Storage initialization");
    initializeStorage();
    resetWatchdog();
    
    Serial.println("[SETUP] Step 3: Display initialization");
    updateDisplayStatus("Init Display...");
    bool displayOK = setupOLED();
    if (!displayOK) {
        Serial.println("[SETUP] WARNING: Display initialization failed!");
    } else {
        displaySplashScreen();
    }
    resetWatchdog();

    if (ENABLE_BLE) {
        Serial.println("[SETUP] Step 4: BLE initialization");
        updateDisplayStatus("Init BLE...");
        
        bool bleSuccess = setupBLE();
        
        if (!bleSuccess) {
            Serial.println("[SETUP] BLE initialization failed!");
            updateDisplayStatus("BLE FAILED");
            safeDelay(2000);
        } else {
            Serial.println("[SETUP] BLE initialization successful!");
            updateDisplayStatus("BLE Ready");
            safeDelay(1000);
        }
        resetWatchdog();
    } else {
        Serial.println("[SETUP] Step 4: BLE DISABLED for debugging");
        updateDisplayStatus("BLE DISABLED");
        safeDelay(2000);
    }

    Serial.println("[SETUP] Step 5: LoRa initialization");
    updateDisplayStatus("Init LoRa...");
    if (!setupLoRaMesh()) {
        Serial.println("[SETUP] LoRa initialization failed!");
        updateDisplayStatus("LoRa FAILED");
        safeDelay(2000);
    } else {
        updateDisplayStatus("LoRa Ready");
    }
    resetWatchdog();

    Serial.println("[SETUP] Step 6: TTL system initialization");
    setupTTL();

    systemReady = true;
    updateDisplayStatus("System Ready");
    updateLastMessage("All systems online");
    resetWatchdog();

    Serial.println("=== HikeSafe System Ready ===");
    Serial.println("Node ID: " + nodeId);
    Serial.println("Display: " + String(displayInitialized ? "OK" : "FAILED"));
    Serial.println("BLE: " + String(ENABLE_BLE ? "ENABLED" : "DISABLED"));
    Serial.println("LoRa: " + String(loraInitialized ? "OK" : "FAILED"));
    Serial.println("Storage: SD=" + String(sdCardAvailable ? "OK" : "FAIL") + 
                   " SPIFFS=" + String(spiffsAvailable ? "OK" : "FAIL"));
    Serial.print("Final free heap: ");
    Serial.println(ESP.getFreeHeap());
    
    logEvent("SYSTEM", "Initialization complete, Node ID: " + getNodeId());

    if (displayInitialized) {
        showMessage("HikeSafe", "System Ready!\nNode: " + nodeId.substring(0, 8) + "\nBLE: " + String(ENABLE_BLE ? "ON" : "OFF"), 3000);
    }
    
    Serial.println("[SETUP] Setup completed successfully!");
    Serial.println("[SETUP] System will now enter main loop...");
}

void loop() {
    resetWatchdog();
    
    // Check system health
    if (!checkSystemHealth()) {
        Serial.println("[SYSTEM] Health check failed - restarting");
        ESP.restart();
    }
    
    // Core communication loops (highest priority)
    if (ENABLE_BLE) {
        loopBLE();
    }
    loopLoRaMesh();
    loopTTL();
    
    // UI and storage (lower priority)
    loopOLED();
    loopStorage();
    
    // Check battery status periodically
    static unsigned long lastBatteryCheck = 0;
    if (millis() - lastBatteryCheck > 30000) { // Every 30 seconds
        checkBatteryStatus();
        lastBatteryCheck = millis();
    }

    // Handle BLE connection state changes (only if BLE enabled)
    if (ENABLE_BLE && deviceConnected != oldDeviceConnected) {
        if (deviceConnected) {
            updateBLEStatus(true);
            sendSystemStatusToBLE();
        } else {
            updateBLEStatus(false);
        }
        oldDeviceConnected = deviceConnected;
    }
    
    // Debug output every 10 seconds
    static unsigned long lastDebugOutput = 0;
    if (millis() - lastDebugOutput > 10000) {
        Serial.println("[DEBUG] System running - Uptime: " + String((millis() - systemStartTime)/1000) + "s, Free heap: " + String(ESP.getFreeHeap()));
        lastDebugOutput = millis();
    }
    
    safeDelay(50); // Slower loop for debugging
}

// ----------------- Helper Functions -----------------------
void updateDisplayStatus(String status) {
    currentStatus = status;
    Serial.println("[STATUS] " + status);
    if (displayInitialized) {
        updateDisplay();
    }
}

void updateLastMessage(String message) {
    lastMessage = message;
    if (displayInitialized) {
        updateDisplay();
    }
    Serial.println("[MSG] " + message);
}

void updateBLEStatus(bool connected) {
    if (!ENABLE_BLE) {
        bleConnected = false;
        updateDisplayStatus(loraInitialized ? "LoRa Mesh Only" : "LoRa Starting");
        return;
    }
    
    bleConnected = connected;
    if (connected) {
        updateDisplayStatus("App Connected");
    } else {
        updateDisplayStatus(loraInitialized ? "LoRa Mesh Active" : "No Connections");
    }
}

void updateNodeCount(int count) {
    connectedNodes = count;
    if (ENABLE_BLE && deviceConnected) {
        sendNodeListToBLE();
    }
}

void handleTTLMessage(String incoming, int rssi) {
    Serial.println("[TTL] Received: " + incoming + " RSSI: " + String(rssi));
}

void forwardTTL(String msg, int currentTTL) {
    Serial.println("[TTL] Forwarding: " + msg);
}

bool reopenSDCard() {
    Serial.println("[SD] Attempting to reopen SD card");
    return false;
}

// ===================== End of No BLE Debug Version =====================