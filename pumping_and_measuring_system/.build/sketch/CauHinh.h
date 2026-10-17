#line 1 "D:\\All_My_Data\\Arduino\\A_Dat_CO2\\pumping_and_measuring_system\\CauHinh.h"
#pragma once
#include <Arduino.h>

// ===== CHI CAN CHINH CAC GIA TRI TRONG FILE NAY =====
const bool SW1 = true, SW2 = true, SW3 = true;
const uint8_t RELAY_PINS[3] = {15, 16, 17};
const uint8_t RELAY_ON = LOW; // HIGH neu relay kich muc cao
const uint8_t RELAY_OFF = RELAY_ON == LOW ? HIGH : LOW;

// Giu cac thoi gian ban dang dung. Nhap so nguyen khong am.
const uint32_t DELAY_START_TIME_MINUTES = 1;
const uint32_t PUMP_TIME_SECONDS = 10;    // 0 = khong bat bom
const uint32_t SETTLE_TIME_SECONDS = 10;
const uint32_t REPEAT_WAIT_SECONDS = 60;
const uint32_t cycle_read_time = 60000; // ms: 60000 = 60 giay
const bool READ_ONLY_AFTER_SETTLE = false;

// Bus 1: MOI_TRUONG = AHT20 + BMP280 + LCD
const int SDA1 = 36, SCL1 = 38;
// Bus 2: BUONG_KHI = AHT20 + BMP280 + SCD30
const int SDA2 = 43, SCL2 = 44;
const uint32_t I2C_CLOCK = 100000;
const uint16_t I2C_TIMEOUT_MS = 20;

// LCD GM12864-59N VER2.0, ST7567S, I2C 7-bit
const bool SU_DUNG_LCD = true;
const uint8_t LCD_ADDRESS = 0x3F; // Sua theo I2C scanner neu module khac
const uint8_t LCD_CONTRAST = 160;
const uint32_t LCD_PAGE_TIME_MS = 5000; // Doi trang moi 5 giay

// ESP TX41 -> TXD, RX42 <- RXD cua TD301D485H-A
const int RS485_TX = 41, RS485_RX = 42;
const int RS485_DE_RE = -1; // TD301D485H-A tu dong chuyen huong
const bool RS485_INVERT_RX = false;
const uint32_t RS485_TIMEOUT_MS = 1000; // Giong project test
const uint32_t RS485_COMMAND_GAP_MS = 50; // Nghi giua hai lenh
const bool RS485_DEBUG = false; // true: in ca TX/RX thanh cong; loi luon duoc in
const uint32_t RS485_CONFIG_TIMEOUT_MS = 1000;
const uint32_t RS485_CONFIG_STARTUP_MS = 10000;

// true: CHI noi MOT module; bom luon tat. false: doc binh thuong.
#ifndef CHE_DO_DOI_DIA_CHI
#define CHE_DO_DOI_DIA_CHI 0
#endif
const bool CONFIGURE_ADDRESS_MODE = CHE_DO_DOI_DIA_CHI;
const uint8_t OLD_ADDRESS = 1, NEW_ADDRESS = 1;
const uint8_t CONFIGURE_GAS_TYPE = 0x29;
// CH3SH: dia chi 1 / ma 0x29; CO: 2 / 0x02; H2S: 3 / 0x09; NH3: 4 / 0x0B.
