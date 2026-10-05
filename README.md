# ESP32-S3 Control Center: Continuous Servo & Telemetry Station

An industrial-grade motion control and hardware telemetry system engineered for the **LILYGO T-Display-S3** (ESP32-S3R8 with 1.9" ST7789 8-bit parallel IPS display), featuring active signal clamping for zero servo drift, dual Wi-Fi/Serial communication, real-time HIL verification, and a modern browser dashboard.

> 📘 **Tài liệu Bàn giao Kỹ thuật & Review Mã nguồn (Dành cho Bên thứ 3):**  
> Xem chi tiết tại [HANDOVER_AND_CODE_REVIEW.md](HANDOVER_AND_CODE_REVIEW.md) để nắm toàn bộ sơ đồ chân, kiến trúc code, API, sơ đồ nguồn tổ ong/LM2596 và hướng dẫn vận hành.

---

## 1. Hardware Specification Summary

| Hardware Dimension | Verified Specification | Engineering Notes |
|---|---|---|
| **Target Board** | **LILYGO T-Display-S3** (Non-Touch & Touch) | Authoritative target (`VERIFIED`) |
| **Microcontroller (MCU)**| **Espressif ESP32-S3 (ESP32-S3R8)** | 240 MHz Dual-Core Xtensa LX7 |
| **System Memory** | 512 KB Internal SRAM • **8 MB Octal PSRAM** | Octal SPI high-speed PSRAM |
| **Storage (Flash)** | **16 MB Quad/Octal SPI Flash** | Ample partition space for OTA / NVS |
| **Integrated Display** | **1.9-inch IPS Color LCD (ST7789V)** | 170 × 320 physical resolution (RGB565) |
| **Display Interface** | **8-bit Parallel 8080 (I80 bus)** | Fast 20 MB/s bus (**NOT SPI**) |
| **Peripheral Power Gate** | **GPIO 15** (`PIN_POWER_ON`) | Must be driven **HIGH** to energize display |
| **Backlight Control** | **GPIO 38** (`LCD_BL`) | N-MOSFET gate, LEDC PWM capable |
| **Primary Actuator Pin** | **GPIO 1** (`SERVO_PIN`) | Continuous 360° rotation servo control |
| **USB Interface** | USB Type-C Native USB CDC/JTAG | Direct connection to GPIO 19/20 |
| **Firmware Framework** | **PlatformIO Core + Arduino Core 2.0.17** | Strictly pinned dependencies |
| **Firmware Location** | Root directory | `ESP32_Control_Center_ServoStop_FIXED_v2.ino` |

For in-depth hardware documentation, see [docs/hardware/lilygo-t-display-s3/](docs/hardware/lilygo-t-display-s3/README.md).

---

## 2. Pinned Version Compatibility Matrix

> [!WARNING]
> Do NOT upgrade these libraries to unpinned `latest`. Upstream versions $\ge 3.0$ introduce breaking driver bugs on ESP32-S3.

| Component | Pinned Version | Status | Technical Rationale |
|---|---|:---:|---|
| **Development Board** | `lilygo-t-display-s3` | `VERIFIED` | Official PlatformIO board identifier |
| **Platform** | `espressif32 @ 7.1.3` | `VERIFIED` | Corresponds to stable Arduino Core 2.0.17 |
| **Framework** | `arduino` | `VERIFIED` | Standard ESP32 Arduino HAL |
| **`ESP32Servo`** | **`1.2.1`** | `VERIFIED` | Versions $\ge 3.1.0$ deadlock on ESP32-S3 MCPWM |
| **`GFX Library for Arduino`**| **`1.4.9`** | `VERIFIED` | Versions $\ge 1.5.0$ fail on missing `esp32-hal-periman.h` |
| **`Preferences`** | `2.0.0` (Core) | `VERIFIED` | NVS parameter persistence |
| **`WebServer`** | `2.0.0` (Core) | `VERIFIED` | Embedded SoftAP Web UI |
| **Host Bridge Runtime** | Python 3.10+ | `VERIFIED` | Starlette, Uvicorn, PySerial |

---

## 3. Quick Start: From Fresh Repo to Running Hardware

### 3.1 Prerequisites
1.  **Hardware:**
    *   LILYGO T-Display-S3 development board.
    *   Continuous 360° servo (e.g. MG995/MG996R/SG90-360) on **GPIO 1**.
    *   External 5V DC power supply (2A min) with common ground to ESP32 GND.
    *   USB-C data cable (must support data lines, not charge-only).
2.  **Software:**
    *   [PlatformIO Core (CLI)](https://platformio.org/install/cli) or VS Code with PlatformIO IDE extension.
    *   Python 3.10+ with `pyserial`, `starlette`, `uvicorn`.

### 3.2 Step 1: Connect Hardware
1. Connect ESP32-S3 to your PC via USB-C.
2. Connect Servo signal wire (Orange/White) to **Left Header Pin 2 (GPIO 1)**.
3. Connect Servo GND (Brown/Black) to **ESP32 GND** and **Power Supply GND**.
4. Connect Servo VCC (+5V, Red) to **External 5V Power Supply (+)**.

### 3.3 Step 2: Compile & Flash Firmware

```bash
# 1. Compile firmware
pio run -e lilygo-t-display-s3

# 2. Upload to board on COM3 (or detected port)
pio run -e lilygo-t-display-s3 -t upload -p COM3

# 3. Monitor boot output
pio device monitor -p COM3 -b 115200
```

### 3.4 Step 3: Expected Boot Output & Smoke Tests
Upon boot, the serial monitor emits:
```
================================
ESP32 CONTROL CENTER
LILYGO T-Display-S3
SERVO STOP BUG FIXED VERSION
================================
[NVS] Loaded config: cycles=1 forwardMs=1000 reverseMs=1000 pauseMs=500 stop=90 fwd=110 rev=70
[SERVO] De-energized and line clamped LOW.
[WIFI] SoftAP started! SSID: ESP32-Servo, IP: 192.168.4.1
[SYSTEM] READY - Cho lenh tu Web hoac Serial...
```

*   **LCD Smoke Test:** The 1.9" display illuminates brightly, displaying the dark-themed dashboard: `"SAN SANG"` (Ready), IP `192.168.4.1`, and cycle counter.
*   **Wi-Fi Smoke Test:** Connect your phone/laptop to Wi-Fi SSID **`ESP32-Servo`** (Password: `12345678`), open `http://192.168.4.1` in your browser. The embedded control deck is active.

### 3.5 Step 4: Launch Host Telemetry & Motion Studio

```bash
python server.py
```
Open **`http://localhost:8000`** in Chrome/Edge. The telemetry dashboard connects via WebSocket, displaying live tachometer angles, oscilloscope waveforms, and the top-level **MOTION CONTROL DECK**.

---

## 4. Key Engineering Innovations in this Firmware

### 1. Zero-Drift Lazy PWM & Active Ground Clamping
Standard servo libraries leave GPIO lines floating in high-impedance mode when calling `detach()`, picking up electromagnetic interference that causes continuous servos to slowly creep.  
This firmware implements an active ground clamp:
```cpp
void servoStopNow() {
    if (servoAttached) {
        servo.detach();
        servoAttached = false;
    }
    pinMode(SERVO_PIN, OUTPUT);
    digitalWrite(SERVO_PIN, LOW); // Clamps line to 0V ground
}
```
*Result:* Absolute zero motor creep ($0\text{ RPM}$) in all idle and pause states.

### 2. Dedicated Open & Close Operating Modes
*   **Mode Mở (OPEN):** Rotates forward at $110^\circ$ for `forwardMs`, halts, and holds the open position without rotating back.
*   **Mode Đóng (CLOSE):** Rotates reverse at $70^\circ$ for `reverseMs`, returning to the exact starting baseline position, then halts and holds closed.

### 3. Automated 5-Phase HIL Verification
Includes [scripts/hil_test_suite.py](scripts/hil_test_suite.py) testing boot timing, REST compliance, NVS flash persistence across RTS resets, servo motion cycle precision, and read-only polling purity against real hardware.

---

## 5. Authoritative Documentation Index

*   [Master Hardware Index](docs/hardware/lilygo-t-display-s3/README.md)
*   [Hardware Specifications](docs/hardware/lilygo-t-display-s3/hardware-specification.md)
*   [Authoritative Pinout Reference](docs/hardware/lilygo-t-display-s3/pinout.md)
*   [Display Subsystem Technical Guide](docs/hardware/lilygo-t-display-s3/display.md)
*   [Power Management & GPIO 15](docs/hardware/lilygo-t-display-s3/power.md)
*   [USB CDC, Flashing & Recovery](docs/hardware/lilygo-t-display-s3/usb-debugging.md)
*   [PlatformIO Build Guide](docs/hardware/lilygo-t-display-s3/platformio.md)
*   [Wi-Fi & SoftAP Architecture](docs/hardware/lilygo-t-display-s3/wifi.md)
*   [Firmware Architecture & State Machine](docs/hardware/lilygo-t-display-s3/firmware-architecture.md)
*   [Serial & REST Communication Protocol](docs/hardware/lilygo-t-display-s3/protocol.md)
*   [Peripheral Safety & Motor Interfacing](docs/hardware/lilygo-t-display-s3/peripherals.md)
*   [Testing & Verification Strategy](docs/hardware/lilygo-t-display-s3/testing.md)
*   [Hardware Troubleshooting Guide](docs/hardware/lilygo-t-display-s3/troubleshooting.md)
*   [HIL Deployment Verification Report](docs/DEPLOYMENT_AND_HIL_REPORT.md)
