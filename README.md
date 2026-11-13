# ESP32-C3 Super Mini - Bluetooth Media & Desk LED RGB Controller (PIXEL-DECK)

Dự án điều khiển âm lượng và phát nhạc qua **Bluetooth BLE HID Keyboard (PIXEL-DECK-AUDIO)** kết hợp **Bộ điều khiển dải đèn LED RGB (PIXEL-DECK-LED)** với giao diện web phong cách WLED thông qua **Wi-Fi Access Point + Captive Portal** (tự động bật trang cài đặt khi kết nối điện thoại).

---

## 🚀 Các Tính Năng Chính

### 1. Điều khiển Media qua Bluetooth (BLE HID)
- **Tên Bluetooth:** `PIXEL-DECK-AUDIO` (Hỗ trợ Windows 10/11, macOS, Android, iOS).
- **Encoder KY-040:**
  - Vặn theo chiều kim đồng hồ (CW): **Tăng âm lượng** (`KEY_MEDIA_VOLUME_UP`).
  - Vặn ngược chiều kim đồng hồ (CCW): **Giảm âm lượng** (`KEY_MEDIA_VOLUME_DOWN`).
  - Nhấn nút trên trục Encoder (SW): **Dừng / Phát nhạc** (`KEY_MEDIA_PLAY_PAUSE`).
- **3 Nút nhấn ngoài:**
  - Nút 1 (GPIO 6): **Chuyển bài tiếp theo** (`KEY_MEDIA_NEXT_TRACK`).
  - Nút 2 (GPIO 7): **Lùi bài trước** (`KEY_MEDIA_PREVIOUS_TRACK`).
  - Nút 3 (GPIO 10): **Dừng / Phát nhạc** (`KEY_MEDIA_PLAY_PAUSE`).
- **Đèn LED tích hợp bo mạch (GPIO 8):**
  - Nhấp nháy đều 500ms: Đang chờ kết nối Bluetooth.
  - Sáng liên tục: Đã kết nối Bluetooth thành công.

### 2. Điều khiển Đèn LED RGB Dây (WS2812B) qua Wi-Fi Web UI
- **Chân điều khiển LED:** **GPIO 2** (Nối vào chân DATA/DIN của dải LED WS2812B).
- **Wi-Fi Access Point tự phát:**
  - SSID: `PIXEL-DECK-LED` (Không có mật khẩu).
  - **Captive Portal:** Khi điện thoại kết nối vào Wi-Fi, màn hình cài đặt sẽ tự động nhảy lên ngay lập tức (không cần gõ địa chỉ IP). Nếu cần truy cập thủ công: mở trình duyệt gõ `192.168.4.1`.
- **Giao diện Web:**
  - Nút nguồn Bật / Tắt LED nhanh (`Power ON/OFF`).
  - Thanh trượt điều chỉnh **Độ sáng (Brightness: 0 - 100%)**.
  - Thanh trượt điều chỉnh **Tốc độ hiệu ứng (Speed: 5 - 100%)**.
  - **Tùy chỉnh số lượng bóng LED (LED Count):** Nhập số bóng (ví dụ: 60 bóng) và nhấn nút `Lưu số LED`.
  - **Bảng chọn màu:** Bánh xe màu cảm ứng (Color Wheel) + 12 ô màu chọn nhanh (Đỏ, Cam, Vàng, Xanh lá, Cyan, Xanh dương, Tím, Hồng, Trắng ấm 2700K, Trắng tự nhiên, Trắng lạnh, Tắt).
  - **Danh sách Hiệu ứng phong phú:**
    - 🌟 **Chạy từ giữa dây chạy ra 2 đầu (Center Out)** *(Hiệu ứng vệt sáng tỏa từ tâm cực đẹp cho đế màn hình/bàn làm việc)*
    - 🔄 **Từ 2 đầu dồn vào giữa (Edges In)**
    - 🌈 **Cầu vồng cuộn (Rainbow Cycle)**
    - 💡 **Sáng tĩnh 1 màu (Solid Color)**
    - 🌬️ **Thở nhịp (Breathing)**
    - ⚡ **Đuổi màu (Chase)**
    - 💫 **Quét màu (Wipe)**
    - ✨ **Sao lấp lánh (Twinkle Stars)**
    - ☄️ **Sao băng (Meteor Rain)**
    - 🔥 **Ngọn lửa ấm (Fire Glow)**
    - ⚡ **Chớp nháy (Strobe)**
- **Tự động lưu bộ nhớ Flash NVS:** Mọi cấu hình (Màu sắc, Hiệu ứng, Số bóng LED, Độ sáng, Trạng thái bật/tắt) đều được tự động lưu vào bộ nhớ trong của ESP32-C3, không bị mất khi mất điện hoặc rút nguồn.

---

## 🔌 Sơ Đồ Đấu Nối Phần Cứng

| Linh kiện | Chân trên linh kiện | Chân nối trên ESP32-C3 Super Mini | Chú thích |
| :--- | :--- | :--- | :--- |
| **Dải LED RGB WS2812B** | **DIN / DATA** | **GPIO 2** | Tín hiệu điều khiển dữ liệu LED |
| | **+5V (VCC)** | **5V / VIN** | Cấp nguồn 5V (Nguồn ngoài nếu dây LED dài > 30 bóng) |
| | **GND** | **GND** | Mass chung |
| **Encoder KY-040** | **CLK** | **GPIO 3** | Xung A |
| | **DT** | **GPIO 4** | Xung B |
| | **SW** | **GPIO 5** | Nút nhấn trục Encoder |
| | **+ (VCC)** | **3V3** | Nguồn 3.3V |
| | **GND** | **GND** | Mass chung |
| **3 Nút nhấn ngoài** | **Chân mass chung** | **GND** | Nối chung về Mass ESP32 |
| | **Nút 1 (Next)** | **GPIO 6** | Chuyển bài |
| | **Nút 2 (Prev)** | **GPIO 7** | Lùi bài |
| | **Nút 3 (Play/Pause)** | **GPIO 10** | Dừng / Phát |
| **LED trạng thái** | *Tích hợp sẵn bo mạch* | **GPIO 8** | Báo trạng thái BLE (Active LOW) |

---

## 🛠 Hướng Dẫn Cài Đặt & Nạp Code

### Cách 1: Nạp bằng PlatformIO (VS Code)
1. Mở thư mục dự án bằng VS Code (đã cài extension PlatformIO).
2. Kiểm tra file `platformio.ini` (đã bao gồm các thư viện `ESP32 BLE Keyboard`, `NimBLE-Arduino`, `Adafruit NeoPixel`).
3. Cắm cáp Type-C kết nối ESP32-C3 Super Mini vào máy tính.
4. Bấm **PlatformIO: Upload** để nạp firmware.

### Cách 2: Nạp bằng Arduino IDE
1. Cài đặt các thư viện trong Arduino IDE:
   - `Adafruit NeoPixel`
   - `ESP32 BLE Keyboard`
   - `NimBLE-Arduino`
2. Chọn Board: **ESP32C3 Dev Module** (hoặc board tương thích ESP32-C3).
   - USB CDC On Boot: **Enabled**
   - Flash Size: **4MB**
3. Mở file `.ino` và bấm **Upload**.
