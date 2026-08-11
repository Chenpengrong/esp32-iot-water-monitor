#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// 水质阈值结构体
struct WaterQualityThresholds {
    float ph_you_min = 6.5f;
    float ph_you_max = 8.5f;
    float ph_liang_min = 5.5f;
    float ph_liang_max = 9.5f;
    float tds_you_min = 0.0f;
    float tds_you_max = 300.0f;
    float tds_liang_min = 300.0f;
    float tds_liang_max = 600.0f;
    float waterAlarmLevel = 20.0f;
};

extern WebServer server;
extern float waterLevel, pHvalue, waterTemp, voltage;
extern SemaphoreHandle_t xDataMutex;

void addRecord(float wl, float ph, float tds, float wt, float volt, String quality);
void initWiFi(const char* ssid, const char* password);
void setupWebServer();
void handleRoot();
void handleData();
void handleHistory();

#endif
