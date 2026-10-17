#line 1 "D:\\All_My_Data\\Arduino\\A_Dat_CO2\\kiem_tra_cam_bien\\CauHinh.h"
#pragma once
#include <Arduino.h>

// Project rieng: chi test va doi dia chi, tat ca bom luon TAT.
const uint8_t CHAN_RELAY[3] = {15, 16, 17};
const uint8_t RELAY_OFF = HIGH; // Relay hien tai kich LOW
const int SDA_MOI_TRUONG = 36, SCL_MOI_TRUONG = 38;
const int SDA_BUONG_KHI = 43, SCL_BUONG_KHI = 44;
const uint32_t I2C_CLOCK = 100000;
const uint16_t I2C_TIMEOUT_MS = 50; // Cho ca clock stretching cua SCD30
const int RS485_TX = 41, RS485_RX = 42, RS485_DE_RE = -1;
const uint32_t RS485_BAUD = 9600;
const uint32_t RS485_TIMEOUT_MS = 1000;
const uint32_t RS485_STARTUP_MS = 10000;
const uint32_t TEST_INTERVAL_MS = 2000;
const uint8_t LCD_ADDRESS = 0x3F; // Dia chi I2C 7-bit
const uint8_t LCD_CONTRAST = 160;
