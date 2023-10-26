
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
// #include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <UniversalTelegramBot.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Custom Files
// #include "handleOTA.h"
#include "secrets.h"
#include "GlobalVars.h"
#include "ServerSetup.h"
#include "CustomUtils.h"

// Telegram Bot
const char *BOTtoken = SECRET_BOT_TOKEN;
const char *CHAT_ID = SECRET_CHAT_ID;
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Wifi and Server
const char *ssid = SECRET_SSID;
const char *password = SECRET_SPASSWORD;
AsyncWebServer server(80);

int temp_data[100];
int humidity_data[100];

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
  setupServer(humiditySensor, humidityRelay, tempRelay, points_read_from_start, temperature, humidity_percent, humidity_setpoint_global, auto_mode, temp_nofo_flag, humidity_nofo_flag, is_pulse_water, no_water_detected);
  populateDataArrays();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  //  setupSavedData("temp");
  //  setupSavedData("humidity");
  //  setupSavedData("point_count");
}

int minute_counter = 0;
void loop(void)
{
  // Handle OTA Updatesa
  // ArduinoOTA.handle();
  unsigned long currentMillis = millis();
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval))
  {
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }

  if ((currentMillis - lastTime) > timerDelay)
  {
    Serial.println(no_water_detected);
    lastTime = millis();
    temperature = getTemperature();
    humidity_percent = getHumidity(humiditySensor);

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
    // previousMillis = currentMillis;
    addValueToHumidityData(humidity_percent);
    addValueToTempData(temperature);
  }

  delay(10);
}

void autoControl(float humidity_percent, float temperature)
{

  /* Pump Control */
  // If humidity is not changing even though we have been watering then dont water any more.
  if (humidity_percent < humidity_setpoint_global && (!no_water_detected))
  {
    humidity_percent_prev = humidity_percent;
    pulseWater();
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

  if (humidity_percent < humidity_setpoint_global && !(humidity_percent > humidity_percent_prev) && !no_water_detected)
  {
    no_water_detected = true;
  }

  /* Temperature Control */
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

void pulseWater()
{
  digitalWrite(humidityRelay, HIGH);
  delay(800);
  digitalWrite(humidityRelay, LOW);
  delay(300);
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
  return number;
}

int addValueToHumidityData(float number)
{

  for (int i = 99; i >= 1; i--)
  {
    humidity_data[i] = humidity_data[i - 1];
  }
  humidity_data[0] = number;
  writeDataToSPIFFS("humidity");
  return number;
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
    if (!file)
    {
      Serial.println("Error opening file for writing");
      return;
    }
  }
  else if (type == "temp")
  {
    data_to_save = temp_data;
    file = SPIFFS.open("/temp.txt", "w");
    if (!file)
    {
      Serial.println("Error opening file for writing");
      return;
    }
  }
  else
  {
    String point_count_string = String(points_read_from_start).c_str();
    file = SPIFFS.open("/point_count.txt", "w");
    if (!file)
    {
      Serial.println("Error opening file for writing");
      return;
    }
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
