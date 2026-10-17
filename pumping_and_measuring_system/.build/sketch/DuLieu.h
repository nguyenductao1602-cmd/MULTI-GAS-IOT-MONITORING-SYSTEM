#line 1 "D:\\All_My_Data\\Arduino\\A_Dat_CO2\\pumping_and_measuring_system\\DuLieu.h"
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
  bool ready = false, checked = false;
  uint32_t lastCheckTime = 0;
  float giaTri = NAN;
};

inline float giaTriHopLe(float value, float minimum, float maximum) {
  return isfinite(value) && value >= minimum && value <= maximum ? value : NAN;
}

inline void inGiaTri(const char *ten, float value, const char *donVi) {
  Serial.print(ten);
  if (isfinite(value)) Serial.printf(": %.2f %s\n", value, donVi);
  else Serial.println(": N/A (doc loi hoac chua co mau moi)");
}
