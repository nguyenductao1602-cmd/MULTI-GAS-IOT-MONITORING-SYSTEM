#pragma once
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SCD30.h>
#include "CauHinh.h"
#include "DuLieu.h"

// Mot doi tuong dai dien cho mot khu vuc, co bus I2C rieng.
class KhuVucCamBien {
public:
  DuLieuKhuVuc duLieu;
  bool ahtKhoiDongOK() const { return ahtOK; }
  bool bmpKhoiDongOK() const { return bmpOK; }
  bool scdKhoiDongOK() const { return scdOK; }
  const char *loiSCD30 = "NOT_READ";
  KhuVucCamBien(const char *tenKhuVuc, TwoWire &duongI2C, bool dungSCD30)
    : ten(tenKhuVuc), bus(duongI2C), bmp(&duongI2C), coSCD30(dungSCD30) {}

  void khoiDong(void (*hamCapNhat)()) {
    phucVu = hamCapNhat;
    ahtOK = beginAHT20(bus);
    bmpOK = beginBMP280(bmp, diaChiBMP);
    if (coSCD30) {
      uint16_t chuKySCD30 = SCD30_MEASURE_TIME_SECONDS < 2 ? 2 :
                            (SCD30_MEASURE_TIME_SECONDS > 1800 ? 1800 : SCD30_MEASURE_TIME_SECONDS);
      scdOK = scd.begin(0x61, &bus) && scd.setMeasurementInterval(chuKySCD30);
      
    }
    
  }
  void doc(const char *camBien="ALL") {
    bool docHet=!strcmp(camBien,"ALL");
    bool docAHT=docHet || !strcmp(camBien,"AHT20");
    bool docBMP=docHet || !strcmp(camBien,"BMP280");
    bool docSCD=coSCD30 && (docHet || !strcmp(camBien,"SCD30"));
    if (docAHT) { duLieu.nhietDoAHT20=NAN; duLieu.doAmAHT20=NAN; }
    if (docBMP) { duLieu.nhietDoBMP280=NAN; duLieu.apSuatBMP280=NAN; }
    if (docSCD) { duLieu.nhietDoSCD30=NAN; duLieu.doAmSCD30=NAN; duLieu.co2SCD30=NAN; }
    if (docAHT && !ahtOK) ahtOK = beginAHT20(bus);
    if (docBMP && !bmpOK) bmpOK = beginBMP280(bmp, diaChiBMP);
    // Chi xoa va doc thiet bi Pi da chon; khong danh dau mau khac la moi.
    float t, h;
    if (docAHT && ahtOK && readAHT20(bus, t, h)) {
      duLieu.nhietDoAHT20 = giaTriHopLe(t, -40, 85);
      duLieu.doAmAHT20 = giaTriHopLe(h, 0, 100);
    }
    if (phucVu) phucVu();
    if (docBMP && bmpOK && i2cPresent(bus, diaChiBMP)) {
      float nhietDo = bmp.readTemperature();
      float apSuat = bmp.readPressure() / 100.0f;
      if (i2cPresent(bus, diaChiBMP)) {
        duLieu.nhietDoBMP280 = giaTriHopLe(nhietDo, -40, 85);
        duLieu.apSuatBMP280 = giaTriHopLe(apSuat, 300, 1100);
      }
    }
    if (phucVu) phucVu();
    if (docSCD) loiSCD30 = scdOK ? "NO_NEW_SAMPLE_OR_I2C_ERROR" : "INIT_FAILED";
    if (docSCD && scdOK && scd.dataReady() && scd.read()) {
      loiSCD30 = "OK";
      duLieu.nhietDoSCD30 = giaTriHopLe(scd.temperature, -40, 85);
      duLieu.doAmSCD30 = giaTriHopLe(scd.relative_humidity, 0, 100);
      duLieu.co2SCD30 = giaTriHopLe(scd.CO2, 0.01f, 40000);
    }
    if (phucVu) phucVu();
  }

private:
  const char *ten;
  TwoWire &bus;
  Adafruit_BMP280 bmp;
  Adafruit_SCD30 scd;
  bool coSCD30, ahtOK = false, bmpOK = false, scdOK = false;
  uint8_t diaChiBMP = 0x76;
  void (*phucVu)() = nullptr;
  void cho(uint32_t ms) {
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < ms) {
      if (phucVu) phucVu();
      delay(1);
    }
  }
  bool ahtCommand(TwoWire &bus, uint8_t command, bool withArguments) {
    bus.beginTransmission(0x38);
    bus.write(command);
    if (withArguments) {
      bus.write(command == 0xAC ? 0x33 : 0x08);
      bus.write(0x00);
    }
    return bus.endTransmission() == 0;
  }
  
  bool beginAHT20(TwoWire &bus) {
    if (!ahtCommand(bus, 0xBA, false)) return false;
    delay(20);
    if (!ahtCommand(bus, 0xBE, true)) return false;
    delay(10);
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < 200) {
      if (bus.requestFrom((uint8_t)0x38, (uint8_t)1) != 1) return false;
      uint8_t status = bus.read();
      if (!(status & 0x80)) return status & 0x08;
      delay(10);
    }
    return false;
  }
  
  bool readAHT20(TwoWire &bus, float &temperature, float &humidity) {
    if (!ahtCommand(bus, 0xAC, true)) return false;
    cho(80);
    uint8_t data[7];
    if (bus.requestFrom((uint8_t)0x38, (uint8_t)7) != 7) return false;
    for (int i = 0; i < 7; ++i) data[i] = bus.read();
    if ((data[0] & 0x80) || !(data[0] & 0x08)) return false;
    uint8_t crc = 0xFF;
    for (int i = 0; i < 6; ++i) {
      crc ^= data[i];
      for (int bit = 0; bit < 8; ++bit)
        crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    if (crc != data[6]) return false;
    uint32_t rawH = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    uint32_t rawT = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
    humidity = rawH * 100.0f / 1048576.0f;
    temperature = rawT * 200.0f / 1048576.0f - 50.0f;
    return true;
  }
  
  bool beginBMP280(Adafruit_BMP280 &sensor, uint8_t &address) {
    address = 0x76;
    if (!sensor.begin(address)) {
      address = 0x77;
      if (!sensor.begin(address)) return false;
    }
    sensor.setSampling(Adafruit_BMP280::MODE_NORMAL,
                       Adafruit_BMP280::SAMPLING_X2,
                       Adafruit_BMP280::SAMPLING_X16,
                       Adafruit_BMP280::FILTER_OFF,
                       Adafruit_BMP280::STANDBY_MS_250);
    return true;
  }
  
  bool i2cPresent(TwoWire &bus, uint8_t address) {
    bus.beginTransmission(address);
    return bus.endTransmission() == 0;
  }
};
