#ifndef WATER_LEVEL_H
#define WATER_LEVEL_H

#include <HardwareSerial.h>

// 帧格式定义
#define FRAME_HEAD 0xA5
#define FRAME_TAIL 0x12
#define FRAME_LEN 8

// 函数声明
uint8_t calculate_checksum(uint8_t* data, uint8_t len);
bool verify_frame(uint8_t *buf);

#endif  // WATER_LEVEL_H