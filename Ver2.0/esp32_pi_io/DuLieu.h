#pragma once
#include <Arduino.h>
#include <math.h>

// NAN nghia la khong co gia tri hop le trong luot doc nay.
struct DuLieuKhuVuc {
  float nhietDoAHT20 = NAN, doAmAHT20 = NAN;
  float nhietDoBMP280 = NAN, apSuatBMP280 = NAN;
  float nhietDoSCD30 = NAN, doAmSCD30 = NAN, co2SCD30 = NAN;
};

struct DuLieuKhi {
  const char *name;
  uint8_t address, type;
  float giaTri = NAN;
};

inline float giaTriHopLe(float value, float minimum, float maximum) {
  return isfinite(value) && value >= minimum && value <= maximum ? value : NAN;
}

