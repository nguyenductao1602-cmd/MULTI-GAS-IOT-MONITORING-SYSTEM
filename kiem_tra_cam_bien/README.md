# Project riêng: kiểm tra cảm biến và đổi địa chỉ

Mở **kiem_tra_cam_bien.ino** trong Arduino IDE. Đây là sketch độc lập, không cần chép file từ project đo. Sketch đo `pumping_and_measuring_system` được giữ nguyên. Các relay IO15/16/17 luôn được giữ mức tắt; project này không có chức năng bật bơm.

## Nạp và mở menu

Chọn ESP32-S3, **USB CDC On Boot Enabled**, PSRAM Disabled. Serial Monitor **115200 baud**, chọn **Newline** hoặc **Both NL & CR**. IO43/44 đang dùng I2C nên không dùng UART0 trên hai chân này để xem log.

Mỗi lần khởi động hoặc mở lại Serial Monitor sẽ có hướng dẫn:

```text
NHAP CHUC NANG
F1: Doi dia chi (ID module)
F2: Check/test tung thiet bi
```

Gõ `F1` hoặc `F2` rồi Enter/Send. Không phân biệt hoa/thường. Nhập từng dòng và đợi lời nhắc tiếp theo; không gửi cả kịch bản nhiều bước một lúc. Khoảng trắng đầu/cuối được bỏ qua. Địa chỉ nhập **thập phân 1..255**, mã loại nhập **HEX 00..FF**: `29` và `0x29` đều là mã CH3SH 0x29; `0B` là NH3.

**`exit` luôn trở về menu chính**, kể cả trong lúc chờ khởi động RS485, chờ phản hồi, quét bus hoặc test liên tục. Các thao tác thư viện/I2C ngắn sẽ hoàn tất hoặc timeout trước khi xử lý exit. Nếu lệnh đổi địa chỉ đã phát ra bus thì exit không thể hoàn tác; chương trình sẽ báo điều đó và bạn cần đọc lại địa chỉ mới/cũ.

## F1 — đổi địa chỉ

Chỉ nối **một bộ SemeaTech** vào bus. Lệnh đổi 0x04 chỉ chứa địa chỉ mới nên không dùng để đổi một module khi nhiều module cùng nối bus. Chức năng này đổi ID giao tiếp, không đổi loại khí/tên khí hay hiệu chuẩn đầu cảm biến.

1. Nhập `F1`.
2. Nhập địa chỉ cũ.
3. Nhập địa chỉ mới.
4. Nhập mã cảm biến HEX.

Nếu thông tin hợp lệ và mã đúng, chương trình tự đổi rồi đọc lại địa chỉ mới. Nếu OLD=NEW thì chỉ xác minh, không ghi địa chỉ.

Nếu mã bạn nhập khác mã thực tế, chương trình báo ví dụ:

```text
[SAI MA] Ma cam bien da nhap khong dung: nhap 0x29 (CH3SH).
Ma code thuc te la 0x0B - loai cam bien NH3.
Ban co chac chan muon doi dia chi cho cam bien khi sai ma code khong?
y=van doi, n=huy
```

- `y`: cho phép đổi **module thực tế vừa được nhận dạng**; mã sai bạn nhập không được dùng để giả nhãn module.
- `n` hoặc `exit`: hủy và về menu chính.
- Giá trị khác: tiếp tục chờ y/n, không đổi.

Trước khi ghi, chương trình đọc lại địa chỉ cũ để kiểm tra module vẫn có cùng mã loại thực tế. Sau ghi, nó đọc lại địa chỉ mới và đối chiếu mã thực tế đó. ACK đúng nhưng không đọc lại được vẫn được báo là **chưa xác minh**, không tự kết luận thành công. Đọc lại đúng nhưng thiếu ACK được thông báo riêng.

Nếu nhận được khung CRC hợp lệ nhưng địa chỉ khác địa chỉ đã nhập, báo địa chỉ thực tế và yêu cầu nhập lại. Nếu không có phản hồi hoặc chỉ nhận dữ liệu lỗi thì **chưa thể biết chắc địa chỉ sai**: có thể do nguồn, dây, baud hoặc địa chỉ. Code báo rõ sự chưa xác nhận này, in TX/RX và cho nhập lại địa chỉ cũ; không đoán mã cảm biến.

Sau khi đổi thành công, tắt/cấp lại nguồn module, vào F1 với địa chỉ cũ=mới để xác minh địa chỉ được lưu sau mất điện.

| Module đang dùng | Địa chỉ | Mã HEX |
| --- | --- | --- |
| CH3SH | 1 | 29 |
| CO | 2 | 02 |
| H2S | 3 | 09 |
| NH3 | 4 | 0B |

Bảng tên hỗ trợ các mã trong tài liệu 4SMART và CH3SH 0x29 đã xác minh thực tế. Mã chưa có tên vẫn được in HEX và ghi `CHUA BIET`; không gán nhầm sang một khí khác.

## F2 — check/test từng thiết bị

Chọn số trong danh sách. Chỉ thiết bị đó được đọc/test, lặp lại sau khoảng 2 giây kể từ lúc hoàn thành lượt trước. `list` trở về danh sách test; `exit` về menu chính. Các mục quét chỉ chạy một lượt rồi hiện lại danh sách.

| Số | Thiết bị / chức năng |
| --- | --- |
| 1 | AHT20 môi trường, SDA36/SCL38 |
| 2 | BMP280 môi trường, SDA36/SCL38 |
| 3 | AHT20 buồng khí, SDA43/SCL44 |
| 4 | BMP280 buồng khí, SDA43/SCL44 |
| 5 | SCD30 buồng khí, SDA43/SCL44 |
| 6 | CH3SH RS485 địa chỉ 1 |
| 7 | CO RS485 địa chỉ 2 |
| 8 | H2S RS485 địa chỉ 3 |
| 9 | NH3 RS485 địa chỉ 4 |
| 10 | LCD GM12864-59N VER2.0 / ST7567S |
| 11 | Quét I2C môi trường |
| 12 | Quét I2C buồng khí |
| 13 | Tự nhập địa chỉ RS485 để đọc thông tin/mã thực tế/nồng độ |

Với log hiện tại của bạn: thử **F2 → 12** trước để xem bus buồng khí có ACK tại 0x38, 0x76/0x77 và 0x61 hay không; sau đó chọn 3/4/5. Thử **F2 → 8** riêng cho H2S. Muốn thử một địa chỉ khác, chọn 13. ACK chỉ xác nhận có thiết bị trên địa chỉ đó, không chứng minh loại cảm biến hay số đo đúng.

Các kết quả chi tiết:

- AHT20: kiểm tra ACK, trạng thái BUSY/calibrated, đủ byte, CRC8, nhiệt độ và độ ẩm. BUSY có giới hạn thời gian để không kẹt chương trình.
- BMP280: tự thử 0x76/0x77, đọc chip ID 0x58 trước khi khởi tạo. ID 0x60 được báo là BME280. In nhiệt độ/áp suất hoặc lỗi cụ thể.
- SCD30: đọc firmware, đặt chu kỳ 2 giây, bắt đầu đo liên tục; kiểm tra CRC8 từng word, data-ready và các giá trị CO2/nhiệt độ/độ ẩm. `DANG CHO` nghĩa là giao tiếp đúng nhưng chưa có mẫu mới; quá 10 giây chưa có mẫu được báo riêng. Không zero/calibrate. Sau exit, SCD30 có thể tiếp tục tự đo nhưng sketch không đọc thêm.
- RS485: in TX/RX raw, phân biệt không nhận byte, khung/CRC lỗi, sai địa chỉ, sai loại và sai định dạng dữ liệu. Nếu sai loại ở mục test, dữ liệu vẫn được đọc để chẩn đoán nhưng được gắn **tên khí thực tế**.
- LCD: hiển thị chữ, khung, số FRAME tăng và ô nhấp nháy. ACK không chứng minh màn hình nhìn thấy được; nhập `y` khi bạn nhìn thấy, `n` nếu không thấy. Khi exit, hình cuối có thể còn trên màn hình nhưng không cập nhật nữa.

## Cấu hình / file

`CauHinh.h` chứa chân, baud, thời gian timeout, địa chỉ LCD và contrast. Mặc định: I2C 100kHz, timeout 50ms; RS485 9600 8N1, TX41/RX42, module tự chuyển hướng (`DE_RE=-1`). Nếu thao tác ngay sau boot, RS485 chờ đủ 10 giây, có thể thoát bằng exit.

| File | Nội dung |
| --- | --- |
| `kiem_tra_cam_bien.ino` | Khởi động, giữ relay tắt, nhận menu |
| `MenuSerial.h` | Các bước F1/F2, xác nhận y/n, kết quả đổi địa chỉ |
| `NhapSerial.h` | Nhập từng dòng, kiểm tra số, xử lý exit khi đang giao tiếp |
| `GiaoThuc485.h` | Khung AA...EE, CRC16, đọc thông tin/nồng độ/đổi địa chỉ |
| `KiemTra.h` | Test cảm biến I2C, RS485, LCD và quét bus |
| `CauHinh.h` | Cấu hình phần cứng và thời gian |

Thư viện: **Adafruit BMP280 Library**, Adafruit BusIO, Adafruit Unified Sensor và **U8g2**. AHT20 và SCD30 được đọc trực tiếp qua Wire để in lỗi CRC/trạng thái rõ ràng. Giao tiếp SCD30 đối chiếu các lệnh trong nguồn thư viện Adafruit SCD30 đang có trên máy; RS485 giữ giao thức 4SMART đã kiểm chứng.

`node tests/verify-menu.mjs` giả lập các thân hàm thực để kiểm tra nhập số, các luồng F1/F2, sai mã/y/n, nhập lại địa chỉ, exit trước/sau ghi, kiểm tra lại loại module, CRC/echo/noise/timeout. Đây là kiểm tra phần mềm với IO giả lập, không thay thế thử trên ESP32 và cảm biến thật.
