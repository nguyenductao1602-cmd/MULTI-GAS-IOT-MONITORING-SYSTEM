#pragma once
#include <Arduino.h>
#if !ARDUINO_USB_CDC_ON_BOOT
#error "Serial phai qua USB. Bat USB CDC On Boot; ESP32-S3-USB-OTG: USB Mode = Hardware CDC and JTAG."
#endif
// Day la gioi han cho phep su dung. Pi van phai gui lenh moi bat relay.
const bool SW1 = false, SW2 = false, SW3 = true;
const uint8_t RELAY_PINS[3] = {15, 16, 17};
const uint8_t RELAY_ON = HIGH, RELAY_OFF = LOW;
// Pi gui PING moi 3s. Qua 15s khong co lenh hop le -> tat bom.
const uint32_t PI_TIMEOUT_MS = 15000;
const int SDA1 = 36, SCL1 = 38;
const int SDA2 = 43, SCL2 = 44; // Chi I2C, khong UART0
const uint32_t I2C_CLOCK = 100000;
const uint16_t I2C_TIMEOUT_MS = 50;
const uint32_t SCD30_MEASURE_TIME_SECONDS = 60; // Noi bo SCD30: 2..1800s
const bool SU_DUNG_LCD = true;
const uint8_t LCD_ADDRESS = 0x3F, LCD_CONTRAST = 255;
const uint32_t LCD_PAGE_TIME_MS = 5000;
const int RS485_TX = 41, RS485_RX = 42, RS485_DE_RE = -1;
const uint32_t RS485_TIMEOUT_MS = 1000, RS485_COMMAND_GAP_MS = 50;
const uint32_t RS485_STARTUP_MS = 10000;
const size_t MAX_COMMAND_BYTES = 384;
