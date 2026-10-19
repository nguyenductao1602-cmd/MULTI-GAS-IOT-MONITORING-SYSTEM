# Giao diện quan trắc đa khí — Windows / Ubuntu

Ứng dụng Python điều khiển ESP32-S3 qua USB. Máy tính/Pi quyết định khi nào bật bơm, khi nào đọc và thứ tự thực hiện; ESP chỉ thực hiện lệnh và bảo vệ khi mất liên lạc. Giao diện chạy trong trình duyệt, không cần Internet.

## Chạy trên laptop Windows

1. Cài Python nếu máy chưa có. Máy hiện tại đã có Python 3.14.
2. Nhấp đúp `CAI_THU_VIEN.bat` một lần để cài thư viện.
3. Nhấp đúp `CHAY_WINDOWS.bat`. Trình duyệt mở tại **http://127.0.0.1:8765**. Giữ cửa sổ chương trình đang chạy.
4. Chọn **Mô phỏng → Kết nối** để thử giao diện trước. Chọn **Đọc một lần** để tạo số đo thử.
5. Vào **Cấu hình & cơ chế**, sửa thông số, nhấn **Lưu cấu hình** rồi **Bắt đầu lịch**.

Nếu chạy trong PowerShell, mở ngay thư mục này rồi nhập:

```powershell
python -m pip install -r requirements.txt
python app.py --open
```

Chạy thử hoàn toàn bằng mô phỏng:

```powershell
python app.py --demo --open
```

Mở trình duyệt không tự bật bơm. Kết nối hoặc khởi động lại ứng dụng cũng không tự chạy lịch; phải nhấn Bắt đầu. Đóng trình duyệt không dừng lịch. Nhấn **Dừng tất cả** trước khi thoát; dùng Ctrl+C trong cửa sổ Python để tắt chương trình. Máy Windows cần tiếp tục chạy và không ngủ để vận hành lịch.

## Kết nối ESP thật

1. Dùng firmware trong project bên cạnh: `../esp32_pi_io/esp32_pi_io.ino`. Bản cập nhật hỗ trợ đọc riêng từng cảm biến; cần nạp lại nếu muốn dùng chu kỳ riêng. Giao thức đọc toàn bộ vẫn tương thích bản cũ.
2. Cắm USB native của ESP tại IO19/20. Đóng Arduino Serial Monitor, RealTerm và `thu_usb.py` để nhường cổng.
3. Chọn **USB / ESP32**, chọn COM thực tế (ví dụ COM15), nhấn **Kết nối**.
4. Thử **Đọc một lần**, kiểm tra các thẻ cảm biến, sau đó mới chạy lịch.

Firmware hiện giữ giới hạn SW: SW1=false, SW2=false, SW3=true. Giao diện không bật được relay bị khóa trong firmware. Muốn sử dụng bơm 1/2, sửa SW tương ứng và nạp lại ESP. Relay HIGH = bật, LOW = tắt.

USB JSON là kênh giao tiếp bắt buộc giữa Python và ESP. Không tắt kênh này khi vận hành. UART0 không dùng để ghi debug; IO43/44 dành cho I2C buồng khí. Nhật ký USB chẩn đoán trên Python mặc định tắt; bật ở Cấu hình khi cần. Số đo, lỗi và sự kiện quan trọng vẫn được lưu dù debug tắt.

## Cài lịch và cơ chế

- **Tự động lặp:** chạy lần lượt các bước, hết bước cuối quay lại đầu sau khoảng nghỉ tối thiểu 0,5 giây. Thêm bước Chờ cuối để đặt thời gian nghỉ mong muốn.
- **Giờ cố định:** nhập các mốc như `08:00, 12:00, 16:00`, theo giờ máy chạy Python. Mỗi mốc chạy một lần trong ngày khi lịch đang hoạt động; không chạy chồng hay bù các mốc đã qua khi máy tắt/bận.
- **Cơ chế:** thêm, xóa và di chuyển bước Chạy bơm / Chờ / Đọc cảm biến. Có mẫu Bơm → đọc và Đọc → bơm. Bước bơm bật đồng thời tất cả bơm được chọn và được SW cho phép; hết thời gian tự tắt trước bước kế tiếp.
- **Chờ khởi động:** tính từ lúc nhấn Bắt đầu, đơn vị giây.
- **Đọc toàn bộ:** có thể chỉ đọc tại bước Đọc, hoặc thêm chu kỳ độc lập.
- **Chu kỳ riêng:** bật cho từng cảm biến cần lấy mẫu riêng. Các lượt đọc được xếp lần lượt; không chồng lên nhau. Đọc toàn bộ cũng cập nhật mốc đọc riêng, tránh đọc trùng ngay sau đó.
- Chu kỳ độc lập/riêng có thể đọc trong khi bơm đang chạy. Nếu chỉ muốn đo sau khi tắt bơm, dùng bước Đọc trong cơ chế và tắt các chu kỳ độc lập.
- Chu kỳ là khoảng yêu cầu, không đảm bảo lấy mẫu nhanh hơn thời gian phần cứng. Nếu đọc chậm hơn chu kỳ, không tạo hàng đợi vô hạn. SCD30 chỉ có mẫu mới theo chu kỳ nội bộ đặt trong firmware.
- Thời gian cho phép 0 đến 604800 giây; chu kỳ đọc từ 0,5 giây. Thử bơm thủ công từ 0,5 đến 600 giây. Chương trình từ chối số không hợp lệ.
- Muốn sửa cấu hình, dừng lịch và chờ đọc xong/bơm tắt rồi lưu.

**Dừng tất cả** hủy lịch và yêu cầu ESP tắt bơm. Một lần đọc đang thực hiện có thể trả kết quả cuối, nhưng không làm lịch chạy lại. Khi mất kết nối, chương trình hủy lịch; kết nối trở lại ở trạng thái chờ, không tự bật lại. ESP có timeout liên lạc 15 giây; không thể thay thế bằng việc đóng trình duyệt.

## Số đo, đồ thị và nhật ký

Có 9 thiết bị/cảm biến và 15 đại lượng: nhiệt độ, độ ẩm, áp suất của môi trường/buồng khí, CO2 SCD30 và CH3SH/CO/H2S/NH3. Trang Đồ thị lọc vị trí/nhóm, xem từ 1 giờ đến 7 ngày và xuất CSV.

Mẫu lỗi hiển thị N/A và tạo khoảng trống trên đồ thị; không thay bằng 0. Giá trị âm từ cảm biến khí giữ nguyên. Khi đọc riêng, thời điểm cập nhật của các cảm biến khác giữ nguyên. Đồ thị gom theo khoảng thời gian để giảm tải; nếu vượt 50000 bản tin trong khoảng chọn sẽ có thông báo giới hạn. CSV dùng dữ liệu đã lưu, không dùng các điểm đồ thị rút gọn.

Dữ liệu được ghi vào `data/quan_trac.sqlite3` gồm số đo, cấu hình và sự kiện. Mô phỏng và số đo thật được phân biệt; CSV/đồ thị theo nguồn đang chọn. Trang Nhật ký có chức năng giả lập lỗi chỉ dành cho mô phỏng.

Bản ghi đã gửi và dữ liệu mô phỏng giữ 90 ngày; dữ liệu thật chưa được xác nhận gửi không tự xóa. Theo dõi dung lượng ổ/thẻ nhớ. Để sao lưu, dừng chương trình rồi chép toàn bộ thư mục `data`. Device Token nằm trong cấu hình cục bộ trong cơ sở dữ liệu; giữ riêng bản sao lưu này.

## ThingsBoard

Nhập **MQTT host, port, TLS và Device Access Token** do người quản trị cung cấp. Host MQTT có thể khác URL đăng nhập. Mặc định chưa bật gửi. Không cần gửi token trong cuộc trò chuyện.

Mỗi số đo thật được ghi SQLite trước rồi gửi MQTT QoS 1 với timestamp lúc máy tính nhận số đo. Mất mạng thì giữ hàng chờ và gửi bù khi mạng trở lại. Chỉ đánh dấu đã gửi sau PUBACK; có thể gửi lặp khi ngắt kết nối đúng lúc xác nhận. PUBACK xác nhận broker nhận, không chứng minh dashboard đã hiển thị. Dữ liệu mô phỏng luôn bị loại khỏi hàng gửi.

Ví dụ telemetry: `MOI_TRUONG_AHT20_temperature_c`, `BUONG_KHI_SCD30_co2_ppm`, `BUONG_KHI_H2S_ppm`, các trường `_ok`/`_error`. Cảm biến lỗi không có giá trị số giả. Chưa kiểm tra với tài khoản ThingsBoard thật vì chưa có thông tin kết nối.

## Chuyển sang Ubuntu trên Orange Pi

Chép toàn bộ project này và firmware tương ứng. Cài Python 3, venv, tạo môi trường riêng rồi cài `requirements.txt`; chạy `python app.py --open`. Cổng thường là `/dev/ttyACM0`, chọn lại trong giao diện; tài khoản cần quyền truy cập serial. Các cổng cụ thể phụ thuộc thiết bị.

Ứng dụng hiện chỉ nghe tại localhost, không mở điều khiển qua mạng LAN. Bản này dùng để thử trên Windows trước. Khi chuyển sang vận hành 24/7, cần cấu hình tự khởi động dịch vụ, quyền USB ổn định, đồng bộ giờ, nguồn và sao lưu trên Pi; chưa cấu hình các việc đó trên Orange Pi đang chạy Android.

## Cấu trúc code

- `app.py`: máy chủ giao diện và API.
- `quantrac/config.py`: cấu hình và kiểm tra thông số.
- `quantrac/link.py`: giao tiếp USB và thiết bị mô phỏng.
- `quantrac/controller.py`: lịch bơm, đọc cảm biến, heartbeat và xử lý mất kết nối.
- `quantrac/storage.py`: SQLite, CSV và dữ liệu đồ thị.
- `quantrac/thingsboard.py`: gửi MQTT và gửi bù.
- `web/`: giao diện, đồ thị, bố cục.
- `tests/`: kiểm tra tự động.

Đã kiểm tra 16 tình huống tự động, thao tác giao diện qua trình duyệt (desktop/mobile), và biên dịch firmware ESP cập nhật. Các kiểm tra dùng thiết bị mô phỏng/serial giả; chưa thay thế thử nghiệm phần cứng thực tế.

Backup firmware trước khi sửa: `../backup_esp32_pi_io_20261019_105706.zip`. Bản firmware tự chạy cũ `../pumping_and_measuring_system` giữ nguyên.
