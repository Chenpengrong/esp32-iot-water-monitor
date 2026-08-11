#ifndef DS18B20_WRAPPER_H
#define DS18B20_WRAPPER_H

#include <OneWire.h>
#include <DallasTemperature.h>

// DS18B20数据引脚
#define ONE_WIRE_BUS 4

// 初始化DS18B20传感器
void initDS18B20();

// 读取水温（摄氏度）
float readWaterTemp();

#endif