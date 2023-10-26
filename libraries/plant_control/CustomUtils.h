
#ifndef SENSOR_FUNCTIONS_H
#define SENSOR_FUNCTIONS_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>

extern OneWire oneWire;
extern DallasTemperature sensors;
extern struct tm timeinfo;
int getHumidity(int humiditySensor);
float getTemperature();
void drawGraph(AsyncWebServerRequest *request, int temp_data[100]);
String getStartTime();
void printLocalTime();
void resetNoWaterDetected(bool &no_water_detected);

#endif