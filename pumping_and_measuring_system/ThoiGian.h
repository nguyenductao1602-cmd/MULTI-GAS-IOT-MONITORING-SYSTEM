#pragma once
#include <Arduino.h>

inline uint32_t doiSangMs(uint32_t value, uint32_t multiplier) {
  uint64_t result = (uint64_t)value * multiplier;
  return result > 2147483647ULL ? 2147483647UL : (uint32_t)result;
}

// Lich doc tach rieng voi lich bom. Khong doc don khi cam bien tra loi cham.
class LichDoc {
public:
  void batDau(uint32_t khoangMs) {
    khoangDoc = khoangMs == 0 ? 500 : doiSangMs(khoangMs, 1);
    lanDoc = millis() - khoangDoc; // Lan dau doc ngay
  }
  bool denLucDoc() const { return (uint32_t)(millis() - lanDoc) >= khoangDoc; }
  void batDauDoc() { lanDoc = millis(); }
  void ketThucDoc() {
    if ((uint32_t)(millis() - lanDoc) >= khoangDoc) lanDoc = millis();
  }
private:
  uint32_t khoangDoc = 500, lanDoc = 0;
};
