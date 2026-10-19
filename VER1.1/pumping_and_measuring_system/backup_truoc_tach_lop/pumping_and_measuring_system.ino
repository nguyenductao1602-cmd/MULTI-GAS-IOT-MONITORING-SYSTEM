#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SCD30.h>
#include "MeasurementTypes.h"

// ================= CAU HINH: CHINH O DAY =================
bool SW1 = true;  // Co su dung bom/relay 1
bool SW2 = true;
bool SW3 = true;
// Cac relay co SW=true luon bat cung luc, tat cung luc.

const uint32_t DELAY_START_TIME_MINUTES = 1;
const uint32_t PUMP_TIME_SECONDS = 0;   // Thoi gian bat chung cac bom
const uint32_t SETTLE_TIME_SECONDS = 10;  // Thoi gian cho sau khi tat bom
const uint32_t REPEAT_WAIT_SECONDS = 60; // Cho truoc khi bat bom lai
const uint32_t cycle_read_time = 60000;   // MILI GIAY: doc/in mot luot moi 3 giay
// 0 duoc thay bang 500ms; neu doc cham hon lich, bo qua luot tre, khong doc don.
const bool READ_ONLY_AFTER_SETTLE = false; // false: doc ca khi bom dang chay

const uint8_t RELAY_PINS[3] = {15, 16, 17};
const uint8_t RELAY_ON = LOW;   // Doi thanh HIGH neu relay kich muc cao
const uint8_t RELAY_OFF = (RELAY_ON == LOW) ? HIGH : LOW;
const int SDA1 = 36, SCL1 = 38;
const int SDA2 = 2, SCL2 = 1;   // Yeu cau ghi SCL2 hai lan: hieu SDA2=IO2
const int RS485_TX = 41, RS485_RX = 42; // Chan TX/RX cua ESP32-S3
// TD301D485H-A: IO41 -> TXD (chan 3, input); RXD (chan 4, output) -> IO42.
// Ten TXD/RXD cua transceiver theo phia MCU: KHONG noi cheo theo ten.
const int RS485_DE_RE = -1; // -1: module tu dong chuyen huong TX/RX
// UART binh thuong, khong dao RX (da doc CO thanh cong).
const bool RS485_INVERT_RX = false;
// TD301D485H-A tu dong chuyen huong: giu -1, cap VCC 3.3V.
// Neu module can DE va /RE: noi chung vao mot GPIO con trong, dien vao tren.
const uint32_t RS485_TIMEOUT_MS = 150;
const uint32_t RS485_CONFIG_TIMEOUT_MS = 1000; // Chi dung khi doi dia chi
const uint32_t RS485_CONFIG_STARTUP_MS = 10000;

// Mac dinh tat. Chi bat khi tren bus co DUY NHAT MOT module SemeaTech.
const bool CONFIGURE_ADDRESS_MODE = true;
const uint8_t OLD_ADDRESS = 1;
const uint8_t NEW_ADDRESS = 1;       // CH3SH=1, CO=2, H2S=3, NH3=4
const uint8_t CONFIGURE_GAS_TYPE = 0x29; // CH3Sh=0x29, CO=0x02, H2S=0x09, NH3=0x0B
// Khong tu dong zero/calibrate. Cam bien thu ba la CO theo xac nhan cua ban.

TwoWire I2C_ENV(1);
HardwareSerial RS485(1);
Adafruit_BMP280 bmpDuct(&Wire), bmpEnv(&I2C_ENV);
Adafruit_SCD30 scd30;
bool ahtDuctOK = false, ahtEnvOK = false;
bool bmpDuctOK = false, bmpEnvOK = false, scdOK = false;
uint8_t bmpDuctAddress = 0x76, bmpEnvAddress = 0x76;
bool configurationOK = false;
GasSensor gases[] = {
  {"SO2", 1, 0x0A}, {"CO", 2, 0x02}, {"H2S", 3, 0x09}, {"NH3", 4, 0x0B}
};
const size_t GAS_COUNT = sizeof(gases) / sizeof(gases[0]);
uint32_t bootTime = 0, readNumber = 0, lastReadTime = 0;
uint32_t pumpTimeMs = 0, settleTimeMs = 0, repeatTimeMs = 0, readTimeMs = 0;
// Dung enum trong file .h de Arduino sinh prototype dung.
PumpPhase pumpPhase = PUMP_STOPPED;
uint32_t phaseStartTime = 0;
bool pumpsStarted = false;

void allRelaysOff() {
  for (int i = 0; i < 3; ++i) digitalWrite(RELAY_PINS[i], RELAY_OFF);
}

// Chuyen don vi bang 64-bit, tranh tran so khi nhap thoi gian lon.
uint32_t safeDuration(uint32_t value, uint32_t multiplier) {
  uint64_t result = (uint64_t)value * multiplier;
  return result > 2147483647ULL ? 2147483647UL : (uint32_t)result;
}

void startPumps() {
  const bool enabled[3] = {SW1, SW2, SW3};
  allRelaysOff();
  // PUMP_TIME_SECONDS=0: bo qua bom, khong tao xung bat relay.
  if (pumpTimeMs > 0) {
    for (int i = 0; i < 3; ++i)
      if (enabled[i]) digitalWrite(RELAY_PINS[i], RELAY_ON);
  }
  pumpPhase = PUMP_RUNNING;
  phaseStartTime = millis();
  Serial.printf("Bom: SW1=%d SW2=%d SW3=%d, thoi gian %lu ms\n",
                SW1, SW2, SW3, (unsigned long)pumpTimeMs);
}

// Goi trong loop va trong cac ham cho UART/AHT20 de cap nhat relay kip thoi.
void updatePumps() {
  if (!pumpsStarted) return;
  uint32_t now = millis();
  // Gioi han 3 buoc: thoi gian 0 khong tao vong lap vo han.
  for (int step = 0; step < 3; ++step) {
    uint32_t elapsed = now - phaseStartTime;
    if (pumpPhase == PUMP_RUNNING && elapsed >= pumpTimeMs) {
      allRelaysOff();
      pumpPhase = PUMP_SETTLING;
      phaseStartTime = now;
      Serial.println("Tat tat ca relay, cho on dinh.");
    } else if (pumpPhase == PUMP_SETTLING && elapsed >= settleTimeMs) {
      pumpPhase = PUMP_WAITING;
      phaseStartTime = now;
      Serial.println("Cho den chu ky bom tiep theo.");
    } else if (pumpPhase == PUMP_WAITING && elapsed >= repeatTimeMs) {
      startPumps();
    } else break;
  }
}

void servicedWait(uint32_t durationMs) {
  uint32_t start = millis();
  while ((uint32_t)(millis() - start) < durationMs) {
    updatePumps();
    delay(1);
  }
}

// AHT20 doc truc tiep voi timeout, tranh vong cho BUSY vo han khi dut day.
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
  servicedWait(80);
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

// CRC16 Modbus, gui byte thap truoc. Day la giao thuc AA...EE,
// KHONG phai Modbus RTU doc holding register.
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
  const bool debug = CONFIGURE_ADDRESS_MODE;
  const uint32_t timeoutMs = debug ? RS485_CONFIG_TIMEOUT_MS : RS485_TIMEOUT_MS;
  uint8_t raw[64];
  size_t rawCount = 0, totalCount = 0, badFrames = 0;
  if (debug) printHex("TX: ", request, sizeof(request));
  while (RS485.available()) RS485.read();
  servicedWait(4);
  if (RS485_DE_RE >= 0) {
    digitalWrite(RS485_DE_RE, HIGH);
    delayMicroseconds(100);
  }
  RS485.write(request, sizeof(request));
  RS485.flush();
  if (RS485_DE_RE >= 0) digitalWrite(RS485_DE_RE, LOW);
  if (debug) Serial.printf("RX FIFO ngay sau khi TX xong: %d byte.\n", RS485.available());
  size_t received = 0;
  uint32_t start = millis();
  while ((uint32_t)(millis() - start) < timeoutMs) {
    updatePumps();
    if (!RS485.available()) { delay(1); continue; }
    uint8_t byteRead = RS485.read();
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
  if (debug) {
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
    Serial.println("Sai loai khi hoac don vi khong phai ppm: khong nhan mau.");
    return false;
  }
  return true;
}

bool readGas(GasSensor &sensor, float &value) {
  uint8_t reply[10];
  if (!gasTransaction(0x01, sensor.address, reply, sizeof(reply))) return false;
  if ((reply[3] != 0x00 && reply[3] != 0x80) || reply[6] > 99) return false;
  value = (((uint16_t)reply[4] << 8) | reply[5]) + reply[6] / 100.0f;
  if (reply[3] == 0x80) value = -value;
  return true;
}


void configureSingleAddress() {
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


bool i2cPresent(TwoWire &bus, uint8_t address) {
  bus.beginTransmission(address);
  return bus.endTransmission() == 0;
}

void printValue(const char *label, float value, const char *unit, float minimum, float maximum) {
  Serial.print(label);
  if (!isfinite(value) || value < minimum || value > maximum)
    Serial.println(": N/A (doc loi hoac chua co mau moi)");
  else Serial.printf(": %.2f %s\n", value, unit);
}

void printAHT(const char *location, TwoWire &bus, bool ready) {
  float t = NAN, h = NAN;
  if (ready && !readAHT20(bus, t, h)) t = h = NAN;
  Serial.printf("%s / AHT20\n", location);
  printValue("  Nhiet do", t, "C", -40, 85);
  printValue("  Do am", h, "%RH", 0, 100);
}

void printBMP(const char *location, TwoWire &bus, Adafruit_BMP280 &sensor,
              uint8_t address, bool ready) {
  float t = NAN, p = NAN;
  if (ready && i2cPresent(bus, address)) {
    t = sensor.readTemperature();
    p = sensor.readPressure() / 100.0f;
    if (!i2cPresent(bus, address)) t = p = NAN;
  }
  Serial.printf("%s / BMP280\n", location);
  printValue("  Nhiet do", t, "C", -40, 85);
  printValue("  Ap suat", p, "hPa", 300, 1100);
}

// Moi luot chi doc tung cam bien mot lan, in ngay, khong tinh trung binh.
void readAndPrintSensors() {
  Serial.printf("\n========== READ %lu | pump=%s ==========\n",
                (unsigned long)++readNumber, pumpPhase == PUMP_RUNNING && pumpTimeMs > 0 ? "on" : "off");
  printAHT("inside", Wire, ahtDuctOK);
  updatePumps();
  printBMP("inside", Wire, bmpDuct, bmpDuctAddress, bmpDuctOK);
  updatePumps();
  Serial.println("inside / SCD30");
  float t = NAN, h = NAN, co2 = NAN;
  if (scdOK && scd30.dataReady() && scd30.read()) {
    t = scd30.temperature;
    h = scd30.relative_humidity;
    co2 = scd30.CO2;
  }
  printValue("  Nhiet do", t, "C", -40, 85);
  printValue("  Do am", h, "%RH", 0, 100);
  printValue("  CO2", co2, "ppm", 0.01f, 40000);
  for (size_t i = 0; i < GAS_COUNT; ++i) {
    updatePumps();
    // Thu nhan dang lai cam bien chua san sang, moi 10 giay.
    if (!gases[i].ready && (!gases[i].checked ||
        (uint32_t)(millis() - gases[i].lastCheckTime) >= 10000)) {
      gases[i].ready = identifyGas(gases[i].address, gases[i].type);
      gases[i].checked = true;
      gases[i].lastCheckTime = millis();
    }
    float value = NAN;
    if (gases[i].ready && !readGas(gases[i], value)) value = NAN;
    Serial.printf("inside / SemeaTech / %s / address %u\n", gases[i].name, gases[i].address);
    printValue("  Nong do", value, "ppm", -65535.99f, 65535.99f);
  }
  updatePumps();
  printAHT("outside", I2C_ENV, ahtEnvOK);
  updatePumps();
  printBMP("outside", I2C_ENV, bmpEnv, bmpEnvAddress, bmpEnvOK);
  updatePumps();
}

void setup() {
  bootTime = millis();
  for (int i = 0; i < 3; ++i) {
    // Nap muc tat vao latch truoc khi chuyen chan thanh output.
    gpio_set_level((gpio_num_t)RELAY_PINS[i], RELAY_OFF);
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], RELAY_OFF);
  }
  allRelaysOff();
  Serial.begin(115200);
  delay(1000);
  RS485.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  esp_err_t uartResult = uart_set_line_inverse(UART_NUM_1,
                          RS485_INVERT_RX ? UART_SIGNAL_RXD_INV : 0);
  if (uartResult != ESP_OK) {
    Serial.printf("Loi cau hinh dao RX UART1: %d. Dung, bom giu TAT.\n", (int)uartResult);
    return;
  }
  if (RS485_DE_RE >= 0) {
    pinMode(RS485_DE_RE, OUTPUT);
    digitalWrite(RS485_DE_RE, LOW);
  }
  if (CONFIGURE_ADDRESS_MODE) {
    configureSingleAddress();
    return;
  }
  pumpTimeMs = safeDuration(PUMP_TIME_SECONDS, 1000);
  settleTimeMs = safeDuration(SETTLE_TIME_SECONDS, 1000);
  repeatTimeMs = safeDuration(REPEAT_WAIT_SECONDS, 1000);
  readTimeMs = cycle_read_time == 0 ? 500 : safeDuration(cycle_read_time, 1);
  // Neu ca ba khoang bom/cho deu 0, cho toi thieu 1 giay tranh relay nhay lien tuc.
  if (pumpTimeMs == 0 && settleTimeMs == 0 && repeatTimeMs == 0) repeatTimeMs = 1000;
  Wire.begin(SDA1, SCL1, 100000);
  I2C_ENV.begin(SDA2, SCL2, 100000);
  Wire.setTimeOut(20);
  I2C_ENV.setTimeOut(20);
  // LCD tren I2C_ENV: danh cho sau nay, CHUA khoi tao/CHUA code.
  ahtDuctOK = beginAHT20(Wire);
  ahtEnvOK = beginAHT20(I2C_ENV);
  bmpDuctOK = beginBMP280(bmpDuct, bmpDuctAddress);
  bmpEnvOK = beginBMP280(bmpEnv, bmpEnvAddress);
  scdOK = scd30.begin(0x61, &Wire);
  if (scdOK) scdOK = scd30.setMeasurementInterval(2);
  Serial.printf("inside: AHT20=%d BMP280=%d SCD30=%d | outside: AHT20=%d BMP280=%d\n",
                ahtDuctOK, bmpDuctOK, scdOK, ahtEnvOK, bmpEnvOK);
  configurationOK = true;
  Serial.printf("Cho khoi dong %lu phut; cycle_read_time=%lu ms.\n",
                (unsigned long)DELAY_START_TIME_MINUTES, (unsigned long)readTimeMs);
}

void loop() {
  if (CONFIGURE_ADDRESS_MODE || !configurationOK) { delay(1); return; }
  if (!pumpsStarted) {
    uint32_t warmupMs = safeDuration(DELAY_START_TIME_MINUTES, 60000);
    if ((uint32_t)(millis() - bootTime) < warmupMs) { delay(1); return; }
    pumpsStarted = true;
    startPumps();
    lastReadTime = millis() - readTimeMs; // Doc lan dau ngay khi het khoi dong.
  }
  updatePumps();
  bool canRead = !READ_ONLY_AFTER_SETTLE || pumpPhase == PUMP_WAITING;
  if (canRead && (uint32_t)(millis() - lastReadTime) >= readTimeMs) {
    lastReadTime = millis();
    readAndPrintSensors();
    // Khong chay don nhieu luot neu giao tiep cham hon cycle_read_time.
    if ((uint32_t)(millis() - lastReadTime) >= readTimeMs) lastReadTime = millis();
  }
  delay(1);
}
