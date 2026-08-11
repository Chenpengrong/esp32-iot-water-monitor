#include <Arduino.h>
#include <ESP32AnalogRead.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "OLED.h"
#include "web_server.h"
#include "adc_sensors.h"
#include "ds18b20_wrapper.h"
#include "buzzer_led.h"
#include "serial.h"

WaterQualityThresholds thresholds;

// ------------------- 全局变量 -------------------
float waterLevel = 0.0f;
float pHvalue = 0.0f;
float tdsValue = 0.0f;
float waterTemp = 0.0f;
float voltage = 0.0f;
String waterQuality = "优";          // 水质等级

bool sensorOLEDEnabled = false;      // Web控制：OLED+电压+水温使能
SemaphoreHandle_t xDataMutex = NULL; // 保护所有共享数据（传感器值、阈值）

// Wi-Fi 凭据
const char* ssid = "你的WIFI名称";
const char* password = "你的WIFI密码";

// 任务函数原型
void taskSensor(void *pvParameters);
void taskWebServer(void *pvParameters);
void taskSerial(void *pvParameters);
//void taskTDS(void *pvParameters);

// ------------------- 水质评估函数 -------------------
void evaluateWaterQuality(float ph, float tds, String &quality) {
    // 判断 pH 的等级
    String phLevel;
    if (ph >= thresholds.ph_you_min && ph <= thresholds.ph_you_max) {
        phLevel = "优";
    } else if (ph >= thresholds.ph_liang_min && ph <= thresholds.ph_liang_max) {
        phLevel = "良";
    } else {
        phLevel = "差";
    }

    // 判断 TDS 的等级
    String tdsLevel;
    if (tds >= thresholds.tds_you_min && tds <= thresholds.tds_you_max) {
        tdsLevel = "优";
    } else if (tds >= thresholds.tds_liang_min && tds <= thresholds.tds_liang_max) {
        tdsLevel = "良";
    } else {
        tdsLevel = "差";
    }

    // 取两者中较差的等级（规则：差 > 良 > 优）
    if (phLevel == "差" || tdsLevel == "差") {
        quality = "差";
    } else if (phLevel == "良" || tdsLevel == "良") {
        quality = "良";
    } else {
        quality = "优";
    }
}

// ------------------- 系统设置 -------------------
void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 16, 17);

    initADCSensors();          // 水位/pH/TDS/电压
    initDS18B20();
    initBuzzerLed();

    xDataMutex = xSemaphoreCreateMutex();
    if (xDataMutex == NULL) {
        Serial.println("Failed to create mutex");
        while (1);
    }

    // 创建串口接收任务（核心1）
    xTaskCreatePinnedToCore(taskSerial, "SerialTask", 4096, NULL, 2, NULL, 1);
    // 创建传感器采集任务（核心1）
    xTaskCreatePinnedToCore(taskSensor, "SensorTask", 12288, NULL, 2, NULL, 1);
    // 创建Web服务器任务（核心0）
    xTaskCreatePinnedToCore(taskWebServer, "WebServerTask", 12288, NULL, 3, NULL, 0);

    vTaskStartScheduler();
}

void loop() {
    // 空，FreeRTOS接管
}

// ------------------- 传感器采集与报警任务 -------------------
void taskSensor(void *pvParameters) {
    initOLED();
    clearOLED();

    static unsigned long lastQualityAlarmTime = 0;
    const unsigned long QUALITY_ALARM_INTERVAL = 5000; // 水质差时每5秒报警一次
    bool waterAlarmActive = false;

    static unsigned long lastSensorTime = 0;
    const unsigned long SENSOR_INTERVAL = 2000;        // 每2秒采集一次

    bool lastEnabledState = false;
    float localPH, localTDS, localTemp, localVolt, localWater;
    String localQuality;

    while (1) {
        // 读取 OLED 使能标志
        bool currentEnabled;
        if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
            currentEnabled = sensorOLEDEnabled;
            xSemaphoreGive(xDataMutex);
        } else continue;

        // OLED 状态变化时的处理
        if (currentEnabled != lastEnabledState) {
            if (currentEnabled) {
                // 刚开启：立即读取一次电压和水温用于显示
                localVolt = readBatteryVoltage();
                localTemp = readWaterTemp();
                // 水位值从全局变量获取
                if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
                    localWater = waterLevel;
                    xSemaphoreGive(xDataMutex);
                }
                updateOLED(localWater, localPH, localTemp, localVolt);
            } else {
                clearOLED();
            }
            lastEnabledState = currentEnabled;
        }

        // 周期性采集（每2秒）
        if (millis() - lastSensorTime >= SENSOR_INTERVAL) {
            lastSensorTime = millis();

            // ----- 读取所有传感器（使用 adc_sensors 模块的函数）-----
            localPH = readPH();

            if (currentEnabled) {
                localVolt = readBatteryVoltage();
                localTemp = readWaterTemp();
            } else {
                localVolt = 0.0f;
                localTemp = 0.0f;
            }

            localTDS = readTDS(localTemp);   // 温度补偿使用当前水温

            // 水位值从串口任务更新的全局变量中获取（加锁保护）
            if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
                localWater = waterLevel;
                xSemaphoreGive(xDataMutex);
            } else {
                continue;
            }

            // ----- 水质评估 -----
            if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
                evaluateWaterQuality(localPH, localTDS, localQuality);
                //Serial.printf("[WQ] pH=%.2f TDS=%.1f => %s\n", localPH, localTDS, localQuality.c_str());
                // 更新全局共享变量
                pHvalue = localPH;
                tdsValue = localTDS;
                waterTemp = localTemp;
                voltage = localVolt;
                waterQuality = localQuality;
                xSemaphoreGive(xDataMutex);
            } else {
                continue;  // 获取互斥锁失败，跳过本次周期
            }

            // 记录历史数据
            addRecord(localWater, localPH, localTDS, localTemp, localVolt, localQuality);

            // 更新 OLED 显示
            if (currentEnabled) {
                updateOLED(localWater, localPH, localTemp, localVolt);
            }

            // ----- 报警阈值读取 -----
            float alarmLevel;
            if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
                alarmLevel = thresholds.waterAlarmLevel;
                xSemaphoreGive(xDataMutex);
            } else {
                alarmLevel = 20.0f;
            }

            // ========== 报警逻辑（水位>阈值 && 水质差 -> 急促报警）==========
            bool bothAlarm = (localWater > alarmLevel) && (localQuality == "差");
            static unsigned long lastFastAlarmToggle = 0;
            static bool fastAlarmState = false;
            static bool fastAlarmActive = false;

            if (bothAlarm) {
                if (!fastAlarmActive) {
                    fastAlarmActive = true;
                    if (waterAlarmActive) {
                        stopContinuousAlarm();
                        waterAlarmActive = false;
                    }
                    lastQualityAlarmTime = millis(); // 重置水质报警计时器
                }
                // 每200ms切换声光状态
                unsigned long now = millis();
                if (now - lastFastAlarmToggle >= 200) {
                    lastFastAlarmToggle = now;
                    fastAlarmState = !fastAlarmState;
                    if (fastAlarmState) {
                        buzzerOn();
                        ledOn();
                    } else {
                        buzzerOff();
                        ledOff();
                    }
                }
            } else {
                // 退出急促报警
                if (fastAlarmActive) {
                    fastAlarmActive = false;
                    buzzerOff();
                    ledOff();
                }

                // 单独水位报警（连续）
                if (localWater > alarmLevel) {
                    if (!waterAlarmActive) {
                        startContinuousAlarm();
                        waterAlarmActive = true;
                    }
                } else {
                    if (waterAlarmActive) {
                        stopContinuousAlarm();
                        waterAlarmActive = false;
                    }
                }

                // 单独水质报警（差时每5秒三次短促）
                if (localQuality == "差") {
                    if (millis() - lastQualityAlarmTime >= QUALITY_ALARM_INTERVAL) {
                        for (int i = 0; i < 3; i++) {
                            buzzerOn();
                            ledOn();
                            vTaskDelay(pdMS_TO_TICKS(200));
                            buzzerOff();
                            ledOff();
                            vTaskDelay(pdMS_TO_TICKS(250));
                        }
                        lastQualityAlarmTime = millis();
                    }
                } else {
                    lastQualityAlarmTime = 0;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));  // 短延时避免任务饿死
    }
}

// ------------------- 串口接收任务 -------------------
void taskSerial(void *pvParameters) {
    static uint8_t rx_buffer[FRAME_LEN];
    static uint8_t rx_index = 0;

    while (1) {
        if (Serial2.available()) {
            uint8_t byte = Serial2.read();

            if (rx_index == 0 && byte != FRAME_HEAD) {
                continue;
            }

            rx_buffer[rx_index++] = byte;

            if (rx_index >= FRAME_LEN) {
                if (verify_frame(rx_buffer)) {
                    uint32_t raw_value = rx_buffer[2] | 
                                         (rx_buffer[3] << 8) | 
                                         (rx_buffer[4] << 16) | 
                                         (rx_buffer[5] << 24);
                    if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
                        waterLevel = raw_value;
                        xSemaphoreGive(xDataMutex);
                    }
                } else {
                    Serial.println("Frame error!\n");
                }

                rx_index = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ------------------- Web服务器任务 -------------------
void taskWebServer(void *pvParameters) {
    initWiFi(ssid, password);
    setupWebServer();
    while (1) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}