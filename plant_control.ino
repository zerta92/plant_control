#include <Arduino.h>
#include "esp_adc_cal.h"
#include "time.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Telegram Bot
#define BOTtoken "5085933205:AAGPxkWO4kgpxgl2B-8zWV2gKyk_7hQebJo"
#define CHAT_ID "1731352194"
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// System Time NTP
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;
struct tm timeinfo;

// Wifi and Server
const char *ssid = "SKYSCVTX";
const char *password = "QB4uJpGUt3gi";
AsyncWebServer server(80);

// Temp and Humidity I/O
int auto_mode = 1;
const int humiditySensor = 34;
const int humidityRelay = 26;
const int tempRelay = 27;
const int led = 13;
int temp_data[100];
int humidity_data[100];

int points_read_from_start = 0;

bool is_pulse_water = false;

// Temp Analog Reading
#define ONE_WIRE_BUS 0
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
unsigned long timerDelay = 1000;
unsigned long lastTime = 0;
unsigned long lastTimeLogToSpiffs = 0;

void printDirectory(File dir, int numTabs = 3);

unsigned long previousMillis = 0;
unsigned long interval = 60000;
float temperature = 0.0;
int humidity_raw = 0;
float humidity_percent = 0.0;
String webString = "";
bool temp_nofo_flag = false;
bool humidity_nofo_flag = false;
int humidity_setpoint_global = 50;
int TEMPERATURE_SETPOINT = 21;
int *data_points_pointer;
char data_points_placeholder[200] = {'\0'};

void setup()
{
  Serial.println("starting up...");
  Serial.begin(115200);
  pinMode(humidityRelay, OUTPUT);
  pinMode(tempRelay, OUTPUT);
  sensors.begin();
  Serial.println("starting SPIFFS...");
  setupSPIFFS();
  Serial.println("Connecting WIFI...");
  connectWifi();
  Serial.println("starting server...");
  setupServer();
  populateDataArrays();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  //  setupSavedData("temp");
  //  setupSavedData("humidity");
  //  setupSavedData("point_count");

  // OTA
  ArduinoOTA
      .onStart([]()
               {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();
}

int minute_counter = 0;
void loop(void)
{
  // Handle OTA Updates
  ArduinoOTA.handle();
  unsigned long currentMillis = millis();
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval))
  {
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }

  if ((currentMillis - lastTime) > timerDelay)
  {
    lastTime = millis();

    temperature = getTemperature();
    humidity_raw = analogRead(humiditySensor);
    humidity_percent = getHumidity(humidity_raw);

    if (auto_mode)
    {
      autoControl(humidity_percent, temperature);
    }
    else if (is_pulse_water)
    {
      pulseWater();
    };
  }

  if (currentMillis - lastTimeLogToSpiffs >= interval)
  {
    lastTimeLogToSpiffs = millis();
    previousMillis = currentMillis;
    addValueToHumidityData(humidity_percent);
    addValueToTempData(temperature);
  }

  //  if (currentMillis - previousMillis >= interval)
  //  {
  //    previousMillis = currentMillis;
  //    addValueToHumidityData(humidity_percent);
  //    addValueToTempData(temperature);
  //  }

  delay(10);
}

void autoControl(float humidity_percent, float temperature)
{
  if (humidity_percent < humidity_setpoint_global)
  {
    digitalWrite(humidityRelay, HIGH);
    delay(1000);
    digitalWrite(humidityRelay, LOW);
    if (humidity_nofo_flag == false)
    {

      String message = "Humidity below " + String(humidity_setpoint_global);
      bot.sendMessage(CHAT_ID, "Humidity Below ", "");
      humidity_nofo_flag = true;
    }
  }
  else
  {
    digitalWrite(humidityRelay, LOW);
  }

  if (temperature < TEMPERATURE_SETPOINT)
  {
    digitalWrite(tempRelay, HIGH);
    if (temp_nofo_flag == false)
    {
      bot.sendMessage(CHAT_ID, "Temperature Below 21C", "");
      temp_nofo_flag = true;
    }
  }
  else
  {
    digitalWrite(tempRelay, LOW);
  }
}

void connectWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  int x = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Signal Strength: ");
    Serial.println(WiFi.RSSI());
    Serial.print("number of attempts: ");
    Serial.println(x);
    delay(2000);
    if (x == 8)
    {
      WiFi.begin(ssid, password);
      delay(6000); // 2500
      x++;
    }
    else if (x > 8)
    {
      Serial.println("Resetting due to Wifi not connecting...");
      ESP.restart();
    }
    else
    {
      x++;
    };
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32"))
  {
    Serial.println("MDNS responder started");
  }

  bot.sendMessage(CHAT_ID, "Plant Bot started up", "");
}

void setupSPIFFS()
{
  if (!SPIFFS.begin())
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    SPIFFS.begin(true);
  }
}

void setupServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", "text/html");
              
                   File file = SPIFFS.open("/index.html");
                    if (!file) {
                      Serial.println("Failed to open index.html");
                      return;
                    }
                   const auto filesize = file.size();
                   file.close();
                   response->addHeader("Content-Length", String(filesize, DEC));
              request->send(response);
              Serial.println("Sent index.html"); });

  // To Download a file
  server.on("/a", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", String(), true);
              response->addHeader("Server", "ESP Async Web Server");
              request->send(response); });

  server.on("/logo.svg", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/logo.svg", "image/svg+xml");
              File file = SPIFFS.open("/logo.svg");
              if (!file)
              {
                Serial.println("Failed to open logo.svg");
                return;
              }
              const auto filesize = file.size();
              response->addHeader("Content-Length", String(filesize, DEC));
              file.close();
              request->send(response); });

  server.on("/temp.svg", HTTP_GET, [](AsyncWebServerRequest *request)
            { drawGraph(request); });

  server.on("/humidity_points", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/humidity.txt"); });
  server.on("/temp_points", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/temp.txt"); });

  server.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              const float temperature = getTemperature();
              const int humidity_raw = analogRead(humiditySensor);
              const float humidity = getHumidity(humidity_raw);
              webString = "Temperature: " + String((int)temperature) + " C" + "\n" + "Soil Humidity: " + String((int)humidity) + " %";
              request->send(200, "text/plain", webString); });

  server.on("/get_points_read_from_start", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", String(points_read_from_start).c_str()); });

  server.on("/temp", HTTP_GET, [](AsyncWebServerRequest *request)
            {
//              const float temperature = getTemperature();
              request->send(200, "text/plain", String(temperature).c_str()); });

  server.on("/get_temp_points", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/temp.txt", "text/plain"); });

  server.on("/get_humidity_points", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/humidity.txt", "text/plain"); });

  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request)
            {
//              const int humidity_raw = analogRead(humiditySensor);
//              const float humidity = getHumidity(humidity_raw);
                Serial.print("/hereee");
              request->send(200, "text/plain", String(humidity_percent).c_str()); });

  server.on("/get_pump_status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              int pump_status = digitalRead(humidityRelay);
              request->send(200, "text/plain", String(pump_status).c_str()); });

  server.on("/get_light_status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              int light_status = digitalRead(tempRelay);
              request->send(200, "text/plain", String(light_status).c_str()); });

  server.on("/get_auto_status", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", String(auto_mode).c_str()); });

  server.on("/get_nofos_status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
            StaticJsonDocument<200> doc;
              doc["temp_flag"] = temp_nofo_flag;
              doc["humidity_flag"] = humidity_nofo_flag;
              String json;
              serializeJson(doc, json);
             request->send(200, "text/plain", json); });

  server.on("/toggle_pump", HTTP_GET, [](AsyncWebServerRequest *request)
            {
//              https://techtutorialsx.com/2017/12/17/esp32-arduino-http-server-getting-query-parameters/
             AsyncWebParameter* p = request->getParam(0);
             int paramsNr = request->params();
             String pump_status = "0";
             for(int i=0;i<paramsNr;i++){
                  AsyncWebParameter* p = request->getParam(i);
                  pump_status = String(p->value()).c_str();
              }
           

              const int humidity_raw = analogRead(humiditySensor);
              const float humidity_percent = getHumidity(humidity_raw);
              if (pump_status == "0" || humidity_percent >= humidity_setpoint_global)
              {
                digitalWrite(humidityRelay, LOW);
                is_pulse_water = false;
                pump_status = "0";
              }
              else if (pump_status == "1" && humidity_percent <= humidity_setpoint_global * 1.2)
              {
                is_pulse_water = true;
                pump_status = "1";
              }
           
              
              request->send(200, "text/plain", String(pump_status).c_str()); });

  server.on("/toggle_light", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              int light_status = digitalRead(tempRelay);
              if (light_status == LOW)
              {
                digitalWrite(tempRelay, HIGH);
              }
              if (light_status == HIGH)
              {
                digitalWrite(tempRelay, LOW);
              }
              light_status = digitalRead(tempRelay);
              request->send(200, "text/plain", String(light_status).c_str()); });

  server.on("/toggle_auto", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              auto_mode = !auto_mode;
              request->send(200, "text/plain", String(auto_mode).c_str()); });

  server.on("/reset_flags", HTTP_GET, [](AsyncWebServerRequest *request)
            {
             humidity_nofo_flag = false;
             temp_nofo_flag = false;
             request->send(200,"text/plain", String(1).c_str()); });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              request->send(200);
              ESP.restart(); });

  server.on("/get_start_time", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String start_time = getStartTime();
              request->send(200, "text/plain", String(start_time).c_str()); });
  server.on("/get_humidity_setpoint", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", String(humidity_setpoint_global).c_str()); });

  server.on("/set_humidity_setpoint", HTTP_GET, [](AsyncWebServerRequest *request)
            {
             AsyncWebParameter* p = request->getParam(0);
             int paramsNr = request->params();
             for(int i=0;i<paramsNr;i++){
                  AsyncWebParameter* p = request->getParam(i);
                  humidity_setpoint_global = p->value().toInt();
              }
              request->send(200, "text/plain", String(humidity_setpoint_global).c_str()); });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
  Serial.println("HTTP server started");
}

int getHumidity(int humidity_raw)
{
  const float slope = -0.0452;
  const float humidity_percent = humidity_raw * slope + 150;

  return humidity_percent;
}

float getTemperature()
{
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  return temperatureC;
}

void pulseWater()
{
  digitalWrite(humidityRelay, HIGH);
  delay(1000);
  digitalWrite(humidityRelay, LOW);
}

void drawGraph(AsyncWebServerRequest *request)
{
  String out = "";
  char plot_data[400];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"350\" height=\"100\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = temp_data[0];
  int x_ = 0;
  for (int x = 0; x < 99; x++)
  {
    int y2 = temp_data[x];
    sprintf(plot_data, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x_ * 10, 100 - y * 2, x * 10, 100 - y2 * 2);
    out += plot_data;
    y = y2;
    x_ = x;
  }

  out += "</g>\n</svg>\n";

  request->send(200, "image/svg+xml", out);
}

int addValueToTempData(float number)
{
  for (int i = 99; i >= 1; i--)
  {
    temp_data[i] = temp_data[i - 1];
  }
  temp_data[0] = number;
  points_read_from_start++;
  writeDataToSPIFFS("temp");
  writeDataToSPIFFS("points_count");
}

int addValueToHumidityData(float number)
{
  for (int i = 99; i >= 1; i--)
  {
    humidity_data[i] = humidity_data[i - 1];
  }
  humidity_data[0] = number;
  writeDataToSPIFFS("humidity");
}

void setupSavedData(String type)
{

  int *data_to_save;
  File file;
  if (type == "humidity")
  {
    data_to_save = humidity_data;
    file = SPIFFS.open("/humidity.txt", "r");
  }
  else if (type == "temp")
  {
    data_to_save = temp_data;
    file = SPIFFS.open("/temp.txt", "r");
  }
  else
  {
    file = SPIFFS.open("/point_count.txt", "r");
    if (file.size() == 0)
    {
      Serial.println("data file empty");
      return;
    };
    float ssidString = file.parseFloat();
    points_read_from_start = static_cast<int>(ssidString);
    file.close();
    return;
  };

  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  };

  if (file.size() == 0)
  {
    Serial.println("data file empty");
    return;
  };

  uint16_t i = 0;
  while (file.available())
  {
    data_points_placeholder[i] = file.read();
    i++;
  }

  file.close();

  data_points_pointer = getValue(data_points_placeholder);

  int y = 0;
  for (int i = 0; i < 99; ++i)
  {
    data_to_save[i] = data_points_pointer[i];
    y++;
  }

  // delete []sa// Clear pointer value from getValue function
}

void writeDataToSPIFFS(String type)
{
  int *data_to_save;
  File file;
  if (type == "humidity")
  {
    data_to_save = humidity_data;
    file = SPIFFS.open("/humidity.txt", "w");
  }
  else if (type == "temp")
  {
    data_to_save = temp_data;
    file = SPIFFS.open("/temp.txt", "w");
  }
  else
  {
    String point_count_string = String(points_read_from_start).c_str();
    file = SPIFFS.open("/point_count.txt", "w");
    int bytesWritten = file.print(point_count_string);
    file.close();
    return;
  }

  String string_data = "";
  String plot_data = "";
  for (int x = 0; x < 99; x++)
  {
    int point = data_to_save[x];
    plot_data = String(point) + String("|");
    string_data += plot_data;
  }

  if (!file)
  {
    Serial.println("Error opening file for writing");
    return;
  }

  int bytesWritten = file.print(string_data);
  file.close();
}

int *getValue(String data_)
{
  int *sa = new int[200];
  int r = 0;
  int t = 0;
  for (int i = 0; i < data_.length() - 1; i++)
  {
    String x = String(data_.charAt(i));
    if (x == "|")
    {
      sa[t] = data_.substring(r, i).toInt();
      r = (i + 1);
      t++;
    }
  }

  return sa;
}

void readFlashData()
{
  //      unsigned int totalBytes = SPIFFS.totalBytes();
  //    unsigned int usedBytes = SPIFFS.usedBytes();
  //
  //    Serial.println("File sistem info.");
  //
  //    Serial.print("Total space:      ");
  //    Serial.print(totalBytes);
  //    Serial.println("byte");
  //
  //    Serial.print("Total space used: ");
  //    Serial.print(usedBytes);
  //    Serial.println("byte");
  //

  // Open dir folder
  //    File dir = SPIFFS.open("/");
  //    File dir_html = SPIFFS.open(F("/index.html"), "r");
  //    if(!dir_html){
  //       Serial.println("error reading file");
  //    }else{
  //      Serial.println("found data");
  //
  //      Serial.println(String(dir_html));
  //    }
  // Cycle all the content
  //    printDirectory(dir);
}

void printDirectory(File dir, int numTabs)
{
  while (true)
  {

    File entry = dir.openNextFile();
    if (!entry)
    {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++)
    {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory())
    {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    }
    else
    {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void populateDataArrays()
{
  for (uint8_t i = 0; i < 99; i++)
  {
    temp_data[i] = 666;
    humidity_data[i] = 666;
  }
}

void printLocalTime()
{
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

String getStartTime()
{
  if (timeinfo.tm_year == 70)
  {
    Serial.println("Failed to get start time, trying again");
    getLocalTime(&timeinfo);
  }

  int _year_days = timeinfo.tm_yday;
  int _day = timeinfo.tm_mday;
  int _hour = timeinfo.tm_hour;
  int _min = timeinfo.tm_min;
  int _year = timeinfo.tm_year;
  int _sec = timeinfo.tm_sec;
  String time_string = String(_year) + String("-") + String(_year_days) + String("-") + String(_day) + String("T") + String(_hour) + String(":") + String(_min) + String(":") + String(_sec);
  return time_string;
}