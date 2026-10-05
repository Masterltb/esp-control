#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESP32Servo.h>
#include <Arduino_GFX_Library.h>

// ============================================================
// ESP32 CONTROL CENTER - DUAL SERVO FIXED VERSION
// Board: LILYGO T-Display-S3 (ESP32-S3R8, 16MB Flash, 8MB PSRAM)
// Servo 1: GPIO 1 (360° Continuous Rotation)
// Servo 2: GPIO 2 (360° Continuous Rotation)
// Both with Lazy PWM Attach & Zero-Drift Active Ground Clamping
// ============================================================

// -----------------------------
// 1. PIN CONFIGURATION
// -----------------------------
#define SERVO_1_PIN         1
#define SERVO_2_PIN         2
#define SERVO_PIN           SERVO_1_PIN // Backwards compatibility
#define IR_RECEIVER_PIN     10  // Left Header Pin 5 (Digital Input from IR Receiver)
#define IR_TRANSMITTER_PIN  11  // Left Header Pin 6 (Digital Output to IR Transmitter LED)
#define LCD_POWER_PIN       15
#define LCD_BL_PIN          38

// -----------------------------
// 2. LCD CONFIGURATION
// -----------------------------
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_ORANGE    0xFD20

Arduino_DataBus *bus = new Arduino_ESP32PAR8Q(
    7, 6, 8, 9,
    39, 40, 41, 42,
    45, 46, 47, 48
);

Arduino_GFX *gfx = new Arduino_ST7789(
    bus,
    5,
    0,
    true,
    170,
    320,
    35, 0, 35, 0
);

// -----------------------------
// 3. WIFI / WEB SERVER
// -----------------------------
const char *AP_SSID = "ESP32-Servo";
const char *AP_PASSWORD = "12345678";

WebServer server(80);
Preferences preferences;

Servo servo1;
bool servo1Attached = false;

Servo servo2;
bool servo2Attached = false;

// Backwards compatibility aliases
#define servo servo1
#define servoAttached servo1Attached

static const uint32_t CONFIG_SCHEMA_VERSION = 5;

// -----------------------------
// 4. DUAL SERVO CONFIGURATION
// -----------------------------
// Servo 1 (GPIO 1):
int servo1Stop = 90;
int servo1Forward = 110;
int servo1Reverse = 70;
unsigned long forwardMs1 = 1000;
unsigned long reverseMs1 = 1000;
unsigned long pauseMs1 = 500;
int cycles1 = 1;

// Servo 2 (GPIO 2):
int servo2Stop = 90;
int servo2Forward = 110;
int servo2Reverse = 70;
unsigned long forwardMs2 = 1000;
unsigned long reverseMs2 = 1000;
unsigned long pauseMs2 = 500;
int cycles2 = 1;

// -----------------------------
// 4B. IR SENSOR & BOWL DETECTION
// -----------------------------
bool irDetected = false;            // True when bowl/object is detected
int irRawValue = HIGH;              // Raw reading from GPIO 10
bool irActiveLow = true;            // Standard optical sensor: LOW when beam broken
uint32_t irObjectCount = 0;         // Total bowls/objects passed
unsigned long irLastDetectionMs = 0;// Timestamp of last detection
unsigned long irBlockStartMs = 0;   // When current beam block started
unsigned long irBlockDurationMs = 0;// How long beam has been blocked
int irAutoTriggerServo = 0;         // 0: Disabled, 1: Auto S1, 2: Auto S2, 3: Auto Both
bool irTxEnabled = true;            // IR Transmitter power state
unsigned long irDebounceMs = 60;    // Non-blocking debounce time in ms
unsigned long irLastStateChangeMs = 0;
bool irStableReading = false;

// Compatibility aliases
#define servoStop servo1Stop
#define servoForward servo1Forward
#define servoReverse servo1Reverse
#define forwardMs forwardMs1
#define reverseMs reverseMs1
#define pauseMs pauseMs1
#define cycles cycles1

// -----------------------------
// 5. LIMITS
// -----------------------------
static const int SERVO_VALUE_MIN = 0;
static const int SERVO_VALUE_MAX = 180;
static const unsigned long MOTION_TIME_MIN = 1;
static const unsigned long MOTION_TIME_MAX = 600000UL; // 10 min
static const int CYCLES_MIN = 1;
static const int CYCLES_MAX = 1000;

// -----------------------------
// 6. STATE MACHINES
// -----------------------------
enum MotionState {
    STATE_IDLE,
    STATE_FORWARD,
    STATE_PAUSE_AFTER_FORWARD,
    STATE_REVERSE,
    STATE_PAUSE_AFTER_REVERSE,
    STATE_MANUAL_FORWARD,
    STATE_MANUAL_REVERSE,
    STATE_OPENING,
    STATE_OPENED,
    STATE_CLOSING,
    STATE_CLOSED,
    STATE_COMPLETE,
    STATE_ERROR
};

MotionState motionState1 = STATE_IDLE;
unsigned long state1StartedAt = 0;
int currentCycle1 = 0;

MotionState motionState2 = STATE_IDLE;
unsigned long state2StartedAt = 0;
int currentCycle2 = 0;

#define motionState motionState1
#define stateStartedAt state1StartedAt
#define currentCycle currentCycle1

// ============================================================
// 7. HELPERS
// ============================================================

String jsonEscape(const String &value) {
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

String motionStateText(MotionState st) {
    switch (st) {
        case STATE_IDLE:                    return "Sẵn sàng";
        case STATE_FORWARD:                 return "Đang quay thuận";
        case STATE_PAUSE_AFTER_FORWARD:     return "Đang dừng";
        case STATE_REVERSE:                 return "Đang quay nghịch";
        case STATE_PAUSE_AFTER_REVERSE:     return "Đang dừng";
        case STATE_MANUAL_FORWARD:          return "Test thuận";
        case STATE_MANUAL_REVERSE:          return "Test nghịch";
        case STATE_OPENING:                 return "Đang mở";
        case STATE_OPENED:                  return "Đã mở";
        case STATE_CLOSING:                 return "Đang đóng";
        case STATE_CLOSED:                  return "Đã đóng";
        case STATE_COMPLETE:                return "Hoàn thành";
        case STATE_ERROR:                   return "Lỗi";
        default:                            return "Không xác định";
    }
}

String motionStateText() {
    return motionStateText(motionState1);
}

bool motionIsRunning(MotionState st) {
    return st == STATE_FORWARD ||
           st == STATE_PAUSE_AFTER_FORWARD ||
           st == STATE_REVERSE ||
           st == STATE_PAUSE_AFTER_REVERSE ||
           st == STATE_MANUAL_FORWARD ||
           st == STATE_MANUAL_REVERSE ||
           st == STATE_OPENING ||
           st == STATE_CLOSING;
}

bool motionIsRunning() {
    return motionIsRunning(motionState1) || motionIsRunning(motionState2);
}

bool parseIntStrict(const String &text, int &value) {
    if (text.length() == 0) return false;
    char *endptr = nullptr;
    long v = strtol(text.c_str(), &endptr, 10);
    if (!endptr || *endptr != '\0') return false;
    value = (int)v;
    return true;
}

bool parseUnsignedLongStrict(const String &text, unsigned long &value) {
    if (text.length() == 0) return false;
    char *endptr = nullptr;
    unsigned long v = strtoul(text.c_str(), &endptr, 10);
    if (!endptr || *endptr != '\0') return false;
    value = v;
    return true;
}

void sendJsonError(int statusCode, const String &message) {
    String payload = "{";
    payload += "\"ok\":false,";
    payload += "\"error\":\"" + jsonEscape(message) + "\"";
    payload += "}";
    server.send(statusCode, "application/json; charset=utf-8", payload);
}

void sendJsonOk(const String &message = "") {
    String payload = "{";
    payload += "\"ok\":true";
    if (message.length() > 0) {
        payload += ",\"message\":\"" + jsonEscape(message) + "\"";
    }
    payload += "}";
    server.send(200, "application/json; charset=utf-8", payload);
}

// ============================================================
// 8. LCD DISPLAY DRIVER
// ============================================================

void lcdShow(
    String title,
    String line1 = "",
    String line2 = "",
    String line3 = ""
) {
    gfx->fillScreen(COLOR_BLACK);
    gfx->setTextColor(COLOR_CYAN);
    gfx->setTextSize(2);
    gfx->setCursor(10, 16);
    gfx->println(title);

    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_WHITE);

    if (line1.length() > 0) {
        gfx->setCursor(10, 50);
        gfx->println(line1);
    }
    if (line2.length() > 0) {
        gfx->setCursor(10, 75);
        gfx->println(line2);
    }
    if (line3.length() > 0) {
        gfx->setCursor(10, 100);
        gfx->println(line3);
    }
}

void updateLcdDual() {
    gfx->fillScreen(COLOR_BLACK);
    gfx->setTextColor(COLOR_CYAN);
    gfx->setTextSize(2);
    gfx->setCursor(10, 14);
    gfx->println("ESP32 DUAL SERVO");

    // S1 Line
    gfx->setTextSize(1);
    gfx->setTextColor(motionIsRunning(motionState1) ? COLOR_GREEN : COLOR_YELLOW);
    gfx->setCursor(10, 44);
    gfx->println("S1(G1): " + motionStateText(motionState1) + " (" + String(currentCycle1) + "/" + String(cycles1) + ")");

    // S2 Line
    gfx->setTextColor(motionIsRunning(motionState2) ? COLOR_GREEN : COLOR_ORANGE);
    gfx->setCursor(10, 68);
    gfx->println("S2(G2): " + motionStateText(motionState2) + " (" + String(currentCycle2) + "/" + String(cycles2) + ")");

    // IR Bowl Sensor Line
    gfx->setTextColor(irDetected ? COLOR_RED : COLOR_GREEN);
    gfx->setCursor(10, 92);
    gfx->println("IR(G10): " + String(irDetected ? "TO DI QUA! [DETECTED]" : "THONG SUOT [CLEAR]") + " #" + String(irObjectCount));

    // AP IP line
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 116);
    gfx->println("AP: " + WiFi.softAPIP().toString() + (irAutoTriggerServo > 0 ? " [AUTO]" : ""));
}

// ============================================================
// 9. SERVO 1 LOW-LEVEL CONTROL (GPIO 1)
// ============================================================

void attachServo1IfNeeded() {
    if (servo1Attached) return;
    servo1.setPeriodHertz(50);
    servo1.attach(SERVO_1_PIN, 500, 2500);
    servo1Attached = true;
    delay(20);
}

void servo1StopNow() {
    if (servo1Attached) {
        servo1.write(servo1Stop);
        delay(20);
        servo1.detach();
        servo1Attached = false;
    }
    // Active ground clamp for zero drift
    pinMode(SERVO_1_PIN, OUTPUT);
    digitalWrite(SERVO_1_PIN, LOW);
    Serial.println("[SERVO1] STOP -> PWM DETACHED, GPIO 1 CLAMPED LOW");
}

void servo1ForwardNow() {
    attachServo1IfNeeded();
    servo1.write(servo1Forward);
    Serial.println("[SERVO1] FORWARD = " + String(servo1Forward));
}

void servo1ReverseNow() {
    attachServo1IfNeeded();
    servo1.write(servo1Reverse);
    Serial.println("[SERVO1] REVERSE = " + String(servo1Reverse));
}

void servo1TestStopValue(int value) {
    attachServo1IfNeeded();
    servo1.write(value);
    Serial.println("[CALIB1] STOP TEST = " + String(value));
}

// ============================================================
// 9B. SERVO 2 LOW-LEVEL CONTROL (GPIO 2)
// ============================================================

void attachServo2IfNeeded() {
    if (servo2Attached) return;
    servo2.setPeriodHertz(50);
    servo2.attach(SERVO_2_PIN, 500, 2500);
    servo2Attached = true;
    delay(20);
}

void servo2StopNow() {
    if (servo2Attached) {
        servo2.write(servo2Stop);
        delay(20);
        servo2.detach();
        servo2Attached = false;
    }
    // Active ground clamp for zero drift
    pinMode(SERVO_2_PIN, OUTPUT);
    digitalWrite(SERVO_2_PIN, LOW);
    Serial.println("[SERVO2] STOP -> PWM DETACHED, GPIO 2 CLAMPED LOW");
}

void servo2ForwardNow() {
    attachServo2IfNeeded();
    servo2.write(servo2Forward);
    Serial.println("[SERVO2] FORWARD = " + String(servo2Forward));
}

void servo2ReverseNow() {
    attachServo2IfNeeded();
    servo2.write(servo2Reverse);
    Serial.println("[SERVO2] REVERSE = " + String(servo2Reverse));
}

void servo2TestStopValue(int value) {
    attachServo2IfNeeded();
    servo2.write(value);
    Serial.println("[CALIB2] STOP TEST = " + String(value));
}

// Aliases for compatibility
void attachServoIfNeeded() { attachServo1IfNeeded(); }
void servoStopNow() { servo1StopNow(); }
void servoForwardNow() { servo1ForwardNow(); }
void servoReverseNow() { servo1ReverseNow(); }
void servoTestStopValue(int val) { servo1TestStopValue(val); }

// ============================================================
// 10. HIGH-LEVEL MOTION ACTIONS
// ============================================================

// --- SERVO 1 ACTIONS ---
void startMotion1() {
    if (cycles1 < CYCLES_MIN) cycles1 = CYCLES_MIN;
    servo1StopNow();
    currentCycle1 = 1;
    motionState1 = STATE_FORWARD;
    state1StartedAt = millis();
    servo1ForwardNow();
    Serial.println("[MOTION1] START cycle=1/" + String(cycles1));
    updateLcdDual();
}

void startOpen1() {
    servo1StopNow();
    currentCycle1 = 0;
    motionState1 = STATE_OPENING;
    state1StartedAt = millis();
    servo1ForwardNow();
    Serial.println("[MOTION1] START OPENING (Forward=" + String(servo1Forward) + ", time=" + String(forwardMs1) + "ms)");
    updateLcdDual();
}

void startClose1() {
    servo1StopNow();
    currentCycle1 = 0;
    motionState1 = STATE_CLOSING;
    state1StartedAt = millis();
    servo1ReverseNow();
    Serial.println("[MOTION1] START CLOSING (Reverse=" + String(servo1Reverse) + ", time=" + String(reverseMs1) + "ms)");
    updateLcdDual();
}

void stopMotion1() {
    servo1StopNow();
    motionState1 = STATE_IDLE;
    currentCycle1 = 0;
    state1StartedAt = millis();
    Serial.println("[MOTION1] STOP -> IDLE");
    updateLcdDual();
}

void testForward1() {
    servo1StopNow();
    currentCycle1 = 0;
    motionState1 = STATE_MANUAL_FORWARD;
    state1StartedAt = millis();
    servo1ForwardNow();
    Serial.println("[MOTION1] TEST FWD");
    updateLcdDual();
}

void testReverse1() {
    servo1StopNow();
    currentCycle1 = 0;
    motionState1 = STATE_MANUAL_REVERSE;
    state1StartedAt = millis();
    servo1ReverseNow();
    Serial.println("[MOTION1] TEST REV");
    updateLcdDual();
}

// Aliases for compatibility
void startMotion() { startMotion1(); }
void startOpen() { startOpen1(); }
void startClose() { startClose1(); }
void stopMotion() { stopMotion1(); }

// --- SERVO 2 ACTIONS ---
void startMotion2() {
    if (cycles2 < CYCLES_MIN) cycles2 = CYCLES_MIN;
    servo2StopNow();
    currentCycle2 = 1;
    motionState2 = STATE_FORWARD;
    state2StartedAt = millis();
    servo2ForwardNow();
    Serial.println("[MOTION2] START cycle=1/" + String(cycles2));
    updateLcdDual();
}

void startOpen2() {
    servo2StopNow();
    currentCycle2 = 0;
    motionState2 = STATE_OPENING;
    state2StartedAt = millis();
    servo2ForwardNow();
    Serial.println("[MOTION2] START OPENING (Forward=" + String(servo2Forward) + ", time=" + String(forwardMs2) + "ms)");
    updateLcdDual();
}

void startClose2() {
    servo2StopNow();
    currentCycle2 = 0;
    motionState2 = STATE_CLOSING;
    state2StartedAt = millis();
    servo2ReverseNow();
    Serial.println("[MOTION2] START CLOSING (Reverse=" + String(servo2Reverse) + ", time=" + String(reverseMs2) + "ms)");
    updateLcdDual();
}

void stopMotion2() {
    servo2StopNow();
    motionState2 = STATE_IDLE;
    currentCycle2 = 0;
    state2StartedAt = millis();
    Serial.println("[MOTION2] STOP -> IDLE");
    updateLcdDual();
}

void testForward2() {
    servo2StopNow();
    currentCycle2 = 0;
    motionState2 = STATE_MANUAL_FORWARD;
    state2StartedAt = millis();
    servo2ForwardNow();
    Serial.println("[MOTION2] TEST FWD");
    updateLcdDual();
}

void testReverse2() {
    servo2StopNow();
    currentCycle2 = 0;
    motionState2 = STATE_MANUAL_REVERSE;
    state2StartedAt = millis();
    servo2ReverseNow();
    Serial.println("[MOTION2] TEST REV");
    updateLcdDual();
}

// --- COMBINED MASTER ACTIONS ---
void startAll() {
    startMotion1();
    startMotion2();
}

void stopAll() {
    stopMotion1();
    stopMotion2();
}

void openAll() {
    startOpen1();
    startOpen2();
}

void closeAll() {
    startClose1();
    startClose2();
}

// ============================================================
// 11. MOTION STATE MACHINE TIMING LOOPS
// ============================================================

void updateMotion1() {
    if (!motionIsRunning(motionState1)) return;
    unsigned long elapsed = millis() - state1StartedAt;

    switch (motionState1) {
        case STATE_FORWARD:
            if (elapsed >= forwardMs1) {
                servo1StopNow();
                motionState1 = STATE_PAUSE_AFTER_FORWARD;
                state1StartedAt = millis();
                updateLcdDual();
            }
            break;

        case STATE_PAUSE_AFTER_FORWARD:
            if (elapsed >= pauseMs1) {
                servo1ReverseNow();
                motionState1 = STATE_REVERSE;
                state1StartedAt = millis();
                updateLcdDual();
            }
            break;

        case STATE_REVERSE:
            if (elapsed >= reverseMs1) {
                servo1StopNow();
                motionState1 = STATE_PAUSE_AFTER_REVERSE;
                state1StartedAt = millis();
                updateLcdDual();
            }
            break;

        case STATE_PAUSE_AFTER_REVERSE:
            if (elapsed >= pauseMs1) {
                if (currentCycle1 >= cycles1) {
                    servo1StopNow();
                    motionState1 = STATE_COMPLETE;
                    state1StartedAt = millis();
                    Serial.println("[MOTION1] COMPLETE");
                    updateLcdDual();
                } else {
                    currentCycle1++;
                    motionState1 = STATE_FORWARD;
                    state1StartedAt = millis();
                    servo1ForwardNow();
                    updateLcdDual();
                }
            }
            break;

        case STATE_OPENING:
            if (elapsed >= forwardMs1) {
                servo1StopNow();
                motionState1 = STATE_OPENED;
                state1StartedAt = millis();
                Serial.println("[MOTION1] OPENED -> HOLDING");
                updateLcdDual();
            }
            break;

        case STATE_CLOSING:
            if (elapsed >= reverseMs1) {
                servo1StopNow();
                motionState1 = STATE_CLOSED;
                state1StartedAt = millis();
                Serial.println("[MOTION1] CLOSED -> HOLDING");
                updateLcdDual();
            }
            break;

        default:
            break;
    }
}

void updateMotion2() {
    if (!motionIsRunning(motionState2)) return;
    unsigned long elapsed = millis() - state2StartedAt;

    switch (motionState2) {
        case STATE_FORWARD:
            if (elapsed >= forwardMs2) {
                servo2StopNow();
                motionState2 = STATE_PAUSE_AFTER_FORWARD;
                state2StartedAt = millis();
                updateLcdDual();
            }
            break;

        case STATE_PAUSE_AFTER_FORWARD:
            if (elapsed >= pauseMs2) {
                servo2ReverseNow();
                motionState2 = STATE_REVERSE;
                state2StartedAt = millis();
                updateLcdDual();
            }
            break;

        case STATE_REVERSE:
            if (elapsed >= reverseMs2) {
                servo2StopNow();
                motionState2 = STATE_PAUSE_AFTER_REVERSE;
                state2StartedAt = millis();
                updateLcdDual();
            }
            break;

        case STATE_PAUSE_AFTER_REVERSE:
            if (elapsed >= pauseMs2) {
                if (currentCycle2 >= cycles2) {
                    servo2StopNow();
                    motionState2 = STATE_COMPLETE;
                    state2StartedAt = millis();
                    Serial.println("[MOTION2] COMPLETE");
                    updateLcdDual();
                } else {
                    currentCycle2++;
                    motionState2 = STATE_FORWARD;
                    state2StartedAt = millis();
                    servo2ForwardNow();
                    updateLcdDual();
                }
            }
            break;

        case STATE_OPENING:
            if (elapsed >= forwardMs2) {
                servo2StopNow();
                motionState2 = STATE_OPENED;
                state2StartedAt = millis();
                Serial.println("[MOTION2] OPENED -> HOLDING");
                updateLcdDual();
            }
            break;

        case STATE_CLOSING:
            if (elapsed >= reverseMs2) {
                servo2StopNow();
                motionState2 = STATE_CLOSED;
                state2StartedAt = millis();
                Serial.println("[MOTION2] CLOSED -> HOLDING");
                updateLcdDual();
            }
            break;

        default:
            break;
    }
}

void updateMotion() {
    updateMotion1();
    updateMotion2();
}

// ============================================================
// 11B. IR SENSOR & BOWL DETECTION ENGINE
// ============================================================

void setupIrSensor() {
    pinMode(IR_RECEIVER_PIN, INPUT_PULLUP);
    pinMode(IR_TRANSMITTER_PIN, OUTPUT);
    digitalWrite(IR_TRANSMITTER_PIN, irTxEnabled ? HIGH : LOW);
    irRawValue = digitalRead(IR_RECEIVER_PIN);
    irDetected = (irActiveLow ? (irRawValue == LOW) : (irRawValue == HIGH));
    irStableReading = irDetected;
    irLastStateChangeMs = millis();
    Serial.println("[IR] Sensor initialized on GPIO 10 (Rx) & GPIO 11 (Tx)");
}

void updateIrSensor() {
    int raw = digitalRead(IR_RECEIVER_PIN);
    irRawValue = raw;
    bool currentReading = (irActiveLow ? (raw == LOW) : (raw == HIGH));
    unsigned long now = millis();

    if (currentReading != irStableReading) {
        if (now - irLastStateChangeMs >= irDebounceMs) {
            irStableReading = currentReading;
            irLastStateChangeMs = now;

            if (irStableReading && !irDetected) {
                // RISING EDGE: Bowl broke the beam
                irDetected = true;
                irObjectCount++;
                irLastDetectionMs = now;
                irBlockStartMs = now;
                irBlockDurationMs = 0;

                Serial.printf("[IR] BOWL DETECTED! Count = %u (GPIO 10)\n", irObjectCount);

                // Auto-trigger servo if configured
                if (irAutoTriggerServo == 1) {
                    if (!motionIsRunning(motionState1)) {
                        Serial.println("[IR->AUTO] Auto-triggering Servo 1 (Open)");
                        startOpen1();
                    }
                } else if (irAutoTriggerServo == 2) {
                    if (!motionIsRunning(motionState2)) {
                        Serial.println("[IR->AUTO] Auto-triggering Servo 2 (Open)");
                        startOpen2();
                    }
                } else if (irAutoTriggerServo == 3) {
                    Serial.println("[IR->AUTO] Auto-triggering All Servos (Open)");
                    openAll();
                }

                updateLcdDual();
            } else if (!irStableReading && irDetected) {
                // FALLING EDGE: Bowl has passed through
                irDetected = false;
                irBlockDurationMs = now - irBlockStartMs;
                Serial.printf("[IR] BEAM CLEAR! Bowl passed through in %lu ms\n", irBlockDurationMs);
                updateLcdDual();
            }
        }
    } else {
        irLastStateChangeMs = now;
        if (irDetected) {
            irBlockDurationMs = now - irBlockStartMs;
            // Auto-clear simulated pulses
            if (!irStableReading && (now - irLastDetectionMs > 600)) {
                irDetected = false;
                Serial.printf("[IR] BEAM CLEAR! Bowl passed through in %lu ms\n", irBlockDurationMs);
                updateLcdDual();
            }
        }
    }
}

// ============================================================
// 12. PREFERENCES (NVS PERSISTENCE)
// ============================================================

void setDefaultConfig() {
    servo1Stop = 90;
    servo1Forward = 110;
    servo1Reverse = 70;
    forwardMs1 = 1000;
    reverseMs1 = 1000;
    pauseMs1 = 500;
    cycles1 = 1;

    servo2Stop = 90;
    servo2Forward = 110;
    servo2Reverse = 70;
    forwardMs2 = 1000;
    reverseMs2 = 1000;
    pauseMs2 = 500;
    cycles2 = 1;

    irAutoTriggerServo = 0;
    irActiveLow = true;
}

void saveConfig() {
    preferences.begin("servo-config", false);
    preferences.putUInt("schema", CONFIG_SCHEMA_VERSION);

    // Servo 1
    preferences.putInt("stop1", servo1Stop);
    preferences.putInt("fwd1", servo1Forward);
    preferences.putInt("rev1", servo1Reverse);
    preferences.putULong("fwdMs1", forwardMs1);
    preferences.putULong("revMs1", reverseMs1);
    preferences.putULong("pauseMs1", pauseMs1);
    preferences.putInt("cycles1", cycles1);

    // Servo 2
    preferences.putInt("stop2", servo2Stop);
    preferences.putInt("fwd2", servo2Forward);
    preferences.putInt("rev2", servo2Reverse);
    preferences.putULong("fwdMs2", forwardMs2);
    preferences.putULong("revMs2", reverseMs2);
    preferences.putULong("pauseMs2", pauseMs2);
    preferences.putInt("cycles2", cycles2);

    // IR Sensor settings
    preferences.putInt("irAuto", irAutoTriggerServo);
    preferences.putBool("irActiveLow", irActiveLow);

    preferences.end();
    Serial.println("[CONFIG] Saved dual-servo and IR configuration to Preferences");
}

void loadConfig() {
    preferences.begin("servo-config", true);
    uint32_t schema = preferences.getUInt("schema", 0);

    if (schema != CONFIG_SCHEMA_VERSION) {
        preferences.end();
        setDefaultConfig();
        saveConfig();
        Serial.println("[CONFIG] Old/missing schema -> defaults applied for S1, S2 & IR");
        return;
    }

    // Load S1
    servo1Stop = preferences.getInt("stop1", 90);
    servo1Forward = preferences.getInt("fwd1", 110);
    servo1Reverse = preferences.getInt("rev1", 70);
    forwardMs1 = preferences.getULong("fwdMs1", 1000);
    reverseMs1 = preferences.getULong("revMs1", 1000);
    pauseMs1 = preferences.getULong("pauseMs1", 500);
    cycles1 = preferences.getInt("cycles1", 1);

    // Load S2
    servo2Stop = preferences.getInt("stop2", 90);
    servo2Forward = preferences.getInt("fwd2", 110);
    servo2Reverse = preferences.getInt("rev2", 70);
    forwardMs2 = preferences.getULong("fwdMs2", 1000);
    reverseMs2 = preferences.getULong("revMs2", 1000);
    pauseMs2 = preferences.getULong("pauseMs2", 500);
    cycles2 = preferences.getInt("cycles2", 1);

    // Load IR
    irAutoTriggerServo = preferences.getInt("irAuto", 0);
    irActiveLow = preferences.getBool("irActiveLow", true);

    preferences.end();

    // Constrain S1
    servo1Stop = constrain(servo1Stop, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    servo1Forward = constrain(servo1Forward, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    servo1Reverse = constrain(servo1Reverse, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    if (forwardMs1 < MOTION_TIME_MIN || forwardMs1 > MOTION_TIME_MAX) forwardMs1 = 1000;
    if (reverseMs1 < MOTION_TIME_MIN || reverseMs1 > MOTION_TIME_MAX) reverseMs1 = 1000;
    if (pauseMs1 > MOTION_TIME_MAX) pauseMs1 = 500;
    if (cycles1 < CYCLES_MIN || cycles1 > CYCLES_MAX) cycles1 = 1;

    // Constrain S2
    servo2Stop = constrain(servo2Stop, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    servo2Forward = constrain(servo2Forward, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    servo2Reverse = constrain(servo2Reverse, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    if (forwardMs2 < MOTION_TIME_MIN || forwardMs2 > MOTION_TIME_MAX) forwardMs2 = 1000;
    if (reverseMs2 < MOTION_TIME_MIN || reverseMs2 > MOTION_TIME_MAX) reverseMs2 = 1000;
    if (pauseMs2 > MOTION_TIME_MAX) pauseMs2 = 500;
    if (cycles2 < CYCLES_MIN || cycles2 > CYCLES_MAX) cycles2 = 1;

    Serial.println("[CONFIG] Dual-servo & IR configuration loaded");
}

// ============================================================
// 13. JSON SERIALIZERS
// ============================================================

String makeConfigJson() {
    String json = "{";
    json += "\"ok\":true,";
    json += "\"servoPin\":" + String(SERVO_1_PIN) + ",";
    json += "\"servoStop\":" + String(servo1Stop) + ",";
    json += "\"servoForward\":" + String(servo1Forward) + ",";
    json += "\"servoReverse\":" + String(servo1Reverse) + ",";
    json += "\"forwardMs\":" + String(forwardMs1) + ",";
    json += "\"reverseMs\":" + String(reverseMs1) + ",";
    json += "\"pauseMs\":" + String(pauseMs1) + ",";
    json += "\"cycles\":" + String(cycles1) + ",";

    // Dual channel details
    json += "\"s1\":{";
    json += "\"pin\":1,";
    json += "\"stop\":" + String(servo1Stop) + ",";
    json += "\"forward\":" + String(servo1Forward) + ",";
    json += "\"reverse\":" + String(servo1Reverse) + ",";
    json += "\"forwardMs\":" + String(forwardMs1) + ",";
    json += "\"reverseMs\":" + String(reverseMs1) + ",";
    json += "\"pauseMs\":" + String(pauseMs1) + ",";
    json += "\"cycles\":" + String(cycles1);
    json += "},";

    json += "\"s2\":{";
    json += "\"pin\":2,";
    json += "\"stop\":" + String(servo2Stop) + ",";
    json += "\"forward\":" + String(servo2Forward) + ",";
    json += "\"reverse\":" + String(servo2Reverse) + ",";
    json += "\"forwardMs\":" + String(forwardMs2) + ",";
    json += "\"reverseMs\":" + String(reverseMs2) + ",";
    json += "\"pauseMs\":" + String(pauseMs2) + ",";
    json += "\"cycles\":" + String(cycles2);
    json += "}";

    json += "}";
    return json;
}

String makeStatusJson() {
    String json = "{";
    json += "\"ok\":true,";
    json += "\"state\":\"" + jsonEscape(motionStateText(motionState1)) + "\",";
    json += "\"running\":" + String(motionIsRunning() ? "true" : "false") + ",";
    json += "\"cycle\":" + String(currentCycle1) + ",";
    json += "\"cycles\":" + String(cycles1) + ",";
    json += "\"servoStop\":" + String(servo1Stop) + ",";
    json += "\"servoForward\":" + String(servo1Forward) + ",";
    json += "\"servoReverse\":" + String(servo1Reverse) + ",";
    json += "\"forwardMs\":" + String(forwardMs1) + ",";
    json += "\"reverseMs\":" + String(reverseMs1) + ",";
    json += "\"pauseMs\":" + String(pauseMs1) + ",";
    json += "\"ip\":\"" + jsonEscape(WiFi.softAPIP().toString()) + "\",";

    // Servo 1 telemetry
    json += "\"s1\":{";
    json += "\"pin\":1,";
    json += "\"state\":\"" + jsonEscape(motionStateText(motionState1)) + "\",";
    json += "\"running\":" + String(motionIsRunning(motionState1) ? "true" : "false") + ",";
    json += "\"cycle\":" + String(currentCycle1) + ",";
    json += "\"cycles\":" + String(cycles1) + ",";
    json += "\"stop\":" + String(servo1Stop) + ",";
    json += "\"forward\":" + String(servo1Forward) + ",";
    json += "\"reverse\":" + String(servo1Reverse) + ",";
    json += "\"forwardMs\":" + String(forwardMs1) + ",";
    json += "\"reverseMs\":" + String(reverseMs1) + ",";
    json += "\"pauseMs\":" + String(pauseMs1);
    json += "},";

    // Servo 2 telemetry
    json += "\"s2\":{";
    json += "\"pin\":2,";
    json += "\"state\":\"" + jsonEscape(motionStateText(motionState2)) + "\",";
    json += "\"running\":" + String(motionIsRunning(motionState2) ? "true" : "false") + ",";
    json += "\"cycle\":" + String(currentCycle2) + ",";
    json += "\"cycles\":" + String(cycles2) + ",";
    json += "\"stop\":" + String(servo2Stop) + ",";
    json += "\"forward\":" + String(servo2Forward) + ",";
    json += "\"reverse\":" + String(servo2Reverse) + ",";
    json += "\"forwardMs\":" + String(forwardMs2) + ",";
    json += "\"reverseMs\":" + String(reverseMs2) + ",";
    json += "\"pauseMs\":" + String(pauseMs2);
    json += "},";

    // IR Sensor telemetry
    json += "\"ir\":{";
    json += "\"pin\":10,";
    json += "\"txPin\":11,";
    json += "\"detected\":" + String(irDetected ? "true" : "false") + ",";
    json += "\"raw\":" + String(irRawValue) + ",";
    json += "\"count\":" + String(irObjectCount) + ",";
    json += "\"blockMs\":" + String(irBlockDurationMs) + ",";
    json += "\"autoTrigger\":" + String(irAutoTriggerServo) + ",";
    json += "\"txEnabled\":" + String(irTxEnabled ? "true" : "false");
    json += "}";

    json += "}";
    return json;
}

// ============================================================
// 14. WEB UI HEADER
// ============================================================
#include "web_ui.h"

// ============================================================
// 15. API ROUTE HANDLERS
// ============================================================

void handleRoot() {
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleStatus() {
    server.send(200, "application/json; charset=utf-8", makeStatusJson());
}

void handleGetConfig() {
    server.send(200, "application/json; charset=utf-8", makeConfigJson());
}

void handleFactoryReset() {
    if (motionIsRunning()) {
        sendJsonError(409, "Không thể khôi phục khi Servo đang chạy");
        return;
    }
    stopAll();
    setDefaultConfig();
    saveConfig();
    servo1StopNow();
    servo2StopNow();
    server.send(200, "application/json; charset=utf-8", makeConfigJson());
}

void handleConfig() {
    // Allows updating S1, S2, or both
    int pStop1 = servo1Stop, pFwd1 = servo1Forward, pRev1 = servo1Reverse, pCyc1 = cycles1;
    unsigned long pFwdMs1 = forwardMs1, pRevMs1 = reverseMs1, pPauseMs1 = pauseMs1;
    int pStop2 = servo2Stop, pFwd2 = servo2Forward, pRev2 = servo2Reverse, pCyc2 = cycles2;
    unsigned long pFwdMs2 = forwardMs2, pRevMs2 = reverseMs2, pPauseMs2 = pauseMs2;

    if (server.hasArg("stop")) pStop1 = server.arg("stop").toInt();
    if (server.hasArg("forward")) pFwd1 = server.arg("forward").toInt();
    if (server.hasArg("reverse")) pRev1 = server.arg("reverse").toInt();
    if (server.hasArg("forwardMs")) pFwdMs1 = strtoul(server.arg("forwardMs").c_str(), NULL, 10);
    if (server.hasArg("reverseMs")) pRevMs1 = strtoul(server.arg("reverseMs").c_str(), NULL, 10);
    if (server.hasArg("pauseMs")) pPauseMs1 = strtoul(server.arg("pauseMs").c_str(), NULL, 10);
    if (server.hasArg("cycles")) pCyc1 = server.arg("cycles").toInt();

    // S2 specific args
    if (server.hasArg("stop2")) pStop2 = server.arg("stop2").toInt();
    if (server.hasArg("forward2")) pFwd2 = server.arg("forward2").toInt();
    if (server.hasArg("reverse2")) pRev2 = server.arg("reverse2").toInt();
    if (server.hasArg("forwardMs2")) pFwdMs2 = strtoul(server.arg("forwardMs2").c_str(), NULL, 10);
    if (server.hasArg("reverseMs2")) pRevMs2 = strtoul(server.arg("reverseMs2").c_str(), NULL, 10);
    if (server.hasArg("pauseMs2")) pPauseMs2 = strtoul(server.arg("pauseMs2").c_str(), NULL, 10);
    if (server.hasArg("cycles2")) pCyc2 = server.arg("cycles2").toInt();

    if (motionIsRunning()) {
        sendJsonError(409, "Không thể lưu khi Servo đang chạy");
        return;
    }

    servo1Stop = constrain(pStop1, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    servo1Forward = constrain(pFwd1, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    servo1Reverse = constrain(pRev1, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    forwardMs1 = constrain(pFwdMs1, MOTION_TIME_MIN, MOTION_TIME_MAX);
    reverseMs1 = constrain(pRevMs1, MOTION_TIME_MIN, MOTION_TIME_MAX);
    pauseMs1 = constrain(pPauseMs1, 0UL, MOTION_TIME_MAX);
    cycles1 = constrain(pCyc1, CYCLES_MIN, CYCLES_MAX);

    servo2Stop = constrain(pStop2, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    servo2Forward = constrain(pFwd2, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    servo2Reverse = constrain(pRev2, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
    forwardMs2 = constrain(pFwdMs2, MOTION_TIME_MIN, MOTION_TIME_MAX);
    reverseMs2 = constrain(pRevMs2, MOTION_TIME_MIN, MOTION_TIME_MAX);
    pauseMs2 = constrain(pPauseMs2, 0UL, MOTION_TIME_MAX);
    cycles2 = constrain(pCyc2, CYCLES_MIN, CYCLES_MAX);

    saveConfig();
    servo1StopNow();
    servo2StopNow();
    server.send(200, "application/json; charset=utf-8", makeConfigJson());
}

// ============================================================
// 16. WEB SERVER SETUP
// ============================================================

void setupWebServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/config", HTTP_GET, handleGetConfig);
    server.on("/api/config/reset", HTTP_POST, handleFactoryReset);
    server.on("/api/config", HTTP_POST, handleConfig);

    // Servo 1 endpoints
    server.on("/api/s1/start", HTTP_POST, []() { startMotion1(); sendJsonOk("S1 đã bắt đầu"); });
    server.on("/api/s1/stop", HTTP_POST, []() { stopMotion1(); sendJsonOk("S1 đã dừng"); });
    server.on("/api/s1/open", HTTP_POST, []() { startOpen1(); sendJsonOk("S1 đang mở"); });
    server.on("/api/s1/close", HTTP_POST, []() { startClose1(); sendJsonOk("S1 đang đóng"); });
    server.on("/api/s1/test/forward", HTTP_POST, []() { testForward1(); sendJsonOk("S1 test thuận"); });
    server.on("/api/s1/test/reverse", HTTP_POST, []() { testReverse1(); sendJsonOk("S1 test nghịch"); });

    // Servo 2 endpoints
    server.on("/api/s2/start", HTTP_POST, []() { startMotion2(); sendJsonOk("S2 đã bắt đầu"); });
    server.on("/api/s2/stop", HTTP_POST, []() { stopMotion2(); sendJsonOk("S2 đã dừng"); });
    server.on("/api/s2/open", HTTP_POST, []() { startOpen2(); sendJsonOk("S2 đang mở"); });
    server.on("/api/s2/close", HTTP_POST, []() { startClose2(); sendJsonOk("S2 đang đóng"); });
    server.on("/api/s2/test/forward", HTTP_POST, []() { testForward2(); sendJsonOk("S2 test thuận"); });
    server.on("/api/s2/test/reverse", HTTP_POST, []() { testReverse2(); sendJsonOk("S2 test nghịch"); });

    // Master / All endpoints
    server.on("/api/all/start", HTTP_POST, []() { startAll(); sendJsonOk("Tất cả đã bắt đầu"); });
    server.on("/api/all/stop", HTTP_POST, []() { stopAll(); sendJsonOk("Tất cả đã dừng"); });
    server.on("/api/all/open", HTTP_POST, []() { openAll(); sendJsonOk("Tất cả đang mở"); });
    server.on("/api/all/close", HTTP_POST, []() { closeAll(); sendJsonOk("Tất cả đang đóng"); });

    // IR Sensor endpoints
    server.on("/api/ir/reset", HTTP_POST, []() {
        irObjectCount = 0;
        updateLcdDual();
        sendJsonOk("Đã reset bộ đếm tô về 0");
    });
    server.on("/api/ir/auto", HTTP_POST, []() {
        if (server.hasArg("mode")) {
            irAutoTriggerServo = server.arg("mode").toInt();
        } else {
            irAutoTriggerServo = (irAutoTriggerServo == 0) ? 1 : 0;
        }
        saveConfig();
        updateLcdDual();
        sendJsonOk("Đã cập nhật chế độ tự động IR");
    });
    server.on("/api/ir/tx", HTTP_POST, []() {
        if (server.hasArg("enable")) {
            irTxEnabled = (server.arg("enable") == "1" || server.arg("enable") == "true");
        } else {
            irTxEnabled = !irTxEnabled;
        }
        digitalWrite(IR_TRANSMITTER_PIN, irTxEnabled ? HIGH : LOW);
        sendJsonOk("Đã đổi trạng thái bộ phát IR");
    });

    // Backwards compatibility endpoints (map to S1)
    server.on("/api/start", HTTP_POST, []() { startMotion1(); sendJsonOk("Đã bắt đầu"); });
    server.on("/api/stop", HTTP_POST, []() { stopAll(); sendJsonOk("Đã dừng"); });
    server.on("/api/open", HTTP_POST, []() { startOpen1(); sendJsonOk("Đang mở"); });
    server.on("/api/close", HTTP_POST, []() { startClose1(); sendJsonOk("Đang đóng"); });
    server.on("/api/test/forward", HTTP_POST, []() { testForward1(); sendJsonOk("Test thuận"); });
    server.on("/api/test/reverse", HTTP_POST, []() { testReverse1(); sendJsonOk("Test nghịch"); });

    server.onNotFound([]() {
        sendJsonError(404, "Không tìm thấy API hoặc trang");
    });

    server.begin();
    Serial.println("[WEB] Server started with Dual-Servo endpoints");
}

// ============================================================
// 17. WIFI SETUP
// ============================================================

void setupWiFi() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.print("[WIFI] SoftAP started! SSID: ");
    Serial.print(AP_SSID);
    Serial.print(" | IP: ");
    Serial.println(WiFi.softAPIP());
}

// ============================================================
// 18. SERIAL CLI COMMAND INTERPRETER
// ============================================================

void handleSerial() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd == "PING") {
        Serial.println("[RES] PONG");
    } else if (cmd == "STATUS" || cmd == "GET /api/status") {
        Serial.println("[RES] " + makeStatusJson());
    } else if (cmd == "CONFIG" || cmd == "GET /api/config") {
        Serial.println("[RES] " + makeConfigJson());

    // --- SERVO 1 COMMANDS ---
    } else if (cmd == "S1_START" || cmd == "START" || cmd == "POST /api/s1/start") {
        if (motionIsRunning(motionState1)) {
            Serial.println("[RES] {\"ok\":false,\"error\":\"Servo 1 đang chạy\"}");
        } else {
            startMotion1();
            Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 1 đã bắt đầu\"}");
        }
    } else if (cmd == "S1_STOP" || cmd == "POST /api/s1/stop") {
        stopMotion1();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 1 đã dừng\"}");
    } else if (cmd == "S1_OPEN" || cmd == "OPEN" || cmd == "POST /api/s1/open") {
        if (motionIsRunning(motionState1)) {
            Serial.println("[RES] {\"ok\":false,\"error\":\"Servo 1 đang chạy\"}");
        } else {
            startOpen1();
            Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 1 đang mở\"}");
        }
    } else if (cmd == "S1_CLOSE" || cmd == "CLOSE" || cmd == "POST /api/s1/close") {
        if (motionIsRunning(motionState1)) {
            Serial.println("[RES] {\"ok\":false,\"error\":\"Servo 1 đang chạy\"}");
        } else {
            startClose1();
            Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 1 đang đóng\"}");
        }
    } else if (cmd == "S1_TEST_FWD" || cmd == "TEST_FWD") {
        testForward1();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 1 quay thuận\"}");
    } else if (cmd == "S1_TEST_REV" || cmd == "TEST_REV") {
        testReverse1();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 1 quay nghịch\"}");
    } else if (cmd.startsWith("S1_CALIB ") || cmd.startsWith("CALIB ")) {
        int val = (cmd.startsWith("S1_CALIB ")) ? cmd.substring(9).toInt() : cmd.substring(6).toInt();
        servo1Stop = val;
        servo1TestStopValue(val);
        Serial.printf("[RES] {\"ok\":true,\"channel\":1,\"value\":%d}\n", val);

    // --- SERVO 2 COMMANDS ---
    } else if (cmd == "S2_START" || cmd == "POST /api/s2/start") {
        if (motionIsRunning(motionState2)) {
            Serial.println("[RES] {\"ok\":false,\"error\":\"Servo 2 đang chạy\"}");
        } else {
            startMotion2();
            Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 2 đã bắt đầu\"}");
        }
    } else if (cmd == "S2_STOP" || cmd == "POST /api/s2/stop") {
        stopMotion2();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 2 đã dừng\"}");
    } else if (cmd == "S2_OPEN" || cmd == "POST /api/s2/open") {
        if (motionIsRunning(motionState2)) {
            Serial.println("[RES] {\"ok\":false,\"error\":\"Servo 2 đang chạy\"}");
        } else {
            startOpen2();
            Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 2 đang mở\"}");
        }
    } else if (cmd == "S2_CLOSE" || cmd == "POST /api/s2/close") {
        if (motionIsRunning(motionState2)) {
            Serial.println("[RES] {\"ok\":false,\"error\":\"Servo 2 đang chạy\"}");
        } else {
            startClose2();
            Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 2 đang đóng\"}");
        }
    } else if (cmd == "S2_TEST_FWD") {
        testForward2();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 2 quay thuận\"}");
    } else if (cmd == "S2_TEST_REV") {
        testReverse2();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Servo 2 quay nghịch\"}");
    } else if (cmd.startsWith("S2_CALIB ")) {
        int val = cmd.substring(9).toInt();
        servo2Stop = val;
        servo2TestStopValue(val);
        Serial.printf("[RES] {\"ok\":true,\"channel\":2,\"value\":%d}\n", val);

    // --- MASTER COMMANDS ---
    } else if (cmd == "ALL_START" || cmd == "POST /api/all/start") {
        startAll();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Tất cả Servo đã bắt đầu\"}");
    } else if (cmd == "ALL_STOP" || cmd == "STOP" || cmd == "POST /api/stop" || cmd == "POST /api/all/stop") {
        stopAll();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Tất cả Servo đã dừng\"}");
    } else if (cmd == "ALL_OPEN" || cmd == "POST /api/all/open") {
        openAll();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Tất cả Servo đang mở\"}");
    } else if (cmd == "ALL_CLOSE" || cmd == "POST /api/all/close") {
        closeAll();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Tất cả Servo đang đóng\"}");

    // --- CONFIG COMMANDS ---
    } else if (cmd.startsWith("CONFIG_SET ") || cmd.startsWith("POST /api/config ")) {
        String params = cmd.substring(cmd.indexOf(' ') + 1);
        int pStop1 = servo1Stop, pFwd1 = servo1Forward, pRev1 = servo1Reverse, pCyc1 = cycles1;
        unsigned long pFwdMs1 = forwardMs1, pRevMs1 = reverseMs1, pPauseMs1 = pauseMs1;
        int pStop2 = servo2Stop, pFwd2 = servo2Forward, pRev2 = servo2Reverse, pCyc2 = cycles2;
        unsigned long pFwdMs2 = forwardMs2, pRevMs2 = reverseMs2, pPauseMs2 = pauseMs2;

        int idx = 0;
        while (idx < params.length()) {
            int eq = params.indexOf('=', idx);
            if (eq == -1) break;
            String key = params.substring(idx, eq);
            key.trim();
            int sp = params.indexOf(' ', eq + 1);
            String val = (sp == -1) ? params.substring(eq + 1) : params.substring(eq + 1, sp);
            val.trim();

            if (key == "stop" || key == "stop1") pStop1 = val.toInt();
            else if (key == "forward" || key == "forward1") pFwd1 = val.toInt();
            else if (key == "reverse" || key == "reverse1") pRev1 = val.toInt();
            else if (key == "forwardMs" || key == "fwdMs" || key == "fwdMs1") pFwdMs1 = strtoul(val.c_str(), NULL, 10);
            else if (key == "reverseMs" || key == "revMs" || key == "revMs1") pRevMs1 = strtoul(val.c_str(), NULL, 10);
            else if (key == "pauseMs" || key == "pauseMs1") pPauseMs1 = strtoul(val.c_str(), NULL, 10);
            else if (key == "cycles" || key == "cycles1") pCyc1 = val.toInt();
            // S2
            else if (key == "stop2") pStop2 = val.toInt();
            else if (key == "forward2") pFwd2 = val.toInt();
            else if (key == "reverse2") pRev2 = val.toInt();
            else if (key == "forwardMs2" || key == "fwdMs2") pFwdMs2 = strtoul(val.c_str(), NULL, 10);
            else if (key == "reverseMs2" || key == "revMs2") pRevMs2 = strtoul(val.c_str(), NULL, 10);
            else if (key == "pauseMs2") pPauseMs2 = strtoul(val.c_str(), NULL, 10);
            else if (key == "cycles2") pCyc2 = val.toInt();

            if (sp == -1) break;
            idx = sp + 1;
        }

        if (motionIsRunning()) {
            Serial.println("[RES] {\"ok\":false,\"error\":\"Không thể đổi cấu hình khi Servo đang chạy\"}");
        } else {
            servo1Stop = constrain(pStop1, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
            servo1Forward = constrain(pFwd1, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
            servo1Reverse = constrain(pRev1, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
            forwardMs1 = constrain(pFwdMs1, MOTION_TIME_MIN, MOTION_TIME_MAX);
            reverseMs1 = constrain(pRevMs1, MOTION_TIME_MIN, MOTION_TIME_MAX);
            pauseMs1 = constrain(pPauseMs1, 0UL, MOTION_TIME_MAX);
            cycles1 = constrain(pCyc1, CYCLES_MIN, CYCLES_MAX);

            servo2Stop = constrain(pStop2, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
            servo2Forward = constrain(pFwd2, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
            servo2Reverse = constrain(pRev2, SERVO_VALUE_MIN, SERVO_VALUE_MAX);
            forwardMs2 = constrain(pFwdMs2, MOTION_TIME_MIN, MOTION_TIME_MAX);
            reverseMs2 = constrain(pRevMs2, MOTION_TIME_MIN, MOTION_TIME_MAX);
            pauseMs2 = constrain(pPauseMs2, 0UL, MOTION_TIME_MAX);
            cycles2 = constrain(pCyc2, CYCLES_MIN, CYCLES_MAX);

            saveConfig();
            servo1StopNow();
            servo2StopNow();
            Serial.println("[RES] " + makeConfigJson());
        }
    } else if (cmd == "RESET" || cmd == "POST /api/config/reset") {
        if (motionIsRunning()) {
            Serial.println("[RES] {\"ok\":false,\"error\":\"Không thể khôi phục khi Servo đang chạy\"}");
        } else {
            stopAll();
            setDefaultConfig();
            saveConfig();
            servo1StopNow();
            servo2StopNow();
            Serial.println("[RES] " + makeConfigJson());
        }
    // --- IR SENSOR COMMANDS ---
    } else if (cmd == "IR_RESET" || cmd == "POST /api/ir/reset") {
        irObjectCount = 0;
        updateLcdDual();
        Serial.println("[RES] {\"ok\":true,\"message\":\"Đã reset bộ đếm tô về 0\"}");
    } else if (cmd.startsWith("IR_AUTO ") || cmd.startsWith("IR_AUTO_SET ")) {
        int m = cmd.substring(cmd.indexOf(' ') + 1).toInt();
        irAutoTriggerServo = m;
        saveConfig();
        updateLcdDual();
        Serial.printf("[RES] {\"ok\":true,\"autoTrigger\":%d}\n", m);
    } else if (cmd == "IR_TX_ON") {
        irTxEnabled = true;
        digitalWrite(IR_TRANSMITTER_PIN, HIGH);
        Serial.println("[RES] {\"ok\":true,\"tx\":true}");
    } else if (cmd == "IR_TX_OFF") {
        irTxEnabled = false;
        digitalWrite(IR_TRANSMITTER_PIN, LOW);
        Serial.println("[RES] {\"ok\":true,\"tx\":false}");
    } else if (cmd == "IR_TRIGGER" || cmd == "POST /api/ir/simulate") {
        irObjectCount++;
        irDetected = true;
        irBlockDurationMs = 350;
        irLastDetectionMs = millis();
        irBlockStartMs = millis();
        Serial.printf("[IR] BOWL DETECTED! Count = %u (SIMULATED)\n", irObjectCount);
        if (irAutoTriggerServo == 1) {
            if (!motionIsRunning(motionState1)) startOpen1();
        } else if (irAutoTriggerServo == 2) {
            if (!motionIsRunning(motionState2)) startOpen2();
        } else if (irAutoTriggerServo == 3) {
            openAll();
        }
        updateLcdDual();
        Serial.printf("[RES] {\"ok\":true,\"count\":%u,\"detected\":true}\n", irObjectCount);
    } else {
        Serial.println("[RES] {\"ok\":false,\"error\":\"Unknown command\"}");
    }
}

// ============================================================
// 19. MAIN SETUP & LOOP
// ============================================================

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(50);
    delay(500);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 DUAL SERVO & IR BOWL DETECTOR");
    Serial.println("LILYGO T-Display-S3");
    Serial.println("SERVO 1: GPIO 1 | SERVO 2: GPIO 2");
    Serial.println("IR RX: GPIO 10  | IR TX: GPIO 11");
    Serial.println("================================");

    // 1. Energize display power and backlight
    pinMode(LCD_POWER_PIN, OUTPUT);
    digitalWrite(LCD_POWER_PIN, HIGH);
    delay(10);

    pinMode(LCD_BL_PIN, OUTPUT);
    digitalWrite(LCD_BL_PIN, HIGH);

    gfx->begin();
    gfx->fillScreen(COLOR_BLACK);
    lcdShow("DUAL SERVO S3", "Dang khoi dong...");

    // 2. Allocate LEDC timers
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    // 3. Load config and clamp both pins firmly to LOW (zero drift)
    loadConfig();
    servo1StopNow();
    servo2StopNow();

    // 3B. Initialize IR Sensor (Receiver & Transmitter)
    setupIrSensor();

    // 4. Start network & server
    setupWiFi();
    setupWebServer();

    updateLcdDual();
    Serial.println("[SYSTEM] READY - Cho lenh tu Web hoac Serial...");
}

void loop() {
    server.handleClient();
    updateMotion();
    updateIrSensor();
    handleSerial();
    delay(2);
}
