#line 1 "D:\\All_My_Data\\Arduino\\A_Dat_CO2\\pumping_and_measuring_system\\backup_truoc_tach_lop\\MeasurementTypes.h"
#pragma once
#include <Arduino.h>
#include <math.h>

// Tach kieu du lieu ra .h de Arduino sinh prototype dung.
enum PumpPhase { PUMP_STOPPED, PUMP_RUNNING, PUMP_SETTLING, PUMP_WAITING };

struct GasSensor {
  const char *name;
  uint8_t address;
  uint8_t type;
  bool ready = false;
  bool checked = false;
  uint32_t lastCheckTime = 0;
};
