#line 1 "D:\\All_My_Data\\Arduino\\A_Dat_CO2\\pumping_and_measuring_system\\backup_truoc_tach_lop\\README.md"
# ESP32-S3: đọc liên tục và điều khiển bơm

Mở `pumping_and_measuring_system.ino` trong Arduino IDE. Mặc định `CONFIGURE_ADDRESS_MODE=false` để đo. Dữ liệu được gửi qua Serial Monitor 115200; chưa có gửi mạng.

## Chỉnh thời gian

| Biến | Mặc định | Đơn vị / tác dụng |
| --- | --- | --- |
| `DELAY_START_TIME_MINUTES` | 1 | Phút chờ khởi động trước khi bơm/đọc |
| `PUMP_TIME_SECONDS` | 10 | Giây bật chung các relay có SW=true |
| `SETTLE_TIME_SECONDS` | 0 | Giây chờ sau khi tắt bơm |
| `REPEAT_WAIT_SECONDS` | 60 | Giây chờ sau giai đoạn ổn định, trước khi bơm lại |
| `cycle_read_time` | 3000 | Mili giây giữa các lượt đọc/in, 3000 = 3 giây |

Lịch bơm và lịch đọc chạy riêng bằng `millis()`. Chu kỳ bơm: bật 10 giây → tắt, ổn định 0 giây → chờ 60 giây → lặp. `SW1/2/3` chọn relay ở IO15/16/17, các relay được chọn bật cùng lúc. Nếu cả ba SW=false, relay đều tắt, dữ liệu vẫn đọc. `RELAY_ON=LOW`, đổi HIGH nếu mạch kích mức cao.

Sau thời gian khởi động, lượt đọc đầu tiên bắt đầu ngay. Mặc định `READ_ONLY_AFTER_SETTLE=false`: đọc cả khi bơm bật và mọi giai đoạn chờ. Đặt true để chỉ bắt đầu lượt đọc khi bơm đã tắt và hết thời gian ổn định; lượt đọc đang thực hiện được hoàn thành ngay cả khi trạng thái bơm đổi. Có thể thấy thông báo trạng thái bơm xen giữa dữ liệu.

Không còn tính trung bình, cửa sổ `MEASURE_TIME_MS` hoặc `SAMPLE_INTERVAL_MS`. Mỗi lượt chỉ đọc từng cảm biến một lần và in giá trị trực tiếp. Nhãn `inside` = trong ống, `outside` = ngoài môi trường. NH3 địa chỉ 4 đã được thêm cùng SO2=1, CO=2, H2S=3; cần đổi địa chỉ NH3 thành công trước khi dùng chung bus.

## Xử lý các giá trị thời gian

- Dùng số nguyên không âm trong các biến uint32_t. Không nhập số âm hoặc số lớn hơn 4294967295.
- Khởi động/ổn định/chờ bằng 0 được bỏ qua; bơm bằng 0 luôn giữ relay tắt, không phát xung bật.
- `cycle_read_time=0` tự dùng 500ms để tránh vòng đọc không nghỉ. Nếu tất cả thời gian bơm/ổn định/chờ bằng 0, chương trình dùng thời gian chờ 1 giây.
- Chuyển phút/giây sang mili giây bằng số 64 bit; khoảng thời gian thực tế giới hạn 2147483647ms (khoảng 24,9 ngày) để tránh tràn số.
- Thời gian đọc thực tế phụ thuộc I2C/UART, timeout và in Serial. Nếu một lượt mất lâu hơn `cycle_read_time`, chương trình hoàn thành lượt đó rồi chờ khoảng đọc tiếp theo; không đọc dồn để bù lượt trễ.
- Relay được cập nhật trong lúc chờ AHT20/UART và giữa các cảm biến. Có sai số nhỏ do giao tiếp I2C, UART và in Serial, không phải điều khiển thời gian thực tuyệt đối.

SCD30 được cấu hình chu kỳ 2 giây. Khi chưa có mẫu mới, lượt đó in N/A, không dùng lại dữ liệu cũ. Đọc nhanh hơn 2 giây không làm SCD30 có thêm mẫu mới.

## Đấu dây và thư viện

| Nhóm | Thiết bị | Chân ESP32-S3 | Địa chỉ |
| --- | --- | --- | --- |
| Trong ống, I2C 1 | AHT20 | SDA IO36, SCL IO38 | 0x38 |
| Trong ống, I2C 1 | BMP280 | SDA IO36, SCL IO38 | 0x76 hoặc 0x77, tự thử |
| Trong ống, I2C 1 | SCD30 | SDA IO36, SCL IO38 | 0x61 |
| Môi trường, I2C 2 | AHT20 | SDA IO2, SCL IO1 | 0x38 |
| Môi trường, I2C 2 | BMP280 | SDA IO2, SCL IO1 | 0x76 hoặc 0x77, tự thử |
| Trong ống, RS485 | SemeaTech SO2 | TX IO41, RX IO42 qua bộ chuyển UART/RS485 | 1 |
| Trong ống, RS485 | SemeaTech CO | Cùng bus RS485 | 2 |
| Trong ống, RS485 | SemeaTech H2S | Cùng bus RS485 | 3 |
| Trong ống, RS485 | SemeaTech NH3 | Cùng bus RS485 | 4 |
| Bơm | Relay 1 / 2 / 3 | IO15 / IO16 / IO17 | — |

Yêu cầu ghi “SCL2 IO1, SCL2 IO2”: chương trình hiểu **SCL2 IO1, SDA2 IO2**. LCD chỉ có comment, chưa khởi tạo.

Bạn đã cung cấp sơ đồ **TD301D485H-A**: **IO41 (ESP TX) nối TXD chân 3 (đầu vào module); IO42 (ESP RX) nối RXD chân 4 (đầu ra module)**. Tên TXD/RXD trên transceiver này theo phía MCU, không nối chéo theo tên. VCC chân 1 cấp 3.3V, GND chân 2 nối GND ESP32. A chân 9/B chân 8 nối A+/B- của transmitter SemeaTech. RGND chân 10 là mass phía RS485 cách ly, dùng làm tham chiếu với phía cảm biến; VO chân 7 là đầu ra nguồn cách ly, không phải đầu vào nguồn chính. Không nối tắt GND và RGND nếu muốn giữ cách ly. Xem [datasheet MORNSUN](https://www.mornsun-power.com/public/uploads/pdf/TDx01D485H-A.pdf).

PN 300-0039-000 cấp nguồn theo datasheet (9–26V; tổ hợp 4SMART-C01-EC được ghi 9–24V), không cấp điện áp này vào GPIO ESP32 hoặc VCC của TD301D485H-A. Đầu cảm biến điện hóa rời phải gắn module 4SMART-C01 tương thích trước khi dùng transmitter.

Mặc định bộ chuyển phía ESP32 tự chuyển hướng. Nếu dùng loại cần DE và /RE, nối chung hai chân điều khiển vào GPIO còn trống và sửa `RS485_DE_RE` từ -1 sang số GPIO đó. HIGH phát, LOW nhận.

Cài qua Library Manager:

- Adafruit BMP280 Library
- Adafruit SCD30
- Adafruit BusIO và Adafruit Unified Sensor (thư viện phụ thuộc)

AHT20 dùng hàm I2C trực tiếp có kiểm tra CRC8 và giới hạn thời gian; không cần cài Adafruit AHTX0. BMP280 giữ cách dùng thư viện Adafruit của code ban đầu và có đối tượng riêng cho mỗi bus.

Chọn ESP32S3 Dev Module, PSRAM Disabled, USB CDC On Boot Enabled nếu dùng USB native để xem Serial Monitor; baud 115200. UART RS485 dùng 9600, 8N1.

## Phải đặt địa chỉ trước khi ghép bus RS485

Theo tài liệu 4SMART-C01, địa chỉ mới xuất xưởng là **1**. Không nối nhiều module cùng địa chỉ khi đọc hoặc đổi địa chỉ. Lệnh đổi địa chỉ `0x04` chỉ mang địa chỉ mới, không có địa chỉ cũ để chọn một module trong nhóm.

Thực hiện từng module:

1. Ngắt các module còn lại, chỉ nối một module vào bus. Relay luôn tắt trong chế độ cấu hình.
2. Đặt `CONFIGURE_ADDRESS_MODE=true`, `OLD_ADDRESS=1` và cấu hình theo bảng dưới; nạp code.
3. Xem Serial Monitor: chương trình kiểm tra đúng loại khí/đơn vị ppm, gửi lệnh đổi địa chỉ, kiểm tra CRC/ACK rồi đọc lại thông tin ở địa chỉ mới.
4. Cấp lại nguồn module. Đặt `OLD_ADDRESS=NEW_ADDRESS` và chạy lại chế độ cấu hình để xác minh địa chỉ được giữ sau khi mất điện. Nếu module đã được đổi địa chỉ từ trước, dùng địa chỉ thực tế của nó làm OLD_ADDRESS.
5. Sau khi làm xong các module: đặt `CONFIGURE_ADDRESS_MODE=false`, nạp lại code, rồi ghép các module lên cùng bus.

| Module | NEW_ADDRESS | CONFIGURE_GAS_TYPE |
| --- | --- | --- |
| SO2 | 1 | 0x0A |
| CO | 2 | 0x02 |
| H2S | 3 | 0x09 |
| NH3 | 4 | 0x0B |

SO2 giữ địa chỉ 1 nên chỉ cần xác minh, không cần gửi lệnh đổi. Chế độ đổi địa chỉ không chạy bơm và không tự lặp lệnh đổi trong `loop()`.

## Dữ liệu và giới hạn

Mỗi giá trị được kiểm tra CRC/timeout và miền hợp lệ; lỗi in N/A, không thay bằng 0. Cảm biến SemeaTech được kiểm tra loại khí và đơn vị ppm trước khi đọc nồng độ. Module chưa nhận dạng được sẽ thử lại mỗi 10 giây. Không tự zero/calibrate. Cảm biến I2C khởi tạo lỗi sẽ in N/A tới lần reset tiếp theo.

PN 051-1200-000 là CO; CO2 lấy riêng từ SCD30. Bảng mã khí trang 3 tài liệu 4SMART: SO2=0x0A, CO=0x02, H2S=0x09, NH3=0x0B. Giao thức AA...EE có CRC16 Modbus, không phải Modbus RTU đọc thanh ghi.

Sơ đồ trước đó có net RX_485 tại IO1/IO42 và TX_485 tại IO2/IO41. Nếu PCB thực tế nối chung, cần tách IO1/IO2 khỏi UART để dùng I2C ngoài môi trường theo yêu cầu.

## Kiểm tra

`node tests/verify-logic.mjs` kiểm tra CRC, giải mã khí, 8 tổ hợp SW, thời gian bằng 0, thời gian ổn định, bộ đếm millis qua điểm tràn, giới hạn chuyển đổi đơn vị, lịch đọc/quá hạn và cách ly chế độ đổi địa chỉ. Đây là giả lập logic từ hàm trong firmware, không thay thế thử toàn bộ trên phần cứng.

Biên dịch cho ESP32 Arduino core 3.3.12, board ESP32-S3, PSRAM disabled, USB CDC enabled. Người dùng đã xác nhận phần cứng đọc CO đúng CRC sau khi sửa trở kháng RS485; phiên bản đọc liên tục mới chưa được nạp thử trên bo.
