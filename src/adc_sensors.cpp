#include "adc_sensors.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Arduino.h>  // for constrain

ESP32AnalogRead adcWaterLevel;  // 虽然未用，保留以兼容原有声明
ESP32AnalogRead adcPH;
ESP32AnalogRead adcVoltage;
ESP32AnalogRead adcTDS;

// 电压校准偏移量 (加法)
static float voltageOffset = 0.0f;
static SemaphoreHandle_t offsetMutex = NULL;

void initADCSensors()
{
    if (offsetMutex == NULL) {
        offsetMutex = xSemaphoreCreateMutex();
    }
    adcPH.attach(PIN_PH);
    adcVoltage.attach(PIN_VOLTAGE);
    adcTDS.attach(PIN_TDS);
}

void setVoltageOffset(float offset)
{
    if (offsetMutex && xSemaphoreTake(offsetMutex, portMAX_DELAY) == pdTRUE) {
        voltageOffset = offset;
        xSemaphoreGive(offsetMutex);
    } else {
        voltageOffset = offset;
    }
}

float getVoltageOffset()
{
    float ret = 0.0f;
    if (offsetMutex && xSemaphoreTake(offsetMutex, portMAX_DELAY) == pdTRUE) {
        ret = voltageOffset;
        xSemaphoreGive(offsetMutex);
    } else {
        ret = voltageOffset;
    }
    return ret;
}

// ---------- 传感器读取实现 ----------

float readPH()
{
    float voltPH = adcPH.readVoltage();
    float pH = PH_SLOPE * voltPH + PH_INTERCEPT;
    return constrain(pH, 0.0f, 14.0f);
}

float readBatteryVoltage()
{
    float voltBattery = adcVoltage.readVoltage();
    float rawVoltage = voltBattery * VOLTAGE_DIVIDER;
    rawVoltage = constrain(rawVoltage, 0.0f, 12.0f);
    float offset = getVoltageOffset();
    float voltage = rawVoltage + offset;
    return constrain(voltage, 0.0f, 12.0f);
}

float readTDS(float waterTemp)
{
    // 直接读取一次 ADC 原始值 (12位分辨率)
    int raw = analogRead(PIN_TDS);   // 注意：如果 ESP32AnalogRead 提供了 readRaw() 可以用，否则直接用 analogRead
    // 转换为电压 (0~3.3V)
    float voltage = raw * 3.3f / 4096.0f;

    // 温度补偿系数 (默认 25°C 为基准)
    float coeff = 1.0f + 0.02f * (waterTemp - 25.0f);
    float compV = voltage / coeff;

    // 多项式拟合: ppm = (133.42 * V^3 - 255.86 * V^2 + 857.39 * V) * 0.5
    float tds = (133.42f * compV * compV * compV - 255.86f * compV * compV + 857.39f * compV) * 0.5f;
    return constrain(tds, 0.0f, 1000.0f);
}