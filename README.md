# Robot Lau Nhà Tự Động - ESP32 + LiDAR

Robot lau nhà tự động sử dụng ESP32, cảm biến LiDAR, điều khiển qua WiFi với giao diện web hiển thị bản đồ radar 2D thời gian thực.

---

## Tính năng

- **Điều khiển qua WiFi**: ESP32 phát WiFi Access Point, người dùng kết nối và điều khiển qua trình duyệt
- **Giao diện Web Radar 2D**: Hiển thị bản đồ LiDAR real-time, cập nhật mỗi 400ms
- **Chế độ tự động**: Robot tự tránh vật cản phía trước (khoảng cách < 25cm)
- **Điều khiển thủ công**: Tiến / Lùi / Trái / Phải / Dừng
- **Điều khiển động cơ PWM**: Dùng LEDC (5 kHz, 8-bit) cho 2 động cơ bánh trái/phải

---

## Phần cứng yêu cầu

| Linh kiện | Mô tả |
|---|---|
| ESP32 | Vi điều khiển chính |
| LiDAR (LDS) | Cảm biến đo khoảng cách 360° (giao tiếp UART) |
| Driver động cơ | Điều khiển 2 động cơ DC (4 kênh PWM) |
| Động cơ DC | 2 động cơ bánh trái / phải |

### Kết nối chân (Pinout)

| Chức năng | Chân ESP32 |
|---|---|
| Motor IN1 (trái tiến) | GPIO 27 |
| Motor IN2 (trái lùi) | GPIO 26 |
| Motor IN3 (phải tiến) | GPIO 25 |
| Motor IN4 (phải lùi) | GPIO 33 |
| UART2 TX (→ LiDAR) | GPIO 17 |
| UART2 RX (← LiDAR) | GPIO 16 |

---

## Cài đặt môi trường

### Yêu cầu

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)
- Python 3.8+
- CMake 3.16+

### Clone và build

```bash
git clone <repo-url>
cd Test2

# Thiết lập môi trường ESP-IDF (chạy 1 lần)
. $IDF_PATH/export.sh   # Linux/macOS
# hoặc: $IDF_PATH\export.bat  (Windows CMD)

# Build firmware
idf.py build

# Flash lên ESP32
idf.py -p COM<PORT> flash monitor
```

> **Windows**: Thay `COM<PORT>` bằng cổng COM của ESP32 (ví dụ: `COM3`, `COM5`)

---

## Sử dụng

1. Sau khi flash, ESP32 phát WiFi với tên **`Robot_Cua_Lan`**
2. Kết nối điện thoại/máy tính vào WiFi đó (mật khẩu mặc định: `12345678`)
3. Mở trình duyệt, truy cập: `http://192.168.4.1`
4. Giao diện web hiện ra với bản đồ radar và các nút điều khiển

> **Lưu ý bảo mật**: Thay đổi `WIFI_SSID` và `WIFI_PASS` trong `main/hello_world_main.c` trước khi triển khai thực tế.

### API HTTP

| Endpoint | Chức năng |
|---|---|
| `GET /` | Giao diện web chính |
| `GET /map` | Dữ liệu LiDAR (360 giá trị, đơn vị mm) |
| `GET /fwd` | Tiến |
| `GET /bwd` | Lùi |
| `GET /left` | Rẽ trái |
| `GET /right` | Rẽ phải |
| `GET /stop` | Dừng |
| `GET /auto` | Bật chế độ tự động tránh vật cản |

---

## Cấu trúc thư mục

```
Test2/
├── CMakeLists.txt          # Cấu hình build ESP-IDF
├── main/
│   ├── CMakeLists.txt
│   └── hello_world_main.c  # Code chính: WiFi, HTTP server, LiDAR, Motor
├── sdkconfig.ci            # Cấu hình ESP-IDF cho CI
└── README.md
```

---

## Thuật toán tránh vật cản (Auto Mode)

1. LiDAR quét 360°, đọc khoảng cách góc 330°–30° (phía trước)
2. Nếu `front_dist < 250mm`: dừng → lùi 400ms → quay phải 800ms
3. Nếu đường trống: tiến thẳng (PWM = 120/255)

---

## Công nghệ sử dụng

- **ESP-IDF** + FreeRTOS
- **LEDC** (PWM điều khiển tốc độ động cơ)
- **UART** (đọc dữ liệu LiDAR)
- **ESP HTTP Server** (REST API + Web UI)
- **WiFi AP Mode** (không cần router)
- **Canvas HTML5** (vẽ radar 2D)
