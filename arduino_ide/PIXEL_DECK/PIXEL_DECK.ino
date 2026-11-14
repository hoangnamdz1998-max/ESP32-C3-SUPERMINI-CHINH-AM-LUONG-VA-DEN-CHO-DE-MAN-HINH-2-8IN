#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include "driver/rmt.h"
#include "soc/gpio_struct.h"
#include <nvs_flash.h>
#include <BleKeyboard.h>
#include "webpage.h"

// ================= Pin Definitions =================
// 1. KY-040 Rotary Encoder (Chỉnh âm lượng)
#define PIN_ENCODER_CLK   3
#define PIN_ENCODER_DT    4
#define PIN_ENCODER_SW    5

// 2. 3 External Push Buttons (1 chân vào GND, 1 chân vào ESP32-C3)
#define PIN_BTN_NEXT      6   // Chuyển bài tiếp theo (Next Track)
#define PIN_BTN_PREV      7   // Lùi bài trước (Previous Track)
#define PIN_BTN_PLAY      10  // Dừng / Phát nhạc (Play / Pause)

// 3. Built-in LED on ESP32-C3 Super Mini (GPIO 8, active LOW)
#define PIN_LED           8

// 4. Chân điều khiển LED dây RGB WS2812B (Data Pin)
#define PIN_LED_STRIP     1

// ================= BLE Keyboard Instance =================
BleKeyboard bleKeyboard("PIXEL-DECK-AUDIO", "NAM", 100);

// ================= Debounced Button Class =================
class Button {
  private:
    uint8_t pin;
    uint32_t lastDebounceTime;
    uint32_t debounceDelay;
    bool lastState;
    bool buttonState;

  public:
    Button(uint8_t p, uint32_t delayMs = 40)
      : pin(p), debounceDelay(delayMs), lastDebounceTime(0), lastState(HIGH), buttonState(HIGH) {}

    void begin() {
      pinMode(pin, INPUT_PULLUP);
    }

    bool wasPressed() {
      bool reading = digitalRead(pin);
      bool pressed = false;

      if (reading != lastState) {
        lastDebounceTime = millis();
      }

      if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
          buttonState = reading;
          if (buttonState == LOW) {
            pressed = true;
          }
        }
      }

      lastState = reading;
      return pressed;
    }
};

Button btnEncoderSW(PIN_ENCODER_SW);
Button btnNext(PIN_BTN_NEXT);
Button btnPrev(PIN_BTN_PREV);
Button btnPlay(PIN_BTN_PLAY);

// ================= Encoder State Machine =================
static const int8_t rot_enc_table[] = {
  0,  1, -1,  0,
 -1,  0,  0,  1,
  1,  0,  0, -1,
  0, -1,  1,  0
};
static uint8_t enc_last_state = 0;
static int8_t enc_counter = 0;

// Bảng màu cài sẵn (Preset Colors) để đổi nhanh bằng nút bấm
struct ColorPreset {
  uint8_t r, g, b;
  const char* name;
};

const ColorPreset PRESET_COLORS[] = {
  {255, 107,   0, "Cam WLED"},
  {255,   0,   0, "Đỏ"},
  {255,  40,   0, "Đỏ cam"},
  {255, 140,   0, "Cam tươi"},
  {255, 215,   0, "Vàng nắng"},
  {  0, 255,   0, "Xanh lục"},
  {  0, 255, 128, "Xanh Mint"},
  {  0, 220, 255, "Xanh Cyan"},
  {  0, 100, 255, "Xanh dương"},
  {128,   0, 255, "Tím"},
  {220,   0, 255, "Tím hồng"},
  {255,   0, 100, "Hồng"},
  {255, 200, 150, "Trắng ấm"},
  {220, 240, 255, "Trắng lạnh"}
};
const int TOTAL_PRESET_COLORS = sizeof(PRESET_COLORS) / sizeof(PRESET_COLORS[0]);
static int currentPresetColorIdx = 0;

// ================= LED Strip & WebServer Configuration =================
#define DNS_PORT 53
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

Adafruit_NeoPixel* ledStrip = nullptr;

// Trạng thái LED
bool ledPower = true;
uint8_t ledBrightness = 150;
uint8_t ledSpeed = 50;
int ledCount = 19;
uint8_t ledFx = 1;            // Mặc định: Hiệu ứng 1 (Center Out - Chạy từ giữa ra 2 đầu)
uint8_t ledR = 255;
uint8_t ledG = 107;
uint8_t ledB = 0;

uint16_t effectStep = 0;
bool rainbowReverse = false;
uint16_t rainbowHue = 0;
unsigned long lastEffectUpdate = 0;

bool needSaveState = false;
unsigned long saveTriggerTime = 0;

const char* apSSID = "PIXEL-DECK-LED";

// ================= RISC-V 160MHz Cycle-Accurate Bitbang Engine =================
// TRIỆT TIÊU 100% HIỆN TƯỢNG CHỚP NHÁY TRÊN ESP32-C3:
// 1. Dùng CSR 0x7e2 (PCCR) đếm chu kỳ phần cứng chính xác từng 6.25ns (160MHz).
// 2. Khóa ngắt toàn cục bằng portENTER_CRITICAL trong đúng 0.57ms truyền 19 bóng LED:
//    Không một tiến trình mạng nào (Wi-Fi beacon, Bluetooth event) có thể chen ngang.
// 3. Tăng dòng phát GPIO_DRIVE_CAP_3 (40mA) trên GPIO 1 cho xung vuông cực chuẩn.

static inline uint32_t IRAM_ATTR getCycleCount(void) {
  uint32_t ccount;
  __asm__ __volatile__("csrr %0, 0x7e2" : "=r"(ccount));
  return ccount;
}

void IRAM_ATTR showLedStrip() {
  if (!ledStrip) return;
  uint8_t* pixels = ledStrip->getPixels();
  uint16_t numBytes = ledStrip->numPixels() * 3;
  if (!pixels || numBytes == 0) return;

  const uint32_t pinMask = (1UL << PIN_LED_STRIP);
  volatile uint32_t* setReg = &GPIO.out_w1ts.val;
  volatile uint32_t* clrReg = &GPIO.out_w1tc.val;

  // Timing WS2812B tại 160 MHz (1 tick = 6.25 ns):
  // T0H: 350 ns -> 56 cycles
  // T1H: 800 ns -> 128 cycles
  // Period: 1250 ns -> 200 cycles
  const uint32_t t0h = 56;
  const uint32_t t1h = 128;
  const uint32_t period = 200;

  const uint8_t* p = pixels;
  const uint8_t* end = pixels + numBytes;

  uint8_t subpix = *p++;
  uint8_t mask = 0x80;
  uint32_t cyclesStart = 0;
  uint32_t cyclesNext = getCycleCount();

  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&mux);

  for (;;) {
    uint32_t cyclesBit = (subpix & mask) ? t1h : t0h;

    while (((cyclesStart = getCycleCount()) - cyclesNext) < period);

    *setReg = pinMask;

    while ((getCycleCount() - cyclesStart) < cyclesBit);

    *clrReg = pinMask;

    cyclesNext = cyclesStart;

    mask >>= 1;
    if (mask == 0) {
      if (p >= end) {
        break;
      }
      mask = 0x80;
      subpix = *p++;
    }
  }

  portEXIT_CRITICAL(&mux);
}

// ---------------- Khởi tạo / Cập nhật dải LED ----------------
void initLedStrip() {
  if (ledStrip != nullptr) {
    delete ledStrip;
    ledStrip = nullptr;
  }
  ledStrip = new Adafruit_NeoPixel(ledCount, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);
  ledStrip->begin();
  pinMode(PIN_LED_STRIP, OUTPUT);
  digitalWrite(PIN_LED_STRIP, LOW);
  gpio_set_drive_capability((gpio_num_t)PIN_LED_STRIP, GPIO_DRIVE_CAP_3);
  ledStrip->setBrightness(ledPower ? ledBrightness : 0);
  showLedStrip();
}

void updateLedBrightness() {
  if (ledStrip != nullptr) {
    ledStrip->setBrightness(ledPower ? ledBrightness : 0);
    showLedStrip();
  }
}

// ---------------- Đọc & Lưu cấu hình vào NVS (Flash) ----------------
void loadSettings() {
  prefs.begin("deskled", true);
  ledPower = prefs.getBool("power", true);
  ledBrightness = prefs.getUChar("bri", 150);
  ledSpeed = prefs.getUChar("spd", 50);
  ledCount = prefs.getInt("count", 19);
  if (ledCount < 1 || ledCount > 300) ledCount = 19;
  ledFx = prefs.getUChar("fx", 1);
  ledR = prefs.getUChar("r", 255);
  ledG = prefs.getUChar("g", 107);
  ledB = prefs.getUChar("b", 0);
  rainbowReverse = prefs.getBool("rb_rev", false);
  prefs.end();
}

void triggerSave() {
  needSaveState = true;
  saveTriggerTime = millis();
}

void savePendingState() {
  if (needSaveState && (millis() - saveTriggerTime > 2000)) {
    needSaveState = false;
    prefs.begin("deskled", false);
    prefs.putBool("power", ledPower);
    prefs.putUChar("bri", ledBrightness);
    prefs.putUChar("spd", ledSpeed);
    prefs.putInt("count", ledCount);
    prefs.putUChar("fx", ledFx);
    prefs.putUChar("r", ledR);
    prefs.putUChar("g", ledG);
    prefs.putUChar("b", ledB);
    prefs.putBool("rb_rev", rainbowReverse);
    prefs.end();
    Serial.println("[NVS] Da luu cau hinh LED vao Flash.");
  }
}

// ---------------- Bánh xe màu Cân Bằng Năng Lượng (Constant Power Rainbow) ----------------
// Đảm bảo tổng năng lượng R + G + B luôn ổn định = 255 tại mọi góc pha (768 bước phân giải).
// Triệt tiêu 100% hiện tượng sụt áp cổng USB và loại bỏ hoàn toàn vết gãy chu kỳ màu (không chớp).
static uint32_t getBalancedRainbowColor(uint16_t hue16) {
  uint32_t pos = ((uint32_t)hue16 * 768UL) >> 16; // 0 đến 767
  uint8_t r = 0, g = 0, b = 0;
  if (pos < 256) {
    r = 255 - (uint8_t)pos;
    g = (uint8_t)pos;
    b = 0;
  } else if (pos < 512) {
    uint8_t p = (uint8_t)(pos - 256);
    r = 0;
    g = 255 - p;
    b = p;
  } else {
    uint8_t p = (uint8_t)(pos - 512);
    r = p;
    g = 0;
    b = 255 - p;
  }
  return Adafruit_NeoPixel::Color(r, g, b);
}

// ---------------- Các Hiệu Ứng LED RGB ----------------
void runLedEffects() {
  if (!ledStrip) return;

  if (!ledPower) {
    ledStrip->clear();
    showLedStrip();
    return;
  }

  // Tốc độ khung hình (Speed: 5 - 100)
  unsigned long interval;
  if (ledFx == 3) {
    interval = map(105 - ledSpeed, 5, 100, 30, 75); // 30ms - 75ms: nhịp quét ổn định mượt mà, triệt tiêu xung đột ngắt
  } else {
    interval = map(105 - ledSpeed, 5, 100, 25, 160);
  }
  if (millis() - lastEffectUpdate < interval) return;
  lastEffectUpdate = millis();

  switch (ledFx) {
    case 0: // Sáng tĩnh 1 màu (Solid)
      for (int i = 0; i < ledCount; i++) {
        ledStrip->setPixelColor(i, ledR, ledG, ledB);
      }
      showLedStrip();
      break;

    case 1: { // 🌟 TỪ GIỮA RA 2 ĐẦU: Toàn bộ sáng, chỉ 2 bóng tắt chạy từ giữa ra 2 mép
      for (int i = 0; i < ledCount; i++) {
        ledStrip->setPixelColor(i, ledR, ledG, ledB);
      }
      float mid = (ledCount - 1) / 2.0;
      int halfLen = (ledCount + 1) / 2;
      int step = effectStep % halfLen;

      int leftIdx = (int)floor(mid - step);
      int rightIdx = (int)ceil(mid + step);
      if (leftIdx >= 0 && leftIdx < ledCount) {
        ledStrip->setPixelColor(leftIdx, 0, 0, 0);
      }
      if (rightIdx >= 0 && rightIdx < ledCount) {
        ledStrip->setPixelColor(rightIdx, 0, 0, 0);
      }
      showLedStrip();
      effectStep++;
      break;
    }

    case 2: { // 🔄 TỪ 2 ĐẦU VÀO GIỮA: Toàn bộ sáng, chỉ 2 bóng tắt chạy từ 2 mép vào giữa
      for (int i = 0; i < ledCount; i++) {
        ledStrip->setPixelColor(i, ledR, ledG, ledB);
      }
      int halfLen = (ledCount + 1) / 2;
      int step = effectStep % halfLen;

      int leftIdx = step;
      int rightIdx = ledCount - 1 - step;
      if (leftIdx >= 0 && leftIdx < ledCount) {
        ledStrip->setPixelColor(leftIdx, 0, 0, 0);
      }
      if (rightIdx >= 0 && rightIdx < ledCount) {
        ledStrip->setPixelColor(rightIdx, 0, 0, 0);
      }
      showLedStrip();
      effectStep++;
      break;
    }

    case 3: { // 🌈 Cầu vồng cân bằng công suất (Chống sụt áp & triệt tiêu nháy 100%)
      uint16_t hueStep = map(ledSpeed, 5, 100, 150, 950);
      if (rainbowReverse) {
        rainbowHue -= hueStep;
      } else {
        rainbowHue += hueStep;
      }
      for (int i = 0; i < ledCount; i++) {
        uint16_t pixelHue = rainbowHue + (i * 65536L / ledCount);
        ledStrip->setPixelColor(i, getBalancedRainbowColor(pixelHue));
      }
      showLedStrip();
      break;
    }

    case 4: { // Thở nhịp (Breathe)
      float val = (exp(sin(effectStep * 0.05)) - 0.36787944) * 108.0;
      uint8_t factor = constrain((int)val, 5, 255);
      uint8_t brR = (ledR * factor) / 255;
      uint8_t brG = (ledG * factor) / 255;
      uint8_t brB = (ledB * factor) / 255;
      for (int i = 0; i < ledCount; i++) {
        ledStrip->setPixelColor(i, brR, brG, brB);
      }
      showLedStrip();
      effectStep++;
      break;
    }

    case 5: { // Đuổi màu (Chase)
      int head = effectStep % ledCount;
      ledStrip->clear();
      for (int t = 0; t < 6; t++) {
        int pos = (head - t + ledCount) % ledCount;
        uint8_t fade = 255 / (t + 1);
        ledStrip->setPixelColor(pos, (ledR * fade) / 255, (ledG * fade) / 255, (ledB * fade) / 255);
      }
      showLedStrip();
      effectStep++;
      break;
    }

    case 6: { // Quét màu (Wipe)
      int pos = effectStep % (ledCount * 2);
      if (pos < ledCount) {
        ledStrip->setPixelColor(pos, ledR, ledG, ledB);
      } else {
        ledStrip->setPixelColor(pos - ledCount, 0, 0, 0);
      }
      showLedStrip();
      effectStep++;
      break;
    }

    case 7: { // Lấp lánh (Twinkle Stars)
      for (int i = 0; i < ledCount; i++) {
        uint32_t c = ledStrip->getPixelColor(i);
        uint8_t r = ((c >> 16) & 0xFF) * 0.85;
        uint8_t g = ((c >> 8) & 0xFF) * 0.85;
        uint8_t b = (c & 0xFF) * 0.85;
        ledStrip->setPixelColor(i, r, g, b);
      }
      if (random(10) < 4) {
        int p = random(ledCount);
        ledStrip->setPixelColor(p, ledR, ledG, ledB);
      }
      showLedStrip();
      break;
    }

    case 8: { // Sao băng (Meteor Rain)
      for (int j = 0; j < ledCount; j++) {
        if (random(10) > 4) {
          uint32_t c = ledStrip->getPixelColor(j);
          uint8_t r = ((c >> 16) & 0xFF) * 0.7;
          uint8_t g = ((c >> 8) & 0xFF) * 0.7;
          uint8_t b = (c & 0xFF) * 0.7;
          ledStrip->setPixelColor(j, r, g, b);
        }
      }
      int head = effectStep % (ledCount + 10);
      if (head < ledCount) {
        ledStrip->setPixelColor(head, ledR, ledG, ledB);
      }
      showLedStrip();
      effectStep++;
      break;
    }

    case 9: { // Ngọn lửa ấm (Fire)
      for (int i = 0; i < ledCount; i++) {
        int flicker = random(0, 55);
        int r1 = constrain(ledR - flicker, 0, 255);
        int g1 = constrain(ledG - flicker * 2, 0, 255);
        int b1 = constrain(ledB - flicker * 3, 0, 255);
        ledStrip->setPixelColor(i, r1, g1, b1);
      }
      showLedStrip();
      break;
    }

    case 10: { // Chớp nháy (Strobe)
      bool on = (effectStep % 2 == 0);
      for (int i = 0; i < ledCount; i++) {
        ledStrip->setPixelColor(i, on ? ledR : 0, on ? ledG : 0, on ? ledB : 0);
      }
      showLedStrip();
      effectStep++;
      break;
    }

    default:
      ledFx = 1;
      break;
  }
}

// ---------------- Cấu hình WebServer & Captive Portal ----------------
void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/state", HTTP_GET, []() {
    String json = "{";
    json += "\"power\":" + String(ledPower ? 1 : 0) + ",";
    json += "\"bri\":" + String(ledBrightness) + ",";
    json += "\"spd\":" + String(ledSpeed) + ",";
    json += "\"count\":" + String(ledCount) + ",";
    json += "\"fx\":" + String(ledFx) + ",";
    json += "\"r\":" + String(ledR) + ",";
    json += "\"g\":" + String(ledG) + ",";
    json += "\"b\":" + String(ledB);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/api/power", HTTP_GET, []() {
    if (server.hasArg("val")) {
      ledPower = (server.arg("val").toInt() == 1);
      updateLedBrightness();
      triggerSave();
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/bri", HTTP_GET, []() {
    if (server.hasArg("val")) {
      ledBrightness = constrain(server.arg("val").toInt(), 0, 255);
      updateLedBrightness();
      triggerSave();
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/spd", HTTP_GET, []() {
    if (server.hasArg("val")) {
      ledSpeed = constrain(server.arg("val").toInt(), 5, 100);
      triggerSave();
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/rainbow_dir", HTTP_GET, []() {
    if (server.hasArg("val")) {
      rainbowReverse = (server.arg("val").toInt() == 1);
    } else {
      rainbowReverse = !rainbowReverse;
    }
    triggerSave();
    server.send(200, "text/plain", rainbowReverse ? "1" : "0");
  });

  server.on("/api/fx", HTTP_GET, []() {
    if (server.hasArg("val")) {
      ledFx = server.arg("val").toInt();
      effectStep = 0;
      triggerSave();
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/color", HTTP_GET, []() {
    if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
      ledR = server.arg("r").toInt();
      ledG = server.arg("g").toInt();
      ledB = server.arg("b").toInt();
      triggerSave();
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/api/ledcount", HTTP_GET, []() {
    if (server.hasArg("val")) {
      int newCount = constrain(server.arg("val").toInt(), 1, 300);
      if (newCount != ledCount) {
        ledCount = newCount;
        initLedStrip();
        triggerSave();
      }
    }
    server.send(200, "text/plain", "OK");
  });

  // Tự động nhận diện Captive Portal cho iOS / Android / Windows
  server.on("/generate_204", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/gen_204", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/canonical.html", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/connecttest.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft Connect Test"); });
  server.on("/ncsi.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft NCSI"); });

  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(500);

  // 1. Khởi tạo Flash NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  Serial.println("\n=================================================");
  Serial.println(" ESP32-C3 SUPER MINI MEDIA & DESK LED CONTROLLER ");
  Serial.println(" BLE Device: PIXEL-DECK-AUDIO | LED Pin: GPIO 1  ");
  Serial.println("=================================================");

  // 2. Khởi động Bluetooth BLE Keyboard TRƯỚC TIÊN (đảm bảo bộ nhớ RAM cho radio Bluetooth)
  bleKeyboard.begin();
  Serial.println("BLE Keyboard khoi dong: 'PIXEL-DECK-AUDIO'");
  delay(150);

  // 3. Tải cài đặt LED đã lưu
  loadSettings();

  // 4. Khởi tạo dải LED RGB (WS2812B trên GPIO 2)
  initLedStrip();

  // 5. Khởi tạo chân nút bấm & Encoder
  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODER_DT,  INPUT_PULLUP);
  
  btnEncoderSW.begin();
  btnNext.begin();
  btnPrev.begin();
  btnPlay.begin();

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH); // LED xanh tích hợp tắt (active LOW)

  uint8_t clk = digitalRead(PIN_ENCODER_CLK);
  uint8_t dt  = digitalRead(PIN_ENCODER_DT);
  enc_last_state = (clk << 1) | dt;

  // 6. Khởi động Wi-Fi Access Point + DNS Server (Captive Portal)
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false); // Vô hiệu hóa WiFi modem sleep để giữ xung APB cố định 100%, chống giật RMT cho LED
  WiFi.softAP(apSSID);
  Serial.print("Wi-Fi AP phát: "); Serial.println(apSSID);
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  setupWebServer();
}

// ================= LOOP =================
void loop() {
  // 1. Xử lý mạng Wi-Fi & WebServer & Captive Portal
  dnsServer.processNextRequest();
  server.handleClient();

  // 2. Chạy hiệu ứng LED RGB
  runLedEffects();
  savePendingState();

  // Đọc trạng thái các chân vật lý (Active LOW: LOW = đang nhấn)
  bool nextPinLow = (digitalRead(PIN_BTN_NEXT) == LOW);
  bool prevPinLow = (digitalRead(PIN_BTN_PREV) == LOW);
  bool playPinLow = (digitalRead(PIN_BTN_PLAY) == LOW);
  bool swPinLow   = (digitalRead(PIN_ENCODER_SW) == LOW);
  bool isHoldingPlay = playPinLow || swPinLow;

  static bool swUsedForEncoder = false;
  static bool playUsedForEncoder = false;
  static bool comboActive = false;

  // Khi nhả hết các nút thì giải phóng cờ combo
  if (!nextPinLow && !prevPinLow && !playPinLow && !swPinLow) {
    comboActive = false;
    swUsedForEncoder = false;
    playUsedForEncoder = false;
  }

  // 3. Xử lý núm xoay Encoder (Chỉnh âm lượng hoặc Tăng/Giảm tốc độ hiệu ứng)
  uint8_t clk = digitalRead(PIN_ENCODER_CLK);
  uint8_t dt  = digitalRead(PIN_ENCODER_DT);
  uint8_t current_state = (clk << 1) | dt;

  if (current_state != enc_last_state) {
    uint8_t index = (enc_last_state << 2) | current_state;
    enc_counter += rot_enc_table[index];
    enc_last_state = current_state;

    // ĐÃ ĐỔI CHIỀU XOAY THEO YÊU CẦU:
    // enc_counter <= -2: Chiều tăng (Volume UP / Tăng tốc độ LED)
    // enc_counter >= 2:  Chiều giảm (Volume DOWN / Giảm tốc độ LED)
    if (enc_counter <= -2) {
      if (isHoldingPlay) {
        // Nhấn nút dừng phát và xoay -> TĂNG TỐC ĐỘ HIỆU ỨNG
        ledSpeed = min(100, (int)ledSpeed + 5);
        Serial.printf("[ENCODER + PLAY] Tăng tốc độ hiệu ứng: %d%%%%\n", ledSpeed);
        if (swPinLow) swUsedForEncoder = true;
        if (playPinLow) playUsedForEncoder = true;
        triggerSave();
      } else {
        // Xoay bình thường -> TĂNG ÂM LƯỢNG (Đã đảo chiều chuẩn)
        if (bleKeyboard.isConnected()) {
          bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
        }
        Serial.println("[ENCODER] Volume UP (+)");
      }
      enc_counter = 0;
    } else if (enc_counter >= 2) {
      if (isHoldingPlay) {
        // Nhấn nút dừng phát và xoay -> GIẢM TỐC ĐỘ HIỆU ỨNG
        ledSpeed = max(5, (int)ledSpeed - 5);
        Serial.printf("[ENCODER + PLAY] Giảm tốc độ hiệu ứng: %d%%%%\n", ledSpeed);
        if (swPinLow) swUsedForEncoder = true;
        if (playPinLow) playUsedForEncoder = true;
        triggerSave();
      } else {
        // Xoay bình thường -> GIẢM ÂM LƯỢNG (Đã đảo chiều chuẩn)
        if (bleKeyboard.isConnected()) {
          bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
        }
        Serial.println("[ENCODER] Volume DOWN (-)");
      }
      enc_counter = 0;
    }
  }

  // 4. Xử lý nút nhấn trên trục Encoder (Play / Pause)
  if (btnEncoderSW.wasPressed()) {
    if (!swUsedForEncoder) {
      Serial.println("[BUTTON] Encoder SW: Play / Pause");
      if (bleKeyboard.isConnected()) {
        bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
      }
    }
  }

  // 5, 6 & 7. Xử lý nút bấm Next (GPIO 6), Prev (GPIO 7), Play (GPIO 10) và các COMBO 2 NÚT
  static unsigned long btnNextPressTime = 0;
  static unsigned long btnPrevPressTime = 0;
  static unsigned long btnPlayPressTime = 0;
  static bool nextPending = false;
  static bool prevPending = false;
  static bool playPending = false;
  const unsigned long COMBO_WINDOW_MS = 80;

  // Lambda: Combo Next + Prev -> Đổi hiệu ứng LED (Mượt mà, không chớp nháy trắng)
  auto triggerEffectCombo = [&]() {
    nextPending = false;
    prevPending = false;
    playPending = false;
    comboActive = true;

    ledFx = (ledFx + 1) % 11;
    effectStep = 0;
    triggerSave();

    Serial.printf("[COMBO Next + Prev] Đổi hiệu ứng LED: #%d\n", ledFx);
  };

  // Lambda: Combo Play + Next -> Đổi chiều Cầu vồng (nếu Fx 3) hoặc Đổi màu tiếp theo
  auto triggerColorNextCombo = [&]() {
    nextPending = false;
    prevPending = false;
    playPending = false;
    comboActive = true;

    if (ledFx == 3) {
      rainbowReverse = !rainbowReverse;
      triggerSave();
      Serial.printf("[COMBO Play + Next] Đổi chiều Cầu vồng: %s\n", rainbowReverse ? "Nghịch (⬅️)" : "Thuận (➡️)");
    } else {
      currentPresetColorIdx = (currentPresetColorIdx + 1) % TOTAL_PRESET_COLORS;
      ledR = PRESET_COLORS[currentPresetColorIdx].r;
      ledG = PRESET_COLORS[currentPresetColorIdx].g;
      ledB = PRESET_COLORS[currentPresetColorIdx].b;
      triggerSave();
      Serial.printf("[COMBO Play + Next] Đổi màu tiếp theo: %s\n", PRESET_COLORS[currentPresetColorIdx].name);
    }
  };

  // Lambda: Combo Play + Prev -> Đổi chiều Cầu vồng (nếu Fx 3) hoặc Lùi màu trước đó
  auto triggerColorPrevCombo = [&]() {
    nextPending = false;
    prevPending = false;
    playPending = false;
    comboActive = true;

    if (ledFx == 3) {
      rainbowReverse = !rainbowReverse;
      triggerSave();
      Serial.printf("[COMBO Play + Prev] Đổi chiều Cầu vồng: %s\n", rainbowReverse ? "Nghịch (⬅️)" : "Thuận (➡️)");
    } else {
      currentPresetColorIdx = (currentPresetColorIdx - 1 + TOTAL_PRESET_COLORS) % TOTAL_PRESET_COLORS;
      ledR = PRESET_COLORS[currentPresetColorIdx].r;
      ledG = PRESET_COLORS[currentPresetColorIdx].g;
      ledB = PRESET_COLORS[currentPresetColorIdx].b;
      triggerSave();
      Serial.printf("[COMBO Play + Prev] Đổi màu trước đó: %s\n", PRESET_COLORS[currentPresetColorIdx].name);
    }
  };

  // Kiểm tra sự kiện bấm nút Next
  if (btnNext.wasPressed()) {
    if (!comboActive) {
      if (playPinLow) {
        triggerColorNextCombo();
      } else if (prevPinLow) {
        triggerEffectCombo();
      } else {
        nextPending = true;
        btnNextPressTime = millis();
      }
    }
  }

  // Kiểm tra sự kiện bấm nút Prev
  if (btnPrev.wasPressed()) {
    if (!comboActive) {
      if (playPinLow) {
        triggerColorPrevCombo();
      } else if (nextPinLow) {
        triggerEffectCombo();
      } else {
        prevPending = true;
        btnPrevPressTime = millis();
      }
    }
  }

  // Kiểm tra sự kiện bấm nút Play (GPIO 10)
  if (btnPlay.wasPressed()) {
    if (!comboActive && !playUsedForEncoder) {
      if (nextPinLow) {
        triggerColorNextCombo();
      } else if (prevPinLow) {
        triggerColorPrevCombo();
      } else {
        playPending = true;
        btnPlayPressTime = millis();
      }
    }
  }

  // Xử lý nút Next sau cửa sổ chờ combo
  if (nextPending) {
    if (playPinLow || (digitalRead(PIN_BTN_PLAY) == LOW)) {
      triggerColorNextCombo();
    } else if (prevPinLow || (digitalRead(PIN_BTN_PREV) == LOW)) {
      triggerEffectCombo();
    } else if (millis() - btnNextPressTime > COMBO_WINDOW_MS) {
      nextPending = false;
      if (!comboActive) {
        Serial.println("[BUTTON 6] Next Track");
        if (bleKeyboard.isConnected()) {
          bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
        }
      }
    }
  }

  // Xử lý nút Prev sau cửa sổ chờ combo
  if (prevPending) {
    if (playPinLow || (digitalRead(PIN_BTN_PLAY) == LOW)) {
      triggerColorPrevCombo();
    } else if (nextPinLow || (digitalRead(PIN_BTN_NEXT) == LOW)) {
      triggerEffectCombo();
    } else if (millis() - btnPrevPressTime > COMBO_WINDOW_MS) {
      prevPending = false;
      if (!comboActive) {
        Serial.println("[BUTTON 7] Previous Track");
        if (bleKeyboard.isConnected()) {
          bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
        }
      }
    }
  }

  // Xử lý nút Play sau cửa sổ chờ combo
  if (playPending) {
    if (nextPinLow || (digitalRead(PIN_BTN_NEXT) == LOW)) {
      triggerColorNextCombo();
    } else if (prevPinLow || (digitalRead(PIN_BTN_PREV) == LOW)) {
      triggerColorPrevCombo();
    } else if (millis() - btnPlayPressTime > COMBO_WINDOW_MS) {
      playPending = false;
      if (!comboActive && !playUsedForEncoder) {
        Serial.println("[BUTTON 10] Play / Pause");
        if (bleKeyboard.isConnected()) {
          bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
        }
      }
    }
  }

  // 8. Đèn LED báo trạng thái Bluetooth trên bo mạch (GPIO 8)
  static uint32_t lastLedBlink = 0;
  static bool ledBlinkState = false;
  uint32_t now = millis();

  if (bleKeyboard.isConnected()) {
    digitalWrite(PIN_LED, LOW); // Đã kết nối: LED xanh sáng đứng
  } else {
    if (now - lastLedBlink >= 500) {
      lastLedBlink = now;
      ledBlinkState = !ledBlinkState;
      digitalWrite(PIN_LED, ledBlinkState ? LOW : HIGH);
    }
  }

  delay(1);
}