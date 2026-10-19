#include <driver/gpio.h>
#include "CauHinh.h"
#include "CamBien.h"
#include "CamBienKhi.h"
#include "DieuKhienBom.h"
#include "ManHinh.h"
#include "ThoiGian.h"

// Moi khu vuc la mot doi tuong, dung bus I2C rieng.
TwoWire I2C_BUONG_KHI(1);
HardwareSerial UART_KHI(1);
KhuVucCamBien MOI_TRUONG("MOI_TRUONG", Wire, false);
KhuVucCamBien BUONG_KHI("BUONG_KHI", I2C_BUONG_KHI, true);
HeCamBienKhi CAM_BIEN_KHI(UART_KHI);
DieuKhienBom BOM;
ManHinh MAN_HINH;
LichDoc LICH_DOC;

bool heThongSanSang = false, daBatDau = false;
uint32_t lucCapNguon = 0, soLanDoc = 0, lanDocGanNhat = 0;

// Ham nay duoc goi ca trong luc cam bien/LCD dang giao tiep.
void capNhatBom() { BOM.capNhat(); }

void setup() {
  lucCapNguon = millis();
  BOM.khoiDong(); // Tat relay ngay khi khoi dong
  Serial0.end(); // Khong dung UART0; Serial ben duoi la USB (kiem tra trong CauHinh.h)
  gpio_reset_pin((gpio_num_t)SDA2);
  gpio_reset_pin((gpio_num_t)SCL2); // Nhuong IO43/44 cho I2C buong khi
  Serial.begin(115200);
  delay(1000);
  if (!CAM_BIEN_KHI.khoiDong(capNhatBom)) {
    Serial.println("Loi UART. Dung chuong trinh, giu bom TAT.");
    return;
  }
  if (CONFIGURE_ADDRESS_MODE) {
    CAM_BIEN_KHI.doiDiaChi();
    return;
  }
  bool bus1OK = Wire.begin(SDA1, SCL1, I2C_CLOCK);
  bool bus2OK = I2C_BUONG_KHI.begin(SDA2, SCL2, I2C_CLOCK);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
  I2C_BUONG_KHI.setTimeOut(I2C_TIMEOUT_MS);
  if (!bus1OK || !bus2OK) {
    Serial.println("Loi khoi tao bus I2C. Giu bom TAT, kiem tra chan SDA/SCL.");
    return;
  }
  MAN_HINH.khoiDong();
  MOI_TRUONG.khoiDong(capNhatBom);
  BUONG_KHI.khoiDong(capNhatBom);
  heThongSanSang = true;
  Serial.printf("Khoi dong: %lu phut. Doc moi %lu ms.\n",
                (unsigned long)DELAY_START_TIME_MINUTES,
                (unsigned long)(cycle_read_time == 0 ? 500 : doiSangMs(cycle_read_time, 1)));
}

void loop() {
  if (!heThongSanSang || CONFIGURE_ADDRESS_MODE) { delay(1); return; }
  uint32_t thoiGianKhoiDong = doiSangMs(DELAY_START_TIME_MINUTES, 60000);
  uint32_t daCho = millis() - lucCapNguon;
  uint32_t conCho = !daBatDau && daCho < thoiGianKhoiDong ? thoiGianKhoiDong - daCho : 0;
  if (!daBatDau && conCho == 0) {
    daBatDau = true;
    BOM.batDau();
    LICH_DOC.batDau(cycle_read_time);
  }
  BOM.capNhat();
  bool vuaDoc = false;
  if (daBatDau && BOM.choPhepDoc() && LICH_DOC.denLucDoc()) {
    LICH_DOC.batDauDoc();
    lanDocGanNhat = millis();
    MOI_TRUONG.doc();
    BUONG_KHI.doc();
    CAM_BIEN_KHI.doc();
    Serial.printf("\n========== LAN DOC %lu | BOM %s ==========\n",
                  (unsigned long)++soLanDoc, BOM.dangBat() ? "BAT" : "TAT");
    MOI_TRUONG.inSerial();
    BUONG_KHI.inSerial();
    CAM_BIEN_KHI.inSerial();
    BOM.capNhat();
    LICH_DOC.ketThucDoc();
    vuaDoc = true;
  }
  MAN_HINH.capNhat(MOI_TRUONG.duLieu, BUONG_KHI.duLieu, CAM_BIEN_KHI.duLieu,
                  BOM.dangBat(), soLanDoc, lanDocGanNhat, conCho, capNhatBom, vuaDoc);
  delay(1);
}
