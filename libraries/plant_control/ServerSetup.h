#ifndef SERVER_SETUP_H
#define SERVER_SETUP_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

extern AsyncWebServer server;

void setupServer(int humiditySensor, int humidityRelay, int tempRelay, int &points_read_from_start, float &temperature, float &humidity_percent, int &humidity_setpoint_global, int &auto_mode, bool &temp_nofo_flag, bool &humidity_nofo_flag, bool &is_pulse_water, bool &no_water_detected);

#endif