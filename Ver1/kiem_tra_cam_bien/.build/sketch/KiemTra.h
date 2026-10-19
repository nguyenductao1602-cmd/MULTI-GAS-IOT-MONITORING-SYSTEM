#line 1 "D:\\All_My_Data\\Arduino\\A_Dat_CO2\\kiem_tra_cam_bien\\KiemTra.h"
#pragma once
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <U8g2lib.h>
#include "GiaoThuc485.h"

inline uint8_t crc8(const uint8_t *data, size_t count) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < count; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}
inline float soThucSCD30(const uint8_t *data) {
  uint32_t bits = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[3] << 8) | data[4];
  float value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}
inline bool trongKhoang(float value, float minValue, float maxValue) {
  return isfinite(value) && value >= minValue && value <= maxValue;
}

class KiemTra {
public:
  bool busMoiTruongOK = false, busBuongKhiOK = false;
  KiemTra(TwoWire &buongKhi, GiaoThuc485 &rs, NhapSerial &nhap)
    : I2C_BUONG_KHI(buongKhi), RS485(rs), NHAP(nhap), bmpMoiTruong(&Wire), bmpBuongKhi(&buongKhi),
      lcd(U8G2_R0, U8X8_PIN_NONE, SCL_MOI_TRUONG, SDA_MOI_TRUONG) {}
  void khoiDong() {
    busMoiTruongOK = Wire.begin(SDA_MOI_TRUONG, SCL_MOI_TRUONG, I2C_CLOCK);
    busBuongKhiOK = I2C_BUONG_KHI.begin(SDA_BUONG_KHI, SCL_BUONG_KHI, I2C_CLOCK);
    Wire.setTimeOut(I2C_TIMEOUT_MS); I2C_BUONG_KHI.setTimeOut(I2C_TIMEOUT_MS);
  }
  void chon(uint8_t muc, uint8_t diaChiTuChon = 1) {
    luaChon = muc; diaChiKhi = diaChiTuChon;
    daKhoiTao = false; soLan = 0; lanTest = millis() - TEST_INTERVAL_MS;
    Serial.println("Bat dau test rieng thiet bi da chon. exit -> menu chinh; list -> danh sach test.");
  }
  void dung() { luaChon = 0; }
  bool dangTestLCD() const { return luaChon == 10; }
  bool lcdCoPhanHoi() const { return luaChon == 10 && daKhoiTao; }
  void capNhat() {
    if (luaChon == 0 || NHAP.daThoat || (uint32_t)(millis() - lanTest) < TEST_INTERVAL_MS) return;
    Serial.printf("\n--- TEST %u / LAN %lu ---\n", luaChon, (unsigned long)++soLan);
    switch (luaChon) {
      case 1: testAHT(Wire, busMoiTruongOK, "MOI_TRUONG"); break;
      case 2: testBMP(Wire, bmpMoiTruong, busMoiTruongOK, "MOI_TRUONG"); break;
      case 3: testAHT(I2C_BUONG_KHI, busBuongKhiOK, "BUONG_KHI"); break;
      case 4: testBMP(I2C_BUONG_KHI, bmpBuongKhi, busBuongKhiOK, "BUONG_KHI"); break;
      case 5: testSCD30(); break;
      case 6: testKhi(1, 0x29); break;
      case 7: testKhi(2, 0x02); break;
      case 8: testKhi(3, 0x09); break;
      case 9: testKhi(4, 0x0B); break;
      case 10: testLCD(); break;
      case 13: testKhi(diaChiKhi, 0, false); break;
    }
    lanTest = millis(); // Luot cham khong bi doc don
  }
  void quetI2C(bool trongBuong) {
    TwoWire &bus = trongBuong ? I2C_BUONG_KHI : Wire;
    bool ready = trongBuong ? busBuongKhiOK : busMoiTruongOK;
    if (!kiemTraBus(ready)) return;
    int sda = trongBuong ? SDA_BUONG_KHI : SDA_MOI_TRUONG;
    int scl = trongBuong ? SCL_BUONG_KHI : SCL_MOI_TRUONG;
    Serial.printf("QUET %s: SDA=IO%d, SCL=IO%d | muc SDA=%d, SCL=%d\n",
                  trongBuong ? "BUONG_KHI" : "MOI_TRUONG", sda, scl, digitalRead(sda), digitalRead(scl));
    Serial.println("Dia chi hien thi o dang HEX 7-bit. ACK chi xac nhan co thiet bi, chua xac nhan loai.");
    uint8_t found = 0, busErrors = 0;
    for (uint8_t addr = 1; addr < 127; ++addr) {
      if (NHAP.kiemTraThoat()) return;
      bus.beginTransmission(addr);
      uint8_t error = bus.endTransmission();
      if (error == 0) { ++found; Serial.printf("  [ACK] 0x%02X\n", addr); }
      else if (error != 2 && error != 3) ++busErrors;
    }
    Serial.printf("Tim thay %u dia chi; loi bus/timeout=%u.\n", found, busErrors);
    Serial.println("Mong doi: AHT20=0x38, BMP280=0x76/0x77, SCD30=0x61, LCD=0x3F.");
    if (!found) Serial.println("[LOI] Khong co ACK. Kiem tra nguon, GND, SDA/SCL, pull-up va dung bus.");
  }
private:
  TwoWire &I2C_BUONG_KHI;
  GiaoThuc485 &RS485;
  NhapSerial &NHAP;
  Adafruit_BMP280 bmpMoiTruong, bmpBuongKhi;
  U8G2_ST7567_ENH_DG128064I_F_HW_I2C lcd;
  uint8_t luaChon = 0, diaChiKhi = 1, diaChiBMP = 0x76;
  bool daKhoiTao = false;
  uint32_t lanTest = 0, soLan = 0, lanCoMauSCD = 0;
  bool kiemTraBus(bool ready) {
    if (!ready) Serial.println("[LOI BUS] Wire.begin that bai. Kiem tra cau hinh chan, reset de thu lai.");
    return ready;
  }
  bool ack(TwoWire &bus, uint8_t addr, bool inLoi = true) {
    bus.beginTransmission(addr);
    uint8_t error = bus.endTransmission();
    if (error && inLoi) Serial.printf("[LOI I2C] Dia chi 0x%02X khong ACK, ma Wire=%u (2=NACK, 5=timeout).\n", addr, error);
    return error == 0;
  }
  bool gui(TwoWire &bus, uint8_t addr, const uint8_t *data, uint8_t count) {
    bus.beginTransmission(addr); bus.write(data, count);
    uint8_t error = bus.endTransmission();
    if (error) Serial.printf("[LOI I2C] Gui 0x%02X that bai, ma Wire=%u.\n", addr, error);
    return error == 0;
  }
  bool nhan(TwoWire &bus, uint8_t addr, uint8_t *data, uint8_t count) {
    size_t got = bus.requestFrom(addr, count);
    if (got != count) {
      while (bus.available()) bus.read();
      Serial.printf("[LOI I2C] 0x%02X can %u byte, chi nhan %u.\n", addr, count, (unsigned)got);
      return false;
    }
    for (uint8_t i = 0; i < count; ++i) data[i] = bus.read();
    return true;
  }
  void testAHT(TwoWire &bus, bool ready, const char *ten) {
    Serial.printf("%s / AHT20 / 0x38\n", ten);
    if (!kiemTraBus(ready) || !ack(bus, 0x38) || NHAP.kiemTraThoat()) return;
    uint8_t status;
    if (!nhan(bus, 0x38, &status, 1)) return;
    Serial.printf("Status=0x%02X (BUSY=%u, calibrated=%u)\n", status, !!(status & 0x80), !!(status & 0x08));
    if (!(status & 0x08)) {
      const uint8_t init[] = {0xBE, 0x08, 0x00};
      if (!gui(bus, 0x38, init, sizeof(init)) || !NHAP.cho(10)) return;
    }
    const uint8_t command[] = {0xAC, 0x33, 0x00};
    if (!gui(bus, 0x38, command, sizeof(command)) || !NHAP.cho(80)) return;
    uint32_t start = millis();
    do {
      if (!nhan(bus, 0x38, &status, 1)) return;
      if (!(status & 0x80)) break;
      if (!NHAP.cho(10)) return;
    } while ((uint32_t)(millis() - start) < 200);
    if (status & 0x80) { Serial.println("[LOI BUSY] AHT20 khong ket thuc chuyen doi trong thoi gian cho."); return; }
    uint8_t data[7];
    if (!nhan(bus, 0x38, data, sizeof(data))) return;
    if ((data[0] & 0x80) || !(data[0] & 0x08)) { Serial.println("[LOI STATUS] AHT20 BUSY/chua calibrated."); return; }
    if (crc8(data, 6) != data[6]) { Serial.println("[LOI CRC8] AHT20 tra ve du lieu loi."); return; }
    uint32_t rawH = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    uint32_t rawT = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
    float h = rawH * 100.0f / 1048576.0f, t = rawT * 200.0f / 1048576.0f - 50.0f;
    if (!trongKhoang(t, -40, 85) || !trongKhoang(h, 0, 100)) { Serial.println("[LOI DATA] AHT20 ngoai khoang hop le."); return; }
    Serial.printf("[OK] Nhiet do=%.2f C | Do am=%.2f %%RH | CRC8 OK\n", t, h);
  }
  void testBMP(TwoWire &bus, Adafruit_BMP280 &bmp, bool ready, const char *ten) {
    Serial.printf("%s / BMP280\n", ten);
    if (!kiemTraBus(ready) || NHAP.kiemTraThoat()) return;
    if (!daKhoiTao) {
      bool found = false;
      for (uint8_t addr = 0x76; addr <= 0x77; ++addr) {
        if (!ack(bus, addr, false)) continue;
        uint8_t reg = 0xD0, id;
        if (!gui(bus, addr, &reg, 1) || !nhan(bus, addr, &id, 1)) continue;
        Serial.printf("ACK tai 0x%02X, chip ID=0x%02X (BMP280 can 0x58).\n", addr, id);
        if (id != 0x58) {
          Serial.println(id == 0x60 ? "[SAI LOAI] Day la BME280, khong phai BMP280." : "[SAI ID] Khong phai BMP280 ID 0x58.");
          continue;
        }
        diaChiBMP = addr; found = true; break;
      }
      if (!found) { Serial.println("[LOI] Khong tim thay BMP280 hop le tai 0x76/0x77."); return; }
      if (NHAP.kiemTraThoat()) return;
      daKhoiTao = bmp.begin(diaChiBMP);
      if (!daKhoiTao) { Serial.println("[LOI INIT] BMP280 co ID dung nhung khoi tao that bai."); return; }
    }
    if (NHAP.kiemTraThoat()) return;
    if (!ack(bus, diaChiBMP)) { daKhoiTao = false; return; }
    float t = bmp.readTemperature(), p = bmp.readPressure() / 100.0f;
    if (!ack(bus, diaChiBMP)) { daKhoiTao = false; return; }
    if (!trongKhoang(t, -40, 85) || !trongKhoang(p, 300, 1100)) { Serial.println("[LOI DATA] BMP280 NaN/ngoai khoang hop le."); return; }
    Serial.printf("[OK] Dia chi=0x%02X | Nhiet do=%.2f C | Ap suat=%.2f hPa\n", diaChiBMP, t, p);
  }
  // SCD30 doc truc tiep de phan biet NACK, thieu byte, CRC sai va chua co mau.
  bool lenhSCD(uint16_t command, uint16_t argument) {
    uint8_t data[5] = {(uint8_t)(command >> 8), (uint8_t)command, (uint8_t)(argument >> 8), (uint8_t)argument, 0};
    data[4] = crc8(data + 2, 2);
    return gui(I2C_BUONG_KHI, 0x61, data, sizeof(data));
  }
  bool docSCD(uint16_t command, uint8_t *data, uint8_t count) {
    uint8_t cmd[2] = {(uint8_t)(command >> 8), (uint8_t)command};
    if (!gui(I2C_BUONG_KHI, 0x61, cmd, 2) || !NHAP.cho(4)) return false;
    if (!nhan(I2C_BUONG_KHI, 0x61, data, count)) return false;
    for (uint8_t i = 0; i < count; i += 3) {
      if (crc8(data + i, 2) != data[i + 2]) { Serial.printf("[LOI CRC8] SCD30 word %u.\n", i / 3); return false; }
    }
    return true;
  }
  void testSCD30() {
    Serial.println("BUONG_KHI / SCD30 / 0x61");
    if (!kiemTraBus(busBuongKhiOK) || NHAP.kiemTraThoat()) return;
    if (!ack(I2C_BUONG_KHI, 0x61)) { daKhoiTao = false; return; }
    if (!daKhoiTao) {
      uint8_t revision[3];
      if (!docSCD(0xD100, revision, 3)) return;
      Serial.printf("Firmware: %u.%u, CRC OK.\n", revision[0], revision[1]);
      if (!lenhSCD(0x4600, 2) || !NHAP.cho(4) || !lenhSCD(0x0010, 0)) return;
      daKhoiTao = true; lanCoMauSCD = millis();
    }
    uint8_t status[3];
    if (!docSCD(0x0202, status, 3)) return;
    uint16_t ready = ((uint16_t)status[0] << 8) | status[1];
    if (ready == 0) {
      Serial.println((uint32_t)(millis() - lanCoMauSCD) < 10000 ?
        "[DANG CHO] Chua co mau moi, giao tiep/CRC van OK." : "[LOI MAU] Qua 10 giay chua co mau moi du giao tiep OK.");
      return;
    }
    if (ready != 1) { Serial.println("[LOI STATUS] SCD30 tra ve data-ready khong hop le."); return; }
    uint8_t data[18];
    if (!docSCD(0x0300, data, sizeof(data))) return;
    float co2 = soThucSCD30(data), t = soThucSCD30(data + 6), h = soThucSCD30(data + 12);
    if (!trongKhoang(co2, 0.01f, 40000) || !trongKhoang(t, -40, 85) || !trongKhoang(h, 0, 100)) {
      Serial.println("[LOI DATA] SCD30 NaN/ngoai khoang hop le."); return;
    }
    lanCoMauSCD = millis();
    Serial.printf("[OK] CO2=%.2f ppm | Nhiet do=%.2f C | Do am=%.2f %%RH | CRC8 OK\n", co2, t, h);
  }
  void testKhi(uint8_t address, uint8_t expectedType, bool checkType = true) {
    Serial.printf("RS485 / dia chi %u\n", address);
    if (!RS485.choKhoiDong()) return;
    ThongTinKhi info;
    if (!RS485.docThongTin(address, info) || NHAP.daThoat) return;
    if (info.diaChi != address) { Serial.printf("[SAI DIA CHI] Hoi %u, nhan tu %u.\n", address, info.diaChi); return; }
    if (checkType && info.maLoai != expectedType) {
      Serial.printf("[SAI LOAI] Mong doi 0x%02X (%s); thuc te 0x%02X (%s).\n",
                    expectedType, tenKhi(expectedType), info.maLoai, tenKhi(info.maLoai));
      Serial.println("Van doc de chan doan; gia tri duoi day thuoc LOAI THUC TE.");
    }
    float value;
    if (RS485.docGiaTri(address, value) && !NHAP.daThoat)
      Serial.printf("[OK DATA] %s = %.2f %s | CRC OK\n", tenKhi(info.maLoai), value, tenDonVi(info.donVi));
  }
  void testLCD() {
    Serial.println("MOI_TRUONG / LCD GM12864-59N / ST7567S");
    if (!kiemTraBus(busMoiTruongOK) || NHAP.kiemTraThoat()) return;
    if (!ack(Wire, LCD_ADDRESS)) { daKhoiTao = false; return; }
    if (!daKhoiTao) {
      lcd.setI2CAddress(LCD_ADDRESS << 1); lcd.setBusClock(I2C_CLOCK);
      lcd.begin(); Wire.setTimeOut(I2C_TIMEOUT_MS);
      lcd.setContrast(LCD_CONTRAST); lcd.setFont(u8g2_font_6x10_tf);
      daKhoiTao = true;
    }
    lcd.clearBuffer();
    lcd.drawFrame(0, 0, 128, 64);
    lcd.drawStr(5, 13, "LCD TEST ST7567S");
    lcd.drawStr(5, 27, "GM12864-59N V2.0");
    lcd.setCursor(5, 41); lcd.print("FRAME "); lcd.print(soLan);
    lcd.drawStr(5, 56, "0123456789 ABCDEF");
    if (soLan % 2) lcd.drawBox(112, 34, 10, 10);
    for (uint8_t row = 0; row < 8; ++row) {
      if (NHAP.kiemTraThoat()) return;
      lcd.updateDisplayArea(0, row, 16, 1);
    }
    Serial.println("[ACK OK] Da gui hinh test. ACK KHONG chung minh LCD hien thi dung.");
    Serial.println("Ban thay chu/khung/FRAME thay doi? y=co, n=khong; exit=menu chinh.");
  }
};
