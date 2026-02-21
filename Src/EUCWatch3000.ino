#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* =====================================================
   OLED
   ===================================================== */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* =====================================================
   PINS – ESP32-C3 MINI
   ===================================================== */
// (les pins ESP32 classiques sont commentés dans ta version d’origine)
/*
#define PIN_SDA 21
#define PIN_SCL 22
#define BTN_BEEP 13
#define BTN_LIGHT 14
#define BTN_POWER 4
#define BAT_ADC_PIN 34
*/

#define PIN_SDA 5
#define PIN_SCL 6
#define BTN_BEEP 7
#define BTN_LIGHT 8
#define BTN_POWER 2 // compatible wakeup
#define BAT_ADC_PIN 4 // ADC GPIO4

/* =====================================================
   POWER
   ===================================================== */
#define LONG_PRESS_MS 2000

unsigned long powerPressStart = 0;
bool powerBtnLastState = HIGH;
bool powerLongTriggered = false;
static unsigned long lastProgressDraw = 0;

/* =====================================================
   BLE – BEGODE A2
   ===================================================== */
static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID charUUID("0000ffe1-0000-1000-8000-00805f9b34fb");

BLEAdvertisedDevice* myDevice = nullptr;
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;

bool bleConnected = false;
bool telemetryValid = false;

unsigned long lastReconnectAttempt = 0;
unsigned long lastTelemetryTime = 0;

#define RECONNECT_INTERVAL_MS 2000
//#define TELEMETRY_TIMEOUT 5000

/* =====================================================
   BEEP
   ===================================================== */
#define BEEP_DELAY_MS 500
#define BEEP_COOLDOWN 1200
#define BEEP_SPEED_LIMIT_KMH 20.0 // Vitesse max autorisée pour le beep

uint8_t beepCmd[] = { 'b' };
unsigned long lastBeepAction = 0;

/* =====================================================
   LIGHT
   ===================================================== */
bool lightOn = false;
unsigned long lastLightBtnCheck = 0;
bool lastLightBtnState = HIGH;

uint8_t cmdLightOn[] = { 'Q' };
uint8_t cmdLightOff[] = { 'E' };

const uint8_t icon_headlight_16x16[] PROGMEM = { // Icone phare
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x1f, 0x03, 0x43, 0xff, 0x79, 0x03, 0x7c,
  0xff, 0x7e, 0x03, 0x7c, 0xff, 0x79, 0x03, 0x43,
  0xff, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/* =====================================================
   BATTERIES
   ===================================================== */
#define LOW_BATT_THRESHOLD 20
#define BLINK_INTERVAL_MS 500

bool blinkState = false;
unsigned long lastBlink = 0;

#define ACCU_MAX_V 4.20
#define ACCU_MIN_V 3.3

/* =====================================================
   TELEMETRY
   ===================================================== */
float speed_kmh = 0.0;
float voltage = 0.0;
int batteryWheel = 0;
int batteryAccu = 0;
int temp_c = 0;
float distance_km = 0.0;

/* =====================================================
   POWER
   ===================================================== */
void goToDeepSleep() {
  display.clearDisplay();
  display.display();

#if CONFIG_IDF_TARGET_ESP32C3
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BTN_POWER, ESP_GPIO_WAKEUP_GPIO_LOW);
#else
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_POWER, 0);
#endif

  esp_deep_sleep_start();
}

/* =====================================================
   UTILS
   ===================================================== */
int readAccuPercent() {
  long sum = 0;
  for (int i = 0; i < 5; i++) {
    sum += analogRead(BAT_ADC_PIN);
    delay(2);
  }

  float raw = sum / 5.0;
  float v_bat = 0.0014 * raw + 0.0668;
  //Serial.print("raw: ");Serial.print(raw);Serial.print(" v_bat: ");Serial.println(v_bat);
  int pct = (v_bat - ACCU_MIN_V) * 100 / (ACCU_MAX_V - ACCU_MIN_V);
  return constrain(pct, -1, 100);
}

/* =====================================================
   GOTWAY / BEGODE PROTOCOL HANDLER
   ===================================================== */
class GotwayUnpacker {
public:
  enum State { UNKNOWN, COLLECTING, DONE };
  State state = UNKNOWN;

  uint8_t buffer[24];
  int pos = 0;
  int oldc = -1;

  bool addChar(uint8_t c) {
    if (state == COLLECTING) {
      buffer[pos++] = c;

      if (pos == 24) {
        state = DONE;
        return true;
      }

      if (pos > 20 && c != 0x5A) { // invalid footer
        state = UNKNOWN;
        pos = 0;
      }
    } else {
      if (c == 0xAA && oldc == 0x55) {
        buffer[0] = 0x55;
        buffer[1] = 0xAA;
        pos = 2;
        state = COLLECTING;
      }
      oldc = c;
    }
    return false;
  }

  void reset() {
    state = UNKNOWN;
    pos = 0;
  }
};

GotwayUnpacker unpacker;

void parseFrame(uint8_t* buff) {
  uint8_t type = buff[18];

  if (type == 0x00) {
    // Frame A: live telemetry

    // --- Speed ---
    int16_t speed_raw = (buff[4] << 8) | buff[5]; // Big Endian
    speed_kmh = speed_raw * 3.6 / 100.0;          // raw to km/h

    // --- Temperature ---
    // Ici on suppose MPU6050, sinon adapter si c'est MPU6500
    int16_t temp_raw = (int16_t)((buff[12] << 8) | buff[13]); // Big Endian
    temp_c = ((float)temp_raw / 340.0f) + 36.53f;

    // --- Voltage & Battery ---
    uint16_t voltage_raw = (buff[2] << 8) | buff[3]; // Big Endian
    voltage = voltage_raw / 100.0f;                  // en volts

    // Calcul du % batterie selon le code Android
    if (voltage_raw > 6680) {
      batteryWheel = 100;
    } else if (voltage_raw > 5440) {
      batteryWheel = (int)round((voltage_raw - 5320) / 13.6);
    } else if (voltage_raw > 5120) {
      batteryWheel = (voltage_raw - 5120) / 36;
    } else {
      batteryWheel = 0;
    }

    // ---------- DISTANCE ----------
    uint16_t distance_m = (buff[8] << 8) | buff[9];
    distance_km = distance_m / 1000.0f;

    // --- Telemetry valid ---
    telemetryValid = true;
    lastTelemetryTime = millis();
  }
}

/* =====================================================
   BLE NOTIFY CALLBACK
   ===================================================== */
class MyNotifyCallback {
public:
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                             uint8_t* pData, size_t length, bool isNotify) {
    /*
    // --- DEBUG: afficher la trame entière ---
    Serial.print("Trame: ");
    for (int i = 0; i < length; i++) {
      Serial.print(i);
      Serial.print(":");
      if (pData[i] < 16) Serial.print("0");
      Serial.print(pData[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    */

    for (size_t i = 0; i < length; i++) {
      if (unpacker.addChar(pData[i])) {
        parseFrame(unpacker.buffer);
        unpacker.reset();
      }
    }
  }
};

/* =====================================================
   BLE SCAN CALLBACK
   ===================================================== */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(serviceUUID)) {
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      BLEDevice::getScan()->stop();
    }
  }
};

MyAdvertisedDeviceCallbacks advCallbacks;

/* =====================================================
   BLE CONNECT / DISCONNECT
   ===================================================== */
bool connectToWheel() {
  if (!myDevice) return false;

  pClient = BLEDevice::createClient();
  if (!pClient->connect(myDevice)) return false;

  BLERemoteService* pService = pClient->getService(serviceUUID);
  if (!pService) return false;

  pRemoteCharacteristic = pService->getCharacteristic(charUUID);
  if (!pRemoteCharacteristic) return false;

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(MyNotifyCallback::notifyCallback);
  }

  bleConnected = true;
  telemetryValid = false;
  return true;
}

void handleBleDisconnect() {
  bleConnected = false;
  telemetryValid = false;
  lightOn = false;

  if (pClient) {
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
  }

  myDevice = nullptr;
}

/* =====================================================
   BEEP
   ===================================================== */
void sendDoubleBeep() {
  if (!bleConnected) return;
  pRemoteCharacteristic->writeValue(beepCmd, sizeof(beepCmd), false);
  delay(BEEP_DELAY_MS);
  pRemoteCharacteristic->writeValue(beepCmd, sizeof(beepCmd), false);
}

void sendSingleBeep() {
  if (!bleConnected) return;
  pRemoteCharacteristic->writeValue(beepCmd, sizeof(beepCmd), false);
}

/* =====================================================
   LIGHT
   ===================================================== */
void setLight(bool on) {
  if (!bleConnected) return;

  if (on) {
    pRemoteCharacteristic->writeValue(cmdLightOn, sizeof(cmdLightOn), false);
  } else {
    pRemoteCharacteristic->writeValue(cmdLightOff, sizeof(cmdLightOff), false);
  }
  // Beep simple
  // sendSingleBeep();
}

/* =====================================================
   DISPLAY
   ===================================================== */
void drawPowerProgress(unsigned long elapsed) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  String text = "HOLD TO TURN OFF";

  int textWidth = text.length() * 6 * 1;
  int textHeight = 8 * 1;

  int barWidth = 100;
  int barHeight = 10;
  int spacing = 8;

  int totalHeight = textHeight + spacing + barHeight;
  int startY = (SCREEN_HEIGHT - totalHeight) / 2;

  int textX = (SCREEN_WIDTH - textWidth) / 2;
  int textY = startY;

  display.setCursor(textX, textY);
  display.print(text);

  int barX = (SCREEN_WIDTH - barWidth) / 2;
  int barY = textY + textHeight + spacing;

  display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);

  int fill = map(elapsed, 0, LONG_PRESS_MS, 0, barWidth - 2);
  fill = constrain(fill, 0, barWidth - 2);

  display.fillRect(barX + 1, barY + 1, fill, barHeight - 2, SSD1306_WHITE);
  display.display();
}

void drawBatteryBar(int x, int y, int w, int h, int percent) {
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  int fill = (w - 2) * percent / 100;
  display.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
}

unsigned long lastDisplayUpdate = 0;
#define DISPLAY_INTERVAL 150

// Affiche °C à la position (x, y)
void printDegC(int x, int y) {
  display.setCursor(x + 18, y + 2); // le C
  display.print("C");
  display.drawCircle(x + 14, y + 2, 2, SSD1306_WHITE); // le °
}

void showOffScreen() {
  display.clearDisplay();
  display.setTextSize(4);

  String txt = "OFF";
  int textWidth = txt.length() * 6 * 4;
  int x = (SCREEN_WIDTH - textWidth) / 2;
  int y = (SCREEN_HEIGHT - 8 * 4) / 2;

  display.setCursor(x, y);
  display.print(txt);
  display.display();
}

void updateDisplay() {
  unsigned long now = millis();

  if (now - lastBlink > BLINK_INTERVAL_MS) {
    blinkState = !blinkState;
    lastBlink = now;
  }

  batteryAccu = readAccuPercent();
  bool lowBattWheel = telemetryValid && (batteryWheel <= LOW_BATT_THRESHOLD);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  /* ================= TOP ================= */
  // Batterie roue (haut gauche)
  display.setTextSize(2);
  display.setCursor(0, 0);
  if (telemetryValid) display.print(batteryWheel);
  else display.print("--");
  display.print("%");

  // Température (haut droite)
  display.setTextSize(2);
  int tempX = SCREEN_WIDTH - 52; // ajusté pour tenir "°C"
  display.setCursor(tempX, 0);
  if (telemetryValid) display.print(temp_c);
  else display.print("--");
  printDegC(tempX + 16, 0); // °C à droite du chiffre

  /* ================= SPEED ================= */
  int speedDisp = telemetryValid ? abs((int)round(speed_kmh)) : 0;

  char speedStr[3];
  snprintf(speedStr, sizeof(speedStr), "%02d", speedDisp);

  display.setTextSize(4);

  int textSize = 4;
  int charWidth = 6 * textSize;
  int speedWidth = charWidth * 2;
  int speedX = (SCREEN_WIDTH - speedWidth) / 2;
  int speedY = 18;

  display.setCursor(speedX, speedY);
  display.print(speedStr);

  // Label km/h
  display.setTextSize(1);
  int kmhX = speedX + speedWidth + 4;
  int kmhY = speedY + (4 * 8 - 8) / 2;
  display.setCursor(kmhX, kmhY);
  display.print("km/h");

  /* ================= LIGHT ================= */
  int iconX = speedX - 36;
  int iconY = speedY + 8;

  if (lightOn) {
    display.drawBitmap(iconX, iconY, icon_headlight_16x16, 16, 16, SSD1306_WHITE);
  }

  /* ================= DISTANCE ================= */
  display.setTextSize(1);
  display.setCursor(0, SCREEN_HEIGHT - 8);
  if (telemetryValid) {
    display.print(distance_km, 1);
  } else {
    display.print("--.-");
  }
  display.print("km");

  /* ================= STATUS ================= */
  display.setTextSize(1);
  String status;
  if (!bleConnected) status = "OFFLINE";
  else if (lowBattWheel) status = "!!!LOW BATTERY!!!";
  else status = "ONLINE";

  int16_t statusX = (SCREEN_WIDTH - status.length() * 6) / 2;
  display.setCursor(statusX, 55);
  display.print(status);

  /* ================= ESP BATTERY ================= */
  int barCount = map(batteryAccu, 0, 90, 1, 5);
  int barW = 3;
  int barH = 8;
  int gap = 2;
  int baseX = SCREEN_WIDTH - (barW * 5 + gap * 4) - 2;
  int baseY = SCREEN_HEIGHT - barH - 2;

  if (batteryAccu < 0) {
    if (blinkState) {
      display.drawRect(baseX, baseY, barW * 5 + gap * 4, barH, SSD1306_WHITE);
    }
  } else {
    for (int i = 0; i < barCount; i++) {
      int x = baseX + i * (barW + gap);
      display.fillRect(x, baseY, barW, barH, SSD1306_WHITE);
    }
  }

  display.display();
}

/* =====================================================
   SETUP
   ===================================================== */
void setup() {
  Serial.begin(115200);

  pinMode(BTN_BEEP, INPUT_PULLUP);
  pinMode(BTN_LIGHT, INPUT_PULLUP);
  pinMode(BTN_POWER, INPUT_PULLUP);

  analogReadResolution(12); // 0-4095

  Wire.begin(PIN_SDA, PIN_SCL);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  BLEDevice::init("ESP32-Begode");
}

/* =====================================================
   LOOP
   ===================================================== */
unsigned long lastBtnCheck = 0;
bool lastBtnState = HIGH;
#define DEBOUNCE_MS 50

void loop() {
  unsigned long now = millis();

  bool powerState = digitalRead(BTN_POWER);

  // Détection appui
  if (powerBtnLastState == HIGH && powerState == LOW) {
    powerPressStart = millis();
    powerLongTriggered = false;
  }

  // ================= POWER HOLD =================
  if (powerState == LOW) {
    if (!powerLongTriggered) {
      unsigned long heldTime = millis() - powerPressStart;

      if (now - lastProgressDraw > 30) {
        drawPowerProgress(heldTime);
        lastProgressDraw = now;
      }

      if (heldTime >= LONG_PRESS_MS) {
        powerLongTriggered = true;
        showOffScreen();
        delay(1000);
        goToDeepSleep();
      }
    }

    powerBtnLastState = powerState;
    return;
  }

  // Relâchement avant 2s → retour affichage normal
  if (powerBtnLastState == LOW && powerState == HIGH) {
    if (!powerLongTriggered) {
      updateDisplay();
    }
  }

  powerBtnLastState = powerState;

  // Déconnexion BLE
  if (bleConnected && pClient && !pClient->isConnected()) {
    handleBleDisconnect();
  }

  // Timeout télémétrie - désactivé car affiche des dashs alors que tout va bien
 /* if (telemetryValid && now - lastTelemetryTime > TELEMETRY_TIMEOUT) {
    telemetryValid = false;
  }*/

  // Reconnexion BLE
  if (!bleConnected && !pClient && now - lastReconnectAttempt > RECONNECT_INTERVAL_MS) {
    lastReconnectAttempt = now;

    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&advCallbacks);
    scan->setActiveScan(true);
    scan->start(3, false);

    if (myDevice) connectToWheel();
  }

  // Bouton BEEP (debounce)
  if (now - lastBtnCheck > DEBOUNCE_MS) {
    bool btn = digitalRead(BTN_BEEP);

    if (lastBtnState == HIGH && btn == LOW) {
      if (now - lastBeepAction > BEEP_COOLDOWN && abs(speed_kmh) < BEEP_SPEED_LIMIT_KMH) {
        lastBeepAction = now;
        sendDoubleBeep();
      }
    }

    lastBtnState = btn;
    lastBtnCheck = now;
  }

  // Bouton phare (toggle ON / OFF)
  if (now - lastLightBtnCheck > DEBOUNCE_MS) {
    bool btn = digitalRead(BTN_LIGHT);

    if (lastLightBtnState == HIGH && btn == LOW) {
      lightOn = !lightOn;
      setLight(lightOn);
    }

    lastLightBtnState = btn;
    lastLightBtnCheck = now;
  }

  // Affichage
  if (now - lastDisplayUpdate > DISPLAY_INTERVAL) {
    lastDisplayUpdate = now;
    updateDisplay();
  }
}
