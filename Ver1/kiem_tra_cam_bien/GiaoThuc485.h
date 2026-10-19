#pragma once
#include <driver/uart.h>
#include <math.h>
#include "CauHinh.h"
#include "NhapSerial.h"

struct ThongTinKhi {
  uint8_t diaChi = 0, maLoai = 0, donVi = 0;
  uint16_t daiDo = 0;
};
inline const char *tenKhi(uint8_t ma) {
  static const char *names[] = {
    "None", "EX", "CO", "O2", "H2", "CH4", "C3H8", "CO2", "O3", "H2S", "SO2", "NH3",
    "Cl2", "ETO", "HCl", "PH3", "HBr", "HCN", "AsH3", "HF", "Br2", "NO", "NO2", "NOX",
    "ClO2", "SiH4", "CS2", "F2", "B2H6", "GeH4", "N2", "THT", "C2H2", "C2H4", "CH2O", "LPG", "HC", "C6H6", "H2O2"
  };
  if (ma == 0x29) return "CH3SH"; // Da xac minh tu module thuc te
  return ma < sizeof(names) / sizeof(names[0]) ? names[ma] : "CHUA BIET";
}
inline const char *tenDonVi(uint8_t ma) {
  switch (ma) {
    case 0: return "%LEL"; case 1: return "%VOL"; case 2: return "ppm"; case 3: return "ppb";
    default: return "don vi chua ro";
  }
}
inline uint16_t crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}
inline bool khungHopLe(const uint8_t *frame, size_t length) {
  if (length < 6 || frame[0] != 0xAA || frame[length - 1] != 0xEE) return false;
  uint16_t crc = crc16(frame + 1, length - 4);
  return frame[length - 3] == (uint8_t)crc && frame[length - 2] == (uint8_t)(crc >> 8);
}
inline bool giaiMaNongDo(const uint8_t *frame, float &value) {
  if ((frame[3] != 0 && frame[3] != 0x80) || frame[6] > 99) return false;
  value = (((uint16_t)frame[4] << 8) | frame[5]) + frame[6] / 100.0f;
  if (frame[3] == 0x80) value = -value;
  return true;
}

class GiaoThuc485 {
public:
  bool sanSang = false, daGuiLenhDoi = false;
  GiaoThuc485(HardwareSerial &cong, NhapSerial &nhap) : uart(cong), NHAP(nhap) {}
  void khoiDong() {
    if (RS485_DE_RE >= 0) { digitalWrite(RS485_DE_RE, LOW); pinMode(RS485_DE_RE, OUTPUT); }
    uart.begin(RS485_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);
    sanSang = uart_set_line_inverse(UART_NUM_1, 0) == ESP_OK;
    lucBatDau = millis();
  }
  bool choKhoiDong() {
    if (!sanSang) { Serial.println("[LOI UART] Khoi tao UART1 that bai."); return false; }
    uint32_t elapsed = millis() - lucBatDau;
    if (elapsed >= RS485_STARTUP_MS) return !NHAP.kiemTraThoat();
    Serial.println("Dang cho module RS485 khoi dong (toi da 10s); exit de huy.");
    return NHAP.cho(RS485_STARTUP_MS - elapsed);
  }
  bool docThongTin(uint8_t diaChi, ThongTinKhi &info) {
    uint8_t data[16];
    if (!giaoDich(0x0F, diaChi, data, sizeof(data))) return false;
    info.diaChi = data[2]; info.maLoai = data[3]; info.donVi = data[12];
    info.daiDo = ((uint16_t)data[4] << 8) | data[5];
    Serial.printf("[INFO] Dia chi=%u | Ma=0x%02X | Loai=%s | Dai do=%u %s | CRC OK\n",
                  info.diaChi, info.maLoai, tenKhi(info.maLoai), info.daiDo, tenDonVi(info.donVi));
    return true;
  }
  bool docGiaTri(uint8_t diaChi, float &value) {
    uint8_t data[10];
    if (!giaoDich(0x01, diaChi, data, sizeof(data))) return false;
    if (data[2] != diaChi) { Serial.printf("[SAI DIA CHI] Nhan tu %u, dang hoi %u.\n", data[2], diaChi); return false; }
    if (!giaiMaNongDo(data, value)) { Serial.println("[LOI DATA] Sai byte dau hoac phan thap phan."); return false; }
    return true;
  }
  bool doiDiaChi(uint8_t diaChiMoi) {
    uint8_t data[7];
    return giaoDich(0x04, diaChiMoi, data, sizeof(data)) && data[2] == diaChiMoi && data[3] == 0x10;
  }
private:
  HardwareSerial &uart;
  NhapSerial &NHAP;
  uint32_t lucBatDau = 0;
  void inHex(const char *label, const uint8_t *data, size_t count) {
    Serial.print(label);
    for (size_t i = 0; i < count; ++i) Serial.printf("%02X ", data[i]);
    Serial.println();
  }
  bool giaoDich(uint8_t command, uint8_t address, uint8_t *reply, size_t length) {
    if (!sanSang || NHAP.kiemTraThoat()) return false;
    uint8_t request[6] = {0xAA, command, address, 0, 0, 0xEE};
    uint16_t crc = crc16(request + 1, 2);
    request[3] = crc & 0xFF; request[4] = crc >> 8;
    // Don du lieu cu co gioi han, khong bi ket neu bus nhieu lien tuc.
    uint32_t start = millis();
    while (uart.available() && (uint32_t)(millis() - start) < 20) uart.read();
    if (!NHAP.cho(4)) return false;
    inHex("TX: ", request, sizeof(request));
    if (NHAP.kiemTraThoat()) return false; // exit truoc thoi diem ghi thi huy
    if (RS485_DE_RE >= 0) { digitalWrite(RS485_DE_RE, HIGH); delayMicroseconds(100); }
    uart.write(request, sizeof(request));
    if (command == 0x04) daGuiLenhDoi = true;
    uart.flush();
    if (RS485_DE_RE >= 0) digitalWrite(RS485_DE_RE, LOW);
    uint8_t raw[64]; size_t rawCount = 0, received = 0, total = 0, badCRC = 0, wrongCommand = 0;
    start = millis();
    while ((uint32_t)(millis() - start) < RS485_TIMEOUT_MS) {
      if (NHAP.kiemTraThoat()) return false;
      if (!uart.available()) { delay(1); continue; }
      uint8_t b = uart.read(); ++total;
      if (rawCount < sizeof(raw)) raw[rawCount++] = b;
      if (received == 0 && b != 0xAA) continue;
      reply[received++] = b;
      if (received != length) continue;
      bool valid = khungHopLe(reply, length);
      if (valid && reply[1] == command) {
        inHex("RX raw: ", raw, rawCount);
        // Giu dia chi thuc trong khung de menu bao sai dia chi neu can.
        return true;
      }
      if (valid) ++wrongCommand;
      else if (reply[length - 1] == 0xEE) ++badCRC;
      size_t next = 1;
      while (next < length && reply[next] != 0xAA) ++next;
      received = length - next;
      memmove(reply, reply + next, received);
    }
    if (total == 0) Serial.println("[KHONG PHAN HOI] Khong nhan duoc byte nao.");
    else {
      inHex("RX raw (toi da 64 byte): ", raw, rawCount);
      Serial.printf("[KHUNG LOI] %u byte, CRC sai=%u, sai lenh=%u, khung dang do=%u.\n",
                    (unsigned)total, (unsigned)badCRC, (unsigned)wrongCommand, (unsigned)received);
    }
    return false;
  }
};
