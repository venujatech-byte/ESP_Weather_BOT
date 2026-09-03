//////////////// Libraries
#include <WiFiManager.h>  // WiFiManager for WiFi configuration
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <UrlEncode.h>
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



// NTP Server
const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = 19800; // Offset for UTC+5:30
// Create instances
WiFiUDP udp;
NTPClient timeClient(udp, ntpServer, utcOffsetInSeconds);


WiFiManager wifiManager;
const char* targetDevice1 = "ff:a1:a0:02:d3:c7";
const char* targetDevice2 = "c1:a1:b2:2b:1c:46";
const char* targetDevice3 = "6b:a4:87:68:03:af";

// Last state of device detection
bool device1LastState = false;
bool device2LastState = false;
bool device3LastState = false;
bool isScanningActive = false;
NimBLEScan* pBLEScan;

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

  NimBLEScanResults foundDevices = pBLEScan->start(5, false);
  int count = foundDevices.getCount();

  for (int i = 0; i < count; i++) {
    NimBLEAdvertisedDevice advertisedDevice = foundDevices.getDevice(i);
    String address = advertisedDevice.getAddress().toString().c_str();
    int rssi = advertisedDevice.getRSSI();
    Serial.println(rssi);

    // Only consider devices within RSSI_THRESHOLD (approx. 3 meters)
    if (rssi > RSSI_THRESHOLD) {
      
      // Check for Device 1
      if (address == targetDevice1) {
        if (!device1LastState) {
          device1LastState = true;
          EEPROM.write(DEVICE1_ADDR, device1LastState);
          EEPROM.commit();
          vTaskSuspend(task1TaskHandle);
          vTaskSuspend(task3TaskHandle);
          Serial.println("Venuja is at Door");
          sendMessageWhatsApp("Venuja is at Door");
          logEvent("motion_sense", "Venuja is at Door");
          vTaskResume(task1TaskHandle);
          vTaskResume(task3TaskHandle);
        }
      } else if (device1LastState) {
        // If the device was previously detected but is now below the threshold
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
          vTaskSuspend(task1TaskHandle);
          vTaskSuspend(task3TaskHandle);
          Serial.println("Sanija is at Door");
          sendMessageWhatsApp("Sanija is at Door");
          logEvent("motion_sense", "Sanija is at Door");
          vTaskResume(task1TaskHandle);
          vTaskResume(task3TaskHandle);
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
          vTaskSuspend(task1TaskHandle);
          vTaskSuspend(task3TaskHandle);
          Serial.println("Athula is at Door");
          sendMessageWhatsApp("Athula is at Door");
          logEvent("motion_sense", "Athula is at Door");
          vTaskResume(task1TaskHandle);
          vTaskResume(task3TaskHandle);
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
/////////////////////////////////////////////////////////////

////////////////  pins   ////////////////////////////
#define BOOT_BUTTON_PIN 0  //GPIO pin for BOOT button (GPIO 0)
//////////////////////////////////////////////////////


////////////////// Battery stats
//Battery18650Stats batteryStats(33, 3.3, 50);  // ADC pin, conversion factor, reads
/////////////////////////////////////////////////////////////


///////////// Telegram Bot Code
#define BOTtoken ""
#define CHAT_ID ""
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
////////////////////////////////////////////////////////


////////// weather API
const char* city = "";  // Replace with your city coordinates
const char* weatherApiUrl = "http://api.weatherapi.com/v1/current.json";
const char* forecastApiUrl = "http://api.weatherapi.com/v1/forecast.json";
const char* weatherApiKey = "";
////////////////////////////////////////////////////////////////////////////

//////////////whatsapp api
String MobileNumber = "";
String APIKey = "";
////////////////////////////////////////////////////////

const char* serverIP = "192.168.1.188";  // Replace with the ESP32 IP address
const int port = 80;                        // HTTP port
WebServer server(80);

//////////////////////////////////////////////

IPAddress staticIP(192, 168, 1, 184);  // Static IP
IPAddress gateway(192, 168, 1, 1);     // Gateway IP
IPAddress subnet(255, 255, 255, 0);    // Subnet mask
IPAddress primaryDNS(8, 8, 8, 8);      // Optional: DNS server
IPAddress secondaryDNS(8, 8, 4, 4);    // Optional: secondary DNS

///////booleans
unsigned long resetButtonPressTime = 0;
bool resetInProgress = false;
bool otaInProgress = false;  // Track if OTA is in progress
bool raining = false;
//bool rainForecasted = false;
unsigned long lastWeatherCheck = 0;
unsigned long weatherCheckInterval = 600000;  // 10 minutes (600,000 ms)
unsigned long lastTimeBotRan;
bool wasRestarted = false;
/////////////////////////////////////////////////



//////// Integers
////////////////////////////////////////////////////////////////
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
int lastsleep = 0;  // Variable to track the last message ID
int distance;
int sonarTriggerThreshold = 100;  // Distance threshold in cm for object detection by sonar sensor
long duration;
int chargeLevel = 0;
float voltage = 0;
////////////////////////////////////////////////////////////////


//////Definitions
///////////////////////////////////////////////////////
#define LED_PIN 2  // Usually onboard LED is connected to GPIO 2
// Define temperature threshold
#define TEMPERATURE_THRESHOLD 95.0
// Define deep sleep duration (5 minutes in microseconds)
#define DEEP_SLEEP_DURATION 300000000  // 5 minutes
// EEPROM address to store the last message ID
#define SLEEPADDR 6
// EEPROM size
#define EEPROM_SIZE 512
#define tf 1000000
//////////////////////////////////////////////////////////




//TaskHandle_t task4TaskHandle = NULL;
//volatile bool heartbeat1Received = false;
//volatile bool heartbeatReceived = false;

void task1(void* pvParameters) {
  UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
    Serial.println("Task1 remaining stack space: " + String(highWaterMark*4/1024));

  
  while (true) {

    if (millis() > lastTimeBotRan + botRequestDelay) {
      int newMessages = bot.getUpdates(bot.last_message_received + 1);
      while (newMessages) {
        handleNewMessages(newMessages);
        newMessages = bot.getUpdates(bot.last_message_received + 1);
      }
      lastTimeBotRan = millis();
    }

    //Serial.println("task1 running smoothly");
    delay(100);  // Simulate work
  }
}

void task2(void* pvParameters) {
  UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
    Serial.println("Task2 remaining stack space: " + String(highWaterMark*4/1024));
  while (true) {
   
      if (isScanningActive) {
        Serial.println("scanning devices");
    scanDevices();
  }
    //Serial.println("task 2 running smoothly");
  }
}

void checkWeatherTask(void* parameter) {
  while (true) {
    checkWeather();
    vTaskDelay(100000 / portTICK_PERIOD_MS);  // Delay for 20 seconds
  }
}
void task3(void* pvParameters) {
  UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
    Serial.println("Task3 remaining stack space: " + String(highWaterMark*4/1024));

   while (true) {
//Serial.println("Task3 running");
     
 server.handleClient();
    ArduinoOTA.handle();  // Handle OTA events

      // Check BOOT button state for WiFi reset (only if OTA is not in progress)
    if (!otaInProgress) {
      if (digitalRead(BOOT_BUTTON_PIN) == LOW && !resetInProgress) {
        resetInProgress = true;
        resetButtonPressTime = millis();
      }

      if (resetInProgress && digitalRead(BOOT_BUTTON_PIN) == HIGH) {
        if (millis() - resetButtonPressTime > 5000) {
          resetWiFiSettings();  // Reset WiFi credentials if button is held for more than 5 seconds
          ESP.restart();        // Restart the ESP
        }
        resetInProgress = false;
      }
    } else {
      if (otaInProgress) { blink(); }
    }}}
/////////////////////////////////////////////////////////
////////////////////////////////////////////////////////
////////////////////////////////////////////////////////

void setup() {
  Serial.begin(9600);
  Serial.println("Configuring Device...");
  // Initialize EEPROM
  printWakeUpReason();
  pinMode(LED_PIN, OUTPUT);
  blink();
  EEPROM.begin(EEPROM_SIZE);
  motionswitch = EEPROM.read(7);  // Use global variables
  sonarswitch = EEPROM.read(8);
  batteryswitch = EEPROM.read(9);
  weatherswitch = EEPROM.read(10);
  voltage = EEPROM.read(90);
  pm = EEPROM.read(70);
  raining = EEPROM.read(11);  // Read the previous state from EEPROM
  raining = (raining == 1);   // Convert byte to boolean
  //rainForecasted = EEPROM.read(11);  // Read the previous state from EEPROM
  //rainForecasted = (rainForecasted == 1);
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

  // Retrieve the last message ID from EEPROM
  lastsleep = EEPROM.read(SLEEPADDR);
  Serial.print("Last sleep status: ");
  Serial.println(lastsleep);
  connectToWiFi();

  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  pinMode(LED_PIN, OUTPUT);
  blink();
  // Start OTA
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    Serial.println("OTA Update Start: " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("OTA Update Finished");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    String errorMessage;
    switch (error) {
      case OTA_AUTH_ERROR: errorMessage = "OTA Auth Failed"; break;
      case OTA_BEGIN_ERROR: errorMessage = "OTA Begin Failed"; break;
      case OTA_CONNECT_ERROR: errorMessage = "OTA Connect Failed"; break;
      case OTA_RECEIVE_ERROR: errorMessage = "OTA Receive Failed"; break;
      case OTA_END_ERROR: errorMessage = "OTA End Failed"; break;
    }
    Serial.println("OTA Error: " + errorMessage);
  });

  sendWelcomeMessage();
  server.on("/data", HTTP_POST, handleReceiveData);
  server.begin();
  Serial.println("ESP32 HTTP server started");
  sendDataToESP8266();

  NimBLEDevice::init("ESP32_BLE_Scanner");
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setActiveScan(true);  // Active scan to get device name
  pBLEScan->setInterval(100);     // Interval for BLE scanning
  pBLEScan->setWindow(99);        // Window for BLE scanning
                                  // Create the Core 1 monitoring task
  Serial.println("setup succeed and starting loop");

  xTaskCreatePinnedToCore(task2, "task2", 5000, NULL, 1, &task2TaskHandle, 1);

  xTaskCreatePinnedToCore(task1, "task1", 5000, NULL, 1, &task1TaskHandle, 0);

  xTaskCreatePinnedToCore(task3, "task3", 4000, NULL, 1, &task3TaskHandle, 0);


  xTaskCreatePinnedToCore(
    checkWeatherTask,  // Task function
    "CheckWeather",    // Task name
    5000,              // Stack size
    NULL,              // Task input parameter
    1,                 // Priority
    NULL,              // Task handle
    0                  // Pin to core 1
  );
  blink();
   timeClient.begin();
  checkWeather();
}


void printWakeUpReason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.println("Normal startup and not caused by deep sleep");
      break;
  }
}


void loop() {

  blink();
  float inttemperature = temperatureRead();
  //Serial.println(inttemperature);
  if (inttemperature > TEMPERATURE_THRESHOLD && !otaInProgress) {
    Serial.println("Temperature exceeds threshold! Going to deep sleep...");
    sendMessageTelegram("Device over heated and starting deep sleep");
    esp_sleep_enable_timer_wakeup(300 * tf);
    esp_deep_sleep_start();
  }

    if (inttemperature > 90.0 && !otaInProgress && tm == 0 && pm == 0) {
    Serial.println("Device over heated and powersaving mood on");
    sendMessageTelegram("Device over heated and powersaving mood on");
    setCpuFrequencyMhz(80);
    tm = 1;
  }else
    {
      if(inttemperature <= 90.0 && tm == 1 && pm == 0){
    setCpuFrequencyMhz(240);
    tm = 0;
    Serial.println("Device cooled down and turning off power saving mood");
    sendMessageTelegram("Device cooled down and turning off power saving mood");}}

  timeClient.update(); // Update time

  // Get current time
  String formattedTime = timeClient.getFormattedTime();
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();

  // Print the current time

  // Check if the current time is within the event time range
  if ((hours >= 22 || hours < 6) || (hours == 6 && minutes == 0)) {
    // Event action here
    isScanningActive = false;
  } else if(scanswitch == 1){
    isScanningActive = true;
  }
}


void handleNewMessages(int newMessages) {

  for (int i = 0; i < newMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);

    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      sendWelcomeMessage();
    }

    if (text == "/weather" && pm == 0) {
      setCpuFrequencyMhz(240);
      sendWeatherInfo();
    } else {
      if (text == "/weather" && pm == 1) {
        setCpuFrequencyMhz(240);
        delay(500);
        sendWeatherInfo();
        delay(1000);
        setCpuFrequencyMhz(80);
      }
    }

    if (text == "/forecast" && pm == 0) {
      sendForecastInfo();
    } else {
      if (text == "/forecast" && pm == 1) {
        setCpuFrequencyMhz(240);
        delay(500);
        sendForecastInfo();
        delay(1000);
        setCpuFrequencyMhz(80);
      }
    }

    if (text == "/motionsensor_on") {
      sendMessageTelegram("motion sensor turned on");
      motionswitch = 1;
      EEPROM.write(7, motionswitch);
      EEPROM.commit();
      updateBlynkSwitch("V1",1);
    }

    if (text == "/motionsensor_off") {
      sendMessageTelegram("motion sensor turned off");
      motionswitch = 0;
      EEPROM.write(7, motionswitch);
      EEPROM.commit();
      updateBlynkSwitch("V1",0);
    }

    if (text == "/dht") {
      sendMessageTelegram("Temperature: " + String(temperature) + " °C" + " , Humidity: " + String(humidity) + " %");
    }

    if (text == "/battery") {
      //double voltage = batteryStats.getBatteryVolts();
      //int chargeLevel = batteryStats.getBatteryChargeLevel(false);  // true for using conversion table
      float storedVoltage;
     EEPROM.get(90, storedVoltage);

      sendMessageTelegram("Battery Level: " + String(chargeLevel) + " , Battery Voltage: " + String(storedVoltage));

      //sendMessageTelegram("Battery Voltage: " + String(voltage));
    }

    if (text == "/distance") {
      sendMessageTelegram(String(distance));
    }

    if (text == "/sonarsensor_on") {
      sendMessageTelegram("sonar sensor turned on");
      sonarswitch = 1;
      EEPROM.write(8, sonarswitch);
      EEPROM.commit();
      sendDataToESP8266();
    }

    if (text == "/sonarsensor_off") {
      sendMessageTelegram("sonar sensor turned off");
      sonarswitch = 0;
      EEPROM.write(8, sonarswitch);
      EEPROM.commit();
      sendDataToESP8266();
    }

    if (text == "/batterynotify_on") {
      sendMessageTelegram("battery notifications on");
      batteryswitch = 1;
      EEPROM.write(9, batteryswitch);
      EEPROM.commit();
      sendDataToESP8266();
    }

    if (text == "/batterynotify_off") {
      sendMessageTelegram("battery notificationa off");
      batteryswitch = 0;
      EEPROM.write(9, batteryswitch);
      EEPROM.commit();
      sendDataToESP8266();
    }

    if (text == "/system_info") {

      float inttemperature = temperatureRead();

      String welcome = "CPU Frequency (MHz): " + String((float)ESP.getCpuFreqMHz()) + "\n";
      welcome += "Free RAM (kB): " + String(ESP.getFreeHeap() / 1024) + "\n";
      welcome += "Total RAM (kB): " + String(ESP.getHeapSize() / 1024) + "\n";
      welcome += "RAM Usage  : " + String((ESP.getHeapSize() - ESP.getFreeHeap()) / (float)ESP.getHeapSize() * 100) + " %" + "\n";
      welcome += "CPU Temperature  : " + String(inttemperature) + " C" + "\n";

      // Send the welcome message to Telegram
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    if (text == "/sleep" && lastsleep >= 1) {
      sendMessageTelegram("deep sleep starting for 5 minutes");
      EEPROM.write(SLEEPADDR, 0);  // Store the message ID
      EEPROM.commit();
      lastsleep = EEPROM.read(SLEEPADDR);
      esp_sleep_enable_timer_wakeup(300 * tf);
      esp_deep_sleep_start();
    }

    else {
      if (text == "/sleep" && lastsleep == 0) {
        EEPROM.write(SLEEPADDR, 1);  // Store the message ID
        EEPROM.commit();
        lastsleep = EEPROM.read(SLEEPADDR);
        //Serial.println("lastsleep status :");
        //Serial.println(lastsleep);
        continue;
      }
    }


    if (text == "/reset" && lastsleep >= 1) {
      sendMessageTelegram("resetting");
      EEPROM.write(SLEEPADDR, 0);  // Store the message ID
      EEPROM.commit();
      lastsleep = EEPROM.read(SLEEPADDR);
      //Serial.println("lastsleep status :");
      //Serial.println(lastsleep);
      EEPROM.write(7, 0);
      EEPROM.write(8, 0);
      EEPROM.write(9, 0);
      EEPROM.write(10, 0);
      EEPROM.write(11, 0);
      EEPROM.write(12, 0);
      EEPROM.commit();
      resetWiFiSettings();  // Reset WiFi credentials if button is held for more than 5 seconds
      ESP.restart();
    }

    else {
      if (text == "/reset" && lastsleep == 0) {
        EEPROM.write(SLEEPADDR, 1);  // Store the message ID
        EEPROM.commit();
        lastsleep = EEPROM.read(SLEEPADDR);
        //Serial.println("lastsleep status :");
        //Serial.println(lastsleep);
        continue;
      }
    }



    if (text == "/restart" && lastsleep >= 1) {
      sendMessageTelegram("restarting");
      EEPROM.write(SLEEPADDR, 0);  // Store the message ID
      EEPROM.commit();
      lastsleep = EEPROM.read(SLEEPADDR);
      //Serial.println("lastsleep status :");
      //Serial.println(lastsleep);
      ESP.restart();
    }

    else {
      if (text == "/restart" && lastsleep == 0) {
        EEPROM.write(SLEEPADDR, 1);  // Store the message ID
        EEPROM.commit();
        lastsleep = EEPROM.read(SLEEPADDR);
        //Serial.println("lastsleep status :");
        //Serial.println(lastsleep);
        continue;
      }
    }
    if (text == "/weather_notify_on") {
      sendMessageTelegram("Weather notifications on");
      weatherswitch = 1;
      EEPROM.write(10, weatherswitch);
      EEPROM.commit();
    }

    if (text == "/weather_notify_off") {
      sendMessageTelegram("Weather notifications off");
      weatherswitch = 0;
      EEPROM.write(10, weatherswitch);
      EEPROM.commit();
    }
    if (text == "/powersavingmode_on") {
      pm = 1;
      EEPROM.write(70, pm);
      EEPROM.commit();
      setCpuFrequencyMhz(80);
      sendMessageTelegram("powersavingmode on");
    }

    if (text == "/powersavingmode_off") {
      pm = 0;
      EEPROM.write(70, pm);
      EEPROM.commit();
      sendMessageTelegram("powersaving mode off");
      setCpuFrequencyMhz(240);
    }

    
    if (text == "/scanningdevicese_on") {
      isScanningActive = true;
      scanswitch = 1;
      vTaskSuspend(task2TaskHandle);
      delay(1000);
        EEPROM.write(60, isScanningActive);
        EEPROM.write(80, scanswitch);
        EEPROM.commit();  // Ensure the data is written
      vTaskResume(task2TaskHandle);
      sendMessageTelegram("Scanning devices active");
    }

    if (text == "/scanningdevicese_off") {
      isScanningActive = false;
      scanswitch = 0;
      vTaskSuspend(task2TaskHandle);
        EEPROM.write(60, isScanningActive);
        EEPROM.write(80, scanswitch);
        EEPROM.commit();  // Ensure the data is written
      sendMessageTelegram("Scanning devices turned off");
    }
    
    
  }
  //Serial.println("Reading telegram messages successfully");
}


void sendWelcomeMessage() {
  String from_name = "User";  // This can be a placeholder as you may not have the user's name in this context
  String welcome = "Welcome, " + from_name + ".\n\n";
  welcome += "Below are the status of the switches:\n\n";
  welcome += "Motion switch status: " + String(motionswitch) + "\n";
  welcome += "Sonar switch status: " + String(sonarswitch) + "\n";
  welcome += "Battery switch status: " + String(batteryswitch) + "\n";
  welcome += "devicescan switch status: " + String(isScanningActive) + "\n";
  welcome += "powersaving mood status: " + String(pm) + "\n";
  welcome += "weather switch status: " + String(weatherswitch) + "\n\n";
  welcome += "Use the following commands to control the system:\n\n";
  welcome += "/start to recieve welcome message\n";
  welcome += "/reset to reset device\n";
  welcome += "/restart to restart device\n";
  welcome += "/weather to get the current weather\n";
  welcome += "/forecast to get weather forecast\n";
  welcome += "/motionsensor_on to turn ON motion sensor\n";
  welcome += "/motionsensor_off to turn OFF motion sensor\n";
  welcome += "/dht to get temperature and humidity\n";
  welcome += "/battery to get battery info\n";
  welcome += "/distance to get distance info\n";
  welcome += "/sonarsensor_on to turn ON sonar sensor\n";
  welcome += "/sonarsensor_off to turn OFF sonar sensor\n";
  welcome += "/batterynotify_on to turn ON battery status notifications\n";
  welcome += "/batterynotify_off to turn OFF battery status notifications\n";
  welcome += "/weather_notify_on to turn ON weather notifications\n";
  welcome += "/weather_notify_off to turn OFF weather notifications\n";
  welcome += "/scanningdevicese_on to turn on scanning devices\n";
  welcome += "/scanningdevicese_off to turn off scanning devices\n";
  welcome += "/system_info get CPU speed and RAM Usage\n";
  welcome += "/sleep to deep sleep the divide for 5 minutes\n";
  welcome += "/powersavingmode_off to turn off powersaving mode\n";
  welcome += "/powersavingmode_on to turn on powersaving mode\n";
  // Send the welcome message to Telegram
  bot.sendMessage(CHAT_ID, welcome, "");
}


void connectToWiFi() {
  WiFiManager wifiManager;
  wifiManager.setTimeout(1200000000000000000000);  // Wait for 2 minutes before falling back to AP mode

  if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  if (!WiFi.isConnected()) {
    if (!wifiManager.autoConnect("MotionSensorAP")) {
      Serial.println("Failed to connect to WiFi after timeout. Restarting ESP...");
      delay(5000);    // Wait a little before restarting
      ESP.restart();  // Restart the ESP if it cannot connect
    }
  }

  // If connected, print the IP address
  if (WiFi.isConnected()) {
    Serial.println("Connected to WiFi. IP: " + WiFi.localIP().toString());
  }

}

void resetWiFiSettings() {
  wifiManager.resetSettings();  // Erase WiFi credentials
  sendMessageTelegram("WiFi settings have been reset.");
}




//////////weather loops


void checkWeather() {
  /* if (!heartbeatReceived) {
    vTaskDelete(core0TaskHandle);
    xTaskCreatePinnedToCore(core0Task, "Core0Task", 10000, NULL, 1, &core0TaskHandle, 0);
  }
  if (!heartbeat1Received) {
    vTaskDelete(core1TaskHandle);
    xTaskCreatePinnedToCore(core1Task, "Core1Task", 10000, NULL, 1, &core1TaskHandle, 1);
  } */
  if (pm == 1) { setCpuFrequencyMhz(240); }
  if (WiFi.status() == WL_CONNECTED && weatherswitch == 1) {
    HTTPClient http;

    // Get current weather data
    String currentWeatherUrl = String(weatherApiUrl) + "?key=" + weatherApiKey + "&q=" + city + "&aqi=no";
    http.begin(currentWeatherUrl);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String currentWeatherPayload = http.getString();
      Serial.println("weather checked");  // Print current weather response

      // Parse the weather data
      DynamicJsonDocument currentDoc(1024);
      deserializeJson(currentDoc, currentWeatherPayload);
      DeserializationError error = deserializeJson(currentDoc, currentWeatherPayload);

      if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        http.end();
        return;  // Exit if parsing fails
      }

      const char* currentWeatherCondition = currentDoc["current"]["condition"]["text"];
      float currentPrecipitation = currentDoc["current"]["precip_mm"];

      // Check if it is raining
      if (String(currentWeatherCondition).indexOf("rain") != -1 && currentPrecipitation > 0) {
        if (!raining) {
          raining = true;
          EEPROM.write(11, raining ? 1 : 0);  // Write '1' for true, '0' for false
          EEPROM.commit();
          vTaskSuspend(task2TaskHandle);
          vTaskSuspend(task3TaskHandle);
          Serial.println(raining ? "true" : "false");
          Serial.println("Alert: It's raining now!");
          sendMessageWhatsApp("Alert: It's raining now!");
          delay(1000);
          logEvent("rain", "Alert: It's raining now!");
          delay(1000);
          vTaskResume(task2TaskHandle);
          vTaskResume(task3TaskHandle);
        }
      } else {
        if (raining) {
          raining = false;
          EEPROM.write(11, raining ? 1 : 0);  // Write '1' for true, '0' for false
          EEPROM.commit();
           vTaskSuspend(task2TaskHandle);
          vTaskSuspend(task3TaskHandle);
          Serial.println(raining ? "true" : "false");
          Serial.println("Alert: Rain has stopped!");
          sendMessageWhatsApp("Alert: The rain has stopped!");
          delay(1000);
          logEvent("rain", "Alert: The rain has stopped!E");
          delay(1000);
          vTaskResume(task2TaskHandle);
          vTaskResume(task3TaskHandle);
        }
      }



    } else {
      Serial.println("Error getting current weather");
    }
    http.end();

    /*
    // Step 2: Get weather forecast data
    String forecastWeatherUrl = String(forecastApiUrl) + "?key=" + weatherApiKey + "&q=" + city + "&days=1&aqi=no";
    http.begin(forecastWeatherUrl);
    httpCode = http.GET();

    if (httpCode > 0) {
    String forecastWeatherPayload = http.getString();
    Parse the forecast weather data
    DynamicJsonDocument forecastDoc(8192);
    deserializeJson(forecastDoc, forecastWeatherPayload);
      DeserializationError error = deserializeJson(forecastDoc, forecastWeatherPayload);

      if (error) {
        Serial.print("JSON deserialization failed for forecast: ");
        Serial.println(error.c_str());
        http.end();
        return;
      }

      // Check for rain in the forecast (assume checking next 24 hours)
      bool futureRain = false;
      String rainTime = "";           // To store the time of future rain
      for (int i = 0; i < 24; i++) {  // Loop through 24 hours forecast
        const char* futureCondition = forecastDoc["forecast"]["forecastday"][0]["hour"][i]["condition"]["text"];
        float futurePrecipitation = forecastDoc["forecast"]["forecastday"][0]["hour"][i]["precip_mm"];
        const char* timeOfRain = forecastDoc["forecast"]["forecastday"][0]["hour"][i]["time"].as<const char*>();  // Get time

        if (String(futureCondition).indexOf("rain") != -1 && futurePrecipitation > 0) {
          futureRain = true;
          rainTime = String(timeOfRain);  // Save the time of rain
          break;
        }
      }

      // Handle future rain alert
      if (futureRain && !rainForecasted) {
        rainForecasted = true;
        EEPROM.write(12, rainForecasted ? 1 : 0);
        EEPROM.commit();
        String alertMessage = "🌧 Alert: Rain is forecasted at " + rainTime + "!";
        Serial.println(alertMessage);
        sendMessageWhatsApp(alertMessage);
        delay(1000);
        Blynk.logEvent("rainforecast");
      } else if (!futureRain && rainForecasted) {
        rainForecasted = false;
        EEPROM.write(12, rainForecasted ? 1 : 0);
        EEPROM.commit();
        Serial.println("Alert: No rain forecasted.");
      }
    } else {
      Serial.println("Error getting forecast weather");
    }
    http.end();
    */
  }
  if (pm == 1) { setCpuFrequencyMhz(80); }
}

void sendWeatherInfo() {
  //esp_task_wdt_reset();
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String currentWeatherUrl = String(weatherApiUrl) + "?key=" + weatherApiKey + "&q=" + city + "&aqi=no";
    http.begin(currentWeatherUrl);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      //Serial.println(payload);  // For debugging

      // Parse JSON response
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, payload);

      // Extract data
      String location = doc["location"]["name"].as<String>() + ", " + doc["location"]["country"].as<String>();
      String condition = doc["current"]["condition"]["text"].as<String>();
      float tempC = doc["current"]["temp_c"].as<float>();
      float feelsLikeC = doc["current"]["feelslike_c"].as<float>();
      float humidity = doc["current"]["humidity"].as<float>();
      float precipitation = doc["current"]["precip_mm"].as<float>();
      String windDir = doc["current"]["wind_dir"].as<String>();
      float windSpeed = doc["current"]["wind_kph"].as<float>();

      // Format message
      String message = "🌍 *Weather Update*\n";
      message += "📍 Location: " + location + "\n";
      message += "🌦 Condition: " + condition + "\n";
      message += "🌡 Temperature: " + String(tempC) + " °C\n";
      message += "🌀 Feels Like: " + String(feelsLikeC) + " °C\n";
      message += "💧 Humidity: " + String(humidity) + " %\n";
      message += "🌧 Precipitation: " + String(precipitation) + " mm\n";
      message += "💨 Wind: " + String(windSpeed) + " kph, " + windDir + "\n";

      // Send message
      bot.sendMessage(CHAT_ID, message, "Markdown");
    } else {
      Serial.println("Error getting current weather");
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void sendForecastInfo() {
  if (pm == 1) { setCpuFrequencyMhz(240); }
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String forecastUrl = String(forecastApiUrl) + "?key=" + weatherApiKey + "&q=" + city + "&days=1&aqi=no&alerts=no";
    
    http.begin(forecastUrl);
    http.setTimeout(5000);  // Optional: Set a timeout to avoid long waits
    
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();

      // Parse JSON response
      DynamicJsonDocument doc(30000);  // Adjust based on response size
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        http.end();  // Ensure connection is closed on failure
        return;
      }

      // Extract daily forecast data
      String location = doc["location"]["name"].as<String>() + ", " + doc["location"]["country"].as<String>();
      String forecastDate = doc["forecast"]["forecastday"][0]["date"].as<String>();
      String condition = doc["forecast"]["forecastday"][0]["day"]["condition"]["text"].as<String>();
      float maxTempC = doc["forecast"]["forecastday"][0]["day"]["maxtemp_c"].as<float>();
      float minTempC = doc["forecast"]["forecastday"][0]["day"]["mintemp_c"].as<float>();
      float maxTempF = doc["forecast"]["forecastday"][0]["day"]["maxtemp_f"].as<float>();
      float minTempF = doc["forecast"]["forecastday"][0]["day"]["mintemp_f"].as<float>();
      float precipitation = doc["forecast"]["forecastday"][0]["day"]["totalprecip_mm"].as<float>();
      float humidity = doc["forecast"]["forecastday"][0]["day"]["avghumidity"].as<float>();
      float windSpeed = doc["forecast"]["forecastday"][0]["day"]["maxwind_kph"].as<float>();

      // Format message for the daily forecast
      String message = "🌍 *Weather Forecast*\n";
      message += "📍 Location: " + location + "\n";
      message += "📅 Date: " + forecastDate + "\n";
      message += "🌦 Condition: " + condition + "\n";
      message += "🌡 Max Temp: " + String(maxTempC, 1) + " °C (" + String(maxTempF, 1) + " °F)\n";
      message += "🌡 Min Temp: " + String(minTempC, 1) + " °C (" + String(minTempF, 1) + " °F)\n";
      message += "💧 Precipitation: " + String(precipitation, 1) + " mm\n";
      message += "💧 Humidity: " + String(humidity, 2) + " %\n";
      message += "💨 Max Wind Speed: " + String(windSpeed, 1) + " kph\n";

      // Extract and format hourly forecast data
      String hourlyForecast = "\n🕒 *Hourly Forecast*:\n";
      for (int z = 0; z < doc["forecast"]["forecastday"][0]["hour"].size(); z++) {
        String time = doc["forecast"]["forecastday"][0]["hour"][z]["time"].as<String>();
        String hourCondition = doc["forecast"]["forecastday"][0]["hour"][z]["condition"]["text"].as<String>();
        float hourTempC = doc["forecast"]["forecastday"][0]["hour"][z]["temp_c"].as<float>();
        float hourTempF = doc["forecast"]["forecastday"][0]["hour"][z]["temp_f"].as<float>();
        float hourPrecipitation = doc["forecast"]["forecastday"][0]["hour"][z]["precip_mm"].as<float>();

        hourlyForecast += String(z + 1) + ". " + time + ": " + hourCondition + "\n";
        hourlyForecast += "   🌡 Temp: " + String(hourTempC, 1) + " °C (" + String(hourTempF, 1) + " °F)\n";
        hourlyForecast += "   💧 Precipitation: " + String(hourPrecipitation, 1) + " mm\n";
        hourlyForecast += "\n";
      }

      message += hourlyForecast;

      // Send message
      bot.sendMessage(CHAT_ID, message, "Markdown");
      Serial.println("forecast sent");
    } else {
      Serial.print("Error getting weather forecast, code: ");
      Serial.println(httpCode);  // Provide exact error code
    }
    http.end();  // Close connection
  } else {
    Serial.println("WiFi not connected");
  }
  if (pm == 1) { setCpuFrequencyMhz(80); }
}


////////////////////////////////////////////





String customUrlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int j = 0; j < str.length(); j++) {
    c = str.charAt(j);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
    delay(10);
  }
  return encodedString;
}


void sendDataToESP8266() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;  // Create a WiFi client
    HTTPClient http;
    String url = "http://" + String(serverIP) + ":" + String(port) + "/data";

    // Use the newer HTTPClient::begin() method with WiFiClient
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    // Prepare the JSON data
    String jsonData = "{\"sonarswitch\": " + String(sonarswitch) + ", \"batteryswitch\": " + String(batteryswitch) + "}";

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("ESP32 received response: " + response);
    } else {
      Serial.println("ESP32: Error in HTTP request");
    }

    http.end();
  }
}

void handleReceiveData() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    Serial.println("ESP32 received data: " + body);

    // Parse the received JSON
    DynamicJsonDocument doc(200);
    DeserializationError error = deserializeJson(doc, body);

    if (!error) {
      temperature = doc["temperature"];
      humidity = doc["humidity"];
      motionsense = doc["motionsense"];
      distance = doc["distance"];
      motionswitch = doc["motionswitch"];
      float voltage = doc["voltage"];
      chargeLevel = doc["chargeLevel"];
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
  //Serial.println("telegram message sent successfully");
}

void sendMessageWhatsApp(String message) {
  const int maxRetries = 10;  // Set maximum number of retries
  int retryCount = 0;        // Initialize retry counter
  int httpResponseCode = 0;  // HTTP response code

  while (retryCount < maxRetries) {
    String url = "https://api.callmebot.com/whatsapp.php?phone=" + MobileNumber + "&apikey=" + APIKey + "&text=" + customUrlEncode(message);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Send the POST request and capture response
    httpResponseCode = http.POST(url);

    if (httpResponseCode == 200) {
      Serial.println("WhatsApp message sent successfully");
      http.end();
      return;  // Exit the function if the message is sent successfully
    } else {
      Serial.println("Error sending WhatsApp message. HTTP response code: " + String(httpResponseCode));
      retryCount++;  // Increment retry counter
      Serial.println("Retry attempt: " + String(retryCount));
      
      if (retryCount < maxRetries) {
        delay(3000);  // Wait for 2 seconds before retrying
      } else {
        Serial.println("Max retries reached. Message sending failed.");
      }
    }

    http.end();  // Close the HTTP connection
  }
}


void blink() {
  // Turn the LED on (HIGH is the voltage level)
  digitalWrite(LED_PIN, HIGH);
  delay(200);  // Wait for a second

  // Turn the LED off (LOW is the voltage level)
  digitalWrite(LED_PIN, LOW);
  delay(600);  // Wait for a second
}

void updateBlynkSwitch(String virtualPin, int value) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;
   //vTaskSuspend(task2TaskHandle);
   //vTaskSuspend(task3TaskHandle);

    // Construct the URL to update the Blynk virtual pin
    String url = "http://blynk.cloud/external/api/update?token=03zI8D5cFgAi06em8E8mm1x_Ps6nobsS&pin=" + virtualPin + "&value=" + String(value);

    Serial.println("URL: " + url);

    http.begin(client, url);  // Start the connection
    http.addHeader("Content-Type", "application/json");  // Add JSON header

    // Send the request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response: " + response);
    } else {
      Serial.println("Error on sending request: " + String(httpResponseCode));
    }

    http.end();  // Close the connection
  } else {
    Serial.println("WiFi not connected");
  }
    // vTaskResume(task2TaskHandle);
   //vTaskResume(task3TaskHandle);
}

void logEvent(String code, String description) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;

    // Construct the URL for sending the log event
   String url = String("http://blynk.cloud/external/api/logEvent?token=03zI8D5cFgAi06em8E8mm1x_Ps6nobsS") + "&code=" + code + "&description=" + customUrlEncode(description);

    
    http.begin(client, url);  // Start connection to the server
    
    int httpResponseCode = http.GET();  // Send the GET request

    // Check the response
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response: " + response);
    } else {
      Serial.println("Error sending request: " + String(httpResponseCode));
    }

    http.end();  // Close connection
  } else {
    Serial.println("WiFi not connected");
  }
}