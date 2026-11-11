#include <Arduino.h>
#include <nvs_flash.h>
#include <BleKeyboard.h>

// ================= Pin Definitions =================
// KY-040 Rotary Encoder
#define PIN_ENCODER_CLK   3
#define PIN_ENCODER_DT    4
#define PIN_ENCODER_SW    5

// 3 External Push Buttons (chân âm chung nối GND, chân còn lại vào ESP)
#define PIN_BTN_NEXT      6   // Chuyển bài (Next track)
#define PIN_BTN_PREV      7   // Back lại (Previous track)
#define PIN_BTN_PLAY      10  // Dừng phát (Play / Pause)

// Built-in LED on ESP32-C3 Super Mini (GPIO 8, active LOW)
#define PIN_LED           8

// ================= BLE Keyboard Instance =================
BleKeyboard bleKeyboard("NAM-DZ", "NAM", 100);

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

// Button instances
Button btnEncoderSW(PIN_ENCODER_SW);
Button btnNext(PIN_BTN_NEXT);
Button btnPrev(PIN_BTN_PREV);
Button btnPlay(PIN_BTN_PLAY);

// ================= Encoder State Variables =================
static const int8_t rot_enc_table[] = {
  0,  1, -1,  0,
 -1,  0,  0,  1,
  1,  0,  0, -1,
  0, -1,  1,  0
};

static uint8_t enc_last_state = 0;
static int8_t enc_counter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize NVS (preserve bonding keys across reboots)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  Serial.println("\n======================================");
  Serial.println("  ESP32-C3 Bluetooth Media Controller ");
  Serial.println("  Device Name: NAM-DZ");
  Serial.println("======================================");

  // Initialize pins
  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODER_DT,  INPUT_PULLUP);
  
  btnEncoderSW.begin();
  btnNext.begin();
  btnPrev.begin();
  btnPlay.begin();

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH); // LED OFF initially (active LOW)

  // Initial encoder state
  uint8_t clk = digitalRead(PIN_ENCODER_CLK);
  uint8_t dt  = digitalRead(PIN_ENCODER_DT);
  enc_last_state = (clk << 1) | dt;

  // Start BLE Keyboard
  bleKeyboard.begin();
  Serial.println("BLE Keyboard started. Advertising as 'NAM-DZ'...");
}

void loop() {
  // 1. Check Encoder Rotation (Volume Control)
  uint8_t clk = digitalRead(PIN_ENCODER_CLK);
  uint8_t dt  = digitalRead(PIN_ENCODER_DT);
  uint8_t current_state = (clk << 1) | dt;

  if (current_state != enc_last_state) {
    uint8_t index = (enc_last_state << 2) | current_state;
    enc_counter += rot_enc_table[index];
    enc_last_state = current_state;

    // Threshold of 2 transitions per volume step
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

  // 2. Check Encoder Switch (Play / Pause)
  if (btnEncoderSW.wasPressed()) {
    Serial.println("[BUTTON] Encoder SW: Play / Pause");
    if (bleKeyboard.isConnected()) {
      bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
    }
  }

  // 3. Check Button Next (GPIO 6)
  if (btnNext.wasPressed()) {
    Serial.println("[BUTTON 6] Next Track (Chuyen bai)");
    if (bleKeyboard.isConnected()) {
      bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
    }
  }

  // 4. Check Button Prev (GPIO 7)
  if (btnPrev.wasPressed()) {
    Serial.println("[BUTTON 7] Previous Track (Back lai)");
    if (bleKeyboard.isConnected()) {
      bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
    }
  }

  // 5. Check Button Play/Pause (GPIO 10)
  if (btnPlay.wasPressed()) {
    Serial.println("[BUTTON 10] Play / Pause (Dung phat)");
    if (bleKeyboard.isConnected()) {
      bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
    }
  }

  // 6. LED Status Indicator
  static uint32_t lastLedBlink = 0;
  static bool ledState = false;
  uint32_t now = millis();

  if (bleKeyboard.isConnected()) {
    // Solid ON when connected (active LOW)
    digitalWrite(PIN_LED, LOW);
  } else {
    // Blink every 500ms when waiting for connection
    if (now - lastLedBlink >= 500) {
      lastLedBlink = now;
      ledState = !ledState;
      digitalWrite(PIN_LED, ledState ? LOW : HIGH);
    }
  }

  // Small delay to prevent tight loop CPU spinning
  delayMicroseconds(500);
}
