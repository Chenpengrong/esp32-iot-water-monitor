#ifndef ADC_SENSORS_H
#define ADC_SENSORS_H

#include <ESP32AnalogRead.h>

// 引脚定义
#define PIN_PH           39
#define PIN_VOLTAGE      34
#define PIN_TDS          35

// pH 换算系数
#define PH_SLOPE     -5.7541f
#define PH_INTERCEPT 16.654f

// 电压分压比（假设电阻分压 5:1，实际根据硬件调整）
#define VOLTAGE_DIVIDER  5.0f

// TDS 换算参数
#define TDS_VOLT_MIN     0.0f
#define TDS_VOLT_MAX     3.3f
#define TDS_PPM_MIN      0.0f
#define TDS_PPM_MAX      1000.0f

extern ESP32AnalogRead adcPH;
extern ESP32AnalogRead adcVoltage;
extern ESP32AnalogRead adcTDS;

void initADCSensors();

// 传感器读取函数
float readPH();                         // 返回 pH 值 (0~14)
float readBatteryVoltage();             // 返回校准后的电池电压 (0~12V)
float readTDS(float waterTemp);         // 返回 TDS 值 (ppm)，需要传入当前水温用于补偿

// 电压校准偏移量管理
void setVoltageOffset(float offset);
float getVoltageOffset();

#endif