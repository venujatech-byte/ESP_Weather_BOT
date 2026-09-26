//////////////// Libraries
#include "secrets.h"
#include <WiFiManager.h>  // WiFiManager for WiFi configuration
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Arduino.h>
#include <EEPROM.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <NimBLEDevice.h>  // NimBLE is a lighter BLE stack alternative
#include <ESPmDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Forward declarations
void scanDevices();
void handleNewMessages(int newMessages);
void sendWelcomeMessage();
void connectToWiFi();
void resetWiFiSettings();
void checkWeather();
void sendWeatherInfo();
void sendForecastInfo();
String customUrlEncode(const String &str);
void sendDataToESP8266();
void handleReceiveData();
void sendMessageTelegram(String message);
void sendMessageWhatsApp(String message);
void updateBlink();
void updateBlynkSwitch(String virtualPin, int value);
void logEvent(String code, String description);
void printWakeUpReason();

// NTP Server
const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = 19800; // Offset for UTC+5:30
WiFiUDP udp;
NTPClient timeClient(udp, ntpServer, utcOffsetInSeconds);

WiFiManager wifiManager;
const char* targetDevice1 = TARGET_BLE_DEVICE_1;
const char* targetDevice2 = TARGET_BLE_DEVICE_2;
const char* targetDevice3 = TARGET_BLE_DEVICE_3;

// Last state of device detection
bool device1LastState = false;
bool device2LastState = false;
bool device3LastState = false;
bool isScanningActive = false;
NimBLEScan* pBLEScan = nullptr;

// RSSI Threshold for 3 meters (approx)
const int RSSI_THRESHOLD = -75;  // Adjust this based on testing
const int DEVICE1_ADDR = 50;
const int DEVICE2_ADDR = 51;
const int DEVICE3_ADDR = 52;

// Core management
TaskHandle_t task1TaskHandle = NULL;
TaskHandle_t task2TaskHandle = NULL;
TaskHandle_t task3TaskHandle = NULL;

void scanDevices() {
  if (!pBLEScan) return;

  // Scan for 3 seconds (passive scan allows WiFi packets to co-exist)
  pBLEScan->start(3, false);
  NimBLEScanResults foundDevices = pBLEScan->getResults();
  int count = foundDevices.getCount();

  for (int i = 0; i < count; i++) {
    const NimBLEAdvertisedDevice* advertisedDevice = foundDevices.getDevice(i);
    String address = advertisedDevice->getAddress().toString().c_str();
    int rssi = advertisedDevice->getRSSI();

    // Only consider devices within RSSI_THRESHOLD (approx. 3 meters)
    if (rssi > RSSI_THRESHOLD) {
      // Check for Device 1
      if (address == targetDevice1) {
        if (!device1LastState) {
          device1LastState = true;
          EEPROM.write(DEVICE1_ADDR, device1LastState);
          EEPROM.commit();
          Serial.println("Venuja is at Door");
          sendMessageWhatsApp("Venuja is at Door");
          logEvent("motion_sense", "Venuja is at Door");
        }
      } else if (device1LastState) {
        device1LastState = false;
        EEPROM.write(DEVICE1_ADDR, device1LastState);
        EEPROM.commit();
        Serial.println("Venuja has left the Door");
      }

      // Check for Device 2
      if (address == targetDevice2) {
        if (!device2LastState) {
          device2LastState = true;
          EEPROM.write(DEVICE2_ADDR, device2LastState);
          EEPROM.commit();
          Serial.println("Sanija is at Door");
          sendMessageWhatsApp("Sanija is at Door");
          logEvent("motion_sense", "Sanija is at Door");
        }
      } else if (device2LastState) {
        device2LastState = false;
        EEPROM.write(DEVICE2_ADDR, device2LastState);
        EEPROM.commit();
        Serial.println("Sanija has left the Door");
      }

      // Check for Device 3
      if (address == targetDevice3) {
        if (!device3LastState) {
          device3LastState = true;
          EEPROM.write(DEVICE3_ADDR, device3LastState);
          EEPROM.commit();
          Serial.println("Athula is at Door");
          sendMessageWhatsApp("Athula is at Door");
          logEvent("motion_sense", "Athula is at Door");
        }
      } else if (device3LastState) {
        device3LastState = false;
        EEPROM.write(DEVICE3_ADDR, device3LastState);
        EEPROM.commit();
        Serial.println("Athula has left the Door");
      }

    } else {
      // If the RSSI is lower than the threshold, mark devices as left
      if (device1LastState) {
        device1LastState = false;
        EEPROM.write(DEVICE1_ADDR, device1LastState);
        EEPROM.commit();
        Serial.println("Venuja has left the Door");
      }

      if (device2LastState) {
        device2LastState = false;
        EEPROM.write(DEVICE2_ADDR, device2LastState);
        EEPROM.commit();
        Serial.println("Sanija has left the Door");
      }

      if (device3LastState) {
        device3LastState = false;
        EEPROM.write(DEVICE3_ADDR, device3LastState);
        EEPROM.commit();
        Serial.println("Athula has left the Door");
      }
    }
  }

  pBLEScan->clearResults();
}

// I2C address of the ESP32
#define ESP32_ADDR 0x08

// Pins
#define BOOT_BUTTON_PIN 0  // GPIO pin for BOOT button (GPIO 0)
#define LED_PIN 2          // Usually onboard LED is connected to GPIO 2

// Telegram Bot Code
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Weather API
const char* city = WEATHER_CITY;
const char* weatherApiUrl = "http://api.weatherapi.com/v1/current.json";
const char* forecastApiUrl = "http://api.weatherapi.com/v1/forecast.json";
const char* weatherApiKey = WEATHER_API_KEY;

// WhatsApp API
String MobileNumber = WHATSAPP_MOBILE_NUMBER;
String APIKey = WHATSAPP_API_KEY;

// Peer ESP server & local WebServer
const char* serverIP = PEER_SERVER_IP;
const int port = PEER_SERVER_PORT;
WebServer server(80);

// Static IP Network
IPAddress staticIP = STATIC_IP;
IPAddress gateway = GATEWAY_IP;
IPAddress subnet = SUBNET_MASK;
IPAddress primaryDNS = PRIMARY_DNS;
IPAddress secondaryDNS = SECONDARY_DNS;

// Booleans & Timers
unsigned long resetButtonPressTime = 0;
bool resetInProgress = false;
bool otaInProgress = false;
bool raining = false;
unsigned long lastWeatherCheck = 0;
unsigned long weatherCheckInterval = 600000;  // 10 minutes
unsigned long lastTimeBotRan = 0;
bool wasRestarted = false;

// Integers & States
int motionsense = 0;
int temperature = 0;
int humidity = 0;
int mo = 0;
int ba = 0;
int pm = 0;
int tm = 0;
int botRequestDelay = 1000;
int sonarswitch = 0;
int batteryswitch = 0;
int motionswitch = 0;
int weatherswitch = 0;
int scanswitch = 1;
int lastsleep = 0;
int distance = 0;
int sonarTriggerThreshold = 100;
long duration = 0;
int chargeLevel = 0;
float voltage = 0;

// Definitions
#define TEMPERATURE_THRESHOLD 95.0
#define DEEP_SLEEP_DURATION 300000000  // 5 minutes
#define SLEEPADDR 6
#define EEPROM_SIZE 512
#define tf 1000000

// Non-blocking LED state tracking
unsigned long lastBlinkToggle = 0;
bool ledState = false;

void updateBlink() {
  unsigned long now = millis();
  if (ledState && (now - lastBlinkToggle >= 200)) {
    digitalWrite(LED_PIN, LOW);
    ledState = false;
    lastBlinkToggle = now;
  } else if (!ledState && (now - lastBlinkToggle >= 600)) {
    digitalWrite(LED_PIN, HIGH);
    ledState = true;
    lastBlinkToggle = now;
  }
}

// Telegram polling task (Core 0)
void task1(void* pvParameters) {
  UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
  Serial.println("Task1 remaining stack space: " + String(highWaterMark * 4 / 1024) + " KB");

  while (true) {
    if (millis() - lastTimeBotRan > (unsigned long)botRequestDelay) {
      int newMessages = bot.getUpdates(bot.last_message_received + 1);
      while (newMessages) {
        handleNewMessages(newMessages);
        newMessages = bot.getUpdates(bot.last_message_received + 1);
      }
      lastTimeBotRan = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// BLE Scanner Task (Core 1)
void task2(void* pvParameters) {
  UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
  Serial.println("Task2 remaining stack space: " + String(highWaterMark * 4 / 1024) + " KB");

  while (true) {
    if (isScanningActive) {
      Serial.println("scanning devices");
      scanDevices();
      // Rest pause between scans to allow WiFi radio coexistence
      vTaskDelay(pdMS_TO_TICKS(1500));
    } else {
      // Must yield CPU when inactive to prevent TWDT starvation on Core 1
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
}

// Periodic Weather Check Task (Core 0)
void checkWeatherTask(void* parameter) {
  while (true) {
    checkWeather();
    vTaskDelay(pdMS_TO_TICKS(100000));  // Check every 100 seconds
  }
}

// Web Server and OTA Task (Core 0)
void task3(void* pvParameters) {
  UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
  Serial.println("Task3 remaining stack space: " + String(highWaterMark * 4 / 1024) + " KB");

  while (true) {
    server.handleClient();
    ArduinoOTA.handle();

    // Check BOOT button state for WiFi reset
    if (!otaInProgress) {
      if (digitalRead(BOOT_BUTTON_PIN) == LOW && !resetInProgress) {
        resetInProgress = true;
        resetButtonPressTime = millis();
      }

      if (resetInProgress && digitalRead(BOOT_BUTTON_PIN) == HIGH) {
        if (millis() - resetButtonPressTime > 5000) {
          resetWiFiSettings();
          ESP.restart();
        }
        resetInProgress = false;
      }
    } else {
      updateBlink();
    }

    // Crucial yield so Core 0 IDLE task can feed the watchdog
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nConfiguring Device...");

  printWakeUpReason();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  motionswitch = EEPROM.read(7);
  sonarswitch = EEPROM.read(8);
  batteryswitch = EEPROM.read(9);
  weatherswitch = EEPROM.read(10);
  voltage = EEPROM.read(90);
  pm = EEPROM.read(70);
  raining = (EEPROM.read(11) == 1);
  device1LastState = EEPROM.read(DEVICE1_ADDR);
  device2LastState = EEPROM.read(DEVICE2_ADDR);
  device3LastState = EEPROM.read(DEVICE3_ADDR);
  isScanningActive = EEPROM.read(60);
  scanswitch = EEPROM.read(80);

  Serial.println("Loaded states from EEPROM:");
  Serial.printf("Device1: %s\n", device1LastState ? "Arrived" : "Left");
  Serial.printf("Device2: %s\n", device2LastState ? "Arrived" : "Left");
  Serial.printf("Device3: %s\n", device3LastState ? "Arrived" : "Left");
  Serial.printf("isScanningActive: %s\n", isScanningActive ? "Active" : "Deactive");

  lastsleep = EEPROM.read(SLEEPADDR);
  Serial.print("Last sleep status: ");
  Serial.println(lastsleep);

  connectToWiFi();

  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  // Configure ArduinoOTA
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("OTA Update Start: " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Finished");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    String errorMessage;
    switch (error) {
      case OTA_AUTH_ERROR:    errorMessage = "OTA Auth Failed"; break;
      case OTA_BEGIN_ERROR:   errorMessage = "OTA Begin Failed"; break;
      case OTA_CONNECT_ERROR: errorMessage = "OTA Connect Failed"; break;
      case OTA_RECEIVE_ERROR: errorMessage = "OTA Receive Failed"; break;
      case OTA_END_ERROR:     errorMessage = "OTA End Failed"; break;
    }
    Serial.println("OTA Error: " + errorMessage);
  });

  sendWelcomeMessage();

  server.on("/data", HTTP_POST, handleReceiveData);
  server.begin();
  Serial.println("ESP32 HTTP server started");
  sendDataToESP8266();

  // Initialize NimBLE with balanced parameters for WiFi coexistence
  NimBLEDevice::init("ESP32_BLE_Scanner");
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setActiveScan(false);  // Passive scan minimizes transmission collisions
  pBLEScan->setInterval(160);      // Interval in ms
  pBLEScan->setWindow(80);         // 50% duty cycle allows WiFi radio time

  Serial.println("Setup succeeded; launching FreeRTOS tasks...");

  // Task 2: BLE scanning pinned to Core 1
  xTaskCreatePinnedToCore(task2, "task2", 6144, NULL, 1, &task2TaskHandle, 1);

  // Task 1: Telegram Bot pinned to Core 0 (increased stack for TLS)
  xTaskCreatePinnedToCore(task1, "task1", 8192, NULL, 1, &task1TaskHandle, 0);

  // Task 3: Web Server & OTA pinned to Core 0
  xTaskCreatePinnedToCore(task3, "task3", 6144, NULL, 1, &task3TaskHandle, 0);

  // Task 4: Weather check pinned to Core 0
  xTaskCreatePinnedToCore(checkWeatherTask, "CheckWeather", 8192, NULL, 1, NULL, 0);

  timeClient.begin();
  checkWeather();
}

void printWakeUpReason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP:      Serial.println("Wakeup caused by ULP program"); break;
    default:                        Serial.println("Normal startup and not caused by deep sleep"); break;
  }
}

void loop() {
  updateBlink();

  float inttemperature = temperatureRead();

  if (inttemperature > TEMPERATURE_THRESHOLD && !otaInProgress) {
    Serial.println("Temperature exceeds threshold! Going to deep sleep...");
    sendMessageTelegram("Device over heated and starting deep sleep");
    esp_sleep_enable_timer_wakeup(300 * tf);
    esp_deep_sleep_start();
  }

  if (inttemperature > 90.0 && !otaInProgress && tm == 0 && pm == 0) {
    Serial.println("Device over heated and powersaving mode on");
    sendMessageTelegram("Device over heated and powersaving mode on");
    setCpuFrequencyMhz(80);
    tm = 1;
  } else if (inttemperature <= 90.0 && tm == 1 && pm == 0) {
    setCpuFrequencyMhz(240);
    tm = 0;
    Serial.println("Device cooled down and turning off power saving mode");
    sendMessageTelegram("Device cooled down and turning off power saving mode");
  }

  timeClient.update();

  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();

  if ((hours >= 22 || hours < 6) || (hours == 6 && minutes == 0)) {
    isScanningActive = false;
  } else if (scanswitch == 1) {
    isScanningActive = true;
  }

  // Yield to avoid pegging the loop CPU
  vTaskDelay(pdMS_TO_TICKS(50));
}

void handleNewMessages(int newMessages) {
  for (int i = 0; i < newMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);

    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    String text = bot.messages[i].text;

    if (text == "/start") {
      sendWelcomeMessage();
    } else if (text == "/weather") {
      sendWeatherInfo();
    } else if (text == "/forecast") {
      sendForecastInfo();
    } else if (text == "/motionsensor_on") {
      sendMessageTelegram("motion sensor turned on");
      motionswitch = 1;
      EEPROM.write(7, motionswitch);
      EEPROM.commit();
      updateBlynkSwitch("V1", 1);
    } else if (text == "/motionsensor_off") {
      sendMessageTelegram("motion sensor turned off");
      motionswitch = 0;
      EEPROM.write(7, motionswitch);
      EEPROM.commit();
      updateBlynkSwitch("V1", 0);
    } else if (text == "/dht") {
      sendMessageTelegram("Temperature: " + String(temperature) + " °C , Humidity: " + String(humidity) + " %");
    } else if (text == "/battery") {
      float storedVoltage;
      EEPROM.get(90, storedVoltage);
      sendMessageTelegram("Battery Level: " + String(chargeLevel) + " , Battery Voltage: " + String(storedVoltage));
    } else if (text == "/distance") {
      sendMessageTelegram(String(distance));
    } else if (text == "/sonarsensor_on") {
      sendMessageTelegram("sonar sensor turned on");
      sonarswitch = 1;
      EEPROM.write(8, sonarswitch);
      EEPROM.commit();
      sendDataToESP8266();
    } else if (text == "/sonarsensor_off") {
      sendMessageTelegram("sonar sensor turned off");
      sonarswitch = 0;
      EEPROM.write(8, sonarswitch);
      EEPROM.commit();
      sendDataToESP8266();
    } else if (text == "/batterynotify_on") {
      sendMessageTelegram("battery notifications on");
      batteryswitch = 1;
      EEPROM.write(9, batteryswitch);
      EEPROM.commit();
      sendDataToESP8266();
    } else if (text == "/batterynotify_off") {
      sendMessageTelegram("battery notifications off");
      batteryswitch = 0;
      EEPROM.write(9, batteryswitch);
      EEPROM.commit();
      sendDataToESP8266();
    } else if (text == "/system_info") {
      float inttemperature = temperatureRead();
      String info = "CPU Frequency (MHz): " + String((float)ESP.getCpuFreqMHz()) + "\n";
      info += "Free RAM (kB): " + String(ESP.getFreeHeap() / 1024) + "\n";
      info += "Total RAM (kB): " + String(ESP.getHeapSize() / 1024) + "\n";
      info += "RAM Usage  : " + String((ESP.getHeapSize() - ESP.getFreeHeap()) / (float)ESP.getHeapSize() * 100) + " %\n";
      info += "CPU Temperature  : " + String(inttemperature) + " C\n";
      bot.sendMessage(CHAT_ID, info, "");
    } else if (text == "/sleep") {
      if (lastsleep >= 1) {
        sendMessageTelegram("deep sleep starting for 5 minutes");
        EEPROM.write(SLEEPADDR, 0);
        EEPROM.commit();
        lastsleep = 0;
        esp_sleep_enable_timer_wakeup(300 * tf);
        esp_deep_sleep_start();
      } else {
        EEPROM.write(SLEEPADDR, 1);
        EEPROM.commit();
        lastsleep = 1;
      }
    } else if (text == "/reset") {
      if (lastsleep >= 1) {
        sendMessageTelegram("resetting");
        EEPROM.write(SLEEPADDR, 0);
        EEPROM.write(7, 0);
        EEPROM.write(8, 0);
        EEPROM.write(9, 0);
        EEPROM.write(10, 0);
        EEPROM.write(11, 0);
        EEPROM.write(12, 0);
        EEPROM.commit();
        resetWiFiSettings();
        ESP.restart();
      } else {
        EEPROM.write(SLEEPADDR, 1);
        EEPROM.commit();
        lastsleep = 1;
      }
    } else if (text == "/restart") {
      if (lastsleep >= 1) {
        sendMessageTelegram("restarting");
        EEPROM.write(SLEEPADDR, 0);
        EEPROM.commit();
        ESP.restart();
      } else {
        EEPROM.write(SLEEPADDR, 1);
        EEPROM.commit();
        lastsleep = 1;
      }
    } else if (text == "/weather_notify_on") {
      sendMessageTelegram("Weather notifications on");
      weatherswitch = 1;
      EEPROM.write(10, weatherswitch);
      EEPROM.commit();
    } else if (text == "/weather_notify_off") {
      sendMessageTelegram("Weather notifications off");
      weatherswitch = 0;
      EEPROM.write(10, weatherswitch);
      EEPROM.commit();
    } else if (text == "/powersavingmode_on") {
      pm = 1;
      EEPROM.write(70, pm);
      EEPROM.commit();
      setCpuFrequencyMhz(80);
      sendMessageTelegram("powersaving mode on");
    } else if (text == "/powersavingmode_off") {
      pm = 0;
      EEPROM.write(70, pm);
      EEPROM.commit();
      sendMessageTelegram("powersaving mode off");
      setCpuFrequencyMhz(240);
    } else if (text == "/scanningdevicese_on") {
      isScanningActive = true;
      scanswitch = 1;
      EEPROM.write(60, 1);
      EEPROM.write(80, 1);
      EEPROM.commit();
      sendMessageTelegram("Scanning devices active");
    } else if (text == "/scanningdevicese_off") {
      isScanningActive = false;
      scanswitch = 0;
      EEPROM.write(60, 0);
      EEPROM.write(80, 0);
      EEPROM.commit();
      sendMessageTelegram("Scanning devices turned off");
    }
  }
}

void sendWelcomeMessage() {
  String welcome = "Welcome to ESP Weather BOT.\n\n";
  welcome += "Below are the status of the switches:\n\n";
  welcome += "Motion switch status: " + String(motionswitch) + "\n";
  welcome += "Sonar switch status: " + String(sonarswitch) + "\n";
  welcome += "Battery switch status: " + String(batteryswitch) + "\n";
  welcome += "Device scan switch status: " + String(isScanningActive) + "\n";
  welcome += "Powersaving mode status: " + String(pm) + "\n";
  welcome += "Weather switch status: " + String(weatherswitch) + "\n\n";
  welcome += "Use the following commands to control the system:\n\n";
  welcome += "/start - receive welcome message\n";
  welcome += "/reset - reset device\n";
  welcome += "/restart - restart device\n";
  welcome += "/weather - get the current weather\n";
  welcome += "/forecast - get weather forecast\n";
  welcome += "/motionsensor_on - turn ON motion sensor\n";
  welcome += "/motionsensor_off - turn OFF motion sensor\n";
  welcome += "/dht - get temperature and humidity\n";
  welcome += "/battery - get battery info\n";
  welcome += "/distance - get distance info\n";
  welcome += "/sonarsensor_on - turn ON sonar sensor\n";
  welcome += "/sonarsensor_off - turn OFF sonar sensor\n";
  welcome += "/batterynotify_on - turn ON battery status notifications\n";
  welcome += "/batterynotify_off - turn OFF battery status notifications\n";
  welcome += "/weather_notify_on - turn ON weather notifications\n";
  welcome += "/weather_notify_off - turn OFF weather notifications\n";
  welcome += "/scanningdevicese_on - turn on scanning devices\n";
  welcome += "/scanningdevicese_off - turn off scanning devices\n";
  welcome += "/system_info - get CPU speed and RAM Usage\n";
  welcome += "/sleep - deep sleep the device for 5 minutes\n";
  welcome += "/powersavingmode_off - turn off powersaving mode\n";
  welcome += "/powersavingmode_on - turn on powersaving mode\n";

  bot.sendMessage(CHAT_ID, welcome, "");
}

void connectToWiFi() {
  // 120 seconds timeout for configuration portal before fallback
  wifiManager.setTimeout(120);

  if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure Static IP");
  }

  if (!WiFi.isConnected()) {
    if (!wifiManager.autoConnect("MotionSensorAP")) {
      Serial.println("Failed to connect to WiFi after timeout. Restarting ESP...");
      delay(5000);
      ESP.restart();
    }
  }

  if (WiFi.isConnected()) {
    Serial.println("Connected to WiFi. IP: " + WiFi.localIP().toString());
  }
}

void resetWiFiSettings() {
  wifiManager.resetSettings();
  sendMessageTelegram("WiFi settings have been reset.");
}

void checkWeather() {
  if (WiFi.status() == WL_CONNECTED && weatherswitch == 1) {
    HTTPClient http;
    String currentWeatherUrl = String(weatherApiUrl) + "?key=" + String(weatherApiKey) + "&q=" + String(city) + "&aqi=no";
    http.begin(currentWeatherUrl);
    http.setTimeout(5000);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String currentWeatherPayload = http.getString();
      Serial.println("weather checked");

      DynamicJsonDocument currentDoc(1024);
      DeserializationError error = deserializeJson(currentDoc, currentWeatherPayload);

      if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        http.end();
        return;
      }

      const char* currentWeatherCondition = currentDoc["current"]["condition"]["text"];
      float currentPrecipitation = currentDoc["current"]["precip_mm"];

      if (String(currentWeatherCondition).indexOf("rain") != -1 && currentPrecipitation > 0) {
        if (!raining) {
          raining = true;
          EEPROM.write(11, 1);
          EEPROM.commit();
          Serial.println("Alert: It's raining now!");
          sendMessageWhatsApp("Alert: It's raining now!");
          logEvent("rain", "Alert: It's raining now!");
        }
      } else {
        if (raining) {
          raining = false;
          EEPROM.write(11, 0);
          EEPROM.commit();
          Serial.println("Alert: Rain has stopped!");
          sendMessageWhatsApp("Alert: The rain has stopped!");
          logEvent("rain", "Alert: The rain has stopped!");
        }
      }
    } else {
      Serial.println("Error getting current weather");
    }
    http.end();
  }
}

void sendWeatherInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String currentWeatherUrl = String(weatherApiUrl) + "?key=" + String(weatherApiKey) + "&q=" + String(city) + "&aqi=no";
    http.begin(currentWeatherUrl);
    http.setTimeout(5000);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();

      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        String location = doc["location"]["name"].as<String>() + ", " + doc["location"]["country"].as<String>();
        String condition = doc["current"]["condition"]["text"].as<String>();
        float tempC = doc["current"]["temp_c"].as<float>();
        float feelsLikeC = doc["current"]["feelslike_c"].as<float>();
        float hum = doc["current"]["humidity"].as<float>();
        float precipitation = doc["current"]["precip_mm"].as<float>();
        String windDir = doc["current"]["wind_dir"].as<String>();
        float windSpeed = doc["current"]["wind_kph"].as<float>();

        String message = "🌍 *Weather Update*\n";
        message += "📍 Location: " + location + "\n";
        message += "🌦 Condition: " + condition + "\n";
        message += "🌡 Temperature: " + String(tempC) + " °C\n";
        message += "🌀 Feels Like: " + String(feelsLikeC) + " °C\n";
        message += "💧 Humidity: " + String(hum) + " %\n";
        message += "🌧 Precipitation: " + String(precipitation) + " mm\n";
        message += "💨 Wind: " + String(windSpeed) + " kph, " + windDir + "\n";

        bot.sendMessage(CHAT_ID, message, "Markdown");
      }
    } else {
      Serial.println("Error getting current weather");
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void sendForecastInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String forecastUrl = String(forecastApiUrl) + "?key=" + String(weatherApiKey) + "&q=" + String(city) + "&days=1&aqi=no&alerts=no";

    http.begin(forecastUrl);
    http.setTimeout(5000);

    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();

      // ArduinoJson filter reduces memory needed from 30,000 bytes down to ~4KB
      StaticJsonDocument<512> filter;
      filter["location"]["name"] = true;
      filter["location"]["country"] = true;
      filter["forecast"]["forecastday"][0]["date"] = true;
      JsonObject day = filter["forecast"]["forecastday"][0]["day"];
      day["condition"]["text"] = true;
      day["maxtemp_c"] = true;
      day["mintemp_c"] = true;
      day["maxtemp_f"] = true;
      day["mintemp_f"] = true;
      day["totalprecip_mm"] = true;
      day["avghumidity"] = true;
      day["maxwind_kph"] = true;
      JsonObject hour = filter["forecast"]["forecastday"][0]["hour"][0];
      hour["time"] = true;
      hour["condition"]["text"] = true;
      hour["temp_c"] = true;
      hour["temp_f"] = true;
      hour["precip_mm"] = true;

      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

      if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        http.end();
        return;
      }

      String location = doc["location"]["name"].as<String>() + ", " + doc["location"]["country"].as<String>();
      String forecastDate = doc["forecast"]["forecastday"][0]["date"].as<String>();
      String condition = doc["forecast"]["forecastday"][0]["day"]["condition"]["text"].as<String>();
      float maxTempC = doc["forecast"]["forecastday"][0]["day"]["maxtemp_c"].as<float>();
      float minTempC = doc["forecast"]["forecastday"][0]["day"]["mintemp_c"].as<float>();
      float maxTempF = doc["forecast"]["forecastday"][0]["day"]["maxtemp_f"].as<float>();
      float minTempF = doc["forecast"]["forecastday"][0]["day"]["mintemp_f"].as<float>();
      float precipitation = doc["forecast"]["forecastday"][0]["day"]["totalprecip_mm"].as<float>();
      float hum = doc["forecast"]["forecastday"][0]["day"]["avghumidity"].as<float>();
      float windSpeed = doc["forecast"]["forecastday"][0]["day"]["maxwind_kph"].as<float>();

      String message = "🌍 *Weather Forecast*\n";
      message += "📍 Location: " + location + "\n";
      message += "📅 Date: " + forecastDate + "\n";
      message += "🌦 Condition: " + condition + "\n";
      message += "🌡 Max Temp: " + String(maxTempC, 1) + " °C (" + String(maxTempF, 1) + " °F)\n";
      message += "🌡 Min Temp: " + String(minTempC, 1) + " °C (" + String(minTempF, 1) + " °F)\n";
      message += "💧 Precipitation: " + String(precipitation, 1) + " mm\n";
      message += "💧 Humidity: " + String(hum, 2) + " %\n";
      message += "💨 Max Wind Speed: " + String(windSpeed, 1) + " kph\n";

      String hourlyForecast = "\n🕒 *Hourly Forecast*:\n";
      JsonArray hoursArr = doc["forecast"]["forecastday"][0]["hour"].as<JsonArray>();
      int hourIdx = 1;
      for (JsonObject h : hoursArr) {
        String time = h["time"].as<String>();
        String hourCondition = h["condition"]["text"].as<String>();
        float hourTempC = h["temp_c"].as<float>();
        float hourTempF = h["temp_f"].as<float>();
        float hourPrecipitation = h["precip_mm"].as<float>();

        hourlyForecast += String(hourIdx++) + ". " + time + ": " + hourCondition + "\n";
        hourlyForecast += "   🌡 Temp: " + String(hourTempC, 1) + " °C (" + String(hourTempF, 1) + " °F)\n";
        hourlyForecast += "   💧 Precipitation: " + String(hourPrecipitation, 1) + " mm\n\n";
      }

      message += hourlyForecast;

      bot.sendMessage(CHAT_ID, message, "Markdown");
      Serial.println("forecast sent");
    } else {
      Serial.print("Error getting weather forecast, code: ");
      Serial.println(httpCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

// Fast, non-blocking URL encoder without busy-wait delay
String customUrlEncode(const String &str) {
  String encodedString;
  encodedString.reserve(str.length() * 3 / 2);
  const char hexChars[] = "0123456789ABCDEF";

  for (size_t j = 0; j < str.length(); j++) {
    char c = str.charAt(j);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encodedString += c;
    } else if (c == ' ') {
      encodedString += '+';
    } else {
      encodedString += '%';
      encodedString += hexChars[(c >> 4) & 0x0F];
      encodedString += hexChars[c & 0x0F];
    }
  }
  return encodedString;
}

void sendDataToESP8266() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient peerClient;
    HTTPClient http;
    String url = "http://" + String(serverIP) + ":" + String(port) + "/data";

    http.begin(peerClient, url);
    http.setTimeout(4000);
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"sonarswitch\": " + String(sonarswitch) + ", \"batteryswitch\": " + String(batteryswitch) + "}";
    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("ESP32 received response: " + response);
    } else {
      Serial.println("ESP32: Error in HTTP request to ESP8266");
    }

    http.end();
  }
}

void handleReceiveData() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    Serial.println("ESP32 received data: " + body);

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);

    if (!error) {
      temperature = doc["temperature"];
      humidity = doc["humidity"];
      motionsense = doc["motionsense"];
      distance = doc["distance"];
      motionswitch = doc["motionswitch"];
      float volt = doc["voltage"];
      chargeLevel = doc["chargeLevel"];
      voltage = volt;
      EEPROM.put(90, voltage);
      EEPROM.commit();

      Serial.println("Parsed temperature: " + String(temperature));
      Serial.println("Parsed humidity: " + String(humidity));
      Serial.println("Parsed motionsense: " + String(motionsense));
      Serial.println("Parsed distance: " + String(distance));
      Serial.println("Parsed motionswitch: " + String(motionswitch));
      Serial.println("Parsed voltage: " + String(voltage, 2));
      Serial.println("Parsed chargeLevel: " + String(chargeLevel));
    } else {
      Serial.println("Failed to parse JSON");
    }

    server.send(200, "text/plain", "ESP32: Data received");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void sendMessageTelegram(String message) {
  bot.sendMessage(CHAT_ID, message, "");
}

void sendMessageWhatsApp(String message) {
  const int maxRetries = 3;
  int retryCount = 0;

  while (retryCount < maxRetries) {
    String url = "https://api.callmebot.com/whatsapp.php?phone=" + MobileNumber + "&apikey=" + APIKey + "&text=" + customUrlEncode(message);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int httpResponseCode = http.POST(url);

    if (httpResponseCode == 200) {
      Serial.println("WhatsApp message sent successfully");
      http.end();
      return;
    } else {
      Serial.println("Error sending WhatsApp message. HTTP response code: " + String(httpResponseCode));
      retryCount++;
      if (retryCount < maxRetries) {
        vTaskDelay(pdMS_TO_TICKS(1500));
      } else {
        Serial.println("Max retries reached. WhatsApp message sending failed.");
      }
    }
    http.end();
  }
}

void updateBlynkSwitch(String virtualPin, int value) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient blynkClient;

    String url = "http://blynk.cloud/external/api/update?token=" + String(BLYNK_AUTH_TOKEN) + "&pin=" + virtualPin + "&value=" + String(value);

    http.begin(blynkClient, url);
    http.setTimeout(4000);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Blynk update response: " + response);
    } else {
      Serial.println("Error sending Blynk update: " + String(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void logEvent(String code, String description) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient blynkClient;

    String url = String("http://blynk.cloud/external/api/logEvent?token=") + String(BLYNK_AUTH_TOKEN) + "&code=" + code + "&description=" + customUrlEncode(description);

    http.begin(blynkClient, url);
    http.setTimeout(4000);

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Blynk logEvent response: " + response);
    } else {
      Serial.println("Error sending Blynk logEvent: " + String(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}
}