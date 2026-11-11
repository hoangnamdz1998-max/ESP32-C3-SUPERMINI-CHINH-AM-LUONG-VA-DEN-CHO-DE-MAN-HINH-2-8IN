# ESP32-C3 Super Mini - Bluetooth Media Controller (NAM-DZ)

Dự án điều khiển âm lượng và phát nhạc qua Bluetooth BLE HID Keyboard dành cho PC/Laptop/Điện thoại sử dụng vi điều khiển **ESP32-C3 Super Mini**, **Encoder xoay KY-040** và **3 nút nhấn cơ**.

## 🚀 Tính năng
- **Tên Bluetooth:** `NAM-DZ` (BLE HID Device, tương thích Windows 10/11, macOS, Android, iOS).
- **Encoder KY-040:**
  - Vặn theo chiều kim đồng hồ (CW): **Tăng âm lượng** (`KEY_MEDIA_VOLUME_UP`).
  - Vặn ngược chiều kim đồng hồ (CCW): **Giảm âm lượng** (`KEY_MEDIA_VOLUME_DOWN`).
  - Nhấn nút trên trục Encoder (SW): **Dừng / Phát nhạc** (`KEY_MEDIA_PLAY_PAUSE`).
- **3 Nút nhấn ngoài:**
  - Nút 1 (GPIO 6): **Chuyển bài tiếp theo** (`KEY_MEDIA_NEXT_TRACK`).
  - Nút 2 (GPIO 7): **Lùi bài trước** (`KEY_MEDIA_PREVIOUS_TRACK`).
  - Nút 3 (GPIO 10): **Dừng / Phát nhạc** (`KEY_MEDIA_PLAY_PAUSE`).
- **Đèn LED báo trạng thái (GPIO 8):**
  - Nhấp nháy đều: Đang chờ kết nối Bluetooth.
  - Sáng đứng: Đã kết nối thành công với thiết bị.

---

## 🔌 Sơ đồ đấu nối phần cứng

| Linh kiện | Chân linh kiện | Chân trên ESP32-C3 Super Mini | Chú thích |
| :--- | :--- | :--- | :--- |
| **Encoder KY-040** | **CLK** | **GPIO 3** | Tín hiệu xung A |
| | **DT** | **GPIO 4** | Tín hiệu xung B |
| | **SW** | **GPIO 5** | Nút nhấn trục Encoder |
| | **+ (VCC)** | **3V3** | Nguồn 3.3V |
| | **GND** | **GND** | Nối Mass chung |
| **3 Nút nhấn cơ** | **Chân âm chung** | **GND** | Nối cực âm chung về GND ESP32 |
| | **Nút 1 (Next)** | **GPIO 6** | Chuyển bài |
| | **Nút 2 (Prev)** | **GPIO 7** | Lùi bài |
| | **Nút 3 (Play/Pause)** | **GPIO 10** | Dừng / Phát |

---

## 🛠 Cấu hình và nạp code (PlatformIO)

Cấu hình trong [platformio.ini](platformio.ini):
- Framework: `Arduino`
- Board: `esp32-c3-devkitm-1`
- Thư viện Bluetooth: `ESP32 BLE Keyboard` kết hợp `NimBLE-Arduino` (tối ưu RAM và tương thích hoàn toàn kiến trúc RISC-V của ESP32-C3).
- Cờ nạp `--no-stub` giúp nạp firmware qua cổng USB-CDC trên ESP32-C3 cực kỳ ổn định.
