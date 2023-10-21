
#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

// Temp and Humidity I/O
extern const int humiditySensor;
extern const int humidityRelay;
extern const int tempRelay;
extern const int led;
extern float temperature;
extern int humidity_raw;
extern float humidity_percent;

// Control Vars
extern int auto_mode;
extern bool temp_nofo_flag;
extern bool humidity_nofo_flag;
extern int humidity_setpoint_global;
extern bool is_pulse_water;
extern int points_read_from_start;

// System Time NTP
extern const char *ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;
// extern struct tm timeinfo;

#endif