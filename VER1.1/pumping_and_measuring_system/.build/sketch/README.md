#line 1 "D:\\All_My_Data\\Arduino\\A_Dat_CO2\\pumping_and_measuring_system\\README.md"
# ESP32-S3: môi trường, buồng khí, bơm và LCD

Mở `pumping_and_measuring_system.ino` trong thư mục này bằng Arduino IDE. Tất cả file `.h` phải nằm cùng thư mục sketch. Code dùng các lớp đơn giản, không kế thừa, không tạo task song song.

## Đấu dây mới

| Khu vực | Bus / chân ESP32-S3 | Thiết bị |
| --- | --- | --- |
| `MOI_TRUONG` | Wire, SDA IO36, SCL IO38 | AHT20 0x38, BMP280 0x76/0x77, LCD GM12864-59N VER2.0 0x3F |
| `BUONG_KHI` | I2C số 1, SDA IO43, SCL IO44 | AHT20 0x38, BMP280 0x76/0x77, SCD30 0x61 |
| `BUONG_KHI` | UART1 TX IO41, RX IO42 qua RS485 | CH3SH địa chỉ 1, CO 2, H2S 3, NH3 4 |
| Bơm | IO15, IO16, IO17 | Relay 1, 2, 3 |

Bạn đã xác nhận “BMI280” là **BMP280**. Hai module AHT20+BMP280 được đặt trên hai bus riêng nên có thể dùng cùng địa chỉ. SCD30 chỉ nằm trên bus buồng khí; LCD chỉ nằm trên bus môi trường.

IO43/44 đang dùng cho I2C, không dùng đồng thời cho UART0. Xem Serial Monitor qua USB native với **USB CDC On Boot Enabled**, baud 115200. Board ESP32-S3, PSRAM Disabled theo phần cứng đã xác nhận.

TD301D485H-A: IO41 → TXD chân 3, IO42 ← RXD chân 4, VCC 3.3V, `RS485_DE_RE=-1`. Giữ phần RS485 đã sửa và thử thành công. Module điện hóa phải gắn bo 4SMART/transmitter tương thích; code không tự hiệu chuẩn khi thay đầu cảm biến.

## Các file và cách đọc code

| File | Nội dung |
| --- | --- |
| `pumping_and_measuring_system.ino` | Tạo đối tượng, khởi động và điều phối trong setup/loop |
| `CauHinh.h` | Chân, thời gian, SW, địa chỉ, chế độ đổi địa chỉ, LCD |
| `CamBien.h` | Lớp `KhuVucCamBien`: AHT20, BMP280, SCD30 trên một bus |
| `CamBienKhi.h` | Lớp `HeCamBienKhi`: RS485, CRC, nhận dạng và đổi địa chỉ |
| `ManHinh.h` | Lớp `ManHinh`: LCD ST7567S, bố cục và tự chuyển trang |
| `DieuKhienBom.h` | Lớp `DieuKhienBom`: 3 relay, chu kỳ bật/ổn định/nghỉ |
| `ThoiGian.h` | Lớp `LichDoc`, chuyển đơn vị có giới hạn chống tràn |
| `DuLieu.h` | Các giá trị đo, kiểm tra giá trị và hàm in Serial |

`MOI_TRUONG` và `BUONG_KHI` là hai đối tượng cùng lớp `KhuVucCamBien`, mỗi đối tượng giữ bus và dữ liệu riêng. Ví dụ `MOI_TRUONG.doc()` đọc cảm biến ngoài môi trường; `MOI_TRUONG.duLieu.apSuatBMP280` là áp suất của BMP280 tại đó. `BUONG_KHI` được tạo với `true` để dùng thêm SCD30.

Trong `loop()`: cập nhật bơm → nếu đến lịch thì đọc từng khu vực và RS485, in Serial → cập nhật LCD. Callback `capNhatBom` giúp cập nhật relay cả khi AHT20/UART/LCD đang chờ hoặc truyền dữ liệu. LCD chỉ hiển thị bản đọc gần nhất, không phát sinh thêm lượt đo.

## Chỉnh cấu hình

Mở **CauHinh.h**. Các giá trị thời gian hiện giữ đúng bản bạn đang dùng:

| Biến | Hiện tại | Ý nghĩa |
| --- | --- | --- |
| `DELAY_START_TIME_MINUTES` | 1 | Phút chờ khởi động trước khi đọc/bơm |
| `PUMP_TIME_SECONDS` | 0 | Giây bật bơm; 0 giữ tất cả bơm tắt |
| `SETTLE_TIME_SECONDS` | 10 | Giây ổn định sau giai đoạn bơm |
| `REPEAT_WAIT_SECONDS` | 60 | Giây nghỉ trước chu kỳ bơm tiếp theo |
| `cycle_read_time` | 60000 | Mili giây giữa các lượt đọc: hiện là 60 giây |
| `READ_ONLY_AFTER_SETTLE` | false | Đọc ở mọi giai đoạn sau khởi động |
| `LCD_PAGE_TIME_MS` | 5000 | Tự đổi trang LCD mỗi 5 giây |

`SW1/2/3=true` chọn dùng relay; các relay được chọn cùng bật, cùng tắt. `PUMP_TIME_SECONDS=0` vẫn giữ bơm tắt dù SW=true. `RELAY_ON=LOW` là kích mức thấp; đổi HIGH nếu phần cứng cần kích mức cao.

Muốn chỉ bắt đầu đọc sau khi bơm tắt và đã ổn định, đặt `READ_ONLY_AFTER_SETTLE=true`. Lượt đang đọc vẫn hoàn tất nếu bơm chuyển trạng thái. Không lấy trung bình: mỗi lượt đọc từng cảm biến một lần và in trực tiếp. Không có mẫu hoặc đọc lỗi thì N/A. SCD30 cần mẫu mới và được cấu hình chu kỳ 2 giây; đặt lịch nhanh hơn không tạo thêm mẫu mới.

Nhập số nguyên không âm trong phạm vi uint32_t. Khoảng đọc bằng 0 tự dùng 500ms. Thời gian bơm bằng 0 không phát xung bật relay. Nếu ba thời gian bơm/ổn định/nghỉ đều bằng 0 thì dùng nghỉ 1 giây. Thời gian sau chuyển đơn vị giới hạn 2147483647ms, khoảng 24,9 ngày. Nếu giao tiếp kéo dài quá khoảng đọc thì bỏ lượt trễ, không đọc dồn. Thời điểm relay có sai số nhỏ do giao tiếp và in Serial; đây không phải bộ điều khiển thời gian thực chính xác tuyệt đối.

## LCD GM12864-59N VER2.0

Driver ST7567S dùng U8g2, bus phần cứng Wire SDA36/SCL38, tốc độ 100kHz. Địa chỉ cấu hình là **7-bit 0x3F**, bên trong lớp màn hình tự chuyển sang dạng địa chỉ U8g2. Nếu module không ACK, chương trình báo `LCD khong phan hoi` rồi tiếp tục đọc cảm biến. Có thể tắt LCD bằng `SU_DUNG_LCD=false`. Chỉnh `LCD_CONTRAST` từ 0–255 khi cần.

Bốn trang tự chuyển:

1. MOI_TRUONG: nhiệt độ/độ ẩm AHT20, nhiệt độ/áp suất BMP280.
2. BUONG_KHI / AHT+BMP: bốn thông số tương ứng trong buồng khí.
3. BUONG_KHI / SCD30: CO2, nhiệt độ, độ ẩm và trạng thái bơm.
4. BUONG_KHI / GAS: CH3SH, CO, H2S, NH3, đơn vị ppm.

Dòng cuối `Mau 12s` nghĩa là lượt đọc hiển thị bắt đầu cách đây 12 giây; không phải LCD đã đo lại. Màn hình dùng chữ không dấu để dễ đọc trên 128×64 pixel. Khi chờ khởi động, LCD hiển thị thời gian còn lại.

Nếu LCD mất kết nối trong lúc chạy, lớp màn hình dừng gửi để tránh liên tục timeout; cắm lại rồi reset để khởi tạo lại. Cảm biến I2C không khởi tạo được cũng cần reset sau khi sửa kết nối. RS485 chưa nhận dạng được thử lại mỗi 10 giây khi tới lượt đọc.

## Đổi địa chỉ RS485

Mặc định `CHE_DO_DOI_DIA_CHI=0`, tức `CONFIGURE_ADDRESS_MODE=false` để chạy đo. Khi đổi địa chỉ, chỉ nối **một module**:

1. Trong CauHinh.h đổi `#define CHE_DO_DOI_DIA_CHI 0` thành `1`.
2. Chỉnh `OLD_ADDRESS`, `NEW_ADDRESS`, `CONFIGURE_GAS_TYPE`.
3. Nạp, xem log kiểm tra loại khí/đơn vị, CRC và xác minh địa chỉ mới. Bơm luôn tắt; không khởi tạo I2C/LCD trong chế độ này.
4. Cấp lại nguồn, đặt OLD=NEW để kiểm tra địa chỉ đã lưu. Nếu OLD=NEW, code chỉ kiểm tra, không gửi lệnh đổi.
5. Đặt `CHE_DO_DOI_DIA_CHI` về 0 và nạp lại trước khi ghép các module chung bus.

| Khí | Địa chỉ | Mã loại |
| --- | --- | --- |
| CH3SH | 1 | 0x29 |
| CO | 2 | 0x02 |
| H2S | 3 | 0x09 |
| NH3 | 4 | 0x0B |

CH3SH thay SO2. Mã 0x29 đã xác minh từ khung thực tế `AA 0F 01 29 00 0A 00 05 00 0A 00 05 02 A4 CC EE`, CRC đúng. Các cảm biến khí ở buồng khí; CO2 riêng từ SCD30. Không tự zero/calibrate.

## Thư viện và kiểm tra

Cần Adafruit BMP280 Library, Adafruit SCD30, Adafruit BusIO, Adafruit Unified Sensor và U8g2. AHT20 dùng I2C trực tiếp có CRC8 và timeout. U8g2 2.36.18 có sẵn trên máy đã được chép vào sketchbook đang dùng `D:/Documents/Arduino/libraries`.

Chạy `node tests/verify-logic.mjs` để kiểm tra các thân hàm thực trong header bằng giả lập IO: tám tổ hợp SW, thời gian 0, ổn định, tràn millis, lịch đọc/quá hạn, CRC/giải mã khí, chặn đổi địa chỉ khi sai loại và ánh xạ bus. Kiểm tra biên dịch không thay thế thử trên bo và LCD thật.

Bản trước khi tách lớp được lưu tại `backup_truoc_tach_lop`; file `.ino.original` cũ cũng được giữ.

Nguồn driver màn hình: [U8g2 — ST7567 ENH_DG128064I](https://github.com/olikraus/u8g2/wiki/u8g2setupcpp#st7567-enh_dg128064i), [API setI2CAddress](https://github.com/olikraus/u8g2/wiki/u8g2reference#seti2caddress), [ví dụ GM12864-59N V2 của tác giả](https://github.com/sskeledzija/lcd-st7567s-128x64-i2c).
