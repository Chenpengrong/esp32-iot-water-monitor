#ifndef OLED_H
#define OLED_H

#include <Arduino.h>
#include <U8g2lib.h>

// 全局 U8G2 对象（已在 .cpp 中定义）
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

void initOLED();
void updateOLED(float localWater, float localPH, float localTemp, float localVolt);
void clearOLED();

#endif