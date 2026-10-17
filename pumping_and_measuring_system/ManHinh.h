#pragma once
#include <Wire.h>
#include <U8g2lib.h>
#include "CauHinh.h"
#include "DuLieu.h"
#include "ThoiGian.h"

// GM12864-59N VER2.0: ST7567S, bus Wire = MOI_TRUONG.
// Chi hien thi ban doc gan nhat, khong tu doc lai cam bien.
class ManHinh {
public:
  ManHinh() : lcd(U8G2_R0, U8X8_PIN_NONE, SCL1, SDA1) {}
  void khoiDong() {
    if (!SU_DUNG_LCD) return;
    Wire.beginTransmission(LCD_ADDRESS);
    sanSang = Wire.endTransmission() == 0;
    if (!sanSang) { Serial.printf("LCD khong phan hoi tai 0x%02X.\n", LCD_ADDRESS); return; }
    lcd.setI2CAddress(LCD_ADDRESS << 1); // U8g2 dung dia chi 8-bit
    lcd.setBusClock(I2C_CLOCK);
    lcd.begin();
    Wire.setTimeOut(I2C_TIMEOUT_MS);
    lcd.setContrast(LCD_CONTRAST);
    lcd.setFont(u8g2_font_6x10_tf);
    mocTrang = millis();
  }
  void capNhat(const DuLieuKhuVuc &moiTruong, const DuLieuKhuVuc &buongKhi,
              const DuLieuKhi (&khi)[4], bool bomBat, uint32_t soLanDoc, uint32_t lanDoc,
              uint32_t conChoMs, void (*phucVu)(), bool batBuoc = false) {
    if (!sanSang) return;
    uint32_t now = millis();
    if (!batBuoc && (uint32_t)(now - lanVe) < 1000) return;
    lanVe = now;
    uint32_t doiTrangMs = LCD_PAGE_TIME_MS == 0 ? 5000 : doiSangMs(LCD_PAGE_TIME_MS, 1);
    if ((uint32_t)(now - mocTrang) >= doiTrangMs) {
      trang = (trang + 1) % 4;
      mocTrang = now;
    }
    // Mat LCD thi dung gui, tranh hang loat timeout anh huong lich bom/doc.
    Wire.beginTransmission(LCD_ADDRESS);
    if (Wire.endTransmission() != 0) { sanSang = false; Serial.println("LCD mat ket noi."); return; }
    lcd.clearBuffer();
    if (conChoMs > 0) {
      dongChu(10, "DANG KHOI DONG");
      lcd.setCursor(0, 30);
      lcd.print("Con "); lcd.print((conChoMs + 999) / 1000); lcd.print(" giay");
      dongChu(50, "Tat ca bom dang tat");
    } else {
      if (trang == 0 || trang == 1) {
        const DuLieuKhuVuc &d = trang == 0 ? moiTruong : buongKhi;
        dongChu(10, trang == 0 ? "MOI_TRUONG" : "BUONG_KHI / AHT+BMP");
        dongSo(20, "AHT T", d.nhietDoAHT20, "C");
        dongSo(30, "AHT RH", d.doAmAHT20, "%");
        dongSo(40, "BMP T", d.nhietDoBMP280, "C");
        dongSo(50, "BMP P", d.apSuatBMP280, "hPa");
      } else if (trang == 2) {
        dongChu(10, "BUONG_KHI / SCD30");
        dongSo(20, "CO2", buongKhi.co2SCD30, "ppm");
        dongSo(30, "Nhiet do", buongKhi.nhietDoSCD30, "C");
        dongSo(40, "Do am", buongKhi.doAmSCD30, "%");
        dongChu(50, bomBat ? "BOM: BAT" : "BOM: TAT");
      } else {
        dongChu(10, "BUONG_KHI / GAS");
        for (int i = 0; i < 4; ++i) dongSo(20 + i * 10, khi[i].name, khi[i].giaTri, "ppm");
      }
      lcd.setCursor(0, 63); lcd.print(trang + 1); lcd.print("/4 ");
      if (soLanDoc == 0) lcd.print("Chua doc");
      else { lcd.print("Mau "); lcd.print((uint32_t)(now - lanDoc) / 1000); lcd.print("s"); }
    }
    // Gui tung hang 8 pixel, cap nhat bom giua cac hang.
    for (uint8_t hang = 0; hang < 8; ++hang) {
      lcd.updateDisplayArea(0, hang, 16, 1);
      if (phucVu) phucVu();
    }
  }
private:
  U8G2_ST7567_ENH_DG128064I_F_HW_I2C lcd;
  bool sanSang = false;
  uint8_t trang = 0;
  uint32_t mocTrang = 0, lanVe = 0;
  void dongChu(uint8_t y, const char *text) { lcd.drawStr(0, y, text); }
  void dongSo(uint8_t y, const char *ten, float value, const char *donVi) {
    lcd.setCursor(0, y); lcd.print(ten); lcd.print(": ");
    if (!isfinite(value)) lcd.print("N/A");
    else { lcd.print(value, 2); lcd.print(" "); lcd.print(donVi); }
  }
};
