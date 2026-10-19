#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include "GiaoTiepPi.h"

TwoWire I2C_BUONG_KHI(1);
HardwareSerial UART_KHI(1);
KhuVucCamBien MOI_TRUONG("MOI_TRUONG",Wire,false);
KhuVucCamBien BUONG_KHI("BUONG_KHI",I2C_BUONG_KHI,true);
HeCamBienKhi CAM_BIEN_KHI(UART_KHI);
DieuKhienBom BOM;
ManHinh MAN_HINH;
GiaoTiepPi CONG_PI(MOI_TRUONG,BUONG_KHI,CAM_BIEN_KHI,BOM,MAN_HINH);

bool watchdogBat=false;
void phucVuPi() { if(watchdogBat) feedLoopWDT(); CONG_PI.capNhat(); }

void setup() {
  BOM.khoiDong(); // HIGH = bat; mac dinh LOW = tat
  Serial0.end();
  gpio_reset_pin((gpio_num_t)SDA2); gpio_reset_pin((gpio_num_t)SCL2);
  Serial.begin(115200); Serial.setTxTimeoutMs(0);
  bool bus1=Wire.begin(SDA1,SCL1,I2C_CLOCK);
  bool bus2=I2C_BUONG_KHI.begin(SDA2,SCL2,I2C_CLOCK);
  Wire.setTimeOut(I2C_TIMEOUT_MS); I2C_BUONG_KHI.setTimeOut(I2C_TIMEOUT_MS);
  bool uart=CAM_BIEN_KHI.khoiDong(phucVuPi);
  if(bus1) { MOI_TRUONG.khoiDong(phucVuPi); MAN_HINH.khoiDong(); }
  if(bus2) BUONG_KHI.khoiDong(phucVuPi);
  enableLoopWDT();
  watchdogBat=esp_task_wdt_status(nullptr)==ESP_OK;
  CONG_PI.watchdogOK=watchdogBat;
  CONG_PI.khoiDong(bus1,bus2,uart);
}

void loop() {
  phucVuPi();
  CONG_PI.thucHienDoc(phucVuPi); // Khong co READ tu Pi thi khong doc.
  CONG_PI.capNhatManHinh(phucVuPi); // Chi ve mau da co, khong do moi.
  delay(1);
}
