#include <Arduino.h>
#define BLYNK_PRINT Serial
#include <HX711_ADC.h>
#include <EEPROM.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <Servo.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include "LittleFS.h"

#define HX711_dout D4
#define HX711_sck D3
#define trigPin D5
#define echoPin D6
#define servoPin D0
#define BLYNK_TEMPLATE_ID "TMPL0DjRC4La"
#define BLYNK_DEVICE_NAME "LED Builtin"
#define BLYNK_AUTH_TOKEN "h2rzIvgZGxv1-5z8DOCIz1AAPHxFWuYK"

AsyncWebServer server(80);

const int calVal_eepromAdress = 0;
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "ip";
const char *PARAM_INPUT_4 = "gateway";

char auth[] = BLYNK_AUTH_TOKEN;

int resetWiFiCounter;
int weight, currentWeight;
int currentHour, currentMinute, currentSecond;
int lastFedHour, lastFedMinute, lastFedSecond;
int minutePlus1;
int schedule1 = 7;
int schedule2 = 12;
int schedule3 = 19;
int previousMinute = 0;
int emptyStorageCounter;
int filledBowlCounter;

long duration;
float distance;
float foodPercentage;
float maxFood = 12.50;

void readWeight();
void hxSettingUp();
void initFS();
void writeFile();
void eraseFile();
void checkWiFi();
void tareFromBlynk();
void fileFunc();
void readCapacity();
void checkCondition();
void smartMode();
void smartModeFlag();
void scheduledMode();
void scheduledModeFlag();
void manualMode();
void time();
void moveServo();
void writeLCD();

String ssid;
String pass;
String ip;
String gateway;
String currentMode;
String timeStamp;
String lastFedTimeStamp;

const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *ipPath = "/ip.txt";
const char *gatewayPath = "/gateway.txt";

IPAddress localIP;
IPAddress localGateway;
IPAddress subnet(255, 255, 255, 0);
IPAddress dnsIP(8, 8, 8, 8); // need to assign a DNS for Blynk connection!

// Timer
unsigned long intervalCheckWiFi = 0;
unsigned long intervalTare = 0;
unsigned long intervalReadCapacity = 0;
unsigned long intervalServo = 0;
unsigned long t = 0;
unsigned long timeKeep = 0;
unsigned long intervalFoodCounter = 0;
const long interval = 10000; // interval to wait for Wi-Fi connection (milliseconds)

String ledState;

boolean restart = false;
boolean tareReq = false;
bool tareFlag = true;
bool tareState = false;
bool smartFlag = false;
bool scheduleFlag = false;
bool scheduleState = true;
bool manualFlag = false;
bool servoFlag = true;
bool servoState;
bool schedule1Flag;
bool schedule2Flag;
bool schedule3Flag;
bool foodIsEmptyFlag;
bool foodIsEmptyState = true;

BlynkTimer timer;
Servo myServo;
RTC_DS3231 rtc;
HX711_ADC LoadCell(HX711_dout, HX711_sck);
WidgetLCD lcd(V6);
WidgetTerminal terminal(V7);

void initFS()
{
  if (!LittleFS.begin())
  {
    Serial.println("An error has occured while mounting LittleFS!");
  }
  else
  {
    Serial.println("LittleFS mounted successfully");
  }
}

String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path, "r");
  if (!file || file.isDirectory())
  {
    Serial.println("-  failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  file.close();
  return fileContent;
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, "w");
  if (!file)
  {
    Serial.println("-  failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("-  file written");
  }
  else
  {
    Serial.println("-  file write failed");
  }
  file.close();
}

void eraseFile()
{
  LittleFS.remove(ssidPath);
  LittleFS.remove(passPath);
  LittleFS.remove(ipPath);
  LittleFS.remove(gatewayPath);
}

bool initWiFi()
{
  if (ssid == "" || ip == "")
  {
    Serial.println("Undefined SSID or IP Address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  if (!WiFi.config(localIP, localGateway, subnet, dnsIP))
  {
    Serial.println("STA Failed to configure");
    return false;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());

  Serial.println("Connecting to WiFi...");
  delay(20000);
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect!!");
    return false;
  }

  Serial.println(WiFi.localIP());
  WiFi.setAutoConnect(true);
  WiFi.persistent(true);
  return true;
}

void hxSettingUp()
{
  LoadCell.begin();
  // LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  float calibrationValue; // calibration value (see example file "Calibration.ino")
// calibrationValue = 696.0; // uncomment this if you want to set the calibration value in the sketch
#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(512); // uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
#endif
  EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the calibration value from eeprom

  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true;                 // set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag())
  {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1)
      ;
  }
  else
  {
    LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
    Serial.println("Startup is complete");
  }
}

void readWeight()
{
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; // increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update())
    newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady)
  {
    if (millis() > t + serialPrintInterval)
    {
      weight = LoadCell.getData();
      // Serial.print("Load_cell output val: ");
      // Serial.print(weight);
      Blynk.virtualWrite(V1, weight);
      // Serial.println(" g");
      newDataReady = false;
      // if (i == 230) {
      //   Serial.println("FUCK YOU");
      //   delay(5000);
      // }
      t = millis();
    }
  }

  // receive command from serial terminal, send 't' to initiate tare operation:
  if (Serial.available() > 0)
  {
    char inByte = Serial.read();
    if (inByte == 't')
      LoadCell.tareNoDelay();
  }

  // check if last tare operation is complete:
  if (LoadCell.getTareStatus() == true)
  {
    Serial.println("Tare complete");
  }
}

void readCapacity()
{
  if (millis() - intervalReadCapacity >= 5000UL)
  {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(5);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH);
    distance = duration * 0.034 / 2;

    foodPercentage = (100 - ((100 / maxFood) * distance));
    if (foodPercentage < 0.00)
    {
      foodPercentage = 0.00;
    }

    // Serial.print("Distance      : ");
    // Serial.print(distance);
    // Serial.println(" CM");
    // Serial.print("Food Capacity : ");
    // Serial.print(foodPercentage);
    // Serial.println(" %");
    Blynk.virtualWrite(V2, foodPercentage);

    intervalReadCapacity = millis();
  }
}

// Show info on webServer
String processor(const String &var)
{
  // if (var == "STATE")
  // {
  //   if (!digitalRead(LED_BUILTIN))
  //   {
  //     ledState = "ON";
  //   }
  //   else
  //   {
  //     ledState = "OFF";
  //   }
  //   return ledState;
  // }
  if (var == "WEIGHT")
  {
    return String(weight);
  }
  else if (var == "PERCENTAGE")
  {
    return String(foodPercentage);
  }
  else if (var == "LAST")
  {
    return String(lastFedTimeStamp);
  }
  else if (var == "MODE")
  {
    return String(currentMode);
  }
  return String();
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  initFS();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  myServo.attach(servoPin);
  myServo.write(0);

  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }
  if (rtc.lostPower())
  {
    Serial.println("RTC Lost Power, let's set the time !");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  rtc.disable32K();

  ssid = readFile(LittleFS, ssidPath);
  pass = readFile(LittleFS, passPath);
  ip = readFile(LittleFS, ipPath);
  gateway = readFile(LittleFS, gatewayPath);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  wifi_set_sleep_type(NONE_SLEEP_T);

  if (initWiFi())
  {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html", false, processor); });

    server.serveStatic("/", LittleFS, "/");

    server.on("/smartMode", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                smartModeFlag();
      request->send(LittleFS, "/index.html", "text/html", false, processor); });
    server.on("/scheduledMode", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                scheduledModeFlag();
      request->send(LittleFS, "/index.html", "text/html", false, processor); });
    server.on("/weight", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", String(weight).c_str()); });
    server.on("/percentage", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", String(foodPercentage).c_str()); });
    server.on("/last", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", String(lastFedTimeStamp).c_str()); });
    server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", String(currentMode).c_str()); });
    server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
              { Serial.println("ESP is restarting!!!");
                eraseFile();
                restart = true; });
    server.begin();
  }
  else
  {
    Serial.println("Setting AP (Access Point)");
    WiFi.softAP("ESP-WiFi-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.println("Connect to : ESP-WiFi-MANAGER");
    Serial.print("Access to  : ");
    Serial.println(IP);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/wifimanager.html", "text/html"); });

    server.serveStatic("/", LittleFS, "/");

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      int params = request-> params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        // HTTP POST ssid Value to file
        if(p->isPost()){
          if(p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to : ");
            Serial.println(ssid);
            writeFile(LittleFS, ssidPath, ssid.c_str());
          }
          if(p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to : ");
            Serial.println(pass);
            writeFile(LittleFS, passPath, pass.c_str());
          }
          if(p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP set to : ");
            Serial.println(ip);
            writeFile(LittleFS, ipPath, ip.c_str());
          }
          if(p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to : ");
            Serial.println(gateway);
            writeFile(LittleFS, gatewayPath, gateway.c_str());
          }
          // Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      restart = true;
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP Address : " + ip); });
    server.begin();
  }
  hxSettingUp();
  Blynk.config(auth, "blynk.cloud", 80);
}

// Smart mode
BLYNK_WRITE(V3)
{
  smartModeFlag();
}

void smartModeFlag()
{
  smartFlag = true;
  scheduleFlag = false;
  manualFlag = false;
  Blynk.virtualWrite(V4, LOW);
  currentMode = "Smart";
  lcd.clear();
  Serial.print("Current Mode : ");
  Serial.println("Smart");
  lcd.print(0, 1, "Mode : " + String(currentMode));
}

// Schedule mode
BLYNK_WRITE(V4)
{
  scheduledModeFlag();
}

void scheduledModeFlag()
{
  smartFlag = false;
  scheduleFlag = true;
  manualFlag = false;
  Blynk.virtualWrite(V3, LOW);
  currentMode = "Scheduled";
  lcd.clear();
  Serial.print("Current Mode : ");
  Serial.println("Scheduled");
  lcd.print(0, 1, "Mode : " + String(currentMode));
}

// Manual mode
BLYNK_WRITE(V5)
{
  smartFlag = false;
  scheduleFlag = false;
  manualFlag = true;
  Blynk.virtualWrite(V3, LOW);
  Blynk.virtualWrite(V4, LOW);
  currentMode = "Manual";
  lcd.clear();
  Serial.print("Current Mode : ");
  Serial.println("Manual");
  lcd.print(0, 1, "Mode : " + String(currentMode));
  // manualMode();

  int pinV5 = param.asInt();
  if (pinV5 == 1)
  {
    myServo.write(180);
    Serial.println("Opening disc");
  }
  else if (pinV5 == 0)
  {
    myServo.write(0);
    Serial.println("Closing disc");
    lastFedHour = currentHour;
    lastFedMinute = currentMinute;
    lastFedSecond = currentSecond;
    previousMinute = currentMinute;
    lastFedTimeStamp = timeStamp;
    Serial.println("Last Fed : " + String(lastFedTimeStamp));
    writeLCD();
  }
}

// Terminal syntax
BLYNK_WRITE(V7)
{
  if (String("help") == param.asStr())
  {
    terminal.println("  clear        : Clear terminal screen");
    terminal.println("  connection   : Show connection info");
    terminal.println("  showmode     : Show current mode");
    terminal.println("  showschedule : Show time schedule");
    terminal.println("  last         : Show last food poured to bowl");
    terminal.println("  ls           : Show listed files");
    terminal.println("  tare         : Reset loadcell to 0");
  }
  else if (String("ls") == param.asStr())
  {
    fileFunc();
  }
  else if (String("connection") == param.asStr())
  {
    terminal.print("  IP Address : ");
    terminal.println(ip);
    terminal.print("  Subnet     : ");
    terminal.println(subnet);
    terminal.print("  Gateway    : ");
    terminal.println(gateway);
    terminal.print("  DNS        : ");
    terminal.println(dnsIP);
  }
  else if (String("clear") == param.asStr())
  {
    terminal.clear();
  }
  else if (String("showmode") == param.asStr())
  {
    terminal.println("  Running on : " + String(currentMode) + " mode");
  }
  else if (String("showschedule") == param.asStr())
  {
    terminal.println("  " + String(schedule1) + ":0:0");
    terminal.println("  " + String(schedule2) + ":0:0");
    terminal.println("  " + String(schedule3) + ":0:0");
  }
  else if (String("last") == param.asStr())
  {
    terminal.println("  Last poured on " + String(lastFedHour) + ":" + String(lastFedMinute) + ":" + String(lastFedSecond));
    writeLCD();
  }
  else if (String("tare") == param.asStr())
  {
    // terminal.println("  Wait for 5 second!");
    terminal.println("  Remove all the mass on the bowl!");
    Serial.println("  OTW Mills");
    tareReq = true;
  }
  else if (String("yes") == param.asStr())
  {
    tareReq = true;
  }
  else
  {
    terminal.println("  Type 'help' for guidance");
  }
  terminal.flush();
}

void tareFromBlynk()
{
  if (tareFlag == true && tareState == false)
  {
    terminal.println("  Type 'yes' to confirm action!");
    Serial.println("Please confirm!");
    tareFlag = false;
    tareState = true;
    tareReq = false;
    terminal.flush();
    intervalTare = millis();
  }
  else if (tareFlag == false && tareState == true)
  {
    terminal.println("  Tare complete!");
    Serial.println("Tare complete! (using millis)");
    LoadCell.tareNoDelay();
    tareFlag = true;
    tareState = false;
    tareReq = false;
    terminal.flush();
  }
}

void loop()
{
  // put your main code here, to run repeatedly:
  Blynk.run();
  if (restart)
  {
    delay(5000);
    ESP.restart();
  }
  if (tareReq)
  {
    tareFromBlynk();
  }
  if (millis() - timeKeep >= 1000)
  {
    time();
    timeKeep = millis();
  }
  checkCondition();
  readWeight();
  readCapacity();
}

void time()
{
  DateTime now = rtc.now();
  // Serial.println(String("DateTime::TIMESTAMP_TIME:\t") + now.timestamp(DateTime::TIMESTAMP_TIME));
  currentHour = now.hour();
  currentMinute = now.minute();
  currentSecond = now.second();
  timeStamp = now.timestamp(DateTime::TIMESTAMP_TIME);
  // Serial.print("Time : ");
  // Serial.print(now.hour());
  // Serial.print(":");
  // Serial.print(now.minute());
  // Serial.print(":");
  // Serial.println(now.second());
}

void checkCondition()
{
  // SmartMode
  if (smartFlag == true && scheduleFlag == false && manualFlag == false)
  {
    smartMode();
  }
  // ScheduledMode
  if (smartFlag == false && scheduleFlag == true && manualFlag == false)
  {
    if (currentHour == schedule1 && currentMinute == 0 && currentSecond == 0)
    {
      schedule1Flag = true;
    }
    else if (currentHour == schedule2 && currentMinute == 0 && currentSecond == 0)
    {
      schedule2Flag = true;
    }
    else if (currentHour == schedule3 && currentMinute == 0 && currentSecond == 0)
    {
      schedule3Flag = true;
    }
    if (schedule1Flag || schedule2Flag || schedule3Flag)
    {
      if (foodPercentage != 0.00)
      {
        foodIsEmptyFlag = false;
        foodIsEmptyState = true;
      }
      else if (foodPercentage == 0.00)
      {
        foodIsEmptyFlag = true;
        foodIsEmptyState = false;
      }
      if (foodIsEmptyFlag == false && foodIsEmptyState == true)
      {
        scheduledMode();
      }
      else if (foodIsEmptyFlag == true && foodIsEmptyState == false)
      {
        Serial.println("Storage is empty! (scheduledMode)");
        lcd.clear();
        lcd.print(0, 0, "Storage is empty");
        lcd.print(0, 1, "Please Refill!");
        Blynk.logEvent("storage_is_empty");
        foodIsEmptyFlag = false;
        foodIsEmptyState = false;
        schedule1Flag = false;
        schedule2Flag = false;
        schedule3Flag = false;
      }
    }
  }
}

void smartMode()
{
  if (currentMinute != previousMinute && weight <= 20 && foodPercentage != 0.00)
  {
    servoState = true;
    if (servoState == true)
    {
      moveServo();
      emptyStorageCounter = 0;
      filledBowlCounter = 0;
    }
  }
  else if (currentMinute != previousMinute && weight >= 20 && foodPercentage != 0.00)
  {
    filledBowlCounter += 1;
    Serial.println("Bowl already filled");
    Serial.println("Weight         : " + String(weight) + " g");
    Serial.println("Last Fed       : " + String(lastFedHour) + ":" + String(lastFedMinute) + ":" + String(lastFedSecond));
    Serial.println("Filled Counter : " + String(filledBowlCounter));
    Blynk.logEvent("food_has_poured", "Food poured on : " + String(lastFedHour) + ":" + String(lastFedMinute) + ":" + String(lastFedSecond));
    if (filledBowlCounter >= 6)
    {
      lcd.clear();
      lcd.print(0, 0, "Food isn't -");
      lcd.print(0, 1, "Check ur pet!");
      Blynk.logEvent("check_your_pet");
    }
    previousMinute = currentMinute;
  }
  else if (currentMinute != previousMinute && weight <= 20 && foodPercentage <= 0.00)
  {
    emptyStorageCounter += 1;
    Serial.println("Food storage is empty!");
    lcd.clear();
    lcd.print(0, 0, "Food storage 0%");
    lcd.print(0, 1, "Counter : " + String(emptyStorageCounter));
    Blynk.logEvent("storage_is_empty");
    if (emptyStorageCounter >= 6)
    {
      Serial.println("Refill ur food storage pls!");
      lcd.clear();
      lcd.print(0, 0, "Refill ur food");
      lcd.print(0, 1, "storage pls!");
      Blynk.logEvent("storage_is_empty", "Refill food storage immediately!");
    }
    previousMinute = currentMinute;
  }
}

void scheduledMode()
{
  servoState = true;
  if (servoState == true)
  {
    moveServo();
  }
}

void manualMode()
{
  if (servoState == true)
  {
    moveServo();
  }
}

void moveServo()
{
  if (millis() - intervalServo >= 3000)
  {
    if (servoFlag == true)
    {
      myServo.write(180);
      Serial.println("Opening disc");
      servoFlag = false;
      servoState = true;
    }
    else
    {
      myServo.write(0);
      Serial.println("Closing disc");
      servoFlag = true;
      servoState = false;
      lastFedHour = currentHour;
      lastFedMinute = currentMinute;
      lastFedSecond = currentSecond;
      previousMinute = currentMinute;
      lastFedTimeStamp = timeStamp;
      Serial.println("Last Fed : " + String(lastFedTimeStamp));
      writeLCD();
      // Serial.println("Last Fed : " + String(lastFedHour) + ":" + String(lastFedMinute) + ":" + String(lastFedSecond));
      Blynk.logEvent("food_has_poured", "Food poured on : " + String(lastFedTimeStamp));
      currentWeight = weight;

      foodIsEmptyFlag = false;
      foodIsEmptyState = false;
      schedule1Flag = false;
      schedule2Flag = false;
      schedule3Flag = false;
    }
    intervalServo = millis();
  }
}

void writeLCD()
{
  lcd.clear();
  lcd.print(0, 0, "Last : " + String(lastFedTimeStamp));
  lcd.print(0, 1, "Mode : " + String(currentMode));
}

void checkWiFi()
{
  unsigned long currentMillis = millis();
  if (currentMillis - intervalCheckWiFi >= 30000UL)
  {
    switch (WiFi.status())
    {
    case WL_NO_SSID_AVAIL:
      Serial.println("Configured SSID cannot be reached");
      break;
    case WL_CONNECTED:
      Serial.println("Connection successfully established");
      resetWiFiCounter = 0;
      break;
    case WL_CONNECT_FAILED:
      Serial.println("Connection failed");
      break;
    }
    if (WiFi.status() == 4)
    {
      resetWiFiCounter += 1;
      Serial.println("True");
    }
    Serial.print("Reset wifi counter : ");
    Serial.println(resetWiFiCounter);
    Serial.printf("Connection status : %d\n", WiFi.status());
    Serial.print("RSSI : ");
    Serial.println(WiFi.RSSI());
    Serial.print("IP : ");
    Serial.println(WiFi.localIP());
    intervalCheckWiFi = currentMillis;
  }
  if (WiFi.status() == 4 && resetWiFiCounter >= 6)
  {
    // Tambahkan validasi seperti tekan tombol untuk memvalidasi reset WiFi config
    // Link tutorial hold button :
    // https://www.baldengineer.com/use-millis-with-buttons-to-delay-events.html
    Serial.println("Network unreachable, Resetting WiFi!");
    eraseFile();
    resetWiFiCounter = 0;
    restart = true;
  }
}

void fileFunc()
{
  Dir dir = LittleFS.openDir("");
  while (dir.next())
  {
    // Serial.println(dir.fileName());
    terminal.print("  - ");
    terminal.println(dir.fileName());
    // Serial.println(dir.fileSize());
  }
}

// Rangkai 3D printed menggunakan tameng faceshield
// Kasih jeda 15-20 detik saat tare melalui Blynk *belum selesai
// Tambahkan button untuk validasi network reset
// https://randomnerdtutorials.com/esp8266-web-server-spiffs-nodemcu/