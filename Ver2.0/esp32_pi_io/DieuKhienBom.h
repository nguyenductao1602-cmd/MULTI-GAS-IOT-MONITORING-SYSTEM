#pragma once
#include <driver/gpio.h>
#include "CauHinh.h"
// Chi thay doi relay khi Pi ra lenh, hoac khi bao ve mat lien lac.
class DieuKhienBom {
public:
  void khoiDong() {
    for (int i=0; i<3; ++i) {
      gpio_set_level((gpio_num_t)RELAY_PINS[i], RELAY_OFF);
      pinMode(RELAY_PINS[i], OUTPUT);
    }
    tatTatCa();
  }
  bool choPhep(int i) const { return i==0 ? SW1 : (i==1 ? SW2 : SW3); }
  bool dat(const bool (&chon)[3]) {
    // Kiem tra tat ca truoc, tranh chi bat duoc mot phan lenh.
    for (int i=0; i<3; ++i) if (chon[i] && !choPhep(i)) return false;
    for (int i=0; i<3; ++i) {
      trangThai[i]=chon[i];
      digitalWrite(RELAY_PINS[i], chon[i] ? RELAY_ON : RELAY_OFF);
    }
    return true;
  }
  void tatTatCa() {
    for (int i=0; i<3; ++i) { trangThai[i]=false; digitalWrite(RELAY_PINS[i],RELAY_OFF); }
  }
  bool dangBat() const { return trangThai[0] || trangThai[1] || trangThai[2]; }
  bool trangThai[3] = {false,false,false};
};
