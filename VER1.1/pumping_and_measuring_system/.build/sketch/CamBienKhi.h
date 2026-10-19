#line 1 "D:\\All_My_Data\\Arduino\\A_Dat_CO2\\pumping_and_measuring_system\\CamBienKhi.h"
#pragma once
#include <driver/uart.h>
#include "CauHinh.h"
#include "DuLieu.h"

// Bus UART1/RS485: CH3SH thay cho SO2 tai dia chi 1.
class HeCamBienKhi {
public:
  DuLieuKhi duLieu[4] = {
    {"CH3SH", 1, 0x29}, {"CO", 2, 0x02}, {"H2S", 3, 0x09}, {"NH3", 4, 0x0B}
  };
  explicit HeCamBienKhi(HardwareSerial &congUART) : uart(congUART) {}
  bool khoiDong(void (*hamCapNhat)()) {
    phucVu = hamCapNhat;
    if (RS485_DE_RE >= 0) {
      digitalWrite(RS485_DE_RE, LOW);
      pinMode(RS485_DE_RE, OUTPUT);
    }
    uart.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
    return uart_set_line_inverse(UART_NUM_1, RS485_INVERT_RX ? UART_SIGNAL_RXD_INV : 0) == ESP_OK;
  }
  void doc() {
    for (auto &khi : duLieu) {
      if (phucVu) phucVu();
      if (!khi.ready && (!khi.checked || (uint32_t)(millis() - khi.lastCheckTime) >= 10000)) {
        khi.ready = identifyGas(khi.address, khi.type);
        khi.checked = true;
        khi.lastCheckTime = millis();
      }
      khi.giaTri = NAN;
      float value;
      if (khi.ready && readGas(khi, value)) khi.giaTri = giaTriHopLe(value, -65535.99f, 65535.99f);
    }
  }
  void inSerial() const {
    for (const auto &khi : duLieu) {
      Serial.printf("BUONG_KHI / %s / dia chi %u\n", khi.name, khi.address);
      inGiaTri("  Nong do", khi.giaTri, "ppm");
    }
  }
  void doiDiaChi() {
    Serial.println("CHE DO DOI DIA CHI: chi noi MOT module, tat ca relay dang TAT.");
    Serial.printf("UART: TX=IO%d, RX=IO%d, 9600 8N1, DE_RE=%d\n", RS485_TX, RS485_RX, RS485_DE_RE);
    Serial.printf("Thu dao rieng RX: %s (TX khong dao).\n", RS485_INVERT_RX ? "BAT" : "TAT");
    Serial.printf("Cho module khoi dong %lu ms...\n", (unsigned long)RS485_CONFIG_STARTUP_MS);
    delay(RS485_CONFIG_STARTUP_MS);
    Serial.printf("Muc dien tai IO%d truoc khi gui: %d (0=LOW, 1=HIGH).\n",
                  RS485_RX, digitalRead(RS485_RX));
    if (NEW_ADDRESS == 0 || !identifyGas(OLD_ADDRESS, CONFIGURE_GAS_TYPE)) {
      Serial.println("Dung: dia chi/loai cam bien khong hop le.");
      Serial.println("CHUA gui lenh doi dia chi. Kiem tra RX raw, nguon, TX/RX, A/B va DE/RE.");
      return;
    }
    if (OLD_ADDRESS == NEW_ADDRESS) {
      Serial.println("Dia chi da dung, khong can doi.");
      return;
    }
    uint8_t reply[7];
    bool acknowledged = gasTransaction(0x04, NEW_ADDRESS, reply, sizeof(reply)) && reply[3] == 0x10;
    delay(200);
    bool verified = identifyGas(NEW_ADDRESS, CONFIGURE_GAS_TYPE);
    if (acknowledged && verified)
      Serial.println("Doi dia chi thanh cong. Tat nguon, cap lai va kiem tra dia chi duoc luu.");
    else if (verified)
      Serial.println("Doc duoc dia chi moi, nhung thieu ACK. Cap lai nguon de kiem tra luu dia chi.");
    else
      Serial.println("Chua xac minh duoc dia chi moi. Kiem tra day/module truoc khi noi chung bus.");
    Serial.println("Sau khi xong: CONFIGURE_ADDRESS_MODE=false, nap lai chuong trinh.");
  }
private:
  HardwareSerial &uart;
  void (*phucVu)() = nullptr;
  void cho(uint32_t ms) {
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < ms) {
      if (phucVu) phucVu();
      delay(1);
    }
  }
  uint16_t crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
      crc ^= data[i];
      for (int bit = 0; bit < 8; ++bit)
        crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
  }
  
  bool validFrame(const uint8_t *frame, size_t length) {
    if (length < 6 || frame[0] != 0xAA || frame[length - 1] != 0xEE) return false;
    uint16_t crc = crc16(frame + 1, length - 4);
    return frame[length - 3] == (uint8_t)crc && frame[length - 2] == (uint8_t)(crc >> 8);
  }
  
  void printHex(const char *label, const uint8_t *data, size_t length) {
    Serial.print(label);
    for (size_t i = 0; i < length; ++i) Serial.printf("%02X ", data[i]);
    Serial.println();
  }
  
  bool gasTransaction(uint8_t command, uint8_t address, uint8_t *reply, size_t length) {
    uint8_t request[6] = {0xAA, command, address, 0, 0, 0xEE};
    uint16_t crc = crc16(request + 1, 2);
    request[3] = crc & 0xFF;
    request[4] = crc >> 8;
    const bool debug = CONFIGURE_ADDRESS_MODE || RS485_DEBUG;
    const uint32_t timeoutMs = CONFIGURE_ADDRESS_MODE ? RS485_CONFIG_TIMEOUT_MS : RS485_TIMEOUT_MS;
    uint8_t raw[64];
    size_t rawCount = 0, totalCount = 0, badFrames = 0;
    if (debug) printHex("TX: ", request, sizeof(request));
    cho(RS485_COMMAND_GAP_MS);
    uint32_t donBatDau = millis();
    while (uart.available() && (uint32_t)(millis() - donBatDau) < 20) uart.read();
    cho(4);
    if (RS485_DE_RE >= 0) {
      digitalWrite(RS485_DE_RE, HIGH);
      delayMicroseconds(100);
    }
    uart.write(request, sizeof(request));
    uart.flush();
    if (RS485_DE_RE >= 0) digitalWrite(RS485_DE_RE, LOW);
    if (debug) Serial.printf("RX FIFO ngay sau khi TX xong: %d byte.\n", uart.available());
    size_t received = 0;
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < timeoutMs) {
      if (phucVu) phucVu();
      if (!uart.available()) { delay(1); continue; }
      uint8_t byteRead = uart.read();
      ++totalCount;
      if (rawCount < sizeof(raw)) raw[rawCount++] = byteRead;
      if (received == 0 && byteRead != 0xAA) continue;
      reply[received++] = byteRead;
      if (received == length) {
        if (validFrame(reply, length) && reply[1] == command && reply[2] == address) {
          if (debug) {
            printHex("RX raw: ", raw, rawCount);
            Serial.println("RX: dung khung, dung dia chi, CRC OK.");
          }
          return true;
        }
        ++badFrames;
        // Bo khung sai hoac echo, tim khung tiep theo bang cua so truot.
        size_t next = 1;
        while (next < length && reply[next] != 0xAA) ++next;
        received = length - next;
        memmove(reply, reply + next, received);
      }
    }
    {
      Serial.printf("[RS485 LOI] Dia chi=%u, lenh=0x%02X, cho=%lu ms\n",
                    address, command, (unsigned long)timeoutMs);
      if (totalCount == 0) Serial.println("RX: KHONG NHAN DUOC BYTE NAO.");
      else {
        printHex("RX raw (toi da 64 byte): ", raw, rawCount);
        Serial.printf("RX: %u byte, %u khung sai CRC/lenh/dia chi, %u byte khung dang do.\n",
                      (unsigned)totalCount, (unsigned)badFrames, (unsigned)received);
      }
    }
    return false;
  }
  
  bool identifyGas(uint8_t address, uint8_t expectedType) {
    uint8_t reply[16];
    if (!gasTransaction(0x0F, address, reply, sizeof(reply))) {
      Serial.printf("RS485 dia chi %u: khong co phan hoi hop le.\n", address);
      return false;
    }
    Serial.printf("RS485 dia chi %u: gas type=0x%02X, unit=0x%02X, range=%u\n",
                  address, reply[3], reply[12], ((uint16_t)reply[4] << 8) | reply[5]);
    if (reply[3] != expectedType || reply[12] != 0x02) {
      Serial.printf("[RS485 SAI LOAI/DON VI] Dia chi %u: can ma=0x%02X, ppm=0x02; nhan ma=0x%02X, don vi=0x%02X.\n", address, expectedType, reply[3], reply[12]);
      return false;
    }
    return true;
  }
  
  bool readGas(DuLieuKhi &sensor, float &value) {
    uint8_t reply[10];
    if (!gasTransaction(0x01, sensor.address, reply, sizeof(reply))) return false;
    if ((reply[3] != 0x00 && reply[3] != 0x80) || reply[6] > 99) {
      Serial.printf("[RS485 DATA LOI] Dia chi %u: dau=0x%02X, phan le=%u\n", sensor.address, reply[3], reply[6]);
      return false;
    }
    value = (((uint16_t)reply[4] << 8) | reply[5]) + reply[6] / 100.0f;
    if (reply[3] == 0x80) value = -value;
    return true;
  }
  
};
