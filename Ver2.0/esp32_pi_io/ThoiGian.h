#pragma once
#include <Arduino.h>

inline uint32_t doiSangMs(uint32_t value, uint32_t multiplier) {
  uint64_t result = (uint64_t)value * multiplier;
  return result > 2147483647ULL ? 2147483647UL : (uint32_t)result;
}

