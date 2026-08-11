#ifndef BUZZER_LED_H
#define BUZZER_LED_H

#include <Arduino.h>

// 引脚定义（可根据需要修改）
#define BUZZER_PIN      26   // 有源蜂鸣器（低电平响）
#define LED_PIN         27   // 报警LED（高电平亮）

// 初始化蜂鸣器和LED引脚
void initBuzzerLed();

// 蜂鸣器控制
void buzzerOn();
void buzzerOff();

// LED控制
void ledOn();
void ledOff();

// 单次短促声光报警（蜂鸣器+LED同步）
void beepOnce(int durationMs);

// 持续声光报警（用于水位过高）
void startContinuousAlarm();

// 停止持续声光报警
void stopContinuousAlarm();

#endif