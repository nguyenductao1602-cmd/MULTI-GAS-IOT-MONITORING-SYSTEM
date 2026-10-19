#pragma once
#include <driver/gpio.h>
#include "CauHinh.h"
#include "ThoiGian.h"

class DieuKhienBom {
public:
  void khoiDong() {
    for (int i = 0; i < 3; ++i) {
      gpio_set_level((gpio_num_t)RELAY_PINS[i], RELAY_OFF);
      pinMode(RELAY_PINS[i], OUTPUT);
    }
    tatTatCa();
    thoiGianBom = doiSangMs(PUMP_TIME_SECONDS, 1000);
    thoiGianOnDinh = doiSangMs(SETTLE_TIME_SECONDS, 1000);
    thoiGianNghi = doiSangMs(REPEAT_WAIT_SECONDS, 1000);
    if (thoiGianBom == 0 && thoiGianOnDinh == 0 && thoiGianNghi == 0) thoiGianNghi = 1000;
  }
  void batDau() {
    Serial.printf("[BOM CAU HINH] SW1=%u SW2=%u SW3=%u; muc BAT=%u, TAT=%u\n",
                  SW1, SW2, SW3, RELAY_ON, RELAY_OFF);
    tatTatCa();
    const bool suDung[3] = {SW1, SW2, SW3};
    for (int i = 0; i < 3; ++i) {
      if (suDung[i] && thoiGianBom > 0) digitalWrite(RELAY_PINS[i], RELAY_ON);
    }
    inChanRelay("BAT DAU CHU KY");
    trangThai = DANG_BOM;
    mocThoiGian = millis();
    Serial.printf("Bom: %s, thoi gian %lu ms\n", dangBat() ? "BAT" : "TAT", (unsigned long)thoiGianBom);
  }
  void capNhat() {
    if (trangThai == DUNG) return;
    // Chi toi da 3 buoc, ke ca khi thoi gian = 0.
    for (int step = 0; step < 3; ++step) {
      uint32_t now = millis();
      uint32_t daQua = now - mocThoiGian;
      if (trangThai == DANG_BOM && daQua >= thoiGianBom) {
        tatTatCa();
        inChanRelay("HET THOI GIAN BOM");
        trangThai = CHO_ON_DINH;
        mocThoiGian = now;
      } else if (trangThai == CHO_ON_DINH && daQua >= thoiGianOnDinh) {
        trangThai = CHO_CHU_KY;
        mocThoiGian = now;
      } else if (trangThai == CHO_CHU_KY && daQua >= thoiGianNghi) batDau();
      else break;
    }
  }
  bool choPhepDoc() const { return !READ_ONLY_AFTER_SETTLE || trangThai == CHO_CHU_KY; }
  bool dangBat() const { return trangThai == DANG_BOM && thoiGianBom > 0 && (SW1 || SW2 || SW3); }
private:
  enum TrangThai { DUNG, DANG_BOM, CHO_ON_DINH, CHO_CHU_KY };
  TrangThai trangThai = DUNG;
  uint32_t mocThoiGian = 0, thoiGianBom = 0, thoiGianOnDinh = 0, thoiGianNghi = 0;
  void inChanRelay(const char *suKien) const {
    Serial.printf("[BOM %s] IO15=%d IO16=%d IO17=%d (muc BAT=%u)\n",
                  suKien, digitalRead(RELAY_PINS[0]), digitalRead(RELAY_PINS[1]),
                  digitalRead(RELAY_PINS[2]), RELAY_ON);
  }
  void tatTatCa() {
    for (int i = 0; i < 3; ++i) digitalWrite(RELAY_PINS[i], RELAY_OFF);
  }
};
