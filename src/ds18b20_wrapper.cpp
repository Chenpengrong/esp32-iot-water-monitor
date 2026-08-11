#include "ds18b20_wrapper.h"

static OneWire oneWire(ONE_WIRE_BUS);
static DallasTemperature sensors(&oneWire);

void initDS18B20()
{
    sensors.begin();
}

float readWaterTemp()
{
    sensors.requestTemperatures();          // 发送转换命令
    float temp = sensors.getTempCByIndex(0); // 读取第一个传感器
    if (temp == DEVICE_DISCONNECTED_C) 
    {
        Serial.println("DS18B20 未连接");
        return -127.0f;   // 错误值
    }
    return temp;
}