#pragma once
#include <ArduinoJson.h>
#include <esp_system.h>
#include "CamBien.h"
#include "CamBienKhi.h"
#include "DieuKhienBom.h"
#include "ManHinh.h"

// USB: mot JSON tren mot dong. Khong dung readString hay delay cho lenh Pi.
class GiaoTiepPi {
public:
  bool watchdogOK=false;
  GiaoTiepPi(KhuVucCamBien &mt,KhuVucCamBien &bk,HeCamBienKhi &k,
             DieuKhienBom &b,ManHinh &mh) : moiTruong(mt),buongKhi(bk),khi(k),bom(b),manHinh(mh) {}
  void khoiDong(bool bus1,bool bus2,bool uart) {
    bus1OK=bus1; bus2OK=bus2; uartOK=uart;
    bootId=soNgauNhien();
    JsonDocument out; chung(out,"event",0,"READY");
    out["reset_reason"]=(uint32_t)esp_reset_reason();
    out["protocol"]=1; out["pi_timeout_ms"]=PI_TIMEOUT_MS;
    trangThai(out); gui(out);
  }
  // Goi ca trong khi I2C/RS485 dang cho: van nhan STOP/PING va bao ve bom.
  void capNhat() {
    if (session && (uint32_t)(millis()-lanLienLac)>=PI_TIMEOUT_MS) {
      bom.tatTatCa();
      JsonDocument out; chung(out,"event",0,"PI_TIMEOUT"); trangThai(out); out["pi_connected"]=false; gui(out);
      session=0; idGanNhat=0; traLoiCu=""; lenhCu="";
    }
    int nganSach=128;
    while (nganSach-- && Serial.available()) {
      char c=Serial.read(); mocKyTu=millis();
      if (c=='\r' || c=='\n') {
        if (dongQuaDai) baoLoi(0,"","LINE_TOO_LONG");
        else if (doDai) { dong[doDai]=0; xuLy(dong); }
        doDai=0; dongQuaDai=false;
      } else if (!dongQuaDai) {
        if (doDai>=MAX_COMMAND_BYTES) { doDai=0; dongQuaDai=true; }
        else dong[doDai++]=c;
      }
    }
    if ((doDai || dongQuaDai) && (uint32_t)(millis()-mocKyTu)>2000) {
      doDai=0; dongQuaDai=false; baoLoi(0,"","LINE_TIMEOUT");
    }
    dayUSB();
  }
  // Chi loop chinh goi ham nay. Khong de READ chen vao READ dang chay.
  void thucHienDoc(void (*phucVu)()) {
    if (!choDoc) return;
    choDoc=false;
    uint32_t id=idDoc, phien=phienDoc, batDau=millis();
    bool bomLucDau[3]; for(int i=0;i<3;++i) bomLucDau[i]=bom.trangThai[i];
    int chon=camBienDoc;
    if(chon==0 || chon==1 || chon==2) {
      if(bus1OK) moiTruong.doc(chon==0?"ALL":chon==1?"AHT20":"BMP280"); else moiTruong.duLieu={};
    }
    if(chon==0 || (chon>=3 && chon<=5)) {
      if(bus2OK) buongKhi.doc(chon==0?"ALL":chon==3?"AHT20":chon==4?"BMP280":"SCD30"); else buongKhi.duLieu={};
    }
    if(chon==0 || chon>=6) khi.doc(chon==0?-1:chon-6);
    lanDoc=millis(); ++soLanDoc; dangDoc=false;
    JsonDocument out; chung(out,"response",id,"READ"); out["session"]=phien;
    out["ok"]=(phien==session);
    if(phien!=session) out["error"]="SESSION_EXPIRED";
    out["target"]=tenDich(chon);
    out["sample_seq"]=soLanDoc; out["read_start_ms"]=batDau;
    out["read_end_ms"]=lanDoc;
    JsonArray start=out["relay_at_start"].to<JsonArray>();
    for(bool b:bomLucDau) start.add(b);
    if(chon<=2) {
      JsonObject area=out["MOI_TRUONG"].to<JsonObject>();
      khuVuc(area,moiTruong,false,bus1OK);
      if(chon==1) area.remove("BMP280");
      if(chon==2) area.remove("AHT20");
    }
    if(chon==0 || (chon>=3 && chon<=5)) {
      JsonObject area=out["BUONG_KHI"].to<JsonObject>();
      khuVuc(area,buongKhi,true,bus2OK);
      if(chon>0) {
        if(chon!=3) area.remove("AHT20");
        if(chon!=4) area.remove("BMP280");
        if(chon!=5) area.remove("SCD30");
      }
    }
    if(chon==0 || chon>=6) {
    JsonObject gas=out["BUONG_KHI"]["RS485"].to<JsonObject>();
    for(int i=0;i<4;++i) {
      if(chon>=6 && i!=chon-6) continue;
      auto &d=khi.duLieu[i]; JsonObject sensor=gas[d.name].to<JsonObject>();
      sensor["address"]=d.address; sensor["gas_type"]=d.type;
      sensor["ok"]=isfinite(d.giaTri); sensor["error"]=khi.loi[i];
      so(sensor,"ppm",d.giaTri);
    }
    }
    trangThai(out); gui(out,id);
    manHinh.capNhat(moiTruong.duLieu,buongKhi.duLieu,khi.duLieu,bom.dangBat(),
                    soLanDoc,lanDoc,0,phucVu,true);
  }
  void capNhatManHinh(void (*phucVu)()) {
    manHinh.capNhat(moiTruong.duLieu,buongKhi.duLieu,khi.duLieu,bom.dangBat(),
                    soLanDoc,lanDoc,0,phucVu);
  }
private:
  KhuVucCamBien &moiTruong,&buongKhi;
  HeCamBienKhi &khi; DieuKhienBom &bom; ManHinh &manHinh;
  bool bus1OK=false,bus2OK=false,uartOK=false,dangDoc=false,choDoc=false;
  uint32_t bootId=0,session=0,lanLienLac=0,idGanNhat=0,idDoc=0,phienDoc=0;
  uint32_t soLanDoc=0,lanDoc=0,mocKyTu=0,boBanTin=0;
  char dong[MAX_COMMAND_BYTES+1]; size_t doDai=0; bool dongQuaDai=false;
  String lenhCu,traLoiCu;
  int camBienDoc=0;
  static const char *tenDich(int i) {
    static const char *names[]={"ALL","MOI_TRUONG.AHT20","MOI_TRUONG.BMP280","BUONG_KHI.AHT20","BUONG_KHI.BMP280","BUONG_KHI.SCD30","BUONG_KHI.CH3SH","BUONG_KHI.CO","BUONG_KHI.H2S","BUONG_KHI.NH3"};
    return names[i];
  }
  int chonCamBien(const char *target) {
    if (!target) return -1;
    for(int i=0;i<10;++i) if(!strcmp(target,tenDich(i))) return i;
    return -1;
  }
  // Hang doi co gioi han; USB mat ket noi khong duoc chan bao ve relay.
  String hangDoi[8]; uint8_t dau=0,cuoi=0,soDong=0; size_t daGui=0;
  uint32_t soNgauNhien() { uint32_t n; do { n=esp_random(); } while(!n); return n; }
  void chung(JsonDocument &out,const char *type,uint32_t id,const char *cmd) {
    out["type"]=type; out["id"]=id; out["cmd"]=cmd;
    out["capabilities"].to<JsonArray>().add("read_sensor");
    out["boot_id"]=bootId; out["session"]=session; out["uptime_ms"]=millis();
  }
  void trangThai(JsonDocument &out) {
    JsonArray relays=out["relays"].to<JsonArray>();
    JsonArray enabled=out["relay_enabled"].to<JsonArray>();
    for(int i=0;i<3;++i) { relays.add(bom.trangThai[i]); enabled.add(bom.choPhep(i)); }
    out["pi_connected"]=session!=0; out["reading"]=dangDoc;
    out["bus1_ok"]=bus1OK; out["bus2_ok"]=bus2OK; out["uart1_ok"]=uartOK;
    out["watchdog_ok"]=watchdogOK;
    out["lcd_ok"]=manHinh.hoatDong(); out["sample_seq"]=soLanDoc;
    if(soLanDoc) out["sample_age_ms"]=(uint32_t)(millis()-lanDoc);
    else out["sample_age_ms"]=nullptr;
    out["dropped_frames"]=boBanTin;
  }
  void xepHang(const String &text) {
    if(soDong==8) { ++boBanTin; return; }
    hangDoi[cuoi]=text; hangDoi[cuoi]+='\n'; cuoi=(cuoi+1)%8; ++soDong;
  }
  void gui(JsonDocument &out,uint32_t idCache=0) {
    String text; serializeJson(out,text);
    if(idCache && idCache==idGanNhat) traLoiCu=text;
    xepHang(text);
  }
  void dayUSB() {
    if(!soDong || !Serial) return;
    int trong=Serial.availableForWrite(); if(trong<=0) return;
    size_t count=hangDoi[dau].length()-daGui;
    if(count>(size_t)trong) count=trong;
    if(count>256) count=256;
    daGui+=Serial.write((const uint8_t *)hangDoi[dau].c_str()+daGui,count);
    if(daGui==hangDoi[dau].length()) { hangDoi[dau]=""; dau=(dau+1)%8; --soDong; daGui=0; }
  }
  void baoLoi(uint32_t id,const char *cmd,const char *code,bool cache=false) {
    JsonDocument out; chung(out,"response",id,cmd); out["ok"]=false; out["error"]=code;
    gui(out,cache?id:0);
  }
  void so(JsonObject out,const char *key,float value) {
    if(isfinite(value)) out[key]=value; else out[key]=nullptr;
  }
  void khuVuc(JsonObject out,const KhuVucCamBien &v,bool coSCD,bool busOK) {
    const auto &d=v.duLieu;
    JsonObject aht=out["AHT20"].to<JsonObject>();
    bool ok=isfinite(d.nhietDoAHT20)&&isfinite(d.doAmAHT20);
    aht["ok"]=ok; aht["error"]=ok?"OK":(busOK?"INIT_OR_READ_FAILED":"BUS_INIT_FAILED");
    so(aht,"temperature_c",d.nhietDoAHT20); so(aht,"humidity_rh",d.doAmAHT20);
    JsonObject bmp=out["BMP280"].to<JsonObject>();
    ok=isfinite(d.nhietDoBMP280)&&isfinite(d.apSuatBMP280);
    bmp["ok"]=ok; bmp["error"]=ok?"OK":(busOK?"INIT_OR_READ_FAILED":"BUS_INIT_FAILED");
    so(bmp,"temperature_c",d.nhietDoBMP280); so(bmp,"pressure_hpa",d.apSuatBMP280);
    if(coSCD) {
      JsonObject scd=out["SCD30"].to<JsonObject>();
      ok=isfinite(d.nhietDoSCD30)&&isfinite(d.doAmSCD30)&&isfinite(d.co2SCD30);
      scd["ok"]=ok; scd["error"]=ok?"OK":(!busOK?"BUS_INIT_FAILED":
                                       (!strcmp(v.loiSCD30,"OK")?"INVALID_DATA":v.loiSCD30));
      so(scd,"temperature_c",d.nhietDoSCD30); so(scd,"humidity_rh",d.doAmSCD30); so(scd,"co2_ppm",d.co2SCD30);
    }
  }
  void xuLy(const char *line) {
    JsonDocument in;
    if(deserializeJson(in,line,DeserializationOption::NestingLimit(4)) || !in.is<JsonObject>()) {
      baoLoi(0,"","INVALID_JSON"); return;
    }
    if(!in["id"].is<uint32_t>() || in["id"].as<uint32_t>()==0 || !in["cmd"].is<const char *>()) {
      baoLoi(0,"","INVALID_ENVELOPE"); return;
    }
    uint32_t id=in["id"];
    const char *cmd=in["cmd"];
    if(!strcmp(cmd,"HELLO")) {
      if(dangDoc) { baoLoi(id,cmd,"BUSY"); return; }
      bom.tatTatCa(); session=soNgauNhien(); lanLienLac=millis();
      idGanNhat=0; lenhCu=""; traLoiCu="";
      JsonDocument out; chung(out,"response",id,cmd); out["ok"]=true;
      out["protocol"]=1; out["pi_timeout_ms"]=PI_TIMEOUT_MS;
      out["reset_reason"]=(uint32_t)esp_reset_reason(); trangThai(out); gui(out); return;
    }
    if(!session || !in["session"].is<uint32_t>() || in["session"].as<uint32_t>()!=session) {
      baoLoi(id,cmd,"HELLO_REQUIRED"); return;
    }
    String text; serializeJson(in,text);
    if(id==idGanNhat) {
      if(text!=lenhCu) { baoLoi(id,cmd,"ID_REUSED"); return; }
      if(!traLoiCu.length()) { baoLoi(id,cmd,"PENDING"); return; }
      JsonDocument old; deserializeJson(old,traLoiCu); old["replayed"]=true;
      gui(old); return; // Khong thuc hien lai, khong gia han heartbeat.
    }
    if(id<idGanNhat) { baoLoi(id,cmd,"OLD_ID"); return; }
    idGanNhat=id; lenhCu=text; traLoiCu="";
    if(!strcmp(cmd,"READ")) {
      if(dangDoc) { baoLoi(id,cmd,"BUSY",true); return; }
      const char *target=in["target"].isNull()?"ALL":in["target"].as<const char *>();
      int chon=chonCamBien(target);
      if(chon<0) { baoLoi(id,cmd,"INVALID_TARGET",true); return; }
      camBienDoc=chon;
      lanLienLac=millis(); dangDoc=true; choDoc=true; idDoc=id; phienDoc=session;
      JsonDocument out; chung(out,"accepted",id,cmd); out["ok"]=true; gui(out); return;
    }
    if(!strcmp(cmd,"RELAY")) {
      if(!in["sw1"].is<bool>() || !in["sw2"].is<bool>() || !in["sw3"].is<bool>()) {
        baoLoi(id,cmd,"BOOLEAN_REQUIRED",true); return;
      }
      const bool chon[3]={in["sw1"].as<bool>(),in["sw2"].as<bool>(),in["sw3"].as<bool>()};
      if(!bom.dat(chon)) { baoLoi(id,cmd,"RELAY_DISABLED",true); return; }
    } else if(!strcmp(cmd,"STOP")) bom.tatTatCa();
    else if(strcmp(cmd,"PING") && strcmp(cmd,"STATUS")) { baoLoi(id,cmd,"UNKNOWN_COMMAND",true); return; }
    lanLienLac=millis();
    JsonDocument out; chung(out,"response",id,cmd); out["ok"]=true; trangThai(out); gui(out,id);
  }
};
