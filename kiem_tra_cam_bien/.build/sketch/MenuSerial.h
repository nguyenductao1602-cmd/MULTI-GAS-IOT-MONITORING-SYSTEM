#line 1 "D:\\All_My_Data\\Arduino\\A_Dat_CO2\\kiem_tra_cam_bien\\MenuSerial.h"
#pragma once
#include "KiemTra.h"

class MenuSerial {
public:
  MenuSerial(NhapSerial &nhap, GiaoThuc485 &rs, KiemTra &test) : NHAP(nhap), RS485(rs), TEST(test) {}
  void hienMenu() {
    TEST.dung(); NHAP.daThoat = false; RS485.daGuiLenhDoi = false;
    trangThai = CHON_CHUC_NANG;
    Serial.println("\n========== NHAP CHUC NANG ==========");
    Serial.println("F1: Doi dia chi (ID module)");
    Serial.println("F2: Check/test tung thiet bi");
    Serial.println("Serial Monitor: 115200 baud, Newline hoac Both NL & CR.");
    Serial.println("Nhap moi lua chon tren MOT dong roi Send/Enter. Khong phan biet hoa/thuong.");
    Serial.println("exit: huy buoc dang cho va ve menu nay. Tat ca relay luon TAT.");
    Serial.println("Lenh doi dia chi DA GUI khong the hoan tac bang exit.");
    Serial.print("> ");
  }
  void thoat() {
    if (RS485.daGuiLenhDoi)
      Serial.println("[THOAT] Lenh doi da duoc gui; hay kiem tra dia chi moi truoc khi thu lai.");
    else Serial.println("[THOAT] Ve menu chinh.");
    hienMenu();
  }
  void xuLy(const char *line) {
    if (strcmp(line, "exit") == 0) { thoat(); return; }
    uint8_t number;
    switch (trangThai) {
      case CHON_CHUC_NANG:
        if (strcmp(line, "f1") == 0) vaoDoiDiaChi();
        else if (strcmp(line, "f2") == 0) hienDanhSach();
        else Serial.println("Chi nhap F1 hoac F2, roi Enter.");
        break;
      case NHAP_DIA_CHI_CU:
        if (!docSo(line, 10, 1, number)) { Serial.println("Dia chi cu phai la so thap phan 1..255. Nhap lai:"); break; }
        diaChiCu = number; trangThai = NHAP_DIA_CHI_MOI;
        Serial.println("Nhap dia chi moi (1..255):");
        break;
      case NHAP_DIA_CHI_MOI:
        if (!docSo(line, 10, 1, number)) { Serial.println("Dia chi moi phai la so thap phan 1..255. Nhap lai:"); break; }
        diaChiMoi = number; trangThai = NHAP_MA_CAM_BIEN;
        Serial.println("Nhap ma code cam bien HEX, vi du 29 hoac 0x29 cho CH3SH:");
        break;
      case NHAP_MA_CAM_BIEN:
        if (!docSo(line, 16, 0, number)) { Serial.println("Ma phai la HEX 00..FF, vi du 0B/0x0B. Nhap lai:"); break; }
        maNhap = number;
        kiemTraThongTin();
        break;
      case XAC_NHAN_SAI_MA:
        if (strcmp(line, "y") == 0) thucHienDoi();
        else if (strcmp(line, "n") == 0) { Serial.println("Da huy. Chua gui lenh doi dia chi."); hienMenu(); }
        else Serial.println("Nhap y de van doi, n de huy, hoac exit.");
        break;
      case CHON_TEST:
        if (!docSo(line, 10, 1, number) || number > 13) { Serial.println("Chon so 1..13 hoac exit."); break; }
        if (number == 11 || number == 12) {
          TEST.quetI2C(number == 12);
          if (!NHAP.daThoat) hienDanhSach();
        } else if (number == 13) {
          trangThai = NHAP_DIA_CHI_TEST;
          Serial.println("Nhap dia chi RS485 can test (thap phan 1..255):");
        } else { TEST.chon(number); trangThai = DANG_TEST; }
        break;
      case NHAP_DIA_CHI_TEST:
        if (!docSo(line, 10, 1, number)) { Serial.println("Nhap dia chi 1..255:"); break; }
        TEST.chon(13, number); trangThai = DANG_TEST;
        break;
      case DANG_TEST:
        if (strcmp(line, "list") == 0) { hienDanhSach(); break; }
        if (TEST.dangTestLCD() && strcmp(line, "y") == 0) {
          if (TEST.lcdCoPhanHoi()) Serial.println("[OK LCD] Ban da xac nhan nhin thay hinh test.");
          else Serial.println("[LOI LCD] Chua gui duoc hinh test, chua the xac nhan.");
        } else if (TEST.dangTestLCD() && strcmp(line, "n") == 0)
          Serial.println("[LOI HIEN THI] Kiem tra nguon LCD, contrast, driver ST7567S va dia chi 0x3F. ACK chua du de ket luan LCD tot.");
        else Serial.println("Dang test: exit -> menu chinh; list -> danh sach test.");
        break;
    }
  }
  void capNhat() { if (trangThai == DANG_TEST && !NHAP.daThoat) TEST.capNhat(); }
private:
  enum TrangThai { CHON_CHUC_NANG, NHAP_DIA_CHI_CU, NHAP_DIA_CHI_MOI, NHAP_MA_CAM_BIEN,
                   XAC_NHAN_SAI_MA, CHON_TEST, NHAP_DIA_CHI_TEST, DANG_TEST };
  TrangThai trangThai = CHON_CHUC_NANG;
  NhapSerial &NHAP;
  GiaoThuc485 &RS485;
  KiemTra &TEST;
  uint8_t diaChiCu = 1, diaChiMoi = 1, maNhap = 0;
  ThongTinKhi thongTin;

  void vaoDoiDiaChi() {
    TEST.dung(); RS485.daGuiLenhDoi = false;
    Serial.println("\n--- F1: DOI DIA CHI ---");
    Serial.println("CHI NOI MOT MODULE tren bus RS485. Lenh doi chi mang dia chi MOI.");
    Serial.println("Dia chi nhap he 10; ma cam bien nhap HEX. exit de ve menu.");
    Serial.println("Ma da biet: CH3SH=29, CO=02, H2S=09, NH3=0B, SO2=0A.");
    trangThai = NHAP_DIA_CHI_CU;
    Serial.println("Nhap dia chi cu (1..255):");
  }
  void hienDanhSach() {
    TEST.dung(); trangThai = CHON_TEST;
    Serial.println("\n--- F2: CHECK/TEST ---");
    Serial.println("Chon so thiet bi. Test doc lap lap lai sau moi 2s; exit -> menu chinh.");
    Serial.println(" 1: AHT20  MOI_TRUONG (SDA36/SCL38)");
    Serial.println(" 2: BMP280 MOI_TRUONG (SDA36/SCL38)");
    Serial.println(" 3: AHT20  BUONG_KHI  (SDA43/SCL44)");
    Serial.println(" 4: BMP280 BUONG_KHI  (SDA43/SCL44)");
    Serial.println(" 5: SCD30  BUONG_KHI  (SDA43/SCL44)");
    Serial.println(" 6: CH3SH  RS485 dia chi 1, ma 29");
    Serial.println(" 7: CO     RS485 dia chi 2, ma 02");
    Serial.println(" 8: H2S    RS485 dia chi 3, ma 09");
    Serial.println(" 9: NH3    RS485 dia chi 4, ma 0B");
    Serial.println("10: LCD GM12864-59N (MOI_TRUONG, 0x3F)");
    Serial.println("11: Quet I2C MOI_TRUONG");
    Serial.println("12: Quet I2C BUONG_KHI");
    Serial.println("13: RS485 - tu nhap dia chi, doc ma/loai/data thuc te");
    Serial.println("Serial 115200, Newline/Both NL & CR. Dang test co the nhap list de chon lai.");
    Serial.print("> ");
  }
  void nhapLaiDiaChi() {
    trangThai = NHAP_DIA_CHI_CU;
    Serial.println("CHUA gui lenh doi. Nhap lai dia chi cu (1..255), hoac exit:");
  }
  void kiemTraThongTin() {
    if (!RS485.choKhoiDong()) { if (!NHAP.daThoat) nhapLaiDiaChi(); return; }
    if (!RS485.docThongTin(diaChiCu, thongTin)) {
      if (NHAP.daThoat) return;
      Serial.println("[CHUA XAC NHAN DIA CHI] Dia chi da nhap co the khong dung; cung co the do nguon/day/baud.");
      Serial.println("Chua nhan duoc thong tin hop le nen chua biet ma code thuc te.");
      nhapLaiDiaChi(); return;
    }
    if (NHAP.daThoat) return;
    if (thongTin.diaChi != diaChiCu) {
      Serial.printf("[SAI DIA CHI] Dia chi da nhap khong dung: nhap %u, khung tra ve %u.\n", diaChiCu, thongTin.diaChi);
      Serial.printf("Ma code thuc te la 0x%02X - loai cam bien %s.\n", thongTin.maLoai, tenKhi(thongTin.maLoai));
      nhapLaiDiaChi(); return;
    }
    if (thongTin.maLoai != maNhap) {
      Serial.printf("[SAI MA] Ma cam bien da nhap khong dung: nhap 0x%02X (%s).\n", maNhap, tenKhi(maNhap));
      Serial.printf("Ma code thuc te la 0x%02X - loai cam bien %s.\n", thongTin.maLoai, tenKhi(thongTin.maLoai));
      trangThai = XAC_NHAN_SAI_MA;
      Serial.println("Ban co chac chan muon doi dia chi cho cam bien khi sai ma code khong? y=van doi, n=huy:");
      return;
    }
    thucHienDoi();
  }
  void thucHienDoi() {
    if (NHAP.kiemTraThoat()) return;
    if (diaChiCu == diaChiMoi) { Serial.println("Dia chi cu = moi. Da kiem tra, KHONG gui lenh doi."); hienMenu(); return; }
    // Doc lai truoc khi ghi: module phai van dung dia chi va ma thuc da hien thi.
    ThongTinKhi kiemTra;
    if (!RS485.docThongTin(diaChiCu, kiemTra)) {
      if (!NHAP.daThoat) { Serial.println("[HUY] Mat phan hoi truoc khi doi."); nhapLaiDiaChi(); }
      return;
    }
    if (NHAP.daThoat) return;
    if (kiemTra.diaChi != diaChiCu || kiemTra.maLoai != thongTin.maLoai) {
      Serial.println("[HUY] Dia chi/loai module da thay doi so voi thong tin vua kiem tra.");
      nhapLaiDiaChi(); return;
    }
    if (NHAP.kiemTraThoat()) return;
    Serial.printf("Doi %s: dia chi %u -> %u.\n", tenKhi(thongTin.maLoai), diaChiCu, diaChiMoi);
    bool ack = RS485.doiDiaChi(diaChiMoi);
    if (NHAP.daThoat || !NHAP.cho(200)) return;
    ThongTinKhi moi;
    bool verified = RS485.docThongTin(diaChiMoi, moi) && moi.diaChi == diaChiMoi && moi.maLoai == thongTin.maLoai;
    if (NHAP.daThoat) return;
    if (verified) {
      Serial.println(ack ? "[THANH CONG] ACK dung, doc lai dia chi moi dung, CRC OK." :
                           "[DA DOC DIA CHI MOI] Thieu ACK hop le, nhung doc lai dung dia chi/loai.");
      Serial.println("Tat/cap lai nguon module. Vao F1 voi dia chi cu=moi de kiem tra da luu.");
    } else Serial.println("[CHUA XAC MINH] Lenh da gui nhung chua doc dung dia chi moi. Kiem tra dia chi moi/cu, khong doan ket qua.");
    hienMenu();
  }
};
