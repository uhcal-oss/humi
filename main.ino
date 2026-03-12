  /* 
  | ***********************************
  | Kural 1:
  | Nereden Geldiğini Unutma!
  | Bu projede takılan her kablo Mustafa Kemal Atatürk'ün şerefine takılır.
  | Bu projede yazılan her Kod Melike Ebru'nun şerefine yazılır. 
  | Yalnız ve derinden yaşa UHÇAL Paşa!
  | ***********************************
  */
  
  /*
  | MIT License
  | 
  | Copyright (c) 2024-2025 HUMI v2 Plant Management System Contributors
  | - Batuhantrkgl (Batuhan Türkoğlu)
  | - Melike Ebru
  | - DrRuzgar (Rüzgar Burak Tunç)
  | 
  | Permission is hereby granted, free of charge, to any person obtaining a copy
  | of this software and associated documentation files (the "Software"), to deal
  | in the Software without restriction, including without limitation the rights
  | to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  | copies of the Software, and to permit persons to whom the Software is
  | furnished to do so, subject to the following conditions:
  | 
  | The above copyright notice and this permission notice shall be included in all
  | copies or substantial portions of the Software.
  | 
  | THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  | IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  | FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  | AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  | LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  | OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  | SOFTWARE.
  | 
  */
  
  /*
  | ***********************************
  | Contributors:
  | - Batuhantrkgl (2024-2025-2026 Season - 11.03.2026)
  | - Melike Ebru (2024-2025 Season - 15.07.2025)
  | - DrRuzgar (2024-2025 Season - 15.07.2025)
  | ***********************************
  | * HUMI v2 - Plant Management System API
  | * 
  | * Board: Arduino Uno R4 WiFi (Renesas RA4M1)
  | * Core: arduino:renesas_uno:unor4wifi
  | * 
  | * Features:
  | * - Built-in WiFi (ESP32-S3 module)
  | * - 48 MHz ARM Cortex-M4 processor
  | * - 32 KB RAM, 256 KB Flash
  | * - 12-bit ADC resolution
  | * - Native USB support
  | * 
  | * REST API Endpoints (all JSON):
  | * GET  /                        - Health check (JSON)
  | * GET  /health                  - Health check (JSON)
  | * GET  /api/status              - Complete system status with logs
  | * GET  /api/status/fast         - Fast status for polling
  | * GET  /api/plant/{id}/status   - Individual plant status with logs (id: 0-2)
  | * POST /api/plant/{id}/pump     - Toggle water pump for plant (auth required)
  | * POST /api/plant/{id}/fan      - Toggle fan for plant (auth required)
  | * GET  /api/thresholds          - Get moisture thresholds
  | * POST /api/thresholds          - Update thresholds (auth required, saved to SD)
  | * GET  /api/logs/export         - Export logs to SD card
  | * POST /api/system/restart      - Restart the system (auth required)
  | * POST /api/system/testmode     - Toggle test mode (auth required)
  | * GET  /api/email/settings      - Get email notification settings
  | * POST /api/email/settings      - Update email settings (auth required)
  | * POST /api/email/test          - Send test email (auth required)
  | * POST /api/email/status        - Send system status email (auth required)
  | * 
  | * Security:
  | * - Credentials loaded from config.txt on SD card (falls back to defaults)
  | * - POST endpoints require Bearer token if API_TOKEN is set in config.txt
  | * - CORS origin configurable via CORS_ORIGIN in config.txt
  | * - CORS preflight (OPTIONS) handled automatically
  | * - Rate limiting: 10 requests/second per client
  | *
  | * SD Card files:
  | *   config.txt  - KEY=VALUE settings (WIFI_SSID, WIFI_PASSWORD,
  | *                 MAILERSEND_TOKEN, FROM_EMAIL, TO_EMAIL, API_TOKEN, CORS_ORIGIN)
  | *   thresh.txt  - Persisted moisture thresholds (auto-saved on update)
  | *
  | * Features:
  | * - NTP time sync via WiFi (hourly resync)
  | * - Persistent thresholds across power cycles (SD card)
  | * - Watchdog timer (16s auto-reset on hang)
  | * 
  | * Plant IDs: 0=Martha(Yaprak Güzeli), 1=Menekşe, 2=Orkide
  | * 
  | * MailerSend Email Features:
  | * - Automatic alerts for low/high moisture levels
  | * - Sensor disconnection notifications
  | * - Daily status reports
  | * - Test email functionality
  | * - Configurable cooldown periods
  | ***********************************
  */
  
  #include <WiFiS3.h>
  #include <SD.h>
  #include <SPI.h>
  #include <WiFiSSLClient.h>
  #include <ArduinoHttpClient.h>
  #include <ArduinoJson.h>
  #include <WDT.h>
  
  // Wi-Fi network credentials (defaults - override via config.txt on SD card)
  char ssid[33] = "Turkoglu_Wi-Fi_2.4Ghz";
  char password[65] = "DDc57C8C9962b";
  
  // MailerSend Configuration (override via config.txt on SD card)
  char MAILERSEND_API_TOKEN[80] = "mlsn.b626ef284198582ebf69c22269cde80985189ec81f5118cb03945e801a04dfa8";
  const char* MAILERSEND_API_URL = "api.mailersend.com";
  const int MAILERSEND_API_PORT = 443;
  char FROM_EMAIL[65] = "humi@batuhantrkgl.tech";
  const char* FROM_NAME = "HUMI v2 Plant Automation System";
  char TO_EMAIL[65] = "batuhanturkoglu37@gmail.com";
  
  // API security (configure via config.txt on SD card)
  char API_AUTH_TOKEN[65] = "";   // Empty = no authentication required
  char CORS_ORIGIN[129] = "*";    // Restrict to your website domain for production
  
  // Create a server on port 80
  WiFiServer server(80);
  
  // SD card configuration and pin connections
  // SD card module to Arduino connections:
  // MOSI -> Pin 11 (Arduino SPI MOSI)
  // MISO -> Pin 12 (Arduino SPI MISO)
  // SCK  -> Pin 13 (Arduino SPI SCK)
  // CS   -> Pin 10 (Defined below as SD_CS_PIN)
  #define SD_CS_PIN 10 // CS pin for SD card module
  bool sdCardInitialized = false;
  const String LOGFILE_PREFIX = "plant_log_";
  const String LOGFILE_EXTENSION = ".txt";
  
  // Variables for uptime tracking
  unsigned long startTime;
  
  // Variables for internet connectivity status
  bool internetConnected = false;
  unsigned long lastConnectivityCheck = 0;
  const unsigned long connectivityCheckInterval = 60000; // Check internet connectivity every 60 seconds
  
  // MailerSend email notification configuration
  bool emailNotificationsEnabled = true;
  unsigned long lastEmailSent = 0;
  const unsigned long emailCooldownPeriod = 300000; // 5 minutes cooldown between emails
  const unsigned long criticalEmailCooldown = 60000; // 1 minute cooldown for critical alerts
  
  // Email notification types
  #define EMAIL_TYPE_MOISTURE_LOW 1
  #define EMAIL_TYPE_MOISTURE_HIGH 2
  #define EMAIL_TYPE_SENSOR_DISCONNECTED 3
  #define EMAIL_TYPE_SYSTEM_STATUS 4
  #define EMAIL_TYPE_CRITICAL_ERROR 5
  #define EMAIL_TYPE_SYSTEM_STARTUP 6
  #define EMAIL_TYPE_COUNT 7  // Total number of email types (0-6)
  
  // Global variables for soil moisture sensors
  #define SOIL_SENSORS_COUNT 3
  
  // Last email sent timestamps for each plant and type
  unsigned long lastEmailPerPlant[SOIL_SENSORS_COUNT][EMAIL_TYPE_COUNT] = {{0}};
  
  // Performance optimization variables
  unsigned long lastStatusUpdate = 0;
  const unsigned long statusCacheInterval = 1000; // Cache status for 1 second
  static String cachedEncryptionType = "";
  static unsigned long encryptionTypeCacheTime = 0;
  
  const int soilSensorPins[SOIL_SENSORS_COUNT] = {A0, A1, A2};
  bool soilSensorConnected[SOIL_SENSORS_COUNT] = {false, false, false};
  int soilMoistureValues[SOIL_SENSORS_COUNT] = {0, 0, 0};
  unsigned long lastSensorCheck = 0;
  const unsigned long sensorCheckInterval = 5000; // Check sensors every 5 seconds
  
  // Standard deviation based sensor detection
  #define READING_HISTORY_SIZE 10
  int sensorReadingHistory[SOIL_SENSORS_COUNT][READING_HISTORY_SIZE];
  int historyIndex[SOIL_SENSORS_COUNT] = {0, 0, 0};
  int historyCount[SOIL_SENSORS_COUNT] = {0, 0, 0};
  const float MAX_STDDEV_CONNECTED = 10.0;  // Max standard deviation for connected sensor
  const float MIN_STDDEV_DISCONNECTED = 50.0; // Min standard deviation for disconnected sensor
  
  // Test mode switch - set to true to bypass sensor detection
  bool TEST_MODE = false;
  
  // Startup detection timing
  bool sensorDetectionReady = false;
  bool initializationEmailSent = false;
  unsigned long systemStartTime = 0;
  const unsigned long SENSOR_DETECTION_DELAY = 10000; // Wait 10 seconds after startup
  
  // Define fan and pump pins for each plant
  const int pumpPins[SOIL_SENSORS_COUNT] = {2, 4, 6};  // D2, D4, D6 for pumps
  const int fanPins[SOIL_SENSORS_COUNT] = {3, 5, 7};   // D3, D5, D7 for fans
  
  // Soil moisture sensor calibration values
  const int SENSOR_RAW_DRY = 1023;    // Raw ADC reading when sensor is completely dry
  const int SENSOR_RAW_WET = 300;     // Raw ADC reading when sensor is fully submerged

  // Define moisture thresholds (in percent) — mutable, saved to SD
  int MOISTURE_TOO_LOW = 30;    // Below this value, turn on pump
  int MOISTURE_TOO_HIGH = 70;   // Above this value, turn on fan
  int MOISTURE_IDEAL_LOW = 40;  // Turn off pump when reaching this value
  int MOISTURE_IDEAL_HIGH = 60; // Turn off fan when reaching this value
  
  // Track fan and pump states
  bool pumpActive[SOIL_SENSORS_COUNT] = {false, false, false};
  bool fanActive[SOIL_SENSORS_COUNT] = {false, false, false};
  unsigned long lastControlCheck = 0;
  const unsigned long controlCheckInterval = 10000; // Check control logic every 10 seconds
  
  // Track previous moisture values and connection status for logging
  int previousMoistureValues[SOIL_SENSORS_COUNT] = {0, 0, 0};
  bool previousSensorConnected[SOIL_SENSORS_COUNT] = {false, false, false};
  bool previousPumpActive[SOIL_SENSORS_COUNT] = {false, false, false};
  bool previousFanActive[SOIL_SENSORS_COUNT] = {false, false, false};
  
  // Plant names for better logging
  const char* plantNames[SOIL_SENSORS_COUNT] = {"Martha (Yaprak Güzeli)", "Menekşe", "Orkide"};
  
  // Log system configuration
  #define MAX_LOGS_PER_PLANT 20
  #define LOG_TYPE_COMPONENT 1
  #define LOG_TYPE_HUMIDITY 2
  #define LOG_TYPE_CONNECTION 3
  
  // Console logging configuration and ANSI color codes
  #define CONSOLE_LOG_ENABLED true
  #define ANSI_COLORS_ENABLED false  // Set to true if your terminal supports ANSI colors
  
  // NTP time synchronization
  unsigned long ntpEpochTime = 0;       // Unix epoch from NTP
  unsigned long ntpSyncMillis = 0;      // millis() at last NTP sync
  unsigned long lastNtpSync = 0;        // millis() when last sync attempted
  const unsigned long ntpSyncInterval = 3600000; // Resync every hour
  
  // API rate limiting
  #define RATE_LIMIT_WINDOW 1000  // 1 second window
  #define RATE_LIMIT_MAX 10       // Max requests per window
  unsigned long rateLimitWindowStart = 0;
  int rateLimitCount = 0;
  #define LOG_LEVEL_INFO 1
  #define LOG_LEVEL_SUCCESS 2
  #define LOG_LEVEL_WARNING 3
  #define LOG_LEVEL_ERROR 4
  #define LOG_LEVEL_DEBUG 5
  
  // ANSI Color codes for fancy console output
  #define ANSI_COLOR_RESET "\033[0m"
  #define ANSI_COLOR_RED "\033[31m"
  #define ANSI_COLOR_GREEN "\033[32m"
  #define ANSI_COLOR_YELLOW "\033[33m"
  #define ANSI_COLOR_BLUE "\033[34m"
  #define ANSI_COLOR_MAGENTA "\033[35m"
  #define ANSI_COLOR_CYAN "\033[36m"
  #define ANSI_COLOR_WHITE "\033[37m"
  #define ANSI_COLOR_BRIGHT_BLACK "\033[90m"
  #define ANSI_COLOR_BRIGHT_RED "\033[91m"
  #define ANSI_COLOR_BRIGHT_GREEN "\033[92m"
  #define ANSI_COLOR_BRIGHT_YELLOW "\033[93m"
  #define ANSI_COLOR_BRIGHT_BLUE "\033[94m"
  #define ANSI_COLOR_BRIGHT_MAGENTA "\033[95m"
  #define ANSI_COLOR_BRIGHT_CYAN "\033[96m"
  #define ANSI_COLOR_BRIGHT_WHITE "\033[97m"
  
  // Text formatting
  #define ANSI_BOLD "\033[1m"
  
  // Log entry structure
  #define LOG_MESSAGE_MAX_LEN 64
  struct LogEntry {
    unsigned long timestamp;  // Timestamp when the log was created
    byte logType;            // Type of log (component, humidity, connection)
    char message[LOG_MESSAGE_MAX_LEN]; // Log message (fixed-size to avoid heap fragmentation)
  };
  
  // Array of log entries for each plant
  LogEntry plantLogs[SOIL_SENSORS_COUNT][MAX_LOGS_PER_PLANT];
  int logIndex[SOIL_SENSORS_COUNT] = {0, 0, 0}; // Current index for circular buffer
  int logCount[SOIL_SENSORS_COUNT] = {0, 0, 0}; // Count of logs for each plant
  
  // Function to log data to SD card
  void logToSD(int plantIndex, String message) {
    if (!sdCardInitialized) return;
    
    String fileName = LOGFILE_PREFIX + String(plantIndex) + LOGFILE_EXTENSION;
    File logFile = SD.open(fileName, FILE_WRITE);
    
    if (logFile) {
      // Format: [timestamp] message
      String logEntry = "[" + String(millis()) + "] " + message;
      logFile.println(logEntry);
      logFile.close();
      
      Serial.print("SD Log: ");
      Serial.println(logEntry);
    } else {
      Serial.print("Error opening log file: ");
      Serial.println(fileName);
    }
  }
  
  // Modified addLogEntry function to also log to SD
  void addLogEntry(int plantIndex, byte logType, String message) {
    // Get current index for this plant's logs
    int currentIndex = logIndex[plantIndex];
    
    // Store the log
    plantLogs[plantIndex][currentIndex].timestamp = millis();
    plantLogs[plantIndex][currentIndex].logType = logType;
    message.toCharArray(plantLogs[plantIndex][currentIndex].message, LOG_MESSAGE_MAX_LEN);
    
    // Update the index (circular buffer)
    logIndex[plantIndex] = (currentIndex + 1) % MAX_LOGS_PER_PLANT;
    
    // Update count
    if (logCount[plantIndex] < MAX_LOGS_PER_PLANT) {
      logCount[plantIndex]++;
    }
  
    // Also output to serial for debugging
    Serial.print(plantNames[plantIndex]);
    Serial.print(" - ");
    Serial.println(message);
    
    // Log to SD card if available
    if (sdCardInitialized) {
      String logMessage = String(plantNames[plantIndex]) + " - " + message;
      logToSD(plantIndex, logMessage);
    }
  }
  
  // Function to format relative time from milliseconds
  String formatTimeAgo(unsigned long timestamp) {
    unsigned long now = millis();
    unsigned long diff = now - timestamp;
    
    // Handle millis() overflow - rare but possible
    if (diff > now) {
      diff = now;
    }
    
    unsigned long seconds = diff / 1000;
    
    if (seconds < 60) {
      return String(seconds) + " seconds ago";
    } else if (seconds < 3600) {
      return String(seconds / 60) + " minutes ago";
    } else if (seconds < 86400) {
      return String(seconds / 3600) + " hours ago";
    } else {
      return String(seconds / 86400) + " days ago";
    }
  }
  
  // Fancy console logging functions
  void printHeader(String title) {
    if (!CONSOLE_LOG_ENABLED) return;
    
    Serial.println();
    if (ANSI_COLORS_ENABLED) {
      Serial.print(ANSI_BOLD ANSI_COLOR_BRIGHT_CYAN);
      Serial.println("╔════════════════════════════════════════════════════════════════╗");
      Serial.print("║" ANSI_COLOR_BRIGHT_WHITE "  HUMI v2 - " + title);
      
      // Pad with spaces to center the title
      int padding = 60 - title.length() - 10; // 10 = "HUMI v2 - ".length()
      for(int i = 0; i < padding; i++) {
        Serial.print(" ");
      }
      
      Serial.println(ANSI_COLOR_BRIGHT_CYAN "║");
      Serial.println("╚════════════════════════════════════════════════════════════════╝" ANSI_COLOR_RESET);
    } else {
      Serial.println("=====================================");
      Serial.println("  HUMI v2 - " + title);
      Serial.println("=====================================");
    }
    Serial.println();
  }
  
  void consoleLog(int level, String category, String message, int plantId = -1) {
    if (!CONSOLE_LOG_ENABLED) return;
    
    // Get current time
    unsigned long currentTime = millis();
    unsigned long seconds = currentTime / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    // Format timestamp
    char timeStr[12];
    snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu", hours % 24, minutes % 60, seconds % 60);
    
    // Color and icon based on log level
    String levelIcon = "";
    String levelColor = "";
    String levelName = "";
    
    switch(level) {
      case LOG_LEVEL_INFO:
        levelIcon = "[I]";
        levelColor = ANSI_COLORS_ENABLED ? ANSI_COLOR_BRIGHT_BLUE : "";
        levelName = "INFO ";
        break;
      case LOG_LEVEL_SUCCESS:
        levelIcon = "[OK]";
        levelColor = ANSI_COLORS_ENABLED ? ANSI_COLOR_BRIGHT_GREEN : "";
        levelName = "SUCC ";
        break;
      case LOG_LEVEL_WARNING:
        levelIcon = "[!]";
        levelColor = ANSI_COLORS_ENABLED ? ANSI_COLOR_BRIGHT_YELLOW : "";
        levelName = "WARN ";
        break;
      case LOG_LEVEL_ERROR:
        levelIcon = "[ERR]";
        levelColor = ANSI_COLORS_ENABLED ? ANSI_COLOR_BRIGHT_RED : "";
        levelName = "ERR  ";
        break;
      case LOG_LEVEL_DEBUG:
        levelIcon = "[DBG]";
        levelColor = ANSI_COLORS_ENABLED ? ANSI_COLOR_BRIGHT_MAGENTA : "";
        levelName = "DEBUG";
        break;
      default:
        levelIcon = "[LOG]";
        levelColor = ANSI_COLORS_ENABLED ? ANSI_COLOR_WHITE : "";
        levelName = "LOG  ";
    }
    
    // Print formatted log entry
    if (ANSI_COLORS_ENABLED) {
      Serial.print(ANSI_COLOR_BRIGHT_BLACK "[" + String(timeStr) + "] " ANSI_COLOR_RESET);
      Serial.print(levelColor);
      Serial.print(ANSI_BOLD);
      Serial.print(levelName);
      Serial.print(ANSI_COLOR_RESET " ");
      Serial.print(levelIcon + " ");
      
      // Category with color
      Serial.print(ANSI_COLOR_CYAN ANSI_BOLD);
      Serial.print(category);
      Serial.print(ANSI_COLOR_RESET ": ");
      
      // Plant name if specified
      if (plantId >= 0 && plantId < SOIL_SENSORS_COUNT) {
        Serial.print(ANSI_COLOR_BRIGHT_MAGENTA "[");
        Serial.print(plantNames[plantId]);
        Serial.print("] " ANSI_COLOR_RESET);
      }
    } else {
      // Plain text format for terminals that don't support ANSI
      Serial.print("[" + String(timeStr) + "] ");
      Serial.print(levelName + " ");
      Serial.print(levelIcon + " ");
      Serial.print(category + ": ");
      
      // Plant name if specified
      if (plantId >= 0 && plantId < SOIL_SENSORS_COUNT) {
        Serial.print("[" + String(plantNames[plantId]) + "] ");
      }
    }
    
    // Message
    Serial.println(message);
  }
  
  void printSystemStatus() {
    if (!CONSOLE_LOG_ENABLED) return;
    
    Serial.println();
    if (ANSI_COLORS_ENABLED) {
      Serial.println(ANSI_BOLD ANSI_COLOR_BRIGHT_CYAN "┌─────────────────────── SYSTEM STATUS ───────────────────────┐" ANSI_COLOR_RESET);
    } else {
      Serial.println("=================== SYSTEM STATUS ===================");
    }
    
    // WiFi Status
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (ANSI_COLORS_ENABLED) {
      Serial.print(ANSI_COLOR_BRIGHT_WHITE "│ WiFi: ");
      if (wifiConnected) {
        Serial.print(ANSI_COLOR_BRIGHT_GREEN "Connected " ANSI_COLOR_RESET);
        Serial.print(ANSI_COLOR_BRIGHT_WHITE "(" + WiFi.localIP().toString() + ")");
        Serial.print("                    │");
      } else {
        Serial.print(ANSI_COLOR_BRIGHT_RED "Disconnected");
        Serial.print("                                        │");
      }
      Serial.println(ANSI_COLOR_RESET);
    } else {
      Serial.print("WiFi: ");
      if (wifiConnected) {
        Serial.println("Connected (" + WiFi.localIP().toString() + ")");
      } else {
        Serial.println("Disconnected");
      }
    }
    
    // Internet Status
    if (ANSI_COLORS_ENABLED) {
      Serial.print(ANSI_COLOR_BRIGHT_WHITE "│ Internet: ");
      if (internetConnected) {
        Serial.print(ANSI_COLOR_BRIGHT_GREEN "Connected");
        Serial.print("                                       │");
      } else {
        Serial.print(ANSI_COLOR_BRIGHT_RED "Disconnected");
        Serial.print("                                    │");
      }
      Serial.println(ANSI_COLOR_RESET);
    } else {
      Serial.print("Internet: ");
      Serial.println(internetConnected ? "Connected" : "Disconnected");
    }
    
    // SD Card Status
    if (ANSI_COLORS_ENABLED) {
      Serial.print(ANSI_COLOR_BRIGHT_WHITE "│ SD Card: ");
      if (sdCardInitialized) {
        Serial.print(ANSI_COLOR_BRIGHT_GREEN "Ready");
        Serial.print("                                           │");
      } else {
        Serial.print(ANSI_COLOR_BRIGHT_RED "Not Available");
        Serial.print("                                   │");
      }
      Serial.println(ANSI_COLOR_RESET);
    } else {
      Serial.print("SD Card: ");
      Serial.println(sdCardInitialized ? "Ready" : "Not Available");
    }
    
    // Memory Status
    int freeMemory = getFreeMemory();
    if (ANSI_COLORS_ENABLED) {
      Serial.print(ANSI_COLOR_BRIGHT_WHITE "│ Free Memory: ");
      Serial.print(ANSI_COLOR_BRIGHT_CYAN + String(freeMemory) + " bytes");
      
      // Pad the line
      String memStr = String(freeMemory) + " bytes";
      int padding = 37 - memStr.length();
      for(int i = 0; i < padding; i++) {
        Serial.print(" ");
      }
      Serial.println("│" ANSI_COLOR_RESET);
    } else {
      Serial.println("Free Memory: " + String(freeMemory) + " bytes");
    }
    
    // Uptime
    unsigned long uptime = (millis() - startTime) / 1000;
    unsigned long days = uptime / 86400;
    unsigned long hours = (uptime % 86400) / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    unsigned long seconds = uptime % 60;
    
    if (ANSI_COLORS_ENABLED) {
      Serial.print(ANSI_COLOR_BRIGHT_WHITE "│ Uptime: ");
      Serial.print(ANSI_COLOR_BRIGHT_YELLOW);
      if (days > 0) {
        Serial.print(String(days) + "d ");
      }
      Serial.print(String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s");
      
      // Calculate padding for uptime
      String uptimeStr = "";
      if (days > 0) uptimeStr += String(days) + "d ";
      uptimeStr += String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
      int uptimePadding = 42 - uptimeStr.length();
      for(int i = 0; i < uptimePadding; i++) {
        Serial.print(" ");
      }
      Serial.println("│" ANSI_COLOR_RESET);
      
      Serial.println(ANSI_BOLD ANSI_COLOR_BRIGHT_CYAN "└──────────────────────────────────────────────────────────────┘" ANSI_COLOR_RESET);
    } else {
      Serial.print("Uptime: ");
      if (days > 0) {
        Serial.print(String(days) + "d ");
      }
      Serial.println(String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s");
      Serial.println("======================================================");
    }
    Serial.println();
  }
  
  void printPlantStatus() {
    if (!CONSOLE_LOG_ENABLED) return;
    
    if (ANSI_COLORS_ENABLED) {
      Serial.println(ANSI_BOLD ANSI_COLOR_BRIGHT_GREEN "┌─────────────────────── PLANT STATUS ────────────────────────┐" ANSI_COLOR_RESET);
    } else {
      Serial.println("=================== PLANT STATUS ===================");
    }
    
    for(int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      // Plant name
      String plantEmoji = "";
      String plantName = String(plantNames[i]);
      
      int moisturePercent = getMoisturePercent(i);
      
      if (ANSI_COLORS_ENABLED) {
        Serial.print(ANSI_COLOR_BRIGHT_WHITE "│ " + plantEmoji);
        Serial.print(ANSI_COLOR_BRIGHT_MAGENTA ANSI_BOLD + plantName + ANSI_COLOR_RESET);
        
        // Pad plant name
        int namePadding = 20 - plantName.length();
        for(int j = 0; j < namePadding; j++) {
          Serial.print(" ");
        }
        
        // Connection status
        if (soilSensorConnected[i]) {
          Serial.print(ANSI_COLOR_BRIGHT_GREEN "CONN");
        } else {
          Serial.print(ANSI_COLOR_BRIGHT_RED "DISC");
        }
        
        // Moisture level
        Serial.print(ANSI_COLOR_BRIGHT_WHITE " M:");
        if (moisturePercent > 60) {
          Serial.print(ANSI_COLOR_BRIGHT_BLUE);
        } else if (moisturePercent > 30) {
          Serial.print(ANSI_COLOR_BRIGHT_YELLOW);
        } else {
          Serial.print(ANSI_COLOR_BRIGHT_RED);
        }
        
        char moistureStr[8];
        snprintf(moistureStr, sizeof(moistureStr), "%3d%%", moisturePercent);
        Serial.print(moistureStr);
        
        // Controls status
        Serial.print(ANSI_COLOR_BRIGHT_WHITE " F:");
        Serial.print(fanActive[i] ? ANSI_COLOR_BRIGHT_GREEN "ON " : ANSI_COLOR_BRIGHT_BLACK "OFF");
        
        Serial.print(ANSI_COLOR_BRIGHT_WHITE " P:");
        Serial.print(pumpActive[i] ? ANSI_COLOR_BRIGHT_GREEN "ON " : ANSI_COLOR_BRIGHT_BLACK "OFF");
        
        Serial.println(ANSI_COLOR_BRIGHT_WHITE " │" ANSI_COLOR_RESET);
      } else {
        // Plain text format
        Serial.print(plantName);
        Serial.print(" | ");
        Serial.print(soilSensorConnected[i] ? "CONNECTED" : "DISCONNECTED");
        Serial.print(" | Moisture: ");
        Serial.print(moisturePercent);
        Serial.print("% | Fan: ");
        Serial.print(fanActive[i] ? "ON" : "OFF");
        Serial.print(" | Pump: ");
        Serial.println(pumpActive[i] ? "ON" : "OFF");
      }
    }
    
    if (ANSI_COLORS_ENABLED) {
      Serial.println(ANSI_BOLD ANSI_COLOR_BRIGHT_GREEN "└──────────────────────────────────────────────────────────────┘" ANSI_COLOR_RESET);
    } else {
      Serial.println("====================================================");
    }
    Serial.println();
  }
  
  // Load configuration from SD card config.txt
  // Format: KEY=VALUE (one per line, # for comments)
  // Supported keys: WIFI_SSID, WIFI_PASSWORD, MAILERSEND_TOKEN,
  //   FROM_EMAIL, TO_EMAIL, API_TOKEN, CORS_ORIGIN
  void loadConfigFromSD() {
    if (!sdCardInitialized) {
      consoleLog(LOG_LEVEL_WARNING, "CONFIG", "SD card not available - using default config");
      return;
    }

    File configFile = SD.open("config.txt");
    if (!configFile) {
      consoleLog(LOG_LEVEL_WARNING, "CONFIG", "config.txt not found on SD - using defaults");
      return;
    }

    consoleLog(LOG_LEVEL_INFO, "CONFIG", "Loading configuration from SD card...");
    int loaded = 0;

    while (configFile.available()) {
      String line = configFile.readStringUntil('\n');
      line.trim();

      if (line.length() == 0 || line.startsWith("#") || line.startsWith("//")) continue;

      int eqPos = line.indexOf('=');
      if (eqPos < 1) continue;

      String key = line.substring(0, eqPos);
      String value = line.substring(eqPos + 1);
      key.trim();
      value.trim();

      if (key == "WIFI_SSID" && value.length() > 0 && value.length() < sizeof(ssid)) {
        value.toCharArray(ssid, sizeof(ssid));
        loaded++;
      } else if (key == "WIFI_PASSWORD" && value.length() > 0 && value.length() < sizeof(password)) {
        value.toCharArray(password, sizeof(password));
        loaded++;
      } else if (key == "MAILERSEND_TOKEN" && value.length() > 0 && value.length() < sizeof(MAILERSEND_API_TOKEN)) {
        value.toCharArray(MAILERSEND_API_TOKEN, sizeof(MAILERSEND_API_TOKEN));
        loaded++;
      } else if (key == "FROM_EMAIL" && value.length() > 0 && value.length() < sizeof(FROM_EMAIL)) {
        value.toCharArray(FROM_EMAIL, sizeof(FROM_EMAIL));
        loaded++;
      } else if (key == "TO_EMAIL" && value.length() > 0 && value.length() < sizeof(TO_EMAIL)) {
        value.toCharArray(TO_EMAIL, sizeof(TO_EMAIL));
        loaded++;
      } else if (key == "API_TOKEN" && value.length() > 0 && value.length() < sizeof(API_AUTH_TOKEN)) {
        value.toCharArray(API_AUTH_TOKEN, sizeof(API_AUTH_TOKEN));
        loaded++;
      } else if (key == "CORS_ORIGIN" && value.length() > 0 && value.length() < sizeof(CORS_ORIGIN)) {
        value.toCharArray(CORS_ORIGIN, sizeof(CORS_ORIGIN));
        loaded++;
      }
    }

    configFile.close();
    consoleLog(LOG_LEVEL_SUCCESS, "CONFIG", "Loaded " + String(loaded) + " settings from config.txt");
  }
  
  // Save moisture thresholds to SD card
  void saveThresholdsToSD() {
    if (!sdCardInitialized) return;
    SD.remove("thresh.txt");
    File f = SD.open("thresh.txt", FILE_WRITE);
    if (f) {
      f.println(String("TOO_LOW=") + String(MOISTURE_TOO_LOW));
      f.println(String("TOO_HIGH=") + String(MOISTURE_TOO_HIGH));
      f.println(String("IDEAL_LOW=") + String(MOISTURE_IDEAL_LOW));
      f.println(String("IDEAL_HIGH=") + String(MOISTURE_IDEAL_HIGH));
      f.close();
      consoleLog(LOG_LEVEL_SUCCESS, "CONFIG", "Thresholds saved to SD card");
    }
  }
  
  // Load moisture thresholds from SD card
  void loadThresholdsFromSD() {
    if (!sdCardInitialized) return;
    File f = SD.open("thresh.txt");
    if (!f) return;
    
    int loaded = 0;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      int eq = line.indexOf('=');
      if (eq < 1) continue;
      String key = line.substring(0, eq);
      int val = line.substring(eq + 1).toInt();
      if (val < 0 || val > 100) continue;
      
      if (key == "TOO_LOW") { MOISTURE_TOO_LOW = val; loaded++; }
      else if (key == "TOO_HIGH") { MOISTURE_TOO_HIGH = val; loaded++; }
      else if (key == "IDEAL_LOW") { MOISTURE_IDEAL_LOW = val; loaded++; }
      else if (key == "IDEAL_HIGH") { MOISTURE_IDEAL_HIGH = val; loaded++; }
    }
    f.close();
    if (loaded > 0) {
      consoleLog(LOG_LEVEL_SUCCESS, "CONFIG", "Loaded " + String(loaded) + " thresholds from SD card");
    }
  }
  
  // Get current Unix epoch time (NTP-corrected if available)
  unsigned long getCurrentEpoch() {
    if (ntpEpochTime == 0) return 0; // No NTP sync yet
    return ntpEpochTime + ((millis() - ntpSyncMillis) / 1000);
  }
  
  // Sync time from NTP via WiFi
  void syncNTP() {
    if (WiFi.status() != WL_CONNECTED) return;
    unsigned long epoch = WiFi.getTime();
    if (epoch > 1000000000UL) { // Sanity check: after ~2001
      ntpEpochTime = epoch;
      ntpSyncMillis = millis();
      consoleLog(LOG_LEVEL_SUCCESS, "NTP", "Time synced: epoch=" + String(epoch));
    } else {
      consoleLog(LOG_LEVEL_WARNING, "NTP", "NTP sync failed (got " + String(epoch) + ")");
    }
  }

  void setup() {
    // Initialize serial communication
    Serial.begin(9600);
    while (!Serial) {
      ; // Wait for serial port to connect
    }
    
    // Clear screen and print fancy header
    Serial.print("\033[2J\033[H"); // Clear screen and move cursor to top
    printHeader("Plant Management System Starting Up");
    
    consoleLog(LOG_LEVEL_INFO, "SYSTEM", "Initializing HUMI v2...");
    
    // Initialize SD card
    consoleLog(LOG_LEVEL_INFO, "SD_CARD", "Initializing SD card...");
    pinMode(SD_CS_PIN, OUTPUT);
    
    if (SD.begin(SD_CS_PIN)) {
      consoleLog(LOG_LEVEL_SUCCESS, "SD_CARD", "SD card initialized successfully");
      sdCardInitialized = true;
      
      // Create a system info file with reset details
      File infoFile = SD.open("system.txt", FILE_WRITE);
      if (infoFile) {
        infoFile.println("System initialized at: " + String(millis()));
        infoFile.println("HUMI v2 - Plant Management System");
        infoFile.close();
        consoleLog(LOG_LEVEL_DEBUG, "SD_CARD", "System info file created");
      }
    } else {
      consoleLog(LOG_LEVEL_ERROR, "SD_CARD", "SD card initialization failed!");
      sdCardInitialized = false;
    }
  
    // Load credentials and settings from SD card (overrides defaults)
    loadConfigFromSD();
    loadThresholdsFromSD();

    consoleLog(LOG_LEVEL_INFO, "NETWORK", "Attempting to connect to Wi-Fi network: " + String(ssid));
  
    // Store the start time for uptime calculation and sensor detection
    startTime = millis();
    systemStartTime = startTime;
  
    // Disconnect from any previous connection
    WiFi.disconnect();
    delay(1000);
    
    // Attempt to connect to Wi-Fi network
    WiFi.begin(ssid, password);
    
    // Wait for connection with timeout
    int timeout = 0;
    consoleLog(LOG_LEVEL_INFO, "NETWORK", "Connecting to WiFi");
    Serial.print(ANSI_COLOR_BRIGHT_YELLOW "      ");
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(1000);
      Serial.print("●");
      timeout++;
    }
    Serial.println(ANSI_COLOR_RESET);
  
    if (WiFi.status() == WL_CONNECTED) {
      // Give DHCP a moment to assign the IP address
      delay(2000);
      
      consoleLog(LOG_LEVEL_SUCCESS, "NETWORK", "Wi-Fi connected successfully!");
      
      // Print detailed connection info
      consoleLog(LOG_LEVEL_INFO, "NETWORK", "IP address: " + WiFi.localIP().toString());
      consoleLog(LOG_LEVEL_INFO, "NETWORK", "Subnet mask: " + WiFi.subnetMask().toString());
      consoleLog(LOG_LEVEL_INFO, "NETWORK", "Gateway IP: " + WiFi.gatewayIP().toString());
      
      // Print MAC address
      byte mac[6];
      WiFi.macAddress(mac);
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
               mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
      consoleLog(LOG_LEVEL_INFO, "NETWORK", "MAC address: " + String(macStr));
  
      // Start the web server
      server.begin();
      consoleLog(LOG_LEVEL_SUCCESS, "SERVER", "Web server started on port 80");
      consoleLog(LOG_LEVEL_INFO, "API", "API available at http://" + WiFi.localIP().toString() + "/api/status");
      
      // Initial connectivity check
      checkInternetConnection();
      
      // Initial NTP time sync
      syncNTP();
    } else {
      consoleLog(LOG_LEVEL_ERROR, "NETWORK", "Failed to connect to Wi-Fi! Check credentials or network.");
    }
  
    // Initialize plant hardware
    consoleLog(LOG_LEVEL_INFO, "HARDWARE", "Initializing plant control hardware...");
    for(int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      pinMode(soilSensorPins[i], INPUT);
      
      // Configure pump and fan pins as outputs
      pinMode(pumpPins[i], OUTPUT);
      pinMode(fanPins[i], OUTPUT);
      
      // Initialize pumps and fans to OFF
      digitalWrite(pumpPins[i], LOW);
      digitalWrite(fanPins[i], LOW);
      
      // Initialize sensor reading history
      for(int j = 0; j < READING_HISTORY_SIZE; j++) {
        sensorReadingHistory[i][j] = 0;
      }
      historyIndex[i] = 0;
      historyCount[i] = 0;
      
      consoleLog(LOG_LEVEL_SUCCESS, "HARDWARE", "Plant " + String(i + 1) + " initialized - Pump: D" + String(pumpPins[i]) + ", Fan: D" + String(fanPins[i]), i);
    }
    
    // Log test mode status
    if (TEST_MODE) {
      consoleLog(LOG_LEVEL_WARNING, "SYSTEM", "TEST_MODE is ENABLED - sensor detection bypassed");
    } else {
      consoleLog(LOG_LEVEL_INFO, "SYSTEM", "Standard deviation sensor detection active (stddev<=" + String(MAX_STDDEV_CONNECTED) + " = connected)");
    }
    
    // Initialize logs for all plants
    for (int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      addLogEntry(i, LOG_TYPE_CONNECTION, "System initialized");
    }
    
    // Log email configuration status
    consoleLog(LOG_LEVEL_INFO, "EMAIL", "Email notifications: " + String(emailNotificationsEnabled ? "ENABLED" : "DISABLED"));
    consoleLog(LOG_LEVEL_INFO, "EMAIL", "MailerSend API configured for: " + String(TO_EMAIL));
    if (String(MAILERSEND_API_TOKEN) == "YOUR_MAILERSEND_API_TOKEN") {
      consoleLog(LOG_LEVEL_WARNING, "EMAIL", "Please configure MAILERSEND_API_TOKEN in the code!");
    }
    if (String(FROM_EMAIL) == "noreply@yourdomain.com") {
      consoleLog(LOG_LEVEL_WARNING, "EMAIL", "Please configure FROM_EMAIL with your verified domain!");
    }
    
    // Print final status
    printSystemStatus();
    printPlantStatus();
    
    consoleLog(LOG_LEVEL_SUCCESS, "SYSTEM", "HUMI v2 initialization complete! System ready to serve plants!");
    
    // Initialize watchdog timer (16 seconds timeout)
    // If the main loop hangs for >16s, the WDT auto-resets the board
    if (WDT.begin(16000)) {
      consoleLog(LOG_LEVEL_SUCCESS, "SYSTEM", "Watchdog timer enabled (16s timeout)");
    } else {
      consoleLog(LOG_LEVEL_WARNING, "SYSTEM", "Watchdog timer failed to initialize");
    }
    
    // Send startup email notification
    delay(2000); // Wait a moment for WiFi to stabilize
    if (WiFi.status() == WL_CONNECTED && internetConnected) {
      consoleLog(LOG_LEVEL_INFO, "EMAIL", "Sending system startup notification...");
      sendSystemStartupEmail();
    } else {
      consoleLog(LOG_LEVEL_WARNING, "EMAIL", "Startup email not sent - WiFi/Internet not available");
    }
  }
  
  void loop() {
    WDT.refresh(); // Reset watchdog timer each loop iteration
    digitalWrite(LED_BUILTIN, HIGH);
    
    // Check if still connected
    if (WiFi.status() != WL_CONNECTED) {
      consoleLog(LOG_LEVEL_WARNING, "NETWORK", "Wi-Fi connection lost! Attempting to reconnect...");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
      
      int attempt = 0;
      Serial.print(ANSI_COLOR_BRIGHT_YELLOW "      Reconnecting ");
      while (WiFi.status() != WL_CONNECTED && attempt < 10) {
        delay(1000);
        Serial.print("●");
        attempt++;
      }
      Serial.println(ANSI_COLOR_RESET);
      
      if (WiFi.status() == WL_CONNECTED) {
        delay(2000); // Wait for IP assignment
        consoleLog(LOG_LEVEL_SUCCESS, "NETWORK", "Reconnected to Wi-Fi! New IP: " + WiFi.localIP().toString());
      } else {
        consoleLog(LOG_LEVEL_ERROR, "NETWORK", "Failed to reconnect after 10 attempts");
      }
    }
    
    // Check internet connectivity periodically
    unsigned long currentMillis = millis();
    if (currentMillis - lastConnectivityCheck >= connectivityCheckInterval) {
      checkInternetConnection();
      lastConnectivityCheck = currentMillis;
    }
    
    // Resync NTP time periodically
    if (currentMillis - lastNtpSync >= ntpSyncInterval) {
      syncNTP();
      lastNtpSync = currentMillis;
    }
    
    // Check soil moisture sensors periodically
    if (currentMillis - lastSensorCheck >= sensorCheckInterval) {
      checkSoilSensors();
      lastSensorCheck = currentMillis;
    }
    
    // Run control logic for fans and pumps periodically
    if (currentMillis - lastControlCheck >= controlCheckInterval) {
      controlPlantsEnvironment();
      lastControlCheck = currentMillis;
    }
    
    // Handle incoming client requests
    WiFiClient client = server.available();
    if (client) {
      handleClient(client);
    }
    
    // Check for daily status email (every 24 hours)
    static unsigned long lastDailyEmail = millis(); // Initialize to current time to avoid sending immediately on startup
    const unsigned long dailyEmailInterval = 86400000; // 24 hours in milliseconds
    if (emailNotificationsEnabled && internetConnected && 
        (currentMillis - lastDailyEmail > dailyEmailInterval)) {
      consoleLog(LOG_LEVEL_INFO, "EMAIL", "Sending daily status email...");
      sendSystemStatusEmail();
      lastDailyEmail = currentMillis;
    }
    
    delay(100); // Reduced from 10000ms for faster response time
  }
  
  void checkInternetConnection() {
    // Ping Google's DNS to check internet connectivity
    IPAddress ip(8, 8, 8, 8);
    consoleLog(LOG_LEVEL_DEBUG, "NETWORK", "Checking internet connectivity...");
    internetConnected = WiFi.ping(ip) > 0;
    
    if (internetConnected) {
      consoleLog(LOG_LEVEL_SUCCESS, "NETWORK", "Internet connection verified");
    } else {
      consoleLog(LOG_LEVEL_WARNING, "NETWORK", "Internet connection failed");
    }
  }
  
  void handleClient(WiFiClient& client) {
    consoleLog(LOG_LEVEL_INFO, "API", "New client connected");
    
    // Read the first line of the HTTP request
    String request = client.readStringUntil('\r');
    client.read(); // Discard '\n'
    consoleLog(LOG_LEVEL_DEBUG, "API", "Request: " + request);
    
    // Read HTTP headers and extract Content-Length and Authorization
    int contentLength = 0;
    String authToken = "";
    String headerLine;
    while (client.available()) {
      headerLine = client.readStringUntil('\r');
      client.read(); // Discard '\n'
      if (headerLine.length() <= 1) break; // Empty line = end of headers
      if (headerLine.startsWith("Content-Length:") || headerLine.startsWith("content-length:")) {
        contentLength = headerLine.substring(headerLine.indexOf(':') + 1).toInt();
      }
      if (headerLine.startsWith("Authorization:") || headerLine.startsWith("authorization:")) {
        authToken = headerLine.substring(headerLine.indexOf(':') + 1);
        authToken.trim();
        if (authToken.startsWith("Bearer ") || authToken.startsWith("bearer ")) {
          authToken = authToken.substring(7);
          authToken.trim();
        }
      }
    }
    
    // Read POST body if present
    String requestBody = "";
    if (contentLength > 0 && contentLength < 1024) { // Cap at 1KB for safety
      unsigned long bodyTimeout = millis() + 2000;
      while (requestBody.length() < (unsigned int)contentLength && millis() < bodyTimeout) {
        if (client.available()) {
          requestBody += char(client.read());
        }
      }
    }
    
    // Extract the path and method from the request
    String method = request.substring(0, request.indexOf(' '));
    String path = request.substring(request.indexOf(' ') + 1);
    path = path.substring(0, path.indexOf(' '));
    
    consoleLog(LOG_LEVEL_DEBUG, "API", method + " " + path);
    
    // Rate limiting: reject if too many requests in the current window
    unsigned long now = millis();
    if (now - rateLimitWindowStart > RATE_LIMIT_WINDOW) {
      rateLimitWindowStart = now;
      rateLimitCount = 1;
    } else {
      rateLimitCount++;
      if (rateLimitCount > RATE_LIMIT_MAX) {
        sendErrorResponse(client, 429, "Too many requests - try again later");
        client.stop();
        consoleLog(LOG_LEVEL_WARNING, "API", "Rate limit exceeded");
        return;
      }
    }
    
    // Handle CORS preflight requests
    if (method == "OPTIONS") {
      client.println("HTTP/1.1 204 No Content");
      client.print("Access-Control-Allow-Origin: ");
      client.println(CORS_ORIGIN);
      client.println("Access-Control-Allow-Methods: GET, POST, OPTIONS");
      client.println("Access-Control-Allow-Headers: Content-Type, Authorization");
      client.println("Access-Control-Max-Age: 86400");
      client.println("Connection: close");
      client.println();
      client.stop();
      return;
    }
    
    // Authenticate POST/PUT/DELETE requests if API_AUTH_TOKEN is configured
    if ((method == "POST" || method == "PUT" || method == "DELETE") && strlen(API_AUTH_TOKEN) > 0) {
      if (authToken != String(API_AUTH_TOKEN)) {
        sendErrorResponse(client, 401, "Unauthorized - valid API token required");
        client.stop();
        consoleLog(LOG_LEVEL_WARNING, "API", "Unauthorized request blocked: " + method + " " + path);
        return;
      }
    }
    
    // Process the request based on the path
    if (path == "/api/status" || path == "/api/status/") {
      sendStatusResponse(client);
    } else if (path == "/api/status/fast") {
      sendFastStatusResponse(client);
    } else if (path.startsWith("/api/plant/") && path.endsWith("/status")) {
      handlePlantStatusRequest(client, path);
    } else if (path.startsWith("/api/plant/") && method == "POST") {
      handlePlantControl(client, path);
    } else if (path == "/api/thresholds" && method == "GET") {
      sendThresholdsResponse(client);
    } else if (path == "/api/thresholds" && method == "POST") {
      handleThresholdUpdate(client, requestBody);
    } else if (path == "/api/logs/export") {
      handleLogExport(client);
    } else if (path == "/api/system/restart" && method == "POST") {
      handleSystemRestart(client);
    } else if (path == "/api/system/testmode" && method == "POST") {
      handleTestModeToggle(client);
    } else if (path == "/api/email/settings" && method == "GET") {
      handleEmailSettingsRequest(client);
    } else if (path == "/api/email/settings" && method == "POST") {
      handleEmailSettingsUpdate(client, requestBody);
    } else if (path == "/api/email/test" && method == "POST") {
      handleTestEmail(client);
    } else if (path == "/api/email/status" && method == "POST") {
      handleStatusEmail(client);
    } else if (path == "/" || path == "/health") {
      // JSON health endpoint
      sendJsonHeaders(client);
      client.print("{\"status\":\"ok\",\"project\":\"HUMI v2 - Plant Management System\",\"uptime\":");
      client.print((millis() - startTime) / 1000);
      client.print(",\"wifi\":");
      client.print(WiFi.status() == WL_CONNECTED ? "true" : "false");
      client.print(",\"internet\":");
      client.print(internetConnected ? "true" : "false");
      client.print(",\"ip\":\"");
      client.print(WiFi.localIP().toString());
      client.print("\",\"free_memory\":");
      client.print(getFreeMemory());
      client.print(",\"sd_card\":");
      client.print(sdCardInitialized ? "true" : "false");
      client.print(",\"sensors_ready\":");
      client.print(sensorDetectionReady ? "true" : "false");
      client.print(",\"test_mode\":");
      client.print(TEST_MODE ? "true" : "false");
      client.print(",\"epoch\":");
      client.print(getCurrentEpoch());
      client.print(",\"timestamp\":");
      client.print(millis());
      client.print("}");
    } else {
      // Send 404
      sendJsonHeaders(client, 404);
      client.println("{\"error\":\"Endpoint not found\",\"available_endpoints\":[\"/api/status\",\"/api/status/fast\",\"/api/plant/{id}/status\",\"/api/plant/{id}/pump\",\"/api/plant/{id}/fan\",\"/api/thresholds\",\"/api/logs/export\",\"/api/system/restart\",\"/api/email/settings\",\"/api/email/test\",\"/api/email/status\"],\"plant_ids\":[0,1,2],\"methods\":{\"GET\":[\"/api/status\",\"/api/status/fast\",\"/api/plant/{id}/status\",\"/api/thresholds\",\"/api/logs/export\",\"/api/email/settings\"],\"POST\":[\"/api/plant/{id}/pump\",\"/api/plant/{id}/fan\",\"/api/thresholds\",\"/api/system/restart\",\"/api/email/settings\",\"/api/email/test\",\"/api/email/status\"]}}");
    }
    
    // Close the connection
    client.stop();
    consoleLog(LOG_LEVEL_INFO, "API", "Client disconnected");
  }
  
  void sendStatusResponse(WiFiClient& client) {
    // Pre-calculate values outside of the JSON building to improve performance
    unsigned long currentTime = millis();
    unsigned long uptime = currentTime - startTime;
    unsigned long uptimeSecs = uptime / 1000;
    unsigned long uptimeMins = uptimeSecs / 60;
    unsigned long uptimeHours = uptimeMins / 60;
    unsigned long uptimeDays = uptimeHours / 24;
    
    uptimeSecs %= 60;
    uptimeMins %= 60;
    uptimeHours %= 24;
    
    // Cache frequently used values
    int freeMemoryValue = getFreeMemory();
    int wifiSignal = WiFi.RSSI();
    bool isWifiConnected = (WiFi.status() == WL_CONNECTED);
    
    // Pre-format MAC address to avoid repeated calls
    static char macStr[18];
    static bool macCached = false;
    if (!macCached) {
      byte mac[6];
      WiFi.macAddress(mac);
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
               mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
      macCached = true;
    }
    
    // Send HTTP headers immediately
    sendJsonHeaders(client);
    
    // Build and send JSON in smaller chunks to reduce memory usage
    client.print("{");
    
    // API metadata
    client.print("\"api\":{");
    client.print("\"version\":\"2.0\",");
    client.print("\"endpoints\":[");
    client.print("\"/api/status\",");
    client.print("\"/api/status/fast\",");
    client.print("\"/api/plant/{id}/status\",");
    client.print("\"/api/plant/{id}/pump\",");
    client.print("\"/api/plant/{id}/fan\",");
    client.print("\"/api/thresholds\",");
    client.print("\"/api/logs/export\",");
    client.print("\"/api/system/restart\"");
    client.print("],");
    client.print("\"request_timestamp\":");
    client.print(millis());
    client.print("},");
    
    // Basic info
    client.print("\"device\":{");
    client.print("\"model\":\"Arduino Uno R4 Wi-Fi\",");
    client.print("\"firmware_version\":\"Arduino IDE ");
    client.print(ARDUINO);
    client.print(", API ");
    client.print(ARDUINO_API_VERSION);
    client.print("\",");
    client.print("\"project_name\":\"HUMI v2 - Plant Management System\"");
    client.print("},");
    client.print("\"connectivity\":{");
    client.print("\"wifi_connected\":");
    client.print(isWifiConnected ? "true" : "false");
    client.print(",\"internet_connected\":");
    client.print(internetConnected ? "true" : "false");
    client.print("},");
    
    // System info
    client.print("\"system_info\":{");
    client.print("\"free_memory\":");
    client.print(freeMemoryValue);
    client.print(",\"free_memory_unit\":\"bytes\",\"wifi_signal_strength\":");
    client.print(wifiSignal);
    client.print(",\"wifi_signal_unit\":\"dBm\",\"wifi_encryption_type\":\"");
    client.print(getEncryptionTypeString(WiFi.encryptionType()));
    client.print("\",\"ssid\":\"");
    client.print(ssid);
    client.print("\",\"network_ip\":\"");
    client.print(WiFi.localIP().toString());
    client.print("\",\"subnet_mask\":\"");
    client.print(WiFi.subnetMask().toString());
    client.print("\",\"gateway_ip\":\"");
    client.print(WiFi.gatewayIP().toString());
    client.print("\",\"mac_address\":\"");
    client.print(macStr);  // Use cached MAC
    client.print("\",\"sd_card_initialized\":");
    client.print(sdCardInitialized ? "true" : "false");
    client.print(",\"email_notifications_enabled\":");
    client.print(emailNotificationsEnabled ? "true" : "false");
    client.print(",\"test_mode\":");
    client.print(TEST_MODE ? "true" : "false");
    client.print(",\"sensor_detection_ready\":");
    client.print(sensorDetectionReady ? "true" : "false");
    client.print("},");
    
    // Add soil moisture sensor data with fan and pump status
    client.print("\"soil_sensors\":[");
    for(int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      client.print("{\"id\":");
      client.print(i);
      client.print(",\"name\":\"");
      client.print(plantNames[i]);
      client.print("\",\"pin\":\"A");
      client.print(i);
      client.print("\",\"connected\":");
      client.print(soilSensorConnected[i] ? "true" : "false");
      client.print(",\"value\":");
      client.print(soilMoistureValues[i]);
      
      int moisturePercent = getMoisturePercent(i);
      
      client.print(",\"moisture_percent\":");
      client.print(moisturePercent);
      client.print(",\"status\":\"");
      client.print(soilSensorConnected[i] ? getMoistureStatus(moisturePercent) : "Not Connected");
      client.print("\",\"pump_pin\":\"D");
      client.print(pumpPins[i]);
      client.print("\",\"pump_active\":");
      client.print(pumpActive[i] ? "true" : "false");
      client.print(",\"fan_pin\":\"D");
      client.print(fanPins[i]);
      client.print("\",\"fan_active\":");
      client.print(fanActive[i] ? "true" : "false");
      client.print(",\"logs\":[");
      
      // Calculate starting index for logs (for circular buffer)
      int startIdx = (logCount[i] < MAX_LOGS_PER_PLANT) ? 0 : logIndex[i];
      int count = min(logCount[i], MAX_LOGS_PER_PLANT);
      
      for(int j = 0; j < count; j++) {
        int idx = (startIdx + j) % MAX_LOGS_PER_PLANT;
        
        client.print("{\"timestamp\":");
        client.print(plantLogs[i][idx].timestamp);
        client.print(",\"time_ago\":\"");
        client.print(formatTimeAgo(plantLogs[i][idx].timestamp));
        client.print("\",\"type\":");
        client.print(plantLogs[i][idx].logType);
        client.print(",\"type_name\":\"");
        
        // Convert log type to string - optimized
        switch(plantLogs[i][idx].logType) {
          case LOG_TYPE_COMPONENT:
            client.print("component");
            break;
          case LOG_TYPE_HUMIDITY:
            client.print("humidity");
            break;
          case LOG_TYPE_CONNECTION:
            client.print("connection");
            break;
          default:
            client.print("unknown");
        }
        
        client.print("\",\"message\":\"");
        client.print(plantLogs[i][idx].message);
        client.print("\"}");
        
        if(j < count - 1) client.print(",");
      }
      
      client.print("]");
      client.print("}");
      
      if(i < SOIL_SENSORS_COUNT - 1) client.print(",");
    }
    client.print("],");
    
    // Add control thresholds
    client.print("\"control_thresholds\":{\"moisture_too_low\":");
    client.print(MOISTURE_TOO_LOW);
    client.print(",\"moisture_too_high\":");
    client.print(MOISTURE_TOO_HIGH);
    client.print(",\"moisture_ideal_low\":");
    client.print(MOISTURE_IDEAL_LOW);
    client.print(",\"moisture_ideal_high\":");
    client.print(MOISTURE_IDEAL_HIGH);
    client.print("},");
    
    // Uptime info
    client.print("\"uptime\":{\"days\":");
    client.print(uptimeDays);
    client.print(",\"hours\":");
    client.print(uptimeHours);
    client.print(",\"minutes\":");
    client.print(uptimeMins);
    client.print(",\"seconds\":");
    client.print(uptimeSecs);
    client.print(",\"total_seconds\":");
    client.print(uptimeSecs + uptimeMins*60 + uptimeHours*3600 + uptimeDays*86400);
    client.print("},");
    
    // Project members
    client.print("\"project_members\":[");
    client.print("\"Batuhan Türkoğlu\",");
    client.print("\"Melike Ebru\",");
    client.print("\"Rüzgar Burak Tunç\"");
    client.print("]");
    
    client.print("}");
  }
  
  // Fast status endpoint with minimal data for quick polling
  void sendFastStatusResponse(WiFiClient& client) {
    sendJsonHeaders(client);
    
    client.print("{\"uptime\":");
    client.print((millis() - startTime) / 1000);
    client.print(",\"wifi\":");
    client.print(WiFi.status() == WL_CONNECTED ? "true" : "false");
    client.print(",\"internet\":");
    client.print(internetConnected ? "true" : "false");
    client.print(",\"plants\":[");
    
    for(int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      client.print("{\"id\":");
      client.print(i);
      client.print(",\"name\":\"");
      client.print(plantNames[i]);
      client.print("\",\"connected\":");
      client.print(soilSensorConnected[i] ? "true" : "false");
      client.print(",\"moisture\":");
      
      int moisturePercent = getMoisturePercent(i);
      
      client.print(moisturePercent);
      client.print(",\"pump\":");
      client.print(pumpActive[i] ? "true" : "false");
      client.print(",\"fan\":");
      client.print(fanActive[i] ? "true" : "false");
      client.print("}");
      
      if(i < SOIL_SENSORS_COUNT - 1) client.print(",");
    }
    
    client.print("],\"timestamp\":");
    client.print(millis());
    client.print("}");
  }
  
  void sendSuccessResponse(WiFiClient& client, String message) {
    sendJsonHeaders(client);
    client.print("{\"success\":true,\"message\":\"" + message + "\",\"timestamp\":" + String(millis()) + "}");
  }
  
  void sendErrorResponse(WiFiClient& client, int code, String message) {
    sendJsonHeaders(client, code);
    client.print("{\"success\":false,\"error\":\"" + message + "\",\"timestamp\":" + String(millis()) + "}");
  }
  
  // ========================================
  // MailerSend Email Notification Functions
  // ========================================
  
  // Calculate the length a string would have after JSON escaping
  size_t calcJsonEscapedLen(const char* str) {
    size_t len = 0;
    while (*str) {
      char c = *str++;
      if (c == '\\' || c == '"' || c == '\n' || c == '\r' || c == '\t') len += 2;
      else len++;
    }
    return len;
  }

  // Stream a string to output with JSON escaping (no heap allocation)
  void streamJsonEscaped(Print& out, const char* str) {
    while (*str) {
      char c = *str++;
      switch(c) {
        case '\\': out.print("\\\\"); break;
        case '"':  out.print("\\\""); break;
        case '\n': out.print("\\n"); break;
        case '\r': out.print("\\r"); break;
        case '\t': out.print("\\t"); break;
        default:   out.write(c); break;
      }
    }
  }
  
  bool sendMailerSendEmail(const String& subject, const String& htmlContent, const String& textContent, bool bypassCooldown = false) {
    if (!internetConnected || !emailNotificationsEnabled) {
      consoleLog(LOG_LEVEL_WARNING, "EMAIL", "Email not sent - internet or notifications disabled");
      return false;
    }
  
    // Check global cooldown (unless bypassed for system emails)
    unsigned long currentTime = millis();
    if (!bypassCooldown && (currentTime - lastEmailSent < emailCooldownPeriod)) {
      consoleLog(LOG_LEVEL_DEBUG, "EMAIL", "Email not sent - cooldown period active");
      return false;
    }
  
    WiFiSSLClient sslClient;
    HttpClient client = HttpClient(sslClient, MAILERSEND_API_URL, MAILERSEND_API_PORT);
    
    consoleLog(LOG_LEVEL_INFO, "EMAIL", "Sending email via MailerSend...");
    
    // Calculate escaped lengths for Content-Length (no String copies needed)
    size_t escapedSubjectLen = calcJsonEscapedLen(subject.c_str());
    size_t escapedTextLen = calcJsonEscapedLen(textContent.c_str());
    size_t escapedHtmlLen = calcJsonEscapedLen(htmlContent.c_str());
    size_t fromNameLen = calcJsonEscapedLen(FROM_NAME);
    
    // JSON structure fixed overhead = 84 bytes:
    // {"from":{"email":"","name":""},"to":[{"email":""}],"subject":"","text":"","html":""}
    size_t contentLength = 84 + strlen(FROM_EMAIL) + fromNameLen + strlen(TO_EMAIL)
      + escapedSubjectLen + escapedTextLen + escapedHtmlLen;
    
    consoleLog(LOG_LEVEL_DEBUG, "EMAIL", "Streaming payload: " + String(contentLength) + " bytes");
    
    // Stream JSON directly to HTTP client - no large String allocations
    client.beginRequest();
    client.post("/v1/email");
    client.sendHeader("Authorization", "Bearer " + String(MAILERSEND_API_TOKEN));
    client.sendHeader("Content-Type", "application/json");
    client.sendHeader("X-Requested-With", "XMLHttpRequest");
    client.sendHeader("Content-Length", contentLength);
    client.beginBody();
    
    client.print("{\"from\":{\"email\":\"");
    client.print(FROM_EMAIL);
    client.print("\",\"name\":\"");
    streamJsonEscaped(client, FROM_NAME);
    client.print("\"},\"to\":[{\"email\":\"");
    client.print(TO_EMAIL);
    client.print("\"}],\"subject\":\"");
    streamJsonEscaped(client, subject.c_str());
    client.print("\",\"text\":\"");
    streamJsonEscaped(client, textContent.c_str());
    client.print("\",\"html\":\"");
    streamJsonEscaped(client, htmlContent.c_str());
    client.print("\"}");
    
    client.endRequest();
    
    // Read response
    int statusCode = client.responseStatusCode();
    String response = client.responseBody();
    
    client.stop();
    
    if (statusCode == 202) {
      consoleLog(LOG_LEVEL_SUCCESS, "EMAIL", "Email sent successfully");
      lastEmailSent = currentTime;
      return true;
    } else {
      consoleLog(LOG_LEVEL_ERROR, "EMAIL", "Email send failed - Status: " + String(statusCode));
      consoleLog(LOG_LEVEL_DEBUG, "EMAIL", "Response: " + response);
      return false;
    }
  }
  
  void sendSystemStartupEmail() {
    if (!internetConnected || !emailNotificationsEnabled) {
      consoleLog(LOG_LEVEL_INFO, "EMAIL", "Startup email not sent - internet or notifications disabled");
      return;
    }
  
    String subject = "HUMI v2 System Successfully Started - Your Plants Are Now Protected!";
    String textContent = "HUMI v2 Plant Management System has started successfully! Device IP: " + WiFi.localIP().toString() + " - System is ready and monitoring plants.";
    
    // Create simpler but still fancy HTML email
    String htmlContent;
    htmlContent.reserve(2700); // Pre-allocate to avoid repeated reallocation
    htmlContent = "<!DOCTYPE html><html><head><meta charset='UTF-8'><style>";
    htmlContent += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f8ff;}";
    htmlContent += ".container{max-width:600px;margin:0 auto;background:white;border-radius:15px;overflow:hidden;box-shadow:0 10px 30px rgba(0,0,0,0.1);}";
    htmlContent += ".header{background:linear-gradient(135deg,#27ae60,#2ecc71);color:white;padding:30px;text-align:center;}";
    htmlContent += ".header h1{margin:0;font-size:2em;}";
    htmlContent += ".content{padding:30px;}";
    htmlContent += ".info-box{background:#f8f9fa;border-left:4px solid #27ae60;padding:15px;margin:15px 0;border-radius:5px;}";
    htmlContent += ".plant-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;margin:20px 0;}";
    htmlContent += ".plant-card{background:#e8f5e8;padding:15px;border-radius:10px;text-align:center;}";
    htmlContent += ".footer{background:#2c3e50;color:white;padding:20px;text-align:center;}";
    htmlContent += "</style></head><body><div class='container'>";
    
    htmlContent += "<div class='header'><h1>HUMI v2 Started!</h1>";
    htmlContent += "<p>Your Plant Management System is Online</p></div>";
    
    htmlContent += "<div class='content'>";
    htmlContent += "<h2>System Successfully Initialized!</h2>";
    htmlContent += "<p>Your HUMI v2 Plant Management System has started and is now protecting your plants.</p>";
    
    htmlContent += "<div class='info-box'><h3>System Information</h3>";
    htmlContent += "<p><strong>Device IP:</strong> " + WiFi.localIP().toString() + "</p>";
    htmlContent += "<p><strong>WiFi Network:</strong> " + String(ssid) + "</p>";
    htmlContent += "<p><strong>SD Card:</strong> " + String(sdCardInitialized ? "Ready" : "Failed") + "</p>";
    htmlContent += "</div>";
    
    htmlContent += "<div class='info-box'><h3>Your Plants</h3>";
    htmlContent += "<div class='plant-grid'>";
    for (int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      htmlContent += "<div class='plant-card'>";
      htmlContent += "<strong>" + String(plantNames[i]) + "</strong><br>";
      htmlContent += "Sensor: A" + String(i) + "<br>";
      htmlContent += "Pump: D" + String(pumpPins[i]) + "<br>";
      htmlContent += "Fan: D" + String(fanPins[i]);
      htmlContent += "</div>";
    }
    htmlContent += "</div></div>";
    
    htmlContent += "<div class='info-box'><h3>Moisture Thresholds</h3>";
    htmlContent += "<p>Too Low (Pump): " + String(MOISTURE_TOO_LOW) + "%</p>";
    htmlContent += "<p>Too High (Fan): " + String(MOISTURE_TOO_HIGH) + "%</p>";
    htmlContent += "<p>Ideal Range: " + String(MOISTURE_IDEAL_LOW) + "% - " + String(MOISTURE_IDEAL_HIGH) + "%</p>";
    htmlContent += "</div>";
    
    String apiBaseUrl = "http://" + WiFi.localIP().toString();
    htmlContent += "<div class='info-box'><h3>Quick Access</h3>";
    htmlContent += "<p><a href='" + apiBaseUrl + "/api/status'>System Status</a></p>";
    htmlContent += "<p><a href='" + apiBaseUrl + "/health'>Health Check</a></p>";
    htmlContent += "</div>";
    
    htmlContent += "</div>";
    htmlContent += "<div class='footer'>";
    htmlContent += "<h3>Your Plants Are Now Protected!</h3>";
    htmlContent += "<p>HUMI v2 is monitoring and will automatically care for your plants.</p>";
    htmlContent += "<p>Started at: " + String(millis()) + "ms</p>";
    htmlContent += "</div></div></body></html>";
    
    if (sendMailerSendEmail(subject, htmlContent, textContent, true)) {
      consoleLog(LOG_LEVEL_SUCCESS, "EMAIL", "System startup email sent successfully");
    } else {
      consoleLog(LOG_LEVEL_WARNING, "EMAIL", "Failed to send system startup email");
    }
  }
  
  void sendInitializationCompleteEmail() {
    if (!internetConnected || !emailNotificationsEnabled) {
      consoleLog(LOG_LEVEL_INFO, "EMAIL", "Initialization complete email not sent - internet or notifications disabled");
      return;
    }
  
    String subject = "HUMI v2 Fully Operational - All Systems Green & Plants Secure!";
    String textContent = "HUMI v2 initialization complete! All systems operational. Device IP: " + WiFi.localIP().toString() + " - Sensor detection ready.";
    
    // Get system uptime
    unsigned long uptime = millis() - startTime;
    unsigned long minutes = uptime / 60000;
    unsigned long seconds = (uptime % 60000) / 1000;
    
    // Create compact but attractive HTML email
    String htmlContent;
    htmlContent.reserve(2700); // Pre-allocate to avoid repeated reallocation
    htmlContent = "<!DOCTYPE html><html><head><style>";
    htmlContent += "body{font-family:Arial;margin:0;padding:20px;background:#e8f5e8;}";
    htmlContent += ".c{max-width:600px;margin:0 auto;background:#fff;border-radius:15px;overflow:hidden;box-shadow:0 10px 30px rgba(0,0,0,0.1);}";
    htmlContent += ".h{background:linear-gradient(135deg,#00b894,#55a3ff);color:#fff;padding:30px;text-align:center;}";
    htmlContent += ".h h1{margin:0;font-size:2em;}";
    htmlContent += ".p{padding:25px;}";
    htmlContent += ".hero{background:#6c5ce7;color:#fff;padding:20px;border-radius:10px;text-align:center;margin-bottom:20px;}";
    htmlContent += ".stats{display:grid;grid-template-columns:repeat(3,1fr);gap:15px;margin:20px 0;}";
    htmlContent += ".stat{background:#74b9ff;color:#fff;padding:15px;border-radius:10px;text-align:center;}";
    htmlContent += ".stat-v{font-size:1.5em;font-weight:bold;}";
    htmlContent += ".plants{background:#fd79a8;color:#fff;border-radius:10px;padding:20px;margin:15px 0;}";
    htmlContent += ".footer{background:#2d3436;color:#fff;padding:20px;text-align:center;}";
    htmlContent += "</style></head><body><div class='c'>";
    
    htmlContent += "<div class='h'><span style='font-size:3em;'></span><h1>System Ready!</h1><p>All plants protected</p></div>";
    
    htmlContent += "<div class='p'><div class='hero'><h2>HUMI v2 Fully Operational!</h2><p>All sensors detected and monitoring active</p></div>";
    
    htmlContent += "<div class='stats'>";
    htmlContent += "<div class='stat'><div class='stat-v'>" + String(minutes) + "m</div><div>Init Time</div></div>";
    htmlContent += "<div class='stat'><div class='stat-v'>" + WiFi.localIP().toString() + "</div><div>Device IP</div></div>";
    htmlContent += "<div class='stat'><div class='stat-v'>" + String(internetConnected ? "Online" : "Offline") + "</div><div>Internet</div></div>";
    htmlContent += "</div>";
    
    htmlContent += "<div class='plants'><h3>Plant Status</h3>";
    for (int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      String status = soilSensorConnected[i] ? "OK" : "DISC";
      String moisture = soilSensorConnected[i] ? String(soilMoistureValues[i]) + "%" : "N/A";
      htmlContent += "<p><strong>" + String(plantNames[i]) + ":</strong> " + status + " | " + moisture;
      if (pumpActive[i]) htmlContent += " | Pump ON";
      if (fanActive[i]) htmlContent += " | Fan ON";
      htmlContent += "</p>";
    }
    htmlContent += "</div>";
    
    String apiUrl = "http://" + WiFi.localIP().toString();
    htmlContent += "<div style='background:#a29bfe;color:#fff;padding:15px;border-radius:10px;text-align:center;'>";
    htmlContent += "<h3>Quick Access</h3>";
    htmlContent += "<a href='" + apiUrl + "/api/status' style='color:#fff;'>Status</a> | ";
    htmlContent += "<a href='" + apiUrl + "/health' style='color:#fff;'>Health</a>";
    htmlContent += "</div></div>";
    
    htmlContent += "<div class='footer'><h3>Plants Protected!</h3><p>Auto-monitoring active. Ready at " + String(millis()) + "ms</p></div>";
    htmlContent += "</div></body></html>";
    
    if (sendMailerSendEmail(subject, htmlContent, textContent, true)) {
      consoleLog(LOG_LEVEL_SUCCESS, "EMAIL", "Initialization complete email sent successfully");
    } else {
      consoleLog(LOG_LEVEL_WARNING, "EMAIL", "Failed to send initialization complete email");
    }
  }
  
  bool canSendEmailForPlant(int plantIndex, int emailType) {
    unsigned long currentTime = millis();
    unsigned long cooldown = (emailType == EMAIL_TYPE_CRITICAL_ERROR) ? criticalEmailCooldown : emailCooldownPeriod;
    
    return (currentTime - lastEmailPerPlant[plantIndex][emailType] > cooldown);
  }
  
  void sendPlantAlertEmail(int plantIndex, int emailType, String alertMessage, int moisturePercent = -1) {
    if (!canSendEmailForPlant(plantIndex, emailType)) {
      return;
    }
    
    String plantName = String(plantNames[plantIndex]);
    String subject = "";
    String htmlContent;
    htmlContent.reserve(5500); // Pre-allocate for large alert templates
    String textContent = "";
    
    // Get current time for email
    unsigned long uptime = millis() - startTime;
    unsigned long hours = uptime / 3600000;
    unsigned long minutes = (uptime % 3600000) / 60000;
    
    String timeInfo = "System Uptime: " + String(hours) + "h " + String(minutes) + "m";
    String ipInfo = "Device IP: " + WiFi.localIP().toString();
    
    // Base fancy HTML template
    String baseHTML = "<!DOCTYPE html>"
      "<html lang='en'>"
      "<head>"
      "<meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
      "<title>HUMI v2 Plant Alert</title>"
      "<style>"
      "body { margin: 0; padding: 0; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }"
      ".container { max-width: 600px; margin: 0 auto; background: #ffffff; border-radius: 25px; overflow: hidden; box-shadow: 0 25px 50px rgba(0,0,0,0.2); }"
      ".alert-header { color: white; padding: 40px 30px; text-align: center; position: relative; overflow: hidden; }"
      ".alert-icon { font-size: 4em; margin-bottom: 15px; display: block; animation: alertPulse 2s infinite; }"
      "@keyframes alertPulse { 0% { transform: scale(1); } 50% { transform: scale(1.1); filter: brightness(1.2); } 100% { transform: scale(1); } }"
      ".alert-title { margin: 0; font-size: 2.2em; font-weight: 800; text-shadow: 3px 3px 6px rgba(0,0,0,0.4); }"
      ".alert-subtitle { margin: 10px 0 0 0; font-size: 1.1em; opacity: 0.95; }"
      ".content { padding: 40px 30px; }"
      ".alert-summary { border-radius: 20px; padding: 30px; margin-bottom: 30px; text-align: center; color: white; position: relative; overflow: hidden; }"
      ".alert-summary::before { content: ''; position: absolute; top: 0; left: -100%; width: 100%; height: 100%; background: linear-gradient(90deg, transparent, rgba(255,255,255,0.2), transparent); animation: alertShine 3s infinite; }"
      "@keyframes alertShine { 0% { left: -100%; } 100% { left: 100%; } }"
      ".summary-content { position: relative; z-index: 1; }"
      ".plant-info { background: linear-gradient(135deg, #f8f9fa 0%, #e9ecef 100%); border-radius: 18px; padding: 25px; margin: 25px 0; border-left: 6px solid; }"
      ".info-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 20px 0; }"
      ".info-item { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); }"
      ".info-label { font-size: 0.9em; color: #6c757d; margin-bottom: 5px; }"
      ".info-value { font-size: 1.3em; font-weight: 700; color: #2c3e50; }"
      ".action-section { border-radius: 18px; padding: 25px; margin: 25px 0; color: white; text-align: center; }"
      ".recommendations { background: linear-gradient(135deg, #a8edea 0%, #fed6e3 100%); border-radius: 18px; padding: 25px; margin: 25px 0; }"
      ".recommendation-item { background: white; border-radius: 12px; padding: 15px; margin: 10px 0; box-shadow: 0 3px 10px rgba(0,0,0,0.1); }"
      ".footer { background: linear-gradient(135deg, #2c3e50 0%, #34495e 100%); color: white; padding: 30px; text-align: center; }"
      ".footer-info { opacity: 0.9; margin: 5px 0; }"
      ".status-badge { display: inline-block; padding: 8px 16px; border-radius: 20px; font-size: 0.9em; font-weight: 600; margin: 5px; }"
      ".emergency { background: #e74c3c; }"
      ".warning { background: #f39c12; }"
      ".info { background: #3498db; }"
      ".success { background: #27ae60; }"
      "</style>"
      "</head>"
      "<body>"
      "<div class='container'>";
    
    switch (emailType) {
      case EMAIL_TYPE_MOISTURE_LOW:
        subject = "URGENT: " + plantName + " Needs Water - HUMI v2 Auto-Response Activated!";
        textContent = "CRITICAL MOISTURE ALERT for " + plantName + "! Current level: " + String(moisturePercent) + "% (below " + String(MOISTURE_TOO_LOW) + "% threshold). Water pump automatically activated. " + timeInfo + " | " + ipInfo;
        
        htmlContent = baseHTML +
          "<div class='alert-header' style='background: linear-gradient(135deg, #e74c3c 0%, #c0392b 100%);'>"
          "<span class='alert-icon'></span>"
          "<h1 class='alert-title'>URGENT WATER ALERT!</h1>"
          "<p class='alert-subtitle'>" + plantName + " needs immediate attention</p>"
          "</div>"
          "<div class='content'>"
          "<div class='alert-summary emergency'>"
          "<div class='summary-content'>"
          "<h2 style='margin: 0 0 15px 0; font-size: 1.6em;'>Critical Moisture Level Detected</h2>"
          "<p style='margin: 0; font-size: 1.1em; opacity: 0.95;'>Your plant's soil is dangerously dry. HUMI v2 has automatically activated the water pump to prevent damage.</p>"
          "</div>"
          "</div>"
          "<div class='plant-info' style='border-left-color: #e74c3c;'>"
          "<h3 style='margin: 0 0 20px 0; color: #e74c3c; font-size: 1.3em;'>" + plantName + " Status</h3>"
          "<div class='info-grid'>"
          "<div class='info-item'>"
          "<div class='info-label'>Current Moisture Level</div>"
          "<div class='info-value' style='color: #e74c3c;'>" + String(moisturePercent) + "%</div>"
          "</div>"
          "<div class='info-item'>"
          "<div class='info-label'>Critical Threshold</div>"
          "<div class='info-value'>" + String(MOISTURE_TOO_LOW) + "%</div>"
          "</div>"
          "<div class='info-item'>"
          "<div class='info-label'>Sensor Location</div>"
          "<div class='info-value'>Pin A" + String(plantIndex) + "</div>"
          "</div>"
          "<div class='info-item'>"
          "<div class='info-label'>Plant Status</div>"
          "<div class='info-value'>THIRSTY</div>"
          "</div>"
          "</div>"
          "</div>"
          "<div class='action-section' style='background: linear-gradient(135deg, #27ae60 0%, #229954 100%);'>"
          "<h3 style='margin: 0 0 15px 0; font-size: 1.4em;'>Automatic Response Activated</h3>"
          "<p style='margin: 0 0 15px 0; font-size: 1.1em;'>Water pump is now ON and providing hydration to your plant</p>"
          "<div style='display: flex; justify-content: center; flex-wrap: wrap; gap: 10px;'>"
          "<span class='status-badge success'>Pump Active</span>"
          "<span class='status-badge info'>Monitoring</span>"
          "</div>"
          "</div>"
          "<div class='recommendations'>"
          "<h3 style='margin: 0 0 20px 0; color: #2c3e50; text-align: center;'>Smart Recommendations</h3>"
          "<div class='recommendation-item'>"
          "<strong>Check:</strong> Ensure water reservoir has sufficient supply for continued operation."
          "</div>"
          "<div class='recommendation-item'>"
          "<strong>Monitor:</strong> Pump will automatically stop when moisture reaches " + String(MOISTURE_IDEAL_LOW) + "% (ideal level)."
          "</div>"
          "<div class='recommendation-item'>"
          "<strong>Consider:</strong> Environmental factors like heat/sunlight may increase water consumption."
          "</div>"
          "</div>"
          "</div>";
        break;
        
      case EMAIL_TYPE_MOISTURE_HIGH:
        subject = "ALERT: " + plantName + " Has Excess Moisture - HUMI v2 Ventilation Activated!";
        textContent = "HIGH MOISTURE ALERT for " + plantName + "! Current level: " + String(moisturePercent) + "% (above " + String(MOISTURE_TOO_HIGH) + "% threshold). Ventilation fan automatically activated. " + timeInfo + " | " + ipInfo;
        
        htmlContent = baseHTML +
          "<div class='alert-header' style='background: linear-gradient(135deg, #3498db 0%, #2980b9 100%);'>"
          "<span class='alert-icon'></span>"
          "<h1 class='alert-title'>MOISTURE OVERFLOW!</h1>"
          "<p class='alert-subtitle'>" + plantName + " has too much water</p>"
          "</div>"
          "<div class='content'>"
          "<div class='alert-summary info'>"
          "<div class='summary-content'>"
          "<h2 style='margin: 0 0 15px 0; font-size: 1.6em;'>Excess Moisture Detected</h2>"
          "<p style='margin: 0; font-size: 1.1em; opacity: 0.95;'>Your plant's soil is oversaturated. HUMI v2 has activated ventilation to prevent root rot.</p>"
          "</div>"
          "</div>"
          "<div class='plant-info' style='border-left-color: #3498db;'>"
          "<h3 style='margin: 0 0 20px 0; color: #3498db; font-size: 1.3em;'>" + plantName + " Status</h3>"
          "<div class='info-grid'>"
          "<div class='info-item'>"
          "<div class='info-label'>Current Moisture Level</div>"
          "<div class='info-value' style='color: #3498db;'>" + String(moisturePercent) + "%</div>"
          "</div>"
          "<div class='info-item'>"
          "<div class='info-label'>High Threshold</div>"
          "<div class='info-value'>" + String(MOISTURE_TOO_HIGH) + "%</div>"
          "</div>"
          "<div class='info-item'>"
          "<div class='info-label'>Sensor Location</div>"
          "<div class='info-value'>Pin A" + String(plantIndex) + "</div>"
          "</div>"
          "<div class='info-item'>"
          "<div class='info-label'>Plant Status</div>"
          "<div class='info-value'>OVERSATURATED</div>"
          "</div>"
          "</div>"
          "</div>"
          "<div class='action-section' style='background: linear-gradient(135deg, #9b59b6 0%, #8e44ad 100%);'>"
          "<h3 style='margin: 0 0 15px 0; font-size: 1.4em;'>Ventilation System Activated</h3>"
          "<p style='margin: 0 0 15px 0; font-size: 1.1em;'>Fan is now running to improve air circulation and reduce moisture</p>"
          "<div style='display: flex; justify-content: center; flex-wrap: wrap; gap: 10px;'>"
          "<span class='status-badge info'>Fan Active</span>"
          "<span class='status-badge warning'>Monitoring</span>"
          "</div>"
          "</div>"
          "<div class='recommendations'>"
          "<h3 style='margin: 0 0 20px 0; color: #2c3e50; text-align: center;'>Smart Recommendations</h3>"
          "<div class='recommendation-item'>"
          "<strong>Ventilation:</strong> Fan will continue until moisture drops to " + String(MOISTURE_IDEAL_HIGH) + "% (safe level)."
          "</div>"
          "<div class='recommendation-item'>"
          "<strong>Watering:</strong> Automatic watering is temporarily disabled to prevent overwatering."
          "</div>"
          "<div class='recommendation-item'>"
          "<strong>Check:</strong> Ensure proper drainage and consider environmental humidity levels."
          "</div>"
          "</div>"
          "</div>";
        break;
        
      case EMAIL_TYPE_SENSOR_DISCONNECTED:
        subject = "CRITICAL: " + plantName + " Sensor Offline - Manual Intervention Required!";
        textContent = "SENSOR DISCONNECTION ALERT for " + plantName + "! Soil moisture sensor appears disconnected. All automatic systems for this plant have been safely disabled. Please check sensor connection. " + timeInfo + " | " + ipInfo;
        
        htmlContent = baseHTML +
          "<div class='alert-header' style='background: linear-gradient(135deg, #f39c12 0%, #e67e22 100%);'>"
          "<span class='alert-icon'></span>"
          "<h1 class='alert-title'>SENSOR OFFLINE!</h1>"
          "<p class='alert-subtitle'>" + plantName + " monitoring disconnected</p>"
          "</div>"
          "<div class='content'>"
          "<div class='alert-summary warning'>"
          "<div class='summary-content'>"
          "<h2 style='margin: 0 0 15px 0; font-size: 1.6em;'>Sensor Connection Lost</h2>"
          "<p style='margin: 0; font-size: 1.1em; opacity: 0.95;'>Unable to read moisture levels. All automation has been safely disabled to prevent damage.</p>"
          "</div>"
          "</div>"
          "<div class='plant-info' style='border-left-color: #f39c12;'>"
          "<h3 style='margin: 0 0 20px 0; color: #f39c12; font-size: 1.3em;'>" + plantName + " Status</h3>"
          "<div class='info-grid'>"
          "<div class='info-item'>"
          "<div class='info-label'>Sensor Status</div>"
          "<div class='info-value' style='color: #f39c12;'>OFFLINE</div>"
          "</div>"
          "<div class='info-item'>"
          "<div class='info-label'>Sensor Location</div>"
          "<div class='info-value'>Pin A" + String(plantIndex) + "</div>"
          "</div>"
          "<div class='info-item'>"
          "<div class='info-label'>Pump Status</div>"
          "<div class='info-value'>DISABLED</div>"
          "</div>"
          "<div class='info-item'>"
          "<div class='info-label'>Fan Status</div>"
          "<div class='info-value'>DISABLED</div>"
          "</div>"
          "</div>"
          "</div>"
          "<div class='action-section' style='background: linear-gradient(135deg, #e74c3c 0%, #c0392b 100%);'>"
          "<h3 style='margin: 0 0 15px 0; font-size: 1.4em;'>Safety Mode Activated</h3>"
          "<p style='margin: 0 0 15px 0; font-size: 1.1em;'>All automatic watering and ventilation disabled until sensor is reconnected</p>"
          "<div style='display: flex; justify-content: center; flex-wrap: wrap; gap: 10px;'>"
          "<span class='status-badge emergency'>Manual Mode</span>"
          "<span class='status-badge warning'>Intervention Needed</span>"
          "</div>"
          "</div>"
          "<div class='recommendations'>"
          "<h3 style='margin: 0 0 20px 0; color: #2c3e50; text-align: center;'>Troubleshooting Steps</h3>"
          "<div class='recommendation-item'>"
          "<strong>1. Check:</strong> Verify sensor cable connections at Pin A" + String(plantIndex) + " are secure."
          "</div>"
          "<div class='recommendation-item'>"
          "<strong>2. Reconnect:</strong> Ensure sensor probe is properly inserted into soil."
          "</div>"
          "<div class='recommendation-item'>"
          "<strong>3. Manual Care:</strong> Monitor and water plant manually until sensor is restored."
          "</div>"
          "<div class='recommendation-item'>"
          "<strong>4. Test:</strong> System will automatically resume when connection is restored."
          "</div>"
          "</div>"
          "</div>";
        break;
        
      default:
        subject = "HUMI v2 Notification - " + plantName;
        textContent = alertMessage + " | " + timeInfo + " | " + ipInfo;
        htmlContent = baseHTML +
          "<div class='alert-header' style='background: linear-gradient(135deg, #6c5ce7 0%, #a29bfe 100%);'>"
          "<span class='alert-icon'></span>"
          "<h1 class='alert-title'>System Notification</h1>"
          "<p class='alert-subtitle'>" + plantName + " status update</p>"
          "</div>"
          "<div class='content'>"
          "<div class='alert-summary info'>"
          "<div class='summary-content'>"
          "<h2 style='margin: 0 0 15px 0; font-size: 1.6em;'>Status Update</h2>"
          "<p style='margin: 0; font-size: 1.1em; opacity: 0.95;'>" + alertMessage + "</p>"
          "</div>"
          "</div>"
          "</div>";
        break;
    }
    
    // Add common footer to all emails
    htmlContent += "<div class='footer'>"
      "<h3 style='margin: 0 0 10px 0;'>HUMI v2 Plant Management System</h3>"
      "<p class='footer-info'>" + ipInfo + " | " + timeInfo + "</p>"
      "<p class='footer-info'><a href='http://" + WiFi.localIP().toString() + "/api/status' style='color: #74b9ff;'>View Live Status</a> | "
      "<a href='http://" + WiFi.localIP().toString() + "/api/email/test' style='color: #74b9ff;'>Test Notifications</a></p>"
      "<p class='footer-info' style='font-size: 0.8em; opacity: 0.7;'>© 2025 HUMI v2 - Intelligent Plant Care System</p>"
      "</div>"
      "</div>"
      "</body>"
      "</html>";
    
    if (sendMailerSendEmail(subject, htmlContent, textContent)) {
      lastEmailPerPlant[plantIndex][emailType] = millis();
      consoleLog(LOG_LEVEL_SUCCESS, "EMAIL", "Fancy alert email sent for " + plantName);
    }
  }
  
  void sendSystemStatusEmail() {
    if (!canSendEmailForPlant(0, EMAIL_TYPE_SYSTEM_STATUS)) { // Use plant 0 for system emails
      return;
    }
    
    String subject = "HUMI v2 Daily Health Report - All Systems Status & Plant Analytics";
    
    // Get system info
    unsigned long uptime = millis() - startTime;
    unsigned long days = uptime / 86400000;
    unsigned long hours = (uptime % 86400000) / 3600000;
    unsigned long minutes = (uptime % 3600000) / 60000;
    
    String textContent = "HUMI v2 Daily Status Report - Uptime: " + String(days) + "d " + String(hours) + "h " + String(minutes) + "m | WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + " | IP: " + WiFi.localIP().toString();
    
    // Create ultra-fancy daily report HTML
    String htmlContent;
    htmlContent.reserve(6500); // Pre-allocate for large status report
    htmlContent = "<!DOCTYPE html>"
      "<html lang='en'>"
      "<head>"
      "<meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
      "<title>HUMI v2 Daily Report</title>"
      "<style>"
      "body { margin: 0; padding: 0; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }"
      ".container { max-width: 700px; margin: 0 auto; background: #ffffff; border-radius: 25px; overflow: hidden; box-shadow: 0 25px 50px rgba(0,0,0,0.2); }"
      ".header { background: linear-gradient(135deg, #6c5ce7 0%, #a29bfe 50%, #fd79a8 100%); color: white; padding: 50px 30px; text-align: center; position: relative; overflow: hidden; }"
      ".header::before { content: ''; position: absolute; top: 0; left: 0; right: 0; bottom: 0; background: url('data:image/svg+xml,<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\"><circle cx=\"10\" cy=\"10\" r=\"1\" fill=\"white\" opacity=\"0.2\"/><circle cx=\"90\" cy=\"20\" r=\"1.5\" fill=\"white\" opacity=\"0.15\"/><circle cx=\"20\" cy=\"80\" r=\"1\" fill=\"white\" opacity=\"0.2\"/><circle cx=\"80\" cy=\"90\" r=\"1.2\" fill=\"white\" opacity=\"0.18\"/></svg>'); animation: sparkle 15s linear infinite; }"
      "@keyframes sparkle { 0% { opacity: 0.3; } 50% { opacity: 0.8; } 100% { opacity: 0.3; } }"
      ".header h1 { margin: 0; font-size: 2.5em; font-weight: 800; text-shadow: 3px 3px 6px rgba(0,0,0,0.3); position: relative; z-index: 1; }"
      ".report-icon { font-size: 4em; margin-bottom: 20px; display: block; position: relative; z-index: 1; }"
      ".content { padding: 40px 30px; }"
      ".summary-section { background: linear-gradient(135deg, #00b894 0%, #00cec9 100%); color: white; border-radius: 20px; padding: 30px; margin-bottom: 30px; text-align: center; }"
      ".summary-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin-top: 20px; }"
      ".summary-item { background: rgba(255,255,255,0.2); border-radius: 15px; padding: 20px; backdrop-filter: blur(10px); }"
      ".summary-value { font-size: 2em; font-weight: 800; margin-bottom: 5px; }"
      ".summary-label { font-size: 0.9em; opacity: 0.9; }"
      ".system-health { background: linear-gradient(135deg, #74b9ff 0%, #0984e3 100%); border-radius: 20px; padding: 30px; margin: 30px 0; color: white; }"
      ".health-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin-top: 20px; }"
      ".health-item { background: rgba(255,255,255,0.15); border-radius: 15px; padding: 20px; text-align: center; }"
      ".health-status { font-size: 1.5em; margin-bottom: 10px; }"
      ".health-label { font-size: 1em; }"
      ".plants-dashboard { background: linear-gradient(135deg, #fd79a8 0%, #fdcb6e 100%); border-radius: 20px; padding: 30px; margin: 30px 0; }"
      ".dashboard-header { text-align: center; margin-bottom: 25px; color: white; }"
      ".dashboard-header h3 { margin: 0; font-size: 1.8em; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }"
      ".plants-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; }"
      ".plant-card { background: rgba(255,255,255,0.95); border-radius: 18px; padding: 25px; color: #2d3436; box-shadow: 0 10px 25px rgba(0,0,0,0.1); }"
      ".plant-header { display: flex; align-items: center; margin-bottom: 20px; }"
      ".plant-icon { font-size: 2em; margin-right: 15px; }"
      ".plant-name { font-size: 1.2em; font-weight: 700; color: #00b894; }"
      ".metrics-row { display: flex; justify-content: space-between; margin: 12px 0; padding: 8px 0; border-bottom: 1px solid #dee2e6; }"
      ".metrics-row:last-child { border-bottom: none; }"
      ".metric-label { font-weight: 500; color: #636e72; }"
      ".metric-value { font-weight: 700; }"
      ".status-good { color: #00b894; }"
      ".status-warning { color: #fdcb6e; }"
      ".status-critical { color: #e17055; }"
      ".recommendations { background: linear-gradient(135deg, #a8edea 0%, #fed6e3 100%); border-radius: 20px; padding: 30px; margin: 30px 0; }"
      ".rec-header { text-align: center; margin-bottom: 25px; }"
      ".rec-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; }"
      ".rec-item { background: white; border-radius: 15px; padding: 20px; box-shadow: 0 5px 15px rgba(0,0,0,0.1); }"
      ".rec-icon { font-size: 2em; margin-bottom: 10px; }"
      ".footer { background: linear-gradient(135deg, #2d3436 0%, #636e72 100%); color: white; padding: 40px 30px; text-align: center; }"
      ".footer-links { margin: 20px 0; }"
      ".footer-link { color: #74b9ff; text-decoration: none; margin: 0 15px; font-weight: 500; }"
      ".footer-link:hover { text-decoration: underline; }"
      "</style>"
      "</head>"
      "<body>"
      "<div class='container'>"
      "<div class='header'>"
      "<span class='report-icon'></span>"
      "<h1>Daily Health Report</h1>"
      "<p style='margin: 10px 0 0 0; font-size: 1.2em; opacity: 0.95; position: relative; z-index: 1;'>Your Plant Management System Analytics</p>"
      "</div>"
      "<div class='content'>";
    
    // System summary section
    htmlContent += "<div class='summary-section'>"
      "<h2 style='margin: 0 0 10px 0; font-size: 1.6em;'>System Performance Summary</h2>"
      "<p style='margin: 0 0 20px 0; opacity: 0.9;'>Your HUMI v2 has been running smoothly and protecting your plants!</p>"
      "<div class='summary-grid'>"
      "<div class='summary-item'>"
      "<div class='summary-value'>" + String(days) + "d " + String(hours) + "h</div>"
      "<div class='summary-label'>Total Uptime</div>"
      "</div>"
      "<div class='summary-item'>"
      "<div class='summary-value'>" + String(SOIL_SENSORS_COUNT) + "</div>"
      "<div class='summary-label'>Plants Monitored</div>"
      "</div>";
    
    // Count connected sensors
    int connectedSensors = 0;
    for (int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      if (soilSensorConnected[i]) connectedSensors++;
    }
    
    htmlContent += "<div class='summary-item'>"
      "<div class='summary-value'>" + String(connectedSensors) + "/" + String(SOIL_SENSORS_COUNT) + "</div>"
      "<div class='summary-label'>Sensors Online</div>"
      "</div>"
      "<div class='summary-item'>"
      "<div class='summary-value'>" + String(internetConnected ? "Yes" : "No") + "</div>"
      "<div class='summary-label'>Internet Status</div>"
      "</div>"
      "</div>"
      "</div>";
    
    // System health section
    htmlContent += "<div class='system-health'>"
      "<h3 style='margin: 0 0 10px 0; text-align: center; font-size: 1.6em;'>System Health Monitor</h3>"
      "<p style='margin: 0 0 20px 0; text-align: center; opacity: 0.9;'>Real-time system diagnostics and performance metrics</p>"
      "<div class='health-grid'>"
      "<div class='health-item'>"
      "<div class='health-status'>OK</div>"
      "<div class='health-label'>WiFi Connection<br><strong>" + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</strong></div>"
      "</div>"
      "<div class='health-item'>"
      "<div class='health-status'>" + String(internetConnected ? "OK" : "FAIL") + "</div>"
      "<div class='health-label'>Internet Access<br><strong>" + String(internetConnected ? "Available" : "Unavailable") + "</strong></div>"
      "</div>"
      "<div class='health-item'>"
      "<div class='health-status'>" + String(sdCardInitialized ? "OK" : "FAIL") + "</div>"
      "<div class='health-label'>SD Card Storage<br><strong>" + String(sdCardInitialized ? "Ready" : "Failed") + "</strong></div>"
      "</div>"
      "<div class='health-item'>"
      "<div class='health-status'>OK</div>"
      "<div class='health-label'>Email Alerts<br><strong>Active</strong></div>"
      "</div>"
      "</div>"
      "</div>";
    
    // Plants dashboard section
    htmlContent += "<div class='plants-dashboard'>"
      "<div class='dashboard-header'>"
      "<h3>Live Plant Dashboard</h3>"
      "<p style='margin: 5px 0 0 0; opacity: 0.9;'>Detailed status and analytics for each plant</p>"
      "</div>"
      "<div class='plants-grid'>";
    
    for (int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      int moisturePercent = getMoisturePercent(i);
      
      String sensorStatusIcon = soilSensorConnected[i] ? "OK" : "FAIL";
      String moistureStatusClass = "status-good";
      String moistureStatusIcon = "";
      
      if (!soilSensorConnected[i]) {
        moistureStatusClass = "status-critical";
        moistureStatusIcon = "X";
      } else if (moisturePercent < MOISTURE_TOO_LOW) {
        moistureStatusClass = "status-critical";
        moistureStatusIcon = "LOW";
      } else if (moisturePercent > MOISTURE_TOO_HIGH) {
        moistureStatusClass = "status-warning";
        moistureStatusIcon = "HIGH";
      }
      
      htmlContent += "<div class='plant-card'>"
        "<div class='plant-header'>"
        "<div class='plant-icon'>" + String(i == 0 ? "Plant 1" : i == 1 ? "Plant 2" : "Plant 3") + "</div>"
        "<div class='plant-name'>" + String(plantNames[i]) + "</div>"
        "</div>"
        "<div class='metrics-row'>"
        "<span class='metric-label'>Sensor Status</span>"
        "<span class='metric-value " + String(soilSensorConnected[i] ? "status-good" : "status-critical") + "'>" + sensorStatusIcon + " " + String(soilSensorConnected[i] ? "Connected" : "Offline") + "</span>"
        "</div>"
        "<div class='metrics-row'>"
        "<span class='metric-label'>Moisture Level</span>"
        "<span class='metric-value " + moistureStatusClass + "'>" + moistureStatusIcon + " " + String(soilSensorConnected[i] ? String(moisturePercent) + "%" : "N/A") + "</span>"
        "</div>"
        "<div class='metrics-row'>"
        "<span class='metric-label'>Water Pump</span>"
        "<span class='metric-value " + String(pumpActive[i] ? "status-warning" : "status-good") + "'>" + String(pumpActive[i] ? "Active" : "Standby") + "</span>"
        "</div>"
        "<div class='metrics-row'>"
        "<span class='metric-label'>Ventilation Fan</span>"
        "<span class='metric-value " + String(fanActive[i] ? "status-warning" : "status-good") + "'>" + String(fanActive[i] ? "Running" : "Standby") + "</span>"
        "</div>"
        "<div class='metrics-row'>"
        "<span class='metric-label'>Pin Assignment</span>"
        "<span class='metric-value'>A" + String(i) + " | Pump D" + String(pumpPins[i]) + " | Fan D" + String(fanPins[i]) + "</span>"
        "</div>"
        "</div>";
      
      // Add to text content
      textContent += " | " + String(plantNames[i]) + ": Sensor " + String(soilSensorConnected[i] ? "OK" : "OFFLINE") + ", Moisture " + String(moisturePercent) + "%, Pump " + String(pumpActive[i] ? "ON" : "OFF") + ", Fan " + String(fanActive[i] ? "ON" : "OFF");
    }
    
    htmlContent += "</div></div>";
    
    // Recommendations section
    htmlContent += "<div class='recommendations'>"
      "<div class='rec-header'>"
      "<h3 style='margin: 0 0 10px 0; color: #2d3436; font-size: 1.6em;'>Smart Recommendations</h3>"
      "<p style='margin: 0; color: #636e72;'>AI-powered insights for optimal plant care</p>"
      "</div>"
      "<div class='rec-grid'>"
      "<div class='rec-item'>"
      "<div class='rec-icon'></div>"
      "<h4 style='margin: 0 0 10px 0; color: #00b894;'>Monitoring Schedule</h4>"
      "<p style='margin: 0; color: #636e72; font-size: 0.9em;'>Sensors checked every 5 seconds, control logic runs every 10 seconds for optimal plant care.</p>"
      "</div>"
      "<div class='rec-item'>"
      "<div class='rec-icon'></div>"
      "<h4 style='margin: 0 0 10px 0; color: #0984e3;'>Moisture Thresholds</h4>"
      "<p style='margin: 0; color: #636e72; font-size: 0.9em;'>Ideal range: " + String(MOISTURE_IDEAL_LOW) + "%-" + String(MOISTURE_IDEAL_HIGH) + "%. Pumps activate below " + String(MOISTURE_TOO_LOW) + "%, fans above " + String(MOISTURE_TOO_HIGH) + "%.</p>"
      "</div>"
      "<div class='rec-item'>"
      "<div class='rec-icon'></div>"
      "<h4 style='margin: 0 0 10px 0; color: #6c5ce7;'>Notification System</h4>"
      "<p style='margin: 0; color: #636e72; font-size: 0.9em;'>Instant alerts for critical issues, with smart cooldown periods to prevent spam.</p>"
      "</div>"
      "<div class='rec-item'>"
      "<div class='rec-icon'></div>"
      "<h4 style='margin: 0 0 10px 0; color: #e17055;'>Automatic Recovery</h4>"
      "<p style='margin: 0; color: #636e72; font-size: 0.9em;'>System automatically resumes monitoring when disconnected sensors are reconnected.</p>"
      "</div>"
      "</div>"
      "</div>";
    
    // Add footer
    String apiBaseUrl = "http://" + WiFi.localIP().toString();
    htmlContent += "</div>"
      "<div class='footer'>"
      "<h3 style='margin: 0 0 15px 0;'>HUMI v2 Plant Management System</h3>"
      "<p style='margin: 0 0 10px 0; opacity: 0.9;'>Device IP: " + WiFi.localIP().toString() + " | Report generated at " + String(days) + "d " + String(hours) + "h " + String(minutes) + "m uptime</p>"
      "<div class='footer-links'>"
      "<a href='" + apiBaseUrl + "/api/status' class='footer-link'>Live System Status</a>"
      "<a href='" + apiBaseUrl + "/health' class='footer-link'>Health Check</a>"
      "<a href='" + apiBaseUrl + "/api/email/test' class='footer-link'>Test Email</a>"
      "</div>"
      "<p style='margin: 15px 0 0 0; font-size: 0.85em; opacity: 0.7;'>2025 HUMI v2 - Your plants are in safe hands!</p>"
      "</div>"
      "</div>"
      "</body>"
      "</html>";
    
    if (sendMailerSendEmail(subject, htmlContent, textContent)) {
      lastEmailPerPlant[0][EMAIL_TYPE_SYSTEM_STATUS] = millis();
      consoleLog(LOG_LEVEL_SUCCESS, "EMAIL", "Fancy system status report sent successfully");
    }
  }
  
  // ========================================
  // End of MailerSend Functions
  // ========================================
  
  // Calculate free memory (stack top minus heap top)
  int getFreeMemory() {
    char top;
    char* heapPtr = reinterpret_cast<char*>(malloc(4));
    int freeMem = &top - heapPtr;
    free(heapPtr);
    return freeMem;
  }
  
  // Helper function to convert encryption type to string - with caching
  String getEncryptionTypeString(int encryptionType) {
    // Cache encryption type for 30 seconds since it rarely changes
    unsigned long currentTime = millis();
    if (currentTime - encryptionTypeCacheTime > 30000 || cachedEncryptionType == "") {
      switch (encryptionType) {
        case 2:
          cachedEncryptionType = "TKIP (WPA)";
          break;
        case 5:
          cachedEncryptionType = "WEP";
          break;
        case 4:
          cachedEncryptionType = "CCMP (WPA2)";
          break;
        case 7:
          cachedEncryptionType = "NONE";
          break;
        case 8:
          cachedEncryptionType = "AUTO";
          break;
        default:
          cachedEncryptionType = "UNKNOWN";
      }
      encryptionTypeCacheTime = currentTime;
    }
    return cachedEncryptionType;
  }
  
  // Add helper function to get MAC address as a string
  String getMACAddressString() {
    byte mac[6];
    WiFi.macAddress(mac);
    
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
    
    return String(macStr);
  }
  
  // Function to check if soil moisture sensors are connected - using standard deviation
  void checkSoilSensors() {
    static int statusUpdateCounter = 0;
    static bool detectionTableShown = false;
    
    // Check if we should start sensor detection
    unsigned long currentTime = millis();
    if (!sensorDetectionReady && (currentTime - systemStartTime >= SENSOR_DETECTION_DELAY)) {
      sensorDetectionReady = true;
      consoleLog(LOG_LEVEL_INFO, "SENSOR", "Sensor detection delay complete - starting analysis...");
      
      // Send initialization complete email (only once)
      if (!initializationEmailSent && WiFi.status() == WL_CONNECTED && internetConnected) {
        consoleLog(LOG_LEVEL_INFO, "EMAIL", "Sending initialization complete notification...");
        delay(1000); // Small delay to avoid timing conflicts with startup email
        sendInitializationCompleteEmail();
        initializationEmailSent = true;
      }
    }
    
    for(int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      // Take multiple readings and average them
      int totalReading = 0;
      const int numSamples = 5;
      
      for(int j = 0; j < numSamples; j++) {
        totalReading += analogRead(soilSensorPins[i]);
        delay(2);
      }
      
      int avgReading = totalReading / numSamples;
      soilMoistureValues[i] = avgReading;
      
      // Add reading to history
      addReadingToHistory(i, avgReading);
      
      // Determine connection status only after detection is ready AND enough samples collected
      if (sensorDetectionReady && historyCount[i] >= READING_HISTORY_SIZE) {
        if (TEST_MODE) {
          // In test mode, assume all sensors are connected
          soilSensorConnected[i] = true;
        } else {
          // Use standard deviation to detect connection
          float stddev = calculateStandardDeviation(i);
          
          // Connected sensors have stable readings (low standard deviation)
          // Disconnected sensors have noisy/unstable readings (high standard deviation)
          if (stddev <= MAX_STDDEV_CONNECTED) {
            soilSensorConnected[i] = true;
          } else if (stddev >= MIN_STDDEV_DISCONNECTED) {
            soilSensorConnected[i] = false;
          }
          // If between thresholds, keep previous state (hysteresis)
        }
        
        // Log connection status changes only
        if (previousSensorConnected[i] != soilSensorConnected[i]) {
          float currentStddev = calculateStandardDeviation(i);
          if (soilSensorConnected[i]) {
            consoleLog(LOG_LEVEL_SUCCESS, "SENSOR", "Sensor connected (stddev=" + String(currentStddev, 2) + ")", i);
            addLogEntry(i, LOG_TYPE_CONNECTION, "Sensor connected (stddev: " + String(currentStddev, 2) + ")");
          } else {
            consoleLog(LOG_LEVEL_WARNING, "SENSOR", "Sensor disconnected (stddev=" + String(currentStddev, 2) + ")", i);
            addLogEntry(i, LOG_TYPE_CONNECTION, "Sensor disconnected (stddev: " + String(currentStddev, 2) + ")");
          }
          previousSensorConnected[i] = soilSensorConnected[i];
        }
      } else {
        // Before detection is ready or insufficient samples, mark as disconnected for safety
        soilSensorConnected[i] = false;
      }
    }
    
    // Show detection table once after enough samples are collected
    if (sensorDetectionReady && !detectionTableShown && historyCount[0] >= READING_HISTORY_SIZE) {
      printSensorDetectionTable();
      detectionTableShown = true;
    }
    
    // Print plant status every 20 sensor checks (reduce console spam)
    if (++statusUpdateCounter >= 20) {
      statusUpdateCounter = 0;
      
      if (sensorDetectionReady) {
        printPlantStatus();
        
        // Debug: Print raw sensor values and standard deviations
        String debugMsg = "Raw values - A0:" + String(soilMoistureValues[0]) + 
                         " A1:" + String(soilMoistureValues[1]) + 
                         " A2:" + String(soilMoistureValues[2]);
        
        if (!TEST_MODE) {
          debugMsg += " | StdDev - A0:" + String(calculateStandardDeviation(0), 2) +
                     " A1:" + String(calculateStandardDeviation(1), 2) +
                     " A2:" + String(calculateStandardDeviation(2), 2);
        } else {
          debugMsg += " | TEST_MODE: ON";
        }
        
        consoleLog(LOG_LEVEL_DEBUG, "SENSOR", debugMsg);
      } else {
        // During initialization phase
        unsigned long remainingTime = SENSOR_DETECTION_DELAY - (currentTime - systemStartTime);
        consoleLog(LOG_LEVEL_INFO, "SENSOR", "Collecting sensor data... " + String(remainingTime/1000) + "s remaining");
      }
    }
  }
  
  // Add a helper function to map moisture sensor readings to descriptive status
  String getMoistureStatus(int moisturePercent) {
    if (moisturePercent < 20) {
      return "Very Dry";
    } else if (moisturePercent < 40) {
      return "Dry";
    } else if (moisturePercent < 60) {
      return "Moderate";
    } else if (moisturePercent < 80) {
      return "Moist";
    } else {
      return "Wet";
    }
  }

  // Helper: Convert raw sensor reading to moisture percentage (0-100%)
  int getMoisturePercent(int sensorIndex) {
    if (!soilSensorConnected[sensorIndex]) return 0;
    int percent = map(soilMoistureValues[sensorIndex], SENSOR_RAW_DRY, SENSOR_RAW_WET, 0, 100);
    return constrain(percent, 0, 100);
  }

  // Helper: Send standard JSON HTTP response headers
  void sendJsonHeaders(WiFiClient& client, int statusCode = 200) {
    client.print("HTTP/1.1 ");
    client.print(statusCode);
    client.println(statusCode == 200 ? " OK" : " Error");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.print("Access-Control-Allow-Origin: ");
    client.println(CORS_ORIGIN);
    client.println();
  }
  
  // Calculate standard deviation for sensor readings
  float calculateStandardDeviation(int sensorIndex) {
    if (historyCount[sensorIndex] < 3) return 0.0; // Need at least 3 readings
    
    // Calculate mean
    float sum = 0;
    int count = min(historyCount[sensorIndex], READING_HISTORY_SIZE);
    for(int i = 0; i < count; i++) {
      sum += sensorReadingHistory[sensorIndex][i];
    }
    float mean = sum / count;
    
    // Calculate variance
    float varianceSum = 0;
    for(int i = 0; i < count; i++) {
      float diff = sensorReadingHistory[sensorIndex][i] - mean;
      varianceSum += diff * diff;
    }
    float variance = varianceSum / count;
    
    // Return standard deviation
    return sqrt(variance);
  }
  
  // Add reading to history buffer
  void addReadingToHistory(int sensorIndex, int reading) {
    sensorReadingHistory[sensorIndex][historyIndex[sensorIndex]] = reading;
    historyIndex[sensorIndex] = (historyIndex[sensorIndex] + 1) % READING_HISTORY_SIZE;
    
    if (historyCount[sensorIndex] < READING_HISTORY_SIZE) {
      historyCount[sensorIndex]++;
    }
  }
  
  // Print sensor detection results table
  void printSensorDetectionTable() {
    if (!CONSOLE_LOG_ENABLED) return;
    
    consoleLog(LOG_LEVEL_INFO, "SENSOR", "Sensor Detection Analysis Complete");
    
    if (ANSI_COLORS_ENABLED) {
      Serial.println(ANSI_BOLD ANSI_COLOR_BRIGHT_YELLOW "┌─────────────────── SENSOR DETECTION RESULTS ───────────────────┐" ANSI_COLOR_RESET);
    } else {
      Serial.println("================== SENSOR DETECTION RESULTS ==================");
    }
    
    for(int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      String plantName = String(plantNames[i]);
      String status = soilSensorConnected[i] ? "CONNECTED" : "DISCONNECTED";
      float stddev = calculateStandardDeviation(i);
      int currentValue = soilMoistureValues[i];
      
      if (ANSI_COLORS_ENABLED) {
        // Plant name and pin
        Serial.print(ANSI_COLOR_BRIGHT_WHITE "│ Pin A" + String(i) + " - ");
        Serial.print(ANSI_COLOR_BRIGHT_MAGENTA + plantName);
        
        // Pad plant name
        int namePadding = 25 - plantName.length();
        for(int j = 0; j < namePadding; j++) {
          Serial.print(" ");
        }
        
        // Status with color
        if (soilSensorConnected[i]) {
          Serial.print(ANSI_COLOR_BRIGHT_GREEN "CONNECTED   ");
        } else {
          Serial.print(ANSI_COLOR_BRIGHT_RED "DISCONNECTED");
        }
        
        Serial.println(ANSI_COLOR_BRIGHT_WHITE " │" ANSI_COLOR_RESET);
        
        // Details line
        Serial.print(ANSI_COLOR_BRIGHT_WHITE "│   Value: ");
        Serial.print(ANSI_COLOR_BRIGHT_CYAN + String(currentValue));
        Serial.print(ANSI_COLOR_BRIGHT_WHITE " | StdDev: ");
        Serial.print(ANSI_COLOR_BRIGHT_YELLOW + String(stddev, 2));
        Serial.print(ANSI_COLOR_BRIGHT_WHITE " | Samples: ");
        Serial.print(ANSI_COLOR_BRIGHT_BLUE + String(historyCount[i]));
        Serial.print(ANSI_COLOR_BRIGHT_WHITE "              │" ANSI_COLOR_RESET);
        Serial.println();
        
        if (i < SOIL_SENSORS_COUNT - 1) {
          Serial.println(ANSI_COLOR_BRIGHT_YELLOW "├──────────────────────────────────────────────────────────────────┤" ANSI_COLOR_RESET);
        }
      } else {
        // Plain text format
        Serial.print("Pin A" + String(i) + " - " + plantName);
        Serial.print(" | Status: " + status);
        Serial.print(" | Value: " + String(currentValue));
        Serial.print(" | StdDev: " + String(stddev, 2));
        Serial.println(" | Samples: " + String(historyCount[i]));
      }
    }
    
    if (ANSI_COLORS_ENABLED) {
      Serial.println(ANSI_BOLD ANSI_COLOR_BRIGHT_YELLOW "└──────────────────────────────────────────────────────────────────┘" ANSI_COLOR_RESET);
    } else {
      Serial.println("================================================================");
    }
    
    // Print detection criteria
    if (TEST_MODE) {
      consoleLog(LOG_LEVEL_WARNING, "SENSOR", "TEST_MODE active - all sensors assumed connected");
    } else {
      consoleLog(LOG_LEVEL_INFO, "SENSOR", "Detection criteria: Connected stddev<=" + String(MAX_STDDEV_CONNECTED) + 
                 ", Disconnected stddev>=" + String(MIN_STDDEV_DISCONNECTED));
    }
    
    Serial.println();
  }
  
  // Control fans and pumps based on moisture readings
  void controlPlantsEnvironment() {
    consoleLog(LOG_LEVEL_DEBUG, "CONTROL", "Running plant environment control...");
    
    for(int i = 0; i < SOIL_SENSORS_COUNT; i++) {
      if(!soilSensorConnected[i]) {
        // Check if sensor was previously connected and send disconnect email
        if(previousSensorConnected[i]) {
          sendPlantAlertEmail(i, EMAIL_TYPE_SENSOR_DISCONNECTED, "Sensor disconnected for " + String(plantNames[i]));
          previousSensorConnected[i] = false;
        }
        
        // If sensor is disconnected, turn off both pump and fan for safety
        if(pumpActive[i]) {
          digitalWrite(pumpPins[i], LOW);
          pumpActive[i] = false;
          consoleLog(LOG_LEVEL_WARNING, "CONTROL", "Water pump turned OFF (sensor disconnected)", i);
          addLogEntry(i, LOG_TYPE_COMPONENT, "Water pump turned OFF (sensor disconnected)");
        }
        
        if(fanActive[i]) {
          digitalWrite(fanPins[i], LOW);
          fanActive[i] = false;
          consoleLog(LOG_LEVEL_WARNING, "CONTROL", "Fan turned OFF (sensor disconnected)", i);
          addLogEntry(i, LOG_TYPE_COMPONENT, "Fan turned OFF (sensor disconnected)");
        }
        continue; // Skip to next plant
      }
      
      // Update sensor connection status
      previousSensorConnected[i] = true;
      
      int moisturePercent = getMoisturePercent(i);
      
      // Pump control logic
      if (moisturePercent <= MOISTURE_TOO_LOW && !pumpActive[i]) {
        // Soil is too dry, turn on pump
        digitalWrite(pumpPins[i], HIGH);
        pumpActive[i] = true;
        consoleLog(LOG_LEVEL_SUCCESS, "CONTROL", "Water pump activated (moisture: " + String(moisturePercent) + "%)", i);
        addLogEntry(i, LOG_TYPE_COMPONENT, "Water pump ON (moisture: " + String(moisturePercent) + "%)");
        
        // Send email notification for low moisture
        sendPlantAlertEmail(i, EMAIL_TYPE_MOISTURE_LOW, "Low moisture detected", moisturePercent);
        
      } else if (moisturePercent >= MOISTURE_IDEAL_LOW && pumpActive[i]) {
        // Soil moisture is adequate, turn off pump
        digitalWrite(pumpPins[i], LOW);
        pumpActive[i] = false;
        consoleLog(LOG_LEVEL_INFO, "CONTROL", "Water pump deactivated (moisture: " + String(moisturePercent) + "%)", i);
        addLogEntry(i, LOG_TYPE_COMPONENT, "Water pump OFF (moisture: " + String(moisturePercent) + "%)");
      }
      
      // Fan control logic
      if (moisturePercent >= MOISTURE_TOO_HIGH && !fanActive[i]) {
        // Soil is too wet, turn on fan
        digitalWrite(fanPins[i], HIGH);
        fanActive[i] = true;
        consoleLog(LOG_LEVEL_SUCCESS, "CONTROL", "Fan activated (moisture: " + String(moisturePercent) + "%)", i);
        addLogEntry(i, LOG_TYPE_COMPONENT, "Fan ON (moisture: " + String(moisturePercent) + "%)");
        
        // Send email notification for high moisture
        sendPlantAlertEmail(i, EMAIL_TYPE_MOISTURE_HIGH, "High moisture detected", moisturePercent);
        
      } else if (moisturePercent <= MOISTURE_IDEAL_HIGH && fanActive[i]) {
        // Soil moisture is adequate, turn off fan
        digitalWrite(fanPins[i], LOW);
        fanActive[i] = false;
        consoleLog(LOG_LEVEL_INFO, "CONTROL", "Fan deactivated (moisture: " + String(moisturePercent) + "%)", i);
        addLogEntry(i, LOG_TYPE_COMPONENT, "Fan OFF (moisture: " + String(moisturePercent) + "%)");
      }
      
      // Log moisture changes if significant
      if (abs(moisturePercent - previousMoistureValues[i]) >= 5) {
        addLogEntry(i, LOG_TYPE_HUMIDITY, "Moisture changed to " + String(moisturePercent) + "%");
        previousMoistureValues[i] = moisturePercent;
      }
    }
  }
  
  // Add function to export all logs to SD card
  void exportLogsToSD() {
    if (!sdCardInitialized) return;
    
    File exportFile = SD.open("all_logs.txt", FILE_WRITE);
    if (exportFile) {
      exportFile.println("===== HUMI v2 System Logs =====");
      exportFile.println("Exported at: " + String(millis()));
      
      for (int i = 0; i < SOIL_SENSORS_COUNT; i++) {
        exportFile.println("\n=== " + String(plantNames[i]) + " ===");
        
        // Calculate starting index for logs (for circular buffer)
        int startIdx = (logCount[i] < MAX_LOGS_PER_PLANT) ? 0 : logIndex[i];
        int count = min(logCount[i], MAX_LOGS_PER_PLANT);
        
        for(int j = 0; j < count; j++) {
          int idx = (startIdx + j) % MAX_LOGS_PER_PLANT;
          
          exportFile.print("[");
          exportFile.print(plantLogs[i][idx].timestamp);
          exportFile.print("] ");
          
          // Add log type
          switch(plantLogs[i][idx].logType) {
            case LOG_TYPE_COMPONENT:
              exportFile.print("COMPONENT: ");
              break;
            case LOG_TYPE_HUMIDITY:
              exportFile.print("HUMIDITY: ");
              break;
            case LOG_TYPE_CONNECTION:
              exportFile.print("CONNECTION: ");
              break;
            default:
              exportFile.print("UNKNOWN: ");
          }
          
          exportFile.println(plantLogs[i][idx].message);
        }
      }
      
      exportFile.close();
      Serial.println("Logs exported to SD card");
    } else {
      Serial.println("Error opening export log file");
    }
  }
  
  // New API endpoint handlers
  void handlePlantControl(WiFiClient& client, String path) {
    // Extract plant ID and action from path like /api/plant/0/pump or /api/plant/1/fan
    int plantId = -1;
    String action = "";
    
    // Parse path: /api/plant/{id}/{action}
    int firstSlash = path.indexOf('/', 11); // After "/api/plant/"
    int secondSlash = path.indexOf('/', firstSlash + 1);
    
    if (firstSlash > 0 && secondSlash > 0) {
      plantId = path.substring(firstSlash + 1, secondSlash).toInt();
      action = path.substring(secondSlash + 1);
    }
    
    consoleLog(LOG_LEVEL_INFO, "API", "Control request for plant " + String(plantId) + " action: " + action);
    
    // Validate plant ID
    if (plantId < 0 || plantId >= SOIL_SENSORS_COUNT) {
      consoleLog(LOG_LEVEL_ERROR, "API", "Invalid plant ID: " + String(plantId));
      sendErrorResponse(client, 400, "Invalid plant ID. Must be 0-" + String(SOIL_SENSORS_COUNT - 1));
      return;
    }
    
    // Process action
    if (action == "pump") {
      // Toggle pump manually
      bool newState = !pumpActive[plantId];
      digitalWrite(pumpPins[plantId], newState ? HIGH : LOW);
      pumpActive[plantId] = newState;
      
      String statusMsg = "Water pump " + String(newState ? "activated" : "deactivated") + " for " + String(plantNames[plantId]);
      consoleLog(newState ? LOG_LEVEL_SUCCESS : LOG_LEVEL_INFO, "CONTROL", statusMsg, plantId);
      addLogEntry(plantId, LOG_TYPE_COMPONENT, "Water pump " + String(newState ? "ON" : "OFF") + " (manual control)");
      
      sendSuccessResponse(client, "Pump " + String(newState ? "activated" : "deactivated") + " for " + String(plantNames[plantId]));
    } 
    else if (action == "fan") {
      // Toggle fan manually
      bool newState = !fanActive[plantId];
      digitalWrite(fanPins[plantId], newState ? HIGH : LOW);
      fanActive[plantId] = newState;
      
      String statusMsg = "Fan " + String(newState ? "activated" : "deactivated") + " for " + String(plantNames[plantId]);
      consoleLog(newState ? LOG_LEVEL_SUCCESS : LOG_LEVEL_INFO, "CONTROL", statusMsg, plantId);
      addLogEntry(plantId, LOG_TYPE_COMPONENT, "Fan " + String(newState ? "ON" : "OFF") + " (manual control)");
      
      sendSuccessResponse(client, "Fan " + String(newState ? "activated" : "deactivated") + " for " + String(plantNames[plantId]));
    }
    else {
      consoleLog(LOG_LEVEL_ERROR, "API", "Invalid action: " + action);
      sendErrorResponse(client, 400, "Invalid action. Use 'pump' or 'fan'");
    }
  }
  
  void handlePlantStatusRequest(WiFiClient& client, String path) {
    // Extract plant ID from path like /api/plant/0/status
    int plantId = -1;
    int firstSlash = path.indexOf('/', 11); // After "/api/plant/"
    int secondSlash = path.indexOf('/', firstSlash + 1);
    
    if (firstSlash > 0 && secondSlash > 0) {
      plantId = path.substring(firstSlash + 1, secondSlash).toInt();
    }
    
    // Validate plant ID
    if (plantId < 0 || plantId >= SOIL_SENSORS_COUNT) {
      sendErrorResponse(client, 400, "Invalid plant ID. Must be 0-" + String(SOIL_SENSORS_COUNT - 1));
      return;
    }
    
    // Send individual plant status
    sendJsonHeaders(client);
    
    client.print("{");
    client.print("\"plant_id\":" + String(plantId) + ",");
    client.print("\"name\":\"" + String(plantNames[plantId]) + "\",");
    client.print("\"pin\":\"A" + String(plantId) + "\",");
    client.print("\"connected\":");
    client.print(soilSensorConnected[plantId] ? "true" : "false");
    client.print(",");
    client.print("\"moisture\":{");
    client.print("\"raw_value\":" + String(soilMoistureValues[plantId]) + ",");
    
    int moisturePercent = getMoisturePercent(plantId);
    
    client.print("\"percentage\":" + String(moisturePercent) + ",");
    client.print("\"status\":\"");
    client.print(soilSensorConnected[plantId] ? getMoistureStatus(moisturePercent) : "Not Connected");
    client.print("\"");
    client.print("},");
    
    client.print("\"controls\":{");
    client.print("\"pump\":{");
    client.print("\"pin\":\"D" + String(pumpPins[plantId]) + "\",");
    client.print("\"active\":");
    client.print(pumpActive[plantId] ? "true" : "false");
    client.print("},");
    client.print("\"fan\":{");
    client.print("\"pin\":\"D" + String(fanPins[plantId]) + "\",");
    client.print("\"active\":");
    client.print(fanActive[plantId] ? "true" : "false");
    client.print("}");
    client.print("},");
    
    // Sensor detection info
    client.print("\"sensor_stddev\":");
    client.print(String(calculateStandardDeviation(plantId), 2));
    client.print(",\"sensor_detection_ready\":");
    client.print(sensorDetectionReady ? "true" : "false");
    client.print(",\"test_mode\":");
    client.print(TEST_MODE ? "true" : "false");
    client.print(",");
    
    // All logs for this plant
    client.print("\"logs\":[");
    int startIdx = (logCount[plantId] < MAX_LOGS_PER_PLANT) ? 0 : logIndex[plantId];
    int count = min(logCount[plantId], MAX_LOGS_PER_PLANT);
    for(int j = 0; j < count; j++) {
      int idx = (startIdx + j) % MAX_LOGS_PER_PLANT;
      client.print("{\"timestamp\":");
      client.print(plantLogs[plantId][idx].timestamp);
      client.print(",\"time_ago\":\"");
      client.print(formatTimeAgo(plantLogs[plantId][idx].timestamp));
      client.print("\",\"type\":");
      client.print(plantLogs[plantId][idx].logType);
      client.print(",\"message\":\"");
      client.print(plantLogs[plantId][idx].message);
      client.print("\"}");
      if(j < count - 1) client.print(",");
    }
    client.print("],");
    
    client.print("\"last_log\":\"");
    if (logCount[plantId] > 0) {
      int lastLogIdx = (logIndex[plantId] - 1 + MAX_LOGS_PER_PLANT) % MAX_LOGS_PER_PLANT;
      client.print(plantLogs[plantId][lastLogIdx].message);
    } else {
      client.print("No logs available");
    }
    client.print("\",");
    
    client.print("\"timestamp\":" + String(millis()));
    client.print("}");
  }
  
  void sendThresholdsResponse(WiFiClient& client) {
    sendJsonHeaders(client);
    
    client.print("{");
    client.print("\"moisture_too_low\":" + String(MOISTURE_TOO_LOW) + ",");
    client.print("\"moisture_too_high\":" + String(MOISTURE_TOO_HIGH) + ",");
    client.print("\"moisture_ideal_low\":" + String(MOISTURE_IDEAL_LOW) + ",");
    client.print("\"moisture_ideal_high\":" + String(MOISTURE_IDEAL_HIGH));
    client.print("}");
  }
  
  void handleThresholdUpdate(WiFiClient& client, String& body) {
    if (body.length() == 0) {
      sendErrorResponse(client, 400, "Empty request body");
      return;
    }
    
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      sendErrorResponse(client, 400, "Invalid JSON: " + String(err.c_str()));
      return;
    }
    
    int updated = 0;
    
    if (doc.containsKey("moisture_too_low")) {
      int v = doc["moisture_too_low"].as<int>();
      if (v >= 0 && v <= 100) { MOISTURE_TOO_LOW = v; updated++; }
    }
    if (doc.containsKey("moisture_too_high")) {
      int v = doc["moisture_too_high"].as<int>();
      if (v >= 0 && v <= 100) { MOISTURE_TOO_HIGH = v; updated++; }
    }
    if (doc.containsKey("moisture_ideal_low")) {
      int v = doc["moisture_ideal_low"].as<int>();
      if (v >= 0 && v <= 100) { MOISTURE_IDEAL_LOW = v; updated++; }
    }
    if (doc.containsKey("moisture_ideal_high")) {
      int v = doc["moisture_ideal_high"].as<int>();
      if (v >= 0 && v <= 100) { MOISTURE_IDEAL_HIGH = v; updated++; }
    }
    
    // Validate logical ordering
    if (MOISTURE_TOO_LOW >= MOISTURE_IDEAL_LOW || MOISTURE_IDEAL_LOW >= MOISTURE_IDEAL_HIGH || MOISTURE_IDEAL_HIGH >= MOISTURE_TOO_HIGH) {
      sendErrorResponse(client, 400, "Invalid threshold order. Required: too_low < ideal_low < ideal_high < too_high");
      return;
    }
    
    if (updated > 0) {
      saveThresholdsToSD();
      consoleLog(LOG_LEVEL_SUCCESS, "CONFIG", "Thresholds updated: " + String(MOISTURE_TOO_LOW) + "/" + String(MOISTURE_IDEAL_LOW) + "/" + String(MOISTURE_IDEAL_HIGH) + "/" + String(MOISTURE_TOO_HIGH));
    }
    
    sendJsonHeaders(client);
    client.print("{\"success\":true,\"updated\":");
    client.print(updated);
    client.print(",\"moisture_too_low\":");
    client.print(MOISTURE_TOO_LOW);
    client.print(",\"moisture_too_high\":");
    client.print(MOISTURE_TOO_HIGH);
    client.print(",\"moisture_ideal_low\":");
    client.print(MOISTURE_IDEAL_LOW);
    client.print(",\"moisture_ideal_high\":");
    client.print(MOISTURE_IDEAL_HIGH);
    client.print(",\"timestamp\":");
    client.print(millis());
    client.print("}");
  }
  
  void handleLogExport(WiFiClient& client) {
    if (!sdCardInitialized) {
      consoleLog(LOG_LEVEL_ERROR, "SD_CARD", "SD card not available for log export");
      sendErrorResponse(client, 503, "SD card not available");
      return;
    }
    
    consoleLog(LOG_LEVEL_INFO, "SD_CARD", "Exporting logs to SD card...");
    exportLogsToSD();
    consoleLog(LOG_LEVEL_SUCCESS, "SD_CARD", "Logs exported successfully");
    sendSuccessResponse(client, "Logs exported to SD card as 'all_logs.txt'");
  }
  
  void handleSystemRestart(WiFiClient& client) {
    consoleLog(LOG_LEVEL_WARNING, "SYSTEM", "System restart requested via API");
    sendSuccessResponse(client, "System restart initiated in 3 seconds");
    
    printHeader("System Restart Initiated");
    consoleLog(LOG_LEVEL_INFO, "SYSTEM", "Goodbye! See you on the other side...");
    
    delay(3000);
    // Software reset for Arduino Uno R4
    NVIC_SystemReset();
  }
  
  void handleTestModeToggle(WiFiClient& client) {
    TEST_MODE = !TEST_MODE;
    
    if (TEST_MODE) {
      consoleLog(LOG_LEVEL_WARNING, "SYSTEM", "TEST_MODE enabled - sensor detection bypassed");
      // Clear history when entering test mode
      for(int i = 0; i < SOIL_SENSORS_COUNT; i++) {
        historyCount[i] = 0;
        historyIndex[i] = 0;
        soilSensorConnected[i] = true; // Assume all connected in test mode
      }
      sendSuccessResponse(client, "Test mode ENABLED - sensor detection bypassed");
    } else {
      consoleLog(LOG_LEVEL_INFO, "SYSTEM", "TEST_MODE disabled - standard deviation detection active");
      // Clear history when exiting test mode to get fresh readings
      for(int i = 0; i < SOIL_SENSORS_COUNT; i++) {
        historyCount[i] = 0;
        historyIndex[i] = 0;
      }
      sendSuccessResponse(client, "Test mode DISABLED - standard deviation detection active");
    }
  }
  
  // ========================================
  // Email Handler Functions
  // ========================================
  
  void handleEmailSettingsRequest(WiFiClient& client) {
    sendJsonHeaders(client);
    
    client.print("{");
    client.print("\"email_notifications_enabled\":" + String(emailNotificationsEnabled ? "true" : "false") + ",");
    client.print("\"from_email\":\"" + String(FROM_EMAIL) + "\",");
    client.print("\"from_name\":\"" + String(FROM_NAME) + "\",");
    client.print("\"to_email\":\"" + String(TO_EMAIL) + "\",");
    client.print("\"email_cooldown_period\":" + String(emailCooldownPeriod) + ",");
    client.print("\"critical_email_cooldown\":" + String(criticalEmailCooldown) + ",");
    client.print("\"internet_connected\":" + String(internetConnected ? "true" : "false") + ",");
    client.print("\"last_email_sent\":" + String(lastEmailSent) + ",");
    client.print("\"time_since_last_email\":" + String(millis() - lastEmailSent));
    client.print("}");
  }
  
  void handleEmailSettingsUpdate(WiFiClient& client, String& body) {
    // Parse simple enable/disable command
    if (body.indexOf("enable") != -1 || body.indexOf("\"enabled\":true") != -1) {
      emailNotificationsEnabled = true;
      consoleLog(LOG_LEVEL_SUCCESS, "EMAIL", "Email notifications enabled");
      sendSuccessResponse(client, "Email notifications enabled");
    } else if (body.indexOf("disable") != -1 || body.indexOf("\"enabled\":false") != -1) {
      emailNotificationsEnabled = false;
      consoleLog(LOG_LEVEL_INFO, "EMAIL", "Email notifications disabled");
      sendSuccessResponse(client, "Email notifications disabled");
    } else {
      sendErrorResponse(client, 400, "Invalid request. Send {\"enabled\":true} or {\"enabled\":false}");
    }
  }
  
  void handleTestEmail(WiFiClient& client) {
    consoleLog(LOG_LEVEL_INFO, "EMAIL", "Test email requested via API");
    
    String subject = "HUMI v2 Test Email";
    String textContent = "This is a test email from your HUMI v2 plant monitoring system.\n\n";
    textContent += "If you received this email, MailerSend integration is working correctly!\n\n";
    textContent += "System IP: " + WiFi.localIP().toString() + "\n";
    textContent += "Timestamp: " + String(millis()) + "ms\n";
    
    String htmlContent = "<h2>HUMI v2 Test Email</h2>";
    htmlContent += "<p>This is a test email from your <strong>HUMI v2</strong> plant monitoring system.</p>";
    htmlContent += "<p>If you received this email, MailerSend integration is working correctly!</p>";
    htmlContent += "<hr>";
    htmlContent += "<p><small>";
    htmlContent += "System IP: " + WiFi.localIP().toString() + "<br>";
    htmlContent += "Timestamp: " + String(millis()) + "ms";
    htmlContent += "</small></p>";
    
    if (sendMailerSendEmail(subject, htmlContent, textContent)) {
      sendSuccessResponse(client, "Test email sent successfully");
    } else {
      sendErrorResponse(client, 500, "Failed to send test email");
    }
  }
  
  void handleStatusEmail(WiFiClient& client) {
    consoleLog(LOG_LEVEL_INFO, "EMAIL", "Status email requested via API");
    
    sendSystemStatusEmail();
    sendSuccessResponse(client, "System status email sent");
  }
  
  // ========================================
  // End of Email Handler Functions
  // ========================================
