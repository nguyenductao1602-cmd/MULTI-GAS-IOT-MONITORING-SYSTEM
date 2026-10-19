#pragma once
#include <driver/uart.h>
#include "CauHinh.h"
#include "DuLieu.h"
class HeCamBienKhi {
public:
  DuLieuKhi duLieu[4]={{"CH3SH",1,0x29},{"CO",2,0x02},{"H2S",3,0x09},{"NH3",4,0x0B}};
  const char *loi[4]={"NOT_READ","NOT_READ","NOT_READ","NOT_READ"};
  explicit HeCamBienKhi(HardwareSerial &cong) : uart(cong) {}
  bool khoiDong(void (*hamPhucVu)()) {
    phucVu=hamPhucVu;
    if (RS485_DE_RE>=0) { digitalWrite(RS485_DE_RE,LOW); pinMode(RS485_DE_RE,OUTPUT); }
    uart.begin(9600,SERIAL_8N1,RS485_RX,RS485_TX);
    sanSang=uart_set_line_inverse(UART_NUM_1,0)==ESP_OK;
    mocKhoiDong=millis();
    return sanSang;
  }
  void doc(int chiSo=-1) {
    for (int i=0;i<4;++i) {
      if (chiSo>=0 && i!=chiSo) continue;
      auto &khi=duLieu[i]; khi.giaTri=NAN;
      if (!sanSang) { loi[i]="UART_INIT_FAILED"; continue; }
      if ((uint32_t)(millis()-mocKhoiDong)<RS485_STARTUP_MS) { loi[i]="WARMING_UP"; continue; }
      uint8_t info[16],data[10];
      // Xac minh loai/don vi moi READ; khong tu doi dia chi hay hieu chuan.
      if (!giaoDich(0x0F,khi.address,info,sizeof(info),loi[i])) continue;
      if (info[3]!=khi.type) { loi[i]="WRONG_GAS_TYPE"; continue; }
      if (info[12]!=2) { loi[i]="WRONG_UNIT"; continue; }
      if (!giaoDich(1,khi.address,data,sizeof(data),loi[i])) continue;
      if ((data[3]!=0 && data[3]!=0x80) || data[6]>99) { loi[i]="INVALID_DATA"; continue; }
      float value=(((uint16_t)data[4]<<8)|data[5])+data[6]/100.0f;
      khi.giaTri=data[3]==0x80 ? -value : value; loi[i]="OK";
    }
  }
private:
  HardwareSerial &uart;
  bool sanSang=false;
  uint32_t mocKhoiDong=0;
  void (*phucVu)()=nullptr;
  void cho(uint32_t ms) {
    uint32_t start=millis();
    while ((uint32_t)(millis()-start)<ms) { if(phucVu) phucVu(); delay(1); }
  }
  uint16_t crc16(const uint8_t *data,size_t len) const {
    uint16_t crc=0xFFFF;
    for(size_t i=0;i<len;++i) {
      crc^=data[i];
      for(int bit=0;bit<8;++bit) crc=(crc&1)?(crc>>1)^0xA001:crc>>1;
    }
    return crc;
  }
  bool giaoDich(uint8_t command,uint8_t address,uint8_t *reply,size_t length,const char *&loi) {
    uint8_t request[6]={0xAA,command,address,0,0,0xEE};
    uint16_t crc=crc16(request+1,2); request[3]=crc; request[4]=crc>>8;
    cho(RS485_COMMAND_GAP_MS);
    uint32_t start=millis();
    while(uart.available() && (uint32_t)(millis()-start)<20) uart.read();
    cho(4);
    if(RS485_DE_RE>=0) { digitalWrite(RS485_DE_RE,HIGH); delayMicroseconds(100); }
    uart.write(request,sizeof(request)); uart.flush();
    if(RS485_DE_RE>=0) digitalWrite(RS485_DE_RE,LOW);
    start=millis(); size_t count=0,total=0;
    while((uint32_t)(millis()-start)<RS485_TIMEOUT_MS) {
      if(phucVu) phucVu();
      if(!uart.available()) { delay(1); continue; }
      uint8_t b=uart.read(); ++total;
      if(count==0 && b!=0xAA) continue;
      reply[count++]=b;
      if(count!=length) continue;
      crc=crc16(reply+1,length-4);
      if(reply[length-1]==0xEE && reply[length-3]==(uint8_t)crc &&
         reply[length-2]==(uint8_t)(crc>>8) && reply[1]==command && reply[2]==address) return true;
      size_t next=1; while(next<length && reply[next]!=0xAA) ++next;
      count=length-next; memmove(reply,reply+next,count);
    }
    loi=total==0?"NO_RESPONSE":"INVALID_FRAME_OR_CRC";
    return false;
  }
};
