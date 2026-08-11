#include <HardwareSerial.h>
#include "serial.h"


// 校验和计算（与STM32完全一致）
uint8_t calculate_checksum(uint8_t *data, uint8_t len) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)(~sum);  // 取反
}

// 验证帧
bool verify_frame(uint8_t *buf) {
    // 1. 检查帧头帧尾
    if (buf[0] != FRAME_HEAD) {
        Serial.printf("Head error: expected 0xA5, got 0x%02X\n", buf[0]);
        return false;
    }
    if (buf[7] != FRAME_TAIL) {
        Serial.printf("Tail error: expected 0x12, got 0x%02X\n", buf[7]);
        return false;
    }
    
    // 2. 检查长度
    if (buf[1] != 4) {
        Serial.printf("Len error: expected 4, got %d\n", buf[1]);
        return false;
    }
    
    // 3. 验证校验和（对 payload：buf[1]~buf[5]，共5字节）
    uint8_t calc = calculate_checksum(&buf[1], 5);
    if (calc != buf[6]) {
        Serial.printf("Checksum error: calc=0x%02X, recv=0x%02X\n", calc, buf[6]);
        return false;
    }
    
    return true;
}