#include "CustomUtils.h"

// #include "GlobalVars.h"
// struct tm timeinfo;
int getHumidity(int humiditySensor)
{
    int humidity_raw = analogRead(humiditySensor);
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

void drawGraph(AsyncWebServerRequest *request, int temp_data[100])
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

void printLocalTime()
{
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}