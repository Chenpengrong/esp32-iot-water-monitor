#include "buzzer_led.h"

void initBuzzerLed() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    buzzerOff();
    ledOff();
}

void buzzerOn() {
    digitalWrite(BUZZER_PIN, LOW);   // 低电平响
}

void buzzerOff() {
    digitalWrite(BUZZER_PIN, HIGH);  // 高电平静音
}

void ledOn() {
    digitalWrite(LED_PIN, HIGH);
}

void ledOff() {
    digitalWrite(LED_PIN, LOW);
}

void beepOnce(int durationMs) {
    buzzerOn();
    ledOn();
    delay(durationMs);
    buzzerOff();
    ledOff();
}

void startContinuousAlarm() {
    buzzerOn();
    ledOn();
}

void stopContinuousAlarm() {
    buzzerOff();
    ledOff();
}