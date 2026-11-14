#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
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
#define PIN_LED_STRIP     2

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
unsigned long lastEffectUpdate = 0;

bool needSaveState = false;
unsigned long saveTriggerTime = 0;

const char* apSSID = "PIXEL-DECK-LED";

// ---------------- Khởi tạo / Cập nhật dải LED ----------------
void initLedStrip() {
  if (ledStrip != nullptr) {
    delete ledStrip;
    ledStrip = nullptr;
  }
  ledStrip = new Adafruit_NeoPixel(ledCount, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);
  ledStrip->begin();
  ledStrip->setBrightness(ledPower ? ledBrightness : 0);
  ledStrip->show();
}

void updateLedBrightness() {
  if (ledStrip != nullptr) {
    ledStrip->setBrightness(ledPower ? ledBrightness : 0);
    ledStrip->show();
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
    prefs.end();
    Serial.println("[NVS] Da luu cau hinh LED vao Flash.");
  }
}

// ---------------- Bánh xe màu cho hiệu ứng Rainbow ----------------
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return Adafruit_NeoPixel::Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return Adafruit_NeoPixel::Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return Adafruit_NeoPixel::Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// ---------------- Các Hiệu Ứng LED RGB ----------------
void runLedEffects() {
  if (!ledStrip) return;

  if (!ledPower) {
    ledStrip->clear();
    ledStrip->show();
    return;
  }

  // Tốc độ khung hình (Speed: 5 - 100)
  unsigned long interval = map(105 - ledSpeed, 5, 100, 10, 150);
  if (millis() - lastEffectUpdate < interval) return;
  lastEffectUpdate = millis();

  switch (ledFx) {
    case 0: // Sáng tĩnh 1 màu (Solid)
      for (int i = 0; i < ledCount; i++) {
        ledStrip->setPixelColor(i, ledR, ledG, ledB);
      }
      ledStrip->show();
      break;

    case 1: { // 🌟 TỪ GIỮA DÂY TỎA RA 2 ĐẦU & GIỮ SÁNG (Center-Out Fill)
      float mid = (ledCount - 1) / 2.0;
      int halfLen = (ledCount + 1) / 2;
      int totalSteps = halfLen * 2 + 10;
      int step = effectStep % totalSteps;

      if (step == 0) {
        ledStrip->clear();
      }

      if (step < halfLen) {
        // Giai đoạn 1: Mở rộng từ tâm ra 2 đầu, CÁC BÓNG ĐÃ CHẠY QUA VẪN GIỮ SÁNG
        int leftIdx = (int)floor(mid - step);
        int rightIdx = (int)ceil(mid + step);
        if (leftIdx >= 0 && leftIdx < ledCount) {
          ledStrip->setPixelColor(leftIdx, ledR, ledG, ledB);
        }
        if (rightIdx >= 0 && rightIdx < ledCount) {
          ledStrip->setPixelColor(rightIdx, ledR, ledG, ledB);
        }
      } else if (step < halfLen + 6) {
        // Giai đoạn 2: Giữ sáng toàn bộ dải LED một nhịp
      } else if (step < halfLen * 2 + 6) {
        // Giai đoạn 3: Tắt dần từ giữa ra 2 đầu theo nhịp thở êm
        int unstep = step - (halfLen + 6);
        int leftIdx = (int)floor(mid - unstep);
        int rightIdx = (int)ceil(mid + unstep);
        if (leftIdx >= 0 && leftIdx < ledCount) {
          ledStrip->setPixelColor(leftIdx, 0, 0, 0);
        }
        if (rightIdx >= 0 && rightIdx < ledCount) {
          ledStrip->setPixelColor(rightIdx, 0, 0, 0);
        }
      }

      ledStrip->show();
      effectStep++;
      break;
    }

    case 2: { // TỪ 2 ĐẦU DỒN VÀO GIỮA & GIỮ SÁNG (Edges In Fill)
      float mid = (ledCount - 1) / 2.0;
      int halfLen = (ledCount + 1) / 2;
      int totalSteps = halfLen * 2 + 10;
      int step = effectStep % totalSteps;

      if (step == 0) {
        ledStrip->clear();
      }

      if (step < halfLen) {
        int leftIdx = step;
        int rightIdx = ledCount - 1 - step;
        if (leftIdx >= 0 && leftIdx < ledCount) {
          ledStrip->setPixelColor(leftIdx, ledR, ledG, ledB);
        }
        if (rightIdx >= 0 && rightIdx < ledCount) {
          ledStrip->setPixelColor(rightIdx, ledR, ledG, ledB);
        }
      } else if (step < halfLen + 6) {
        // Giữ sáng toàn bộ dải đèn
      } else if (step < halfLen * 2 + 6) {
        int unstep = step - (halfLen + 6);
        int leftIdx = unstep;
        int rightIdx = ledCount - 1 - unstep;
        if (leftIdx >= 0 && leftIdx < ledCount) {
          ledStrip->setPixelColor(leftIdx, 0, 0, 0);
        }
        if (rightIdx >= 0 && rightIdx < ledCount) {
          ledStrip->setPixelColor(rightIdx, 0, 0, 0);
        }
      }

      ledStrip->show();
      effectStep++;
      break;
    }

    case 3: { // Cầu vồng cuộn mượt mà (Rainbow Cycle - Không nháy, 16-bit HSV)
      uint16_t baseHue = effectStep * 256;
      for (int i = 0; i < ledCount; i++) {
        uint16_t pixelHue = baseHue + (i * 65536L / ledCount);
        ledStrip->setPixelColor(i, ledStrip->gamma32(ledStrip->ColorHSV(pixelHue)));
      }
      ledStrip->show();
      effectStep++;
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
      ledStrip->show();
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
      ledStrip->show();
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
      ledStrip->show();
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
      ledStrip->show();
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
      ledStrip->show();
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
      ledStrip->show();
      break;
    }

    case 10: { // Chớp nháy (Strobe)
      bool on = (effectStep % 2 == 0);
      for (int i = 0; i < ledCount; i++) {
        ledStrip->setPixelColor(i, on ? ledR : 0, on ? ledG : 0, on ? ledB : 0);
      }
      ledStrip->show();
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
  Serial.println(" BLE Device: PIXEL-DECK-AUDIO | LED Pin: GPIO 2  ");
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

  // 3. Xử lý núm xoay Encoder (Chỉnh âm lượng Volume)
  uint8_t clk = digitalRead(PIN_ENCODER_CLK);
  uint8_t dt  = digitalRead(PIN_ENCODER_DT);
  uint8_t current_state = (clk << 1) | dt;

  if (current_state != enc_last_state) {
    uint8_t index = (enc_last_state << 2) | current_state;
    enc_counter += rot_enc_table[index];
    enc_last_state = current_state;

    if (enc_counter >= 2) {
      if (bleKeyboard.isConnected()) {
        bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
      }
      Serial.println("[ENCODER] Volume UP (+)");
      enc_counter = 0;
    } else if (enc_counter <= -2) {
      if (bleKeyboard.isConnected()) {
        bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
      }
      Serial.println("[ENCODER] Volume DOWN (-)");
      enc_counter = 0;
    }
  }

  // 4. Xử lý nút nhấn trên trục Encoder (Play / Pause)
  if (btnEncoderSW.wasPressed()) {
    Serial.println("[BUTTON] Encoder SW: Play / Pause");
    if (bleKeyboard.isConnected()) {
      bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
    }
  }

  // 5 & 6. Xử lý nút bấm Next (GPIO 6), Prev (GPIO 7) và COMBO BẤM CẢ 2 NÚT ĐỂ ĐỔI HIỆU ỨNG LED
  static unsigned long btnNextPressTime = 0;
  static unsigned long btnPrevPressTime = 0;
  static bool nextPending = false;
  static bool prevPending = false;
  static bool comboActive = false;
  const unsigned long COMBO_WINDOW_MS = 80; // Cửa sổ 80ms để nhận diện bấm đồng thời 2 nút

  bool nextPinLow = (digitalRead(PIN_BTN_NEXT) == LOW);
  bool prevPinLow = (digitalRead(PIN_BTN_PREV) == LOW);

  // Khi cả 2 nút đều đã nhả ra thì giải phóng cờ combo
  if (!nextPinLow && !prevPinLow) {
    comboActive = false;
  }

  // Lambda hàm chuyển hiệu ứng khi bấm combo 2 nút
  auto triggerEffectCombo = [&]() {
    nextPending = false;
    prevPending = false;
    comboActive = true;

    // Chuyển sang hiệu ứng kế tiếp (0 -> 10)
    ledFx = (ledFx + 1) % 11;
    effectStep = 0;
    triggerSave();

    Serial.printf("[COMBO 2 NÚT] Bấm cùng lúc Next + Prev -> Đổi hiệu ứng LED: #%d\n", ledFx);

    // Chớp trắng nhanh báo hiệu đã chuyển hiệu ứng thành công
    if (ledStrip && ledPower) {
      for (int i = 0; i < ledCount; i++) {
        ledStrip->setPixelColor(i, Adafruit_NeoPixel::Color(255, 255, 255));
      }
      ledStrip->show();
      delay(40);
    }
  };

  // Kiểm tra sự kiện bấm nút Next
  if (btnNext.wasPressed()) {
    if (!comboActive) {
      if (prevPinLow) {
        // Nút Prev đang giữ sẵn -> Kích hoạt Combo!
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
      if (nextPinLow) {
        // Nút Next đang giữ sẵn -> Kích hoạt Combo!
        triggerEffectCombo();
      } else {
        prevPending = true;
        btnPrevPressTime = millis();
      }
    }
  }

  // Xử lý nút Next sau thời gian chờ combo
  if (nextPending) {
    if (prevPinLow || (digitalRead(PIN_BTN_PREV) == LOW)) {
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

  // Xử lý nút Prev sau thời gian chờ combo
  if (prevPending) {
    if (nextPinLow || (digitalRead(PIN_BTN_NEXT) == LOW)) {
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

  // 7. Nút bấm 3 (GPIO 10): Dừng / Phát (Play / Pause)
  if (btnPlay.wasPressed()) {
    Serial.println("[BUTTON 10] Play / Pause");
    if (bleKeyboard.isConnected()) {
      bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
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
