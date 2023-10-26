#include "ServerSetup.h"
#include <SPIFFS.h>
#include "CustomUtils.h"
// #include "GlobalVars.h"

String webString = "";

void setupServer(int humiditySensor, int humidityRelay, int tempRelay, int &points_read_from_start, float &temperature, float &humidity_percent, int &humidity_setpoint_global, int &auto_mode, bool &temp_nofo_flag, bool &humidity_nofo_flag, bool &is_pulse_water, bool &no_water_detected)
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", "text/html");

    File file = SPIFFS.open("/index.html");
    if (!file)
    {
      Serial.println("Failed to open index.html");
      return;
    }
    const auto filesize = file.size();
    file.close();
    response->addHeader("Content-Length", String(filesize, DEC));
    request->send(response); });

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

  // server.on("/temp.svg", HTTP_GET, [](AsyncWebServerRequest *request)
  //           { drawGraph(request); });

  server.on("/humidity_points", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/humidity.txt"); });
  server.on("/temp_points", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/temp.txt"); });

  server.on("/stats", HTTP_GET, [humiditySensor, &temperature](AsyncWebServerRequest *request)
            {
              const float temperature = getTemperature();
              const float humidity = getHumidity(humiditySensor);
              webString = "Temperature: " + String((int)temperature) + " C" + "\n" + "Soil Humidity: " + String((int)humidity) + " %";
              request->send(200, "text/plain", webString); });

  server.on("/get_points_read_from_start", HTTP_GET, [&points_read_from_start](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", String(points_read_from_start).c_str()); });

  server.on("/temp", HTTP_GET, [&temperature](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", String(temperature).c_str()); });

  server.on("/get_temp_points", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/temp.txt", "text/plain"); });

  server.on("/get_humidity_points", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/humidity.txt", "text/plain"); });

  server.on("/humidity", HTTP_GET, [&humidity_percent](AsyncWebServerRequest *request)
            {
//              const int humidity_raw = analogRead(humiditySensor);
//              const float humidity = getHumidity(humidity_raw);
              request->send(200, "text/plain", String(humidity_percent).c_str()); });

  server.on("/get_pump_status", HTTP_GET, [humidityRelay](AsyncWebServerRequest *request)
            {
              int pump_status = digitalRead(humidityRelay);
              request->send(200, "text/plain", String(pump_status).c_str()); });

  server.on("/get_light_status", HTTP_GET, [tempRelay](AsyncWebServerRequest *request)
            {
              int light_status = digitalRead(tempRelay);
              request->send(200, "text/plain", String(light_status).c_str()); });

  server.on("/get_auto_status", HTTP_GET, [&auto_mode](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", String(auto_mode).c_str()); });

  server.on("/get_nofos_status", HTTP_GET, [&temp_nofo_flag, &humidity_nofo_flag](AsyncWebServerRequest *request)
            {
            StaticJsonDocument<200> doc;
              doc["temp_flag"] = temp_nofo_flag;
              doc["humidity_flag"] = humidity_nofo_flag;
              String json;
              serializeJson(doc, json);
             request->send(200, "text/plain", json); });

  server.on("/toggle_pump", HTTP_GET, [humiditySensor, humidityRelay, &humidity_percent, &humidity_setpoint_global, &is_pulse_water](AsyncWebServerRequest *request)
            {
//              https://techtutorialsx.com/2017/12/17/esp32-arduino-http-server-getting-query-parameters/
             AsyncWebParameter* p = request->getParam(0);
             int paramsNr = request->params();
             String pump_status = "0";
             for(int i=0;i<paramsNr;i++){
                  AsyncWebParameter* p = request->getParam(i);
                  pump_status = String(p->value()).c_str();
              }
           

              const float humidity_percent = getHumidity(humiditySensor);
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

  server.on("/toggle_light", HTTP_GET, [tempRelay](AsyncWebServerRequest *request)
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

  server.on("/toggle_auto", HTTP_GET, [&auto_mode](AsyncWebServerRequest *request)
            {
              auto_mode = !auto_mode;
              request->send(200, "text/plain", String(auto_mode).c_str()); });

  server.on("/reset_no_water_detected", HTTP_GET, [&no_water_detected](AsyncWebServerRequest *request)
            {
              resetNoWaterDetected(no_water_detected);
              request->send(200, "text/plain", String(false).c_str()); });

  server.on("/get_no_water_detected_status", HTTP_GET, [&no_water_detected](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", String(no_water_detected).c_str()); });

  server.on("/reset_flags", HTTP_GET, [&temp_nofo_flag, &humidity_nofo_flag](AsyncWebServerRequest *request)
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
  server.on("/get_humidity_setpoint", HTTP_GET, [&humidity_setpoint_global](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", String(humidity_setpoint_global).c_str()); });

  server.on("/set_humidity_setpoint", HTTP_GET, [&humidity_setpoint_global](AsyncWebServerRequest *request)
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
