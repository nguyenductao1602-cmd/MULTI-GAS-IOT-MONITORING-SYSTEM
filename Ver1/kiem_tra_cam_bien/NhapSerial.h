#pragma once
#include <Arduino.h>
#include <ctype.h>
#include <string.h>

// Dia chi: he 10. Ma cam bien: HEX (29 hoac 0x29 deu la 0x29).
inline bool docSo(const char *text, uint8_t base, uint8_t minimum, uint8_t &value) {
  if (base == 16 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
  if (!*text) return false;
  uint16_t number = 0;
  while (*text) {
    char c = tolower((unsigned char)*text++);
    int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
    if (digit < 0 || digit >= base) return false;
    number = number * base + digit;
    if (number > 255) return false;
  }
  if (number < minimum) return false;
  value = number;
  return true;
}

class NhapSerial {
public:
  bool daThoat = false;
  // Khong dung readStringUntil: Serial khong bi cho timeout.
  bool docDong(char *out) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r' || c == '\n') {
        if (quaDai) {
          Serial.println("[LOI NHAP] Dong qua dai. Hay nhap lai.");
          soKyTu = 0; quaDai = false;
          continue;
        }
        if (soKyTu == 0) continue; // Bo dong rong va LF sau CR
        boDem[soKyTu] = '\0';
        soKyTu = 0;
        char *dau = boDem;
        while (isspace((unsigned char)*dau)) ++dau;
        char *cuoi = dau + strlen(dau);
        while (cuoi > dau && isspace((unsigned char)cuoi[-1])) --cuoi;
        *cuoi = '\0';
        if (!*dau) continue;
        for (char *p = dau; *p; ++p) *p = tolower((unsigned char)*p);
        strcpy(out, dau);
        return true;
      }
      if (c == '\b' || c == 127) { if (soKyTu && !quaDai) --soKyTu; continue; }
      if (quaDai) continue;
      if (soKyTu >= sizeof(boDem) - 1) { quaDai = true; continue; }
      boDem[soKyTu++] = c;
    }
    return false;
  }
  // Trong mot giao dich chi nhan exit; khong tu dong xu ly lenh nhap som.
  bool kiemTraThoat() {
    char line[48];
    while (docDong(line)) {
      if (strcmp(line, "exit") == 0) daThoat = true;
      else Serial.println("[DANG XU LY] Chi nhan exit. Hay doi loi nhac roi nhap.");
    }
    return daThoat;
  }
  bool cho(uint32_t ms) {
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < ms) {
      if (kiemTraThoat()) return false;
      delay(1);
    }
    return !kiemTraThoat();
  }
private:
  char boDem[48] = {};
  uint8_t soKyTu = 0;
  bool quaDai = false;
};
