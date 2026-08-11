#include "OLED.h"

// 使用硬件 I2C，构造函数根据你的屏幕分辨率选择
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

void updateOLED(float localWater, float localPH, float localTemp, float localVolt)
{
    u8g2.firstPage();
    do
    {
        // 第1行：水位
        u8g2.setCursor(0, 12);
        u8g2.print("Level: ");
        u8g2.print(localWater, 2);
        u8g2.print(" mm");

        // 第2行：pH值
        u8g2.setCursor(0, 28);
        u8g2.print("pH: ");
        u8g2.print(localPH, 2);

        // 第3行：水温
        u8g2.setCursor(0, 44);
        u8g2.print("Temp: ");
        u8g2.print(localTemp, 1);
        u8g2.print(" C");

        // 第4行：电压
        u8g2.setCursor(0, 60);
        u8g2.print("Voltage: ");
        u8g2.print(localVolt, 2);
        u8g2.print(" V");

    } while (u8g2.nextPage());
}

void initOLED()
{
    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_7x14_tf); // 选择合适字体
}

// 清空 OLED 显示内容（不关闭屏幕电源，仅清除图像）
void clearOLED()
{
    u8g2.clearBuffer();          // 清空内部缓冲区
    u8g2.sendBuffer();           // 将清空的缓冲区发送到屏幕，实现全黑显示
}