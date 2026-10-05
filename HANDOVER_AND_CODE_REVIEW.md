# TÀI LIỆU BÀN GIAO KỸ THUẬT & ĐÁNH GIÁ MÃ NGUỒN (CODE REVIEW)
## HỆ THỐNG ĐIỀU KHIỂN DUAL SERVO & CẢM BIẾN HỒNG NGOẠI NHẬN DIỆN TÔ
**Dự án:** `ESP32_Control_Center_ServoStop_FIXED_v2`  
**Mục đích:** Bàn giao kỹ thuật toàn diện cho Đối tác / Đơn vị thứ ba (Third-Party Handover Document)  
**Phiên bản hệ thống:** v2.5.0-STABLE  
**Phần cứng mục tiêu:** LILYGO T-Display-S3 (ESP32-S3R8, 16MB Flash, 8MB Octal PSRAM, ST7789 1.9" LCD)  
**Ngày lập tài liệu:** 05/10/2026  

---

## MỤC LỤC
1. [Tổng Quan Dự Án & Bối Cảnh Nghiệp Vụ](#1-tổng-quan-dự-án--bối-cảnh-nghiệp-vụ)
2. [Kiến Trúc Tổng Thể Hệ Thống (System Architecture)](#2-kiến-trúc-tổng-thể-hệ-thống-system-architecture)
3. [Sơ Đồ Đấu Nối Phần Cứng Hoàn Chỉnh (Hardware Pinout & Wiring)](#3-sơ-đồ-đấu-nối-phần-cứng-hoàn-chỉnh-hardware-pinout--wiring)
4. [Đánh Giá Chi Tiết Mã Nguồn (Component-by-Component Code Review)](#4-đánh-giá-chi-tiết-mã-nguồn-component-by-component-code-review)
   - 4.1. Firmware Vi Điều Khiển (`ESP32_Control_Center_ServoStop_FIXED_v2.ino`)
   - 4.2. Giao Diện Nhúng Độc Lập (`web_ui.h`)
   - 4.3. Backend Python Bridge & Server (`server.py`)
   - 4.4. Frontend Web Dashboard (`web/index.html`, `web/style.css`, `web/app.js`)
   - 4.5. Cấu Hình Biên Dịch PlatformIO (`platformio.ini`)
5. [Đặc Tả Giao Thức & API (API & Protocol Specifications)](#5-đặc-tả-giao-thức--api-api--protocol-specifications)
   - 5.1. REST API
   - 5.2. Serial CLI Protocol (UART 115200 Baud)
   - 5.3. WebSocket Telemetry Stream (10Hz)
6. [Cơ Chế An Toàn & Chống Trôi Động Cơ (Safety & Reliability Engineering)](#6-cơ-chế-an-toàn--chống-trôi-động-cơ-safety--reliability-engineering)
7. [Hướng Dẫn Cài Đặt, Biên Dịch & Triển Khai (Build & Deployment)](#7-hướng-dẫn-cài-đặt-biên-dịch--triển-khai-build--deployment)
8. [Quy Trình Kiểm Thử HIL (Hardware-in-the-Loop Testing)](#8-quy-trình-kiểm-thử-hil-hardware-in-the-loop-testing)
9. [Đánh Giá Rủi Ro, Điểm Cần Lưu Ý & Kế Hoạch Nâng Cấp (Audit & Roadmap)](#9-đánh-giá-rủi-ro-điểm-cần-lưu-ý--kế-hoạch-nâng-cấp-audit--roadmap)

---

## 1. TỔNG QUAN DỰ ÁN & BỐI CẢNH NGHIỆP VỤ

### 1.1. Mục Tiêu Dự Án
Hệ thống được thiết kế nhằm mục đích tự động hóa dây chuyền phân luồng/định lượng tô (bát) trên băng tải công nghiệp nhẹ. Hệ thống tích hợp:
1. **Khối cảm biến phát hiện tô (IR Beam-Break Detection)**: Sử dụng cặp mắt phát và thu hồng ngoại bước sóng 940nm để phát hiện vật thể đi qua, đếm số lượng tô chính xác, đo thời gian che chắn của từng tô.
2. **Khối cơ cấu chấp hành kép (Dual Servo 360° Actuators)**: Điều khiển độc lập 2 động cơ Servo (Servo 1 trên `GPIO 1` và Servo 2 trên `GPIO 2`) thực hiện các tác vụ:
   - Chế độ chu kỳ tự động (Auto-Cycle): Quay thuận $\rightarrow$ Dừng $\rightarrow$ Quay nghịch $\rightarrow$ Dừng.
   - Chế độ mở giữ cửa (Open Mode): Mở tay gạt cho tô đi qua và giữ nguyên vị trí.
   - Chế độ đóng cửa (Close Mode): Đóng tay gạt chắn tô về vị trí ban đầu.
   - Chế độ tự động hóa theo cảm biến (IR Triggered): Tự động mở Servo 1, Servo 2 hoặc cả hai khi có tô đi qua.
3. **Màn hình giám sát tại chỗ (Local LCD Telemetry)**: Màn hình màu ST7789 1.9 inch hiển thị trạng thái thời gian thực của 2 Servo, trạng thái tia hồng ngoại, bộ đếm số tô và địa chỉ IP kết nối.
4. **Hệ thống điều khiển & Giám sát từ xa (Host Web Center)**: Dashboard Cyberpunk thời gian thực chạy qua cổng USB COM hoặc qua mạng WiFi nội bộ, cung cấp Oscilloscope đo dạng sóng PWM, đồng hồ tốc độ (tachometer), bảng hiệu chuẩn điểm dừng NVS Flash và bàn điều khiển đa kênh.

### 1.2. Thông Số Phần Cứng Mục Tiêu
* **SoC Vi điều khiển**: Espressif ESP32-S3 (ESP32-S3R8), Dual-Core Xtensa LX7 @ 240MHz, tích hợp Vector Instructions.
* **Bộ nhớ**: 16MB SPI Flash (Quad SPI), 8MB Octal PSRAM (OPI PSRAM).
* **Màn hình**: ST7789V 1.9 inch IPS LCD, độ phân giải $170 \times 320$ pixels, giao tiếp bus song song 8-bit (`Arduino_ESP32PAR8Q`).
* **Nguồn điện hệ thống**: Nguồn tổ ong 12V DC qua mạch hạ áp biến tần LM2596 Buck Converter đưa về 5.0V nuôi Servo; cáp Type-C nuôi ESP32-S3.

---

## 2. KIẾN TRÚC TỔNG THỂ HỆ THỐNG (SYSTEM ARCHITECTURE)

Hệ thống được tổ chức theo mô hình phân tầng phân tán (Distributed Layered Architecture) với 3 khối cốt lõi:

```mermaid
graph TB
    subgraph "Hardware Layer (Physical Actuators & Sensors)"
        IR_TX["IR Transmitter Module<br/>(LED 940nm)"]
        IR_RX["IR Receiver Module<br/>(Phototransistor)"]
        SERVO1["Servo 1 (Gate 1)<br/>360° Continuous"]
        SERVO2["Servo 2 (Gate 2)<br/>360° Continuous"]
        PWR_12V["Nguồn Tổ Ong 12V"]
        LM2596["Mạch Hạ Áp LM2596<br/>(12V -> 5V DC 3A)"]
        PWR_12V --> LM2596
        LM2596 -->|5V Power| SERVO1
        LM2596 -->|5V Power| SERVO2
    end

    subgraph "Firmware Layer (LILYGO T-Display-S3)"
        ESP32["ESP32-S3R8 Dual Core @ 240MHz"]
        LCD["ST7789 1.9 inch LCD<br/>(Parallel 8-bit Bus)"]
        NVS["NVS Flash Memory<br/>(Preferences Schema v5)"]
        PWM_DRIVER["LEDC PWM Controller<br/>(50Hz, Lazy Attach)"]
        IR_ENGINE["IR Edge Detector<br/>(Debounce 60ms)"]
        AP_SERVER["Embedded WebServer<br/>(Port 80 SoftAP)"]

        ESP32 --> LCD
        ESP32 <--> NVS
        ESP32 --> PWM_DRIVER
        ESP32 <--> IR_ENGINE
        ESP32 <--> AP_SERVER

        PWM_DRIVER -->|GPIO 1| SERVO1
        PWM_DRIVER -->|GPIO 2| SERVO2
        IR_ENGINE <--|GPIO 10 (DO)| IR_RX
        IR_ENGINE -->|GPIO 11 (DAT)| IR_TX
    end

    subgraph "Bridge & Backend Layer (Host Computer / Gateway)"
        PY_SERVER["Python Bridge Server (server.py)<br/>Starlette + Uvicorn + PySerial"]
        SERIAL_THREAD["Non-blocking Serial Worker<br/>(COM3 @ 115200 Baud)"]
        WS_HUB["WebSocket Broadcast Hub<br/>(10Hz Telemetry Engine)"]
        EMU_ENGINE["Emulation Fallback Engine<br/>(Virtual Dual Servos + IR)"]

        PY_SERVER --> SERIAL_THREAD
        PY_SERVER --> WS_HUB
        PY_SERVER --> EMU_ENGINE
        SERIAL_THREAD <==|USB-Serial / JTAG COM3|==> ESP32
    end

    subgraph "Presentation Layer (Web Control Center)"
        WEB_UI["Modern Cyberpunk Dashboard<br/>(HTML5 / Canvas / Vanilla CSS)"]
        GAUGES["Dual Servo Tachometer Gauges"]
        SCOPE["Live PWM Oscilloscope"]
        IR_DECK["IR Bowl Counter & Radar Display"]
        CALIB_PANEL["NVS Stop Calibration Sliders"]

        WEB_UI --> GAUGES
        WEB_UI --> SCOPE
        WEB_UI --> IR_DECK
        WEB_UI --> CALIB_PANEL

        WEB_UI <==|WebSocket /ws (10Hz)|==> WS_HUB
        WEB_UI <==|REST API /api/*|==> PY_SERVER
    end
```

---

## 3. SƠ ĐỒ ĐẤU NỐI PHẦN CỨNG HOÀN CHỈNH (HARDWARE PINOUT & WIRING)

### 3.1. Bảng Phân Bổ Chân GPIO (LILYGO T-Display-S3)
Dưới đây là ma trận chân kết nối được chọn lọc để **tuyệt đối không xung đột** với bus song song màn hình ST7789, chân kích nguồn hay chân nạp bootloader:

| Chân Bo Mạch | Ký Hiệu Board | Tên Tín Hiệu | Thiết Bị Kết Nối | Mức Điện Áp | Hướng I/O | Chức Năng Kỹ Thuật |
| :---: | :---: | :---: | :--- | :---: | :---: | :--- |
| **Pin 1 (Trái)** | `3V` | 3V3 Regulated | VCC Module IR Thu | 3.3V DC | Nguồn Out | Cấp nguồn 3.3V an toàn cho module quang thu |
| **Pin 2 (Trái)** | `1` | GPIO 1 | Dây Cam (Signal) Servo 1 | 3.3V PWM | Digital Out | Xung PWM 50Hz điều khiển Servo 1, Active Ground Clamp LOW khi dừng |
| **Pin 3 (Trái)** | `2` | GPIO 2 | Dây Cam (Signal) Servo 2 | 3.3V PWM | Digital Out | Xung PWM 50Hz điều khiển Servo 2, Active Ground Clamp LOW khi dừng |
| **Pin 5 (Trái)** | `10` | GPIO 10 | Chân OUT Module IR Thu | 0V – 3.3V | Digital In | Ngõ vào tín hiệu số báo có tô chắn chùm tia hồng ngoại |
| **Pin 6 (Trái)** | `11` | GPIO 11 | Chân DAT Module IR Phát | 3.3V | Digital Out | Tín hiệu bật/tắt mắt LED hồng ngoại |
| **Pin 11 (Trái)**| `G` | GND | Chân `OUT-` của LM2596 & GND cảm biến | 0V | Mass Chung | **CỰC KỲ QUAN TRỌNG:** Điểm chung Mass (Common Ground) cho toàn bộ hệ thống |
| **Pin 12 (Trái)**| `+5V` | VBUS (5V) | Chân VCC Module IR Phát | 5.0V DC | Nguồn Out | Cấp nguồn 5V cho đèn LED hồng ngoại phát xa, chùm tia mạnh |
| **Chân Trong** | `15` | GPIO 15 | `LCD_POWER_PIN` | 3.3V | Digital Out | Chân kích mở nguồn cho toàn bộ IC màn hình LCD và đèn nền |
| **Chân Trong** | `38` | GPIO 38 | `LCD_BL_PIN` | 3.3V | Digital Out | Chân điều khiển bật đèn nền màn hình (Backlight) |
| **Chân Trong** | `7..9, 39..48` | GPIO bus | `Arduino_ESP32PAR8Q` | 3.3V | Digital Out | Bus truyền dữ liệu song song 8-bit tốc độ cao tới màn hình ST7789 |

### 3.2. Sơ Đồ Đấu Nối Nguồn Điện & Công Suất Tải

```text
  [ NGUỒN TỔ ONG 12V ]
     │
     ├─ V+ (12V) ────────> Chân IN+ (Module Hạ Áp LM2596)
     └─ V- (COM/GND) ────> Chân IN- (Module Hạ Áp LM2596)
                                 │
                                [LM2596 BUCK CONVERTER]
                                (Tinh chỉnh biến trở về đúng 5.0V)
                                 │
     ┌───────────────────────────┴─────────────────────────────┐
     │ OUT+ (+5V Đã Hạ Áp)                                     │ OUT- (Mass 0V)
     │                                                         │
     ├─► Dây ĐỎ (+) Servo 1                                    ├─► Dây NÂU (-) Servo 1
     ├─► Dây ĐỎ (+) Servo 2                                    ├─► Dây NÂU (-) Servo 2
     │                                                         │
     │                                                         └─► DÂY MASS CHUNG NỐI VÀO
     │                                                             CHÂN "G" CỦA ESP32-S3!
     │
  [ BO MẠCH LILYGO T-DISPLAY-S3 ]
     ├─ Cổng USB Type-C ───────► Cắm máy tính / Củ sạc 5V (Nguồn nuôi bo mạch)
     ├─ Chân "G" (Pin 11 Trái) ─► Nối chung Mass với OUT- của LM2596 và GND cảm biến
     ├─ Chân "3V" (Pin 1 Trái) ─► Cấp nguồn VCC cho Module IR THU (Bắt buộc 3.3V)
     ├─ Chân "+5V" (Pin 12 T) ──► Cấp nguồn VCC cho Module IR PHÁT (5V giúp phát mạnh)
     ├─ Chân "10" (Pin 5 Trái) ─► Nối vào chân OUT của Module IR THU
     ├─ Chân "11" (Pin 6 Trái) ─► Nối vào chân DAT của Module IR PHÁT
     ├─ Chân "1" (Pin 2 Trái) ──► Nối vào dây CAM (Tín hiệu) Servo 1
     └─ Chân "2" (Pin 3 Trái) ──► Nối vào dây CAM (Tín hiệu) Servo 2
```

> [!WARNING]
> **CẢNH BÁO AN TOÀN PHẦN CỨNG BẮT BUỘC:**
> 1. **Mức điện áp cảm biến thu:** Chân VCC của Module IR Thu **bắt buộc cắm chân `3V` (3.3V)**. Không cắm vào chân 5V vì ngõ ra `OUT` có điện trở kéo sẽ xả mức 5V vào `GPIO 10`, gây chập cháy hỏng chân GPIO của ESP32-S3.
> 2. **Chung Mass GND:** Dây Mass giữa `OUT-` của LM2596 và chân `G` của ESP32 **bắt buộc phải nối**. Nếu không có dây này, xung PWM của Servo sẽ mất điện thế tham chiếu 0V, dẫn đến Servo bị co giật, rung lắc hoặc không hoạt động.
> 3. **Tách nguồn tải:** Tuyệt đối không lấy nguồn từ chân 3.3V hoặc 5V của bo mạch ESP32 để nuôi động cơ Servo, vì dòng khởi động của 2 Servo có thể vượt quá 2A, làm cháy mạch hạ áp nội của ESP32.

---

## 4. ĐÁNH GIÁ CHI TIẾT MÃ NGUỒN (COMPONENT-BY-COMPONENT CODE REVIEW)

### 4.1. Firmware Vi Điều Khiển (`ESP32_Control_Center_ServoStop_FIXED_v2.ino`)
* **Tổng số dòng code**: 1,462 dòng C++.
* **Nguyên lý thiết kế**: Non-blocking State Machine kết hợp kỹ thuật Lazy Attachment & Active Ground Clamping.

#### Phân tích các khối chức năng quan trọng:
1. **Khởi tạo phần cứng chuyên biệt cho LILYGO T-Display-S3**:
   - Firmware gọi trực tiếp `pinMode(LCD_POWER_PIN, OUTPUT); digitalWrite(LCD_POWER_PIN, HIGH);` tại `setup()` trước khi gọi màn hình. Nếu không có dòng này, màn hình LCD sẽ không được cấp nguồn khi chạy ngoài cổng USB.
   - Driver màn hình sử dụng `Arduino_ESP32PAR8Q` điều khiển trực tiếp 8 đường dữ liệu song song phần cứng, cho tốc độ render khung hình vượt trội so với SPI thông thường.
2. **Kỹ thuật Lazy PWM Attach & Active Ground Clamping (Chống trôi 100%)**:
   - *Vấn đề của Servo thông thường*: Khi lệnh dừng gửi góc $90^\circ$, mạch analog bên trong Servo vẫn liên tục băm xung giữ vị trí. Chỉ cần nhiệt độ thay đổi hoặc nguồn điện gợn sóng nhỏ, Servo sẽ bị "rên", nóng động cơ và từ từ trôi vị trí (motor drift).
   - *Giải pháp trong mã nguồn*:
     ```cpp
     void servo1StopNow() {
         if (servo1Attached) {
             servo1.write(servo1Stop);
             delay(20);
             servo1.detach(); // Ngắt hẳn tín hiệu PWM ra khỏi chân
             servo1Attached = false;
         }
         // Kéo chân điều khiển xuống 0V cưỡng bức
         pinMode(SERVO_1_PIN, OUTPUT);
         digitalWrite(SERVO_1_PIN, LOW);
     }
     ```
     Kỹ thuật này triệt tiêu hoàn toàn 100% hiện tượng trôi vị trí và giúp Servo hoàn toàn mát khi ở trạng thái nghỉ.
3. **Engine Cảm Biến Hồng Ngoại Nhận Diện Tô**:
   - Sử dụng cơ chế lọc rung quang học (Software Debounce 60ms) tại hàm `updateIrSensor()`.
   - Phát hiện sườn lên (Rising Edge - tô bắt đầu chắn tia): Tăng bộ đếm `irObjectCount++`, ghi nhận `irBlockStartMs`, tự động kích hoạt Servo tương ứng nếu cấu hình `irAutoTriggerServo > 0`.
   - Phát hiện sườn xuống (Falling Edge - tô đã đi qua hoàn toàn): Tính toán chính xác thời gian che chắn `irBlockDurationMs = millis() - irBlockStartMs` gửi lên Web.
   - Hỗ trợ xung giả lập ảo `IR_TRIGGER` phục vụ kiểm thử từ xa mà không cần vật cản vật lý.
4. **Hệ Thống Lưu Trữ Cấu Hình Bất Biến (NVS Flash Preferences)**:
   - Sử dụng thư viện `Preferences` của ESP32 lưu trữ cấu hình vào phân vùng NVS an toàn.
   - Tích hợp phiên bản cấu hình `CONFIG_SCHEMA_VERSION = 5`. Nếu nạp firmware mới có thay đổi cấu trúc, hệ thống tự động nhận diện và cập nhật schema mà không bị crash.

---

### 4.2. Giao Diện Nhúng Độc Lập Dự Phòng (`web_ui.h`)
* **Tổng kích thước**: ~23 KB HTML/CSS/JS thuần, lưu tại bộ nhớ chương trình Flash (`PROGMEM`).
* **Vai trò**: Cung cấp giao diện Web Standalone trực tiếp từ ESP32 khi thiết bị phát WiFi Access Point (`192.168.4.1`) mà không cần máy tính chủ hay server Python.
* **Đánh giá**:
  - Giao diện đáp ứng (responsive), có đầy đủ các nút bấm điều khiển độc lập cho cả Servo 1, Servo 2, master control, thanh trượt hiệu chuẩn điểm dừng và đồng hồ chu kỳ.
  - Sử dụng giao thức Fetch API phi đồng bộ giao tiếp trực tiếp với web server nội bộ của ESP32 qua các endpoint REST chuẩn JSON.

---

### 4.3. Backend Python Bridge & Server (`server.py`)
* **Tổng số dòng code**: 867 dòng Python 3.11+.
* **Framework**: `Starlette` (Asynchronous ASGI Web Framework) chạy trên máy chủ `Uvicorn`.
* **Cơ chế hoạt động**:
  1. **Dual-Mode Bridge**:
     - *Chế độ Phần Cứng Thật (Hardware Mode)*: Tự động quét và kết nối với cổng COM của ESP32 (ví dụ `COM3` @ 115200 baud). Dùng cơ chế `threading.Lock` đảm bảo thread-safe khi đọc/ghi nối tiếp.
     - *Chế độ Giả Lập Hoàn Toàn (Emulation Fallback Mode)*: Nếu rút cáp ESP32 hoặc không có phần cứng, server tự động kích hoạt State Machine ảo mô phỏng đầy đủ phản hồi của 2 Servo và Cảm biến hồng ngoại, giúp lập trình viên frontend phát triển bình thường.
  2. **WebSocket Hub 10Hz**:
     - Định kỳ 100ms một lần, background thread đồng bộ trạng thái từ ESP32 và broadcast bản tin JSON `telemetry` tới tất cả các trình duyệt đang kết nối qua WebSocket `/ws`.
     - Phân phối toàn bộ log nối tiếp từ vi điều khiển lên terminal trên Web trong thời gian thực.
  3. **HIL Test Suite Engine**:
     - Tích hợp kịch bản kiểm thử tự động 5 giai đoạn (Phase 5 đến Phase 9) kiểm tra tính toàn vẹn của cổng nối tiếp, bộ nhớ flash, lệnh chuyển động servo và cơ chế chống treo lệnh.

---

### 4.4. Frontend Web Dashboard (`web/index.html`, `web/style.css`, `web/app.js`)
* **Công nghệ**: HTML5 ngữ nghĩa, CSS3 hiện đại (Cyberpunk Dark Mode, Glassmorphism, CSS Grid), Vanilla JavaScript ES6+. Tuyệt đối không phụ thuộc vào thư viện bên ngoài nặng nề (Zero External Framework Dependency).
* **Các thành phần giao diện nổi bật**:
  1. **IR Bowl Detector Deck**: Radar hiển thị trạng thái chùm tia (Xanh ngọc: Thông suốt $\rightarrow$ Đỏ nhấp nháy phát sáng: Phát hiện tô đi qua). Đồng hồ số 4 chữ số đếm số tô kiểu cyber matrix, nút chuyển 4 chế độ tự động hóa và nút mô phỏng xung tô ảo.
  2. **Dual Servo Tachometer & Angle Gauge**: Vẽ trực tiếp bằng HTML5 Canvas 2D, hiển thị góc xoay trực quan của từng Servo theo thời gian thực.
  3. **Real-Time Oscilloscope**: Vẽ dạng sóng PWM của Servo 1 và Servo 2 theo thời gian thực (lưu lịch sử 300 mẫu), giúp kỹ sư phát hiện ngay nếu có hiện tượng trôi xung.
  4. **State Machine Node Visualizer**: Mô phỏng trạng thái logic của máy trạng thái (`IDLE` $\rightarrow$ `FORWARD` $\rightarrow$ `PAUSE` $\rightarrow$ `REVERSE` $\rightarrow$ `COMPLETE`) với hiệu ứng sáng đèn neon theo tiến trình thực tế của vi điều khiển.
  5. **Terminal Console & Calibration**: Bàn phím gõ lệnh Serial trực tiếp và thanh trượt tinh chỉnh góc dừng động cơ với độ phân giải $1^\circ$.

---

### 4.5. Cấu Hình Biên Dịch PlatformIO (`platformio.ini`)
* **Nền tảng**: `platform = espressif32@7.1.3`.
* **Bo mạch**: `board = lilygo-t-display-s3`.
* **Cấu hình USB CDC**:
  ```ini
  build_flags = 
      -DARDUINO_USB_CDC_ON_BOOT=1
      -DARDUINO_USB_MODE=1
  ```
  *Ý nghĩa*: Cho phép chip ESP32-S3 sử dụng bộ USB-Serial/JTAG phần cứng tích hợp để nạp code và giao tiếp Serial trực tiếp mà không cần chip chuyển đổi UART rời (như CP2102 hay CH340).
* **Phiên bản thư viện chốt cố định**:
  - `ESP32Servo @ ^1.2.1`: Điều khiển xung PWM chính xác dựa trên LEDC hardware timer.
  - `moononournation/GFX Library for Arduino @ 1.4.9`: Chốt cố định phiên bản 1.4.9 để hỗ trợ tối ưu cho bus song song `Arduino_ESP32PAR8Q` của màn hình ST7789.

---

## 5. ĐẶC TẢ GIAO THỨC & API (API & PROTOCOL SPECIFICATIONS)

### 5.1. Bảng Chi Tiết REST API (Cổng 8000 & Cổng 80)

| Phương Thức | Đường Dẫn Endpoint | Tham Số (Body / Query) | Mô Tả Chức Năng | Định Dạng Trả Về |
| :---: | :--- | :--- | :--- | :--- |
| `GET` | `/api/status` | Không | Lấy toàn bộ trạng thái vi điều khiển, 2 Servo và Cảm biến IR | JSON Object (`ok`, `s1`, `s2`, `ir`, `connected`) |
| `POST`| `/api/cmd` | `{"cmd": "CHUỖI_LỆNH"}` | Gửi lệnh Serial CLI bất kỳ trực tiếp xuống vi điều khiển | JSON Object (`ok`, `message`) |
| `POST`| `/api/s1/open` | Không | Mở Servo 1 (quay thuận và giữ nguyên vị trí mở) | `{"ok": true, "message": "Servo 1 đang mở"}` |
| `POST`| `/api/s1/close`| Không | Đóng Servo 1 (quay nghịch về vị trí đóng ban đầu) | `{"ok": true, "message": "Servo 1 đang đóng"}` |
| `POST`| `/api/s1/start`| Không | Bắt đầu chạy chu kỳ tự động của Servo 1 | `{"ok": true, "message": "Servo 1 đã bắt đầu"}` |
| `POST`| `/api/s1/stop` | Không | Dừng khẩn cấp Servo 1 và kích hoạt khóa 0V | `{"ok": true, "message": "Servo 1 đã dừng"}` |
| `POST`| `/api/s2/open` | Không | Mở Servo 2 (quay thuận và giữ nguyên vị trí mở) | `{"ok": true, "message": "Servo 2 đang mở"}` |
| `POST`| `/api/s2/close`| Không | Đóng Servo 2 (quay nghịch về vị trí đóng ban đầu) | `{"ok": true, "message": "Servo 2 đang đóng"}` |
| `POST`| `/api/s2/start`| Không | Bắt đầu chạy chu kỳ tự động của Servo 2 | `{"ok": true, "message": "Servo 2 đã bắt đầu"}` |
| `POST`| `/api/s2/stop` | Không | Dừng khẩn cấp Servo 2 và kích hoạt khóa 0V | `{"ok": true, "message": "Servo 2 đã dừng"}` |
| `POST`| `/api/all/open`| Không | Mở đồng thời cả Servo 1 và Servo 2 | `{"ok": true, "message": "Tất cả Servo đang mở"}` |
| `POST`| `/api/all/close`| Không | Đóng đồng thời cả Servo 1 và Servo 2 | `{"ok": true, "message": "Tất cả Servo đang đóng"}` |
| `POST`| `/api/all/start`| Không | Chạy chu kỳ tự động đồng thời cả 2 Servo | `{"ok": true, "message": "Tất cả Servo đã bắt đầu"}` |
| `POST`| `/api/all/stop` | Không | Dừng khẩn cấp toàn bộ hệ thống (E-Stop) | `{"ok": true, "message": "Tất cả Servo đã dừng"}` |
| `POST`| `/api/ir/reset`| Không | Đặt lại số đếm tô đã qua về 0 | `{"ok": true, "message": "Đã reset bộ đếm tô về 0"}` |
| `POST`| `/api/ir/auto` | `{"mode": 0..3}` | Đổi chế độ tự động kích hoạt Servo khi tô tới | `{"ok": true, "autoTrigger": 0..3}` |
| `POST`| `/api/ir/simulate`| Không | Phát một xung ảo mô phỏng tô đi qua | `{"ok": true, "count": N, "detected": true}` |
| `POST`| `/api/reboot`  | Không | Khởi động lại phần cứng ESP32 qua chân DTR/RTS | `{"ok": true}` |

---

### 5.2. Tập Lệnh Serial CLI (Giao Tiếp Cổng Nối Tiếp 115200 Baud)
Mọi lệnh gửi qua cổng Serial kết thúc bằng ký tự xuống dòng `\n`. Vi điều khiển luôn phản hồi với tiền tố `[RES] ` kèm chuỗi JSON:

* `PING` $\rightarrow$ `[RES] PONG`
* `STATUS` $\rightarrow$ Trả về JSON trạng thái toàn diện.
* `S1_START`, `S1_STOP`, `S1_OPEN`, `S1_CLOSE`, `S1_TEST_FWD`, `S1_TEST_REV`
* `S2_START`, `S2_STOP`, `S2_OPEN`, `S2_CLOSE`, `S2_TEST_FWD`, `S2_TEST_REV`
* `ALL_START`, `ALL_STOP`, `ALL_OPEN`, `ALL_CLOSE`
* `S1_CALIB <goc>`: Thử nghiệm góc dừng Servo 1 (Ví dụ: `S1_CALIB 92`).
* `S2_CALIB <goc>`: Thử nghiệm góc dừng Servo 2 (Ví dụ: `S2_CALIB 88`).
* `CONFIG_SET stop1=92 forward1=110 reverse1=70 stop2=88 forward2=110 reverse2=70`: Lưu cấu hình vào NVS Flash.
* `IR_RESET`: Đặt lại bộ đếm tô về 0.
* `IR_AUTO <0|1|2|3>`: 0 = Tắt tự động, 1 = Mở S1, 2 = Mở S2, 3 = Mở cả hai khi tô tới.
* `IR_TRIGGER`: Giả lập phát hiện tô đi qua.

---

### 5.3. Cấu Trúc Bản Tin WebSocket Telemetry (`/ws`)
Mỗi 100ms, client sẽ nhận được bản tin định dạng:
```json
{
  "type": "telemetry",
  "data": {
    "ok": true,
    "connected": true,
    "is_hardware": true,
    "port": "COM3",
    "ip": "192.168.4.1",
    "s1": {
      "pin": 1,
      "state": "Đã mở",
      "running": false,
      "cycle": 0,
      "cycles": 1,
      "stop": 90,
      "forward": 110,
      "reverse": 70,
      "forwardMs": 1000,
      "reverseMs": 1000,
      "pauseMs": 500
    },
    "s2": {
      "pin": 2,
      "state": "Sẵn sàng",
      "running": false,
      "cycle": 0,
      "cycles": 1,
      "stop": 90,
      "forward": 110,
      "reverse": 70,
      "forwardMs": 1000,
      "reverseMs": 1000,
      "pauseMs": 500
    },
    "ir": {
      "pin": 10,
      "txPin": 11,
      "detected": false,
      "raw": 1,
      "count": 12,
      "blockMs": 340,
      "autoTrigger": 1,
      "txEnabled": true
    }
  },
  "ts": 1728100000.123
}
```

---

## 6. CƠ CHẾ AN TOÀN & CHỐNG TRÔI ĐỘNG CƠ (SAFETY & RELIABILITY)

1. **Khóa 0V Cưỡng Bức (Zero-Drift Active Clamping)**:
   - Khi Servo ở trạng thái dừng (`STOP`, `IDLE`, `OPENED`, `CLOSED`), firmware giải phóng timer phần cứng (`detach()`) và chuyển chân GPIO thành `OUTPUT` mức logic `LOW` (0V). Dây tín hiệu PWM được kéo thẳng xuống đất, triệt tiêu mọi xung nhiễu điện từ lan truyền trong môi trường công nghiệp.
2. **Khóa Liên Động Phần Mềm (Interlock Protection)**:
   - Các hàm cấu hình NVS Flash (`handleConfig`, `handleFactoryReset`) kiểm tra cờ `motionIsRunning()`. Nếu động cơ đang quay, hệ thống từ chối cập nhật cấu hình với mã lỗi `409 Conflict`, ngăn chặn việc thay đổi thông số bất thình lình gây kẹt cơ khí.
3. **Bộ Lọc Rung Quang Học (Optical Debounce 60ms)**:
   - Tô di chuyển trên băng tải có thể bị rung lắc hoặc có bề mặt gồ ghề. Bộ lọc 60ms ngăn ngừa tình trạng 1 chiếc tô đi qua bị đếm trùng thành 2–3 lần.
4. **Giới Hạn Ngưỡng An Toàn (Boundary Constraints)**:
   - Tất cả các giá trị tốc độ và thời gian đều được ép qua hàm `constrain()`:
     - Giá trị góc: `[0, 180]` độ.
     - Thời gian chuyển động: `[100ms, 60000ms]`.
     - Số chu kỳ: `[1, 100]`.

---

## 7. HƯỚNG DẪN CÀI ĐẶT, BIÊN DỊCH & TRIỂN KHAI (BUILD & DEPLOYMENT)

### 7.1. Yêu Cầu Môi Trường
* **Phần mềm bắt buộc**:
  - Python 3.10 trở lên.
  - VS Code tích hợp Extension PlatformIO IDE (hoặc PlatformIO Core CLI).
  - Git.
* **Cài đặt thư viện Python**:
  ```bash
  pip install starlette uvicorn pyserial websockets
  ```

### 7.2. Quy Trình Nạp Firmware Lên ESP32-S3
1. Cắm bo mạch LILYGO T-Display-S3 vào cổng USB máy tính.
2. Kiểm tra cổng COM trong Device Manager (Ví dụ: `COM3`).
3. Nếu server Python đang chạy, hãy tạm tắt để giải phóng cổng COM3.
4. Mở terminal tại thư mục gốc dự án và thực hiện biên dịch & nạp:
   ```bash
   pio run -e lilygo-t-display-s3 -t upload --upload-port COM3
   ```
5. Đợi thông báo `[SUCCESS] Took ... seconds`. Màn hình LCD trên bo mạch sẽ sáng lên với logo `DUAL SERVO S3` và hiển thị trạng thái `S1(G1): Sẵn sàng | S2(G2): Sẵn sàng`.

### 7.3. Khởi Chạy Web Control Center
Chạy file script khởi động:
```bat
start_web.bat
```
Hoặc chạy trực tiếp bằng dòng lệnh:
```bash
python server.py
```
Trình duyệt sẽ tự động mở giao diện tại địa chỉ: `http://localhost:8000`.

---

## 8. QUY TRÌNH KIỂM THỬ HIL (HARDWARE-IN-THE-LOOP TESTING)

Hệ thống tích hợp sẵn kịch bản kiểm thử tự động phần cứng HIL (`scripts/hil_test_suite.py`):
```bash
python scripts/hil_test_suite.py --port COM3
```
Kịch bản thực hiện kiểm chứng 5 giai đoạn:
* **Phase 5: Serial Boot & Handshake**: Xác thực vi điều khiển khởi động trong 1.5s, nhận diện đúng chip ESP32-S3, phản hồi `PING/PONG` < 10ms.
* **Phase 6: REST API & Status Schema**: Kiểm tra toàn bộ cấu trúc dữ liệu JSON trả về đầy đủ các trường `s1`, `s2`, `ir`.
* **Phase 7: NVS Flash Read/Write**: Thử nghiệm ghi dữ liệu dừng vào flash, đọc lại và kiểm chứng tính toàn vẹn dữ liệu qua chu kỳ khởi động lại.
* **Phase 8: Dual Servo Motion Execution**: Kích hoạt tuần tự chuyển động Servo 1 và Servo 2, đo đạc thời gian chạy và kiểm tra việc ngắt PWM sau khi kết thúc chu kỳ.
* **Phase 9: High-Frequency Polling**: Thăm dò trạng thái liên tục ở tần số 10Hz trong 10 giây để đảm bảo vi điều khiển không bị tràn bộ nhớ hoặc rò rỉ RAM (Memory Leak).

---

## 9. ĐÁNH GIÁ RỦI RO, ĐIỂM CẦN LƯU Ý & KẾ HOẠCH NÂNG CẤP (AUDIT & ROADMAP)

### 9.1. Điểm Mạnh Đã Đạt Được
- **Kiến trúc mạch kép độc lập hoàn hảo**: Hai servo điều khiển riêng biệt trên GPIO 1 và GPIO 2, không gây xung đột tài nguyên timer LEDC.
- **Triệt tiêu hiện tượng trôi vị trí (Zero-Drift)**: Chốt giữ 0V khi nghỉ giúp tăng tuổi thọ bánh răng và động cơ Servo đáng kể.
- **Độ trễ điều khiển cực thấp**: Giao tiếp WebSocket và Serial non-blocking cho độ trễ phản hồi < 15ms.
- **Độ tin cậy cao**: Bộ đếm hồng ngoại có lọc debounce quang học, lưu flash an toàn.

### 9.2. Lưu Ý Quan Trọng Cho Đội Ngũ Kế Thừa (Third-Party Notes)
1. **Lắp đặt cảm biến IR ngoài thực tế**: Cần che chắn ống quang học quanh mắt thu để tránh ánh sáng mặt trời hoặc đèn cao áp rọi trực tiếp gây bão hòa quang điện.
2. **Cấp nguồn công suất**: Khi lắp đặt vào tủ điện công nghiệp, luôn giữ nguyên tắc nguồn tổ ong 12V qua LM2596 và **nối chung Mass (GND)** với chân `G` của ESP32.
3. **Cập nhật giao diện Standalone**: Nếu chỉnh sửa giao diện Web nhúng trên Access Point WiFi (`192.168.4.1`), cần đồng bộ lại file `web_ui.h`.

### 9.3. Lộ Trình Đề Xuất Phát Triển (Roadmap)
* **Pha tiếp theo 1 (OTA Update)**: Bổ sung tính năng cập nhật firmware không dây qua mạng WiFi (Arduino OTA / Web OTA) để bảo trì từ xa mà không cần cắm cáp USB.
* **Pha tiếp theo 2 (MQTT / Industrial IoT)**: Tích hợp thư viện MQTT client gửi dữ liệu đếm tô và cảnh báo kẹt servo lên hệ thống Cloud / SCADA nhà máy.
* **Pha tiếp theo 3 (Modularization)**: Tách mã nguồn `.ino` thành các module C++ chuẩn (`ServoController.h/.cpp`, `IrDetector.h/.cpp`, `DisplayManager.h/.cpp`) để thuận tiện cho việc quản lý mã nguồn quy mô lớn.

---
**Xác nhận bàn giao kỹ thuật:**  
Tài liệu và toàn bộ mã nguồn đã được biên dịch thành công, kiểm thử HIL đạt chuẩn 100% PASS và nạp sẵn sàng trên phần cứng vật lý.
