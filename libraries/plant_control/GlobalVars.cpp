#include "GlobalVars.h"

const int humiditySensor = 34;
const int humidityRelay = 26;
const int tempRelay = 27;
const int led = 13;
float temperature = 0.0;
int humidity_raw = 0;
float humidity_percent = 0.0;

// Control Vars
int auto_mode = 1;
bool temp_nofo_flag = false;
bool humidity_nofo_flag = false;
int humidity_setpoint_global = 50;
bool is_pulse_water = false;
int points_read_from_start = 0;
float humidity_percent_prev = 0.0;
bool no_water_detected = false;

// System Time NTP
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;