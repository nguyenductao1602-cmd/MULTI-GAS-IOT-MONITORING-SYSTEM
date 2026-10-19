# ESP32-S3 chi lam I/O, Orange Pi quyet dinh lich

Mo `esp32_pi_io.ino`. Day la project MOI, khong ghi de `pumping_and_measuring_system`.
Ban code tu chay cu da duoc luu ZIP; xem duong dan trong `BACKUP.txt`.

## Phan chia cong viec

- Pi quyet dinh khi bat bom, chay bao lau, khi tat, cho on dinh bao lau va khi doc.
- ESP chi bat/tat relay theo lenh, doc mot luot khi co READ va tra JSON.
- ESP khong co chu ky bom/doc tu dong, khong co delay cho 1 phut/30s/60s.
- ESP van co timeout giao tiep cam bien va watchdog; cac khoang cho ngan nay khong phai lich van hanh.
- Pi phai gui PING moi 3s, dung ID moi. Qua 15s khong co lenh HOP LE thi ESP tat tat ca bom,
  huy session. Khong tu bat lai bom khi Pi ket noi lai. Trong luc cho RS485/I2C van xu ly USB.
- LCD chi ve mau gan nhat va tuoi mau; khong tu doc cam bien.
- SCD30 van tu do noi bo theo chu ky 60s; ESP chi lay ket qua khi Pi gui READ.
- Khi mat Pi, ESP khong tu doc hay luu so do. Chua them log flash/ACK luu tru o project nay.

## Phan cung duoc giu nguyen

| Nhom | Ket noi |
| --- | --- |
| MOI_TRUONG | SDA36/SCL38: AHT20, BMP280, LCD ST7567S 0x3F |
| BUONG_KHI | SDA43/SCL44: AHT20, BMP280, SCD30 0x61 |
| RS485 UART1 | TX41/RX42, 9600 8N1, TD301D485H-A tu chuyen huong |
| Khi | CH3SH=1/0x29; CO=2/0x02; H2S=3/0x09; NH3=4/0x0B |
| Relay | IO15/16/17, HIGH bat, LOW tat |
| USB Pi | IO19 D-, IO20 D+; dung cap USB co day data |

Ca hai bus I2C can pull-up ve 3.3V. IO43/44 chi dung I2C khi sketch chay: UART0 duoc dung
va hai chan duoc reset truoc Wire.begin. Log ROM truoc setup khong nam trong pham vi nay.
Khong thay doi eFuse. RS485 khong doi dia chi/zero/calibrate tu dong; van dung project
`kiem_tra_cam_bien` rieng de doi dia chi va chan doan.

## Cau hinh va thu vien

`CauHinh.h` giu dung ban dang dung: SW1=false, SW2=false, SW3=true. Day la QUYEN cho
phep dung relay, khong phai lenh bat. Pi bat relay bi khoa se nhan RELAY_DISABLED,
va ca lenh khong duoc ap dung mot phan. Tat relay luon duoc phep.

`PI_TIMEOUT_MS=15000` la bao ve mat Pi. `SCD30_MEASURE_TIME_SECONDS=60` dat chu ky noi bo
(2..1800s, tu gioi han). Pi can cho cam bien on dinh va gui READ theo lich cua Pi.
RS485 bao WARMING_UP neu READ trong 10s dau. SCD30 chua co mau moi se tra null va co loi.
Khoi dong/reinit AHT20/BMP280 chi la cau hinh phan cung, khong phat sinh goi so do tu dong.

Thu vien: ArduinoJson 7, Adafruit BMP280, Adafruit SCD30, Adafruit BusIO,
Adafruit Unified Sensor, U8g2. Da co tren may nay.

Board ESP32-S3-USB-OTG: USB Mode = Hardware CDC and JTAG;
Upload Mode = UART0 / Hardware CDC. Neu dung ESP32S3 Dev Module: USB CDC On Boot = Enabled,
PSRAM = Disabled. Code se bao loi bien dich neu Serial duoc gan UART0.
USB 115200, Newline hoac Both NL & CR. USB chi xuat JSON, khong tron log chu.

## Bo lenh USB protocol 1

Mot JSON/dong, ket thuc LF, toi da 384 byte (khong tinh LF), phan biet HOA/thuong.
Khong duoc bo sung ghi chu vao dong JSON. ID la so nguyen 1..4294967295.
Chi cho 1 Pi/process mo cong USB tai mot thoi diem; dong Serial Monitor khi dung Python.

1. Pi gui HELLO. Moi HELLO thanh cong se TAT tat ca relay va tao session moi.
2. Lay session tu phan hoi. Moi lenh sau do phai mang session nay.
3. ID cac lenh trong session phai tang dan. HELLO khong chiem ID trong session.
4. Pi gui PING voi ID moi moi 3s, ke ca trong luc dang cho READ.
5. Sau PI_TIMEOUT hoac ESP reset: HELLO lai, xac minh trang thai, khong tu bat lai bom cu.

```json
{"id":1,"cmd":"HELLO"}
```

Phan hoi HELLO co `session` va `boot_id` (so ngau nhien cho moi lan khoi dong),
`reset_reason` theo esp_reset_reason() va `relays:[false,false,false]`.
Vi du session nhan duoc la 2468:

```json
{"id":1,"session":2468,"cmd":"PING"}
{"id":2,"session":2468,"cmd":"STATUS"}
{"id":3,"session":2468,"cmd":"RELAY","sw1":false,"sw2":false,"sw3":true}
{"id":4,"session":2468,"cmd":"STOP"}
{"id":5,"session":2468,"cmd":"READ"}
```

RELAY dat CA BA relay trong mot lenh; Pi can gui du ba bool true/false.
Khong co tham so thoi gian: Pi tu dem thoi gian va gui STOP hoac RELAY tat.
Pi tuyet doi khong dung sleep chan heartbeat; dung lich/event loop/luong rieng.

| cmd | Ket qua |
| --- | --- |
| HELLO | Tat bom, tao session, tra trang thai, thong tin boot/reset |
| PING | Gia han lien lac, tra trang thai |
| STATUS | Trang thai GPIO logic, bus khoi tao, LCD ACK, tuoi mau; khong doc cam bien |
| RELAY | Dat 3 relay, tra trang thai sau khi dat |
| STOP | Tat tat ca relay ngay khi xu ly lenh, tra trang thai |
| READ | Gui accepted, doc tung cam bien mot lan, tra response cung ID |

READ co the mat khoang 9s neu 4 gas deu cham (2 lenh/module), cong thoi gian I2C.
Pi nen cho response READ toi da 20s nhung van gui PING/STOP neu can. Trong luc READ,
READ tiep theo va HELLO se bao BUSY; PING, STATUS, STOP, RELAY van duoc xu ly.
STOP KHONG huy READ, chi tat relay. Mac dinh bom tat sau reset va truoc HELLO.
Gioi han phan hoi STOP/timeout con phu thuoc lenh I2C ngan dang chay, khong la realtime cung.

### ID, thu lai va phan hoi

- Moi response co type=response, id, cmd, boot_id, session. type=accepted chua phai so do.
- Thanh cong `ok:true`; loi `ok:false,error:...`.
- Lap lai dung lenh CO ID GAN NHAT: tra lai ket qua cache voi `replayed:true`, khong tac dong
  relay hay doc lai. Cache la lich su, khong khang dinh relay hien tai; dung STATUS de biet hien tai.
- Cung ID nhung noi dung khac: ID_REUSED; ID nho hon ID gan nhat: OLD_ID.
- Lap READ dang cho: PENDING; muon doc tiep phai cho response READ va dung ID moi.
- Moi chi co 1 ket qua cache gan nhat, khong phai hang doi giao dich ben vung. Neu PING da dung
  ID cao hon READ, gui lai ID READ cu nhan OLD_ID; Pi co the gui READ moi de lay mau moi.
- Gui lai cache KHONG gia han heartbeat. Lenh sai/khong ro/session sai KHONG gia han heartbeat.
- Lenh loi da nhan ID trong session van tieu thu ID: sua lenh thi dung ID moi.
- Hang doi USB co gioi han 8 ban tin. Khi Pi khong doc kip, ESP khong treo; STATUS co dropped_frames.
  Khong gui lien tuc hang tram lenh/giay. Day khong phai giao thuc dam bao luu tru/khong mat mau.

### Du lieu so do

`MOI_TRUONG.AHT20`, `MOI_TRUONG.BMP280`, `BUONG_KHI.AHT20`, `BUONG_KHI.BMP280`,
`BUONG_KHI.SCD30`, `BUONG_KHI.RS485.{CH3SH,CO,H2S,NH3}`.
Moi cam bien co `ok` va `error`. Don vi trong ten key: temperature_c, humidity_rh,
pressure_hpa, co2_ppm, ppm. Gia tri loi la JSON null, KHONG doi thanh 0. So am duoc giu nguyen.
`sample_seq`, `read_start_ms`, `read_end_ms` la moc uptime cua ESP, KHONG phai ngay gio thuc.
Pi gan timestamp UTC va luu boot_id + sample_seq de phan biet ESP reset.
`relay_at_start` va `relays` cho biet bom luc bat dau va ket thuc doc; mau khong duoc do dong thoi.

Loi RS485: WARMING_UP, UART_INIT_FAILED, NO_RESPONSE, INVALID_FRAME_OR_CRC,
WRONG_GAS_TYPE, WRONG_UNIT, INVALID_DATA. AHT/BMP: INIT_OR_READ_FAILED.
SCD30: INIT_FAILED, NO_NEW_SAMPLE_OR_I2C_ERROR, INVALID_DATA; thu vien hien tai khong
phan biet het truong hop chua co mau va loi bus. SCD30 khoi dong that bai thi can kiem tra
phan cung va reset ESP; khong tu reset/zero/calibrate cam bien trong nen.
`bus*_ok` chi la ket qua Wire.begin, khong chung minh cam bien tren bus con hoat dong.
`lcd_ok` chi ACK, khong chung minh man hinh co hien thi ro.
`relays` la trang thai chan dieu khien, KHONG phan hoi tiep diem hay dong dien bom thuc.

## Thu tu laptop hoac Pi

Cong cu `tools/thu_usb.py` chi thu USB, chua phai chuong trinh Pi van hanh 24/7.
No tu HELLO va PING moi 3s, KHONG tu bat bom hoac doc, chi lam khi ban nhap lenh.

```text
python -m pip install pyserial
python tools/thu_usb.py --port COM15
```

Tren Linux thay COM15 bang duong dan /dev/serial/by-id/... cua ESP.
Lenh trong cong cu: `status`, `read`, `relay 0 0 1`, `stop`, `hello`, `quit`.
Relay 1/2 dang khoa bang SW; lenh bat chung se bi tu choi. `quit` co gui STOP truoc khi dong.
Thu tat cong cu/dut USB khi relay dang bat, xac minh ESP tat bom sau toi da 15s + do tre giao tiep.
Neu ESP mat nguon thi phan cung phai giu relay tat (pull-down ngoai neu can).

## Kiem tra da lam

`node tests/verify_protocol.mjs` chay than ham C++ duoc chuyen cu phap de mo phong,
mock GPIO/USB: ID, session, lenh loi, READ busy, STOP/PING khi dang doc, timeout/rollover,
input phan manh/qua dai, relay masks, khong co lich tu dong. Day khong thay the test tren bo.
Da bien dich ESP32-S3; chua nap/thu tren phan cung trong lan tao project nay.

## Cac file chinh

- esp32_pi_io.ino: setup va loop, tao cac doi tuong.
- GiaoTiepPi.h: JSON, session/ID, heartbeat, hang doi USB va bao ve mat Pi.
- DieuKhienBom.h: chi dat GPIO theo lenh; khong co lich chay tu dong.
- CamBien.h / CamBienKhi.h: giao tiep I2C / RS485 va du lieu loi.
- ManHinh.h: ve mau gan nhat; CauHinh.h: chan, SW, timeout va chu ky SCD30.

Heartbeat chi chung minh phan mem Pi con gui lenh, khong chung minh moi thanh phan
tren Pi deu tot. Chuong trinh Pi van hanh can giam sat ca lich bom va tien trinh luu tru;
khong de mot luong heartbeat van gia han relay neu phan dieu khien chinh da loi.

## READ tung cam bien (capability read_sensor)

HELLO/STATUS nay co capabilities:["read_sensor"]. READ khong co target van doc tat ca.
Them target vi du "BUONG_KHI.H2S" hoac "MOI_TRUONG.AHT20" chi giao tiep voi cam bien do.
Cac target: ALL, MOI_TRUONG.AHT20, MOI_TRUONG.BMP280, BUONG_KHI.AHT20, BUONG_KHI.BMP280,
BUONG_KHI.SCD30, BUONG_KHI.CH3SH, BUONG_KHI.CO, BUONG_KHI.H2S, BUONG_KHI.NH3.
Phan hoi chi chua cam bien da chon va target; khong danh dau cac mau cu la moi.
USB chi co ban tin JSON can thiet, khong co log chu TX/RX tu dong.
