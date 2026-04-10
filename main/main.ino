#include <Wire.h>
#include <SparkFun_GridEYE_Arduino_Library.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <U8g2lib.h>

#include <Motoron.h>
#include <Servo.h>

GridEYE grideye;
float px[64];

MotoronI2C mc;
Servo servo;

bool door_state = false;

// MOTOR SETUP
const uint16_t referenceMv = 5000;
const auto vinType = MotoronVinSenseType::Motoron256;
const uint16_t minVinVoltageMv = 4500;

const uint16_t errorMask =
    (1 << MOTORON_STATUS_FLAG_PROTOCOL_ERROR) |
    (1 << MOTORON_STATUS_FLAG_CRC_ERROR) |
    (1 << MOTORON_STATUS_FLAG_COMMAND_TIMEOUT_LATCHED) |
    (1 << MOTORON_STATUS_FLAG_MOTOR_FAULT_LATCHED) |
    (1 << MOTORON_STATUS_FLAG_NO_POWER_LATCHED) |
    (1 << MOTORON_STATUS_FLAG_UART_ERROR) |
    (1 << MOTORON_STATUS_FLAG_RESET) |
    (1 << MOTORON_STATUS_FLAG_COMMAND_TIMEOUT);

// ENCODER SETUP
const int pinA = A2;
const int pinB = A3;

volatile long position = 0;
volatile int lastAState = LOW;

const float N_PPR = 114.0;
const float DRUM_RADIUS_M = 0.0127;

// ================== OLED ==================
U8G2_SSD1362_256X64_F_4W_SW_SPI u8g2(U8G2_R0, 13, 11, 10, 12, 5);

// ================== PINS ==================
const int rfidRxPin = 2;
const int rfidTxPin = 7;

const int redPin    = 4;
const int greenPin  = 8;
const int bluePin   = 9;
const int buzzerPin = A0;
const int buttonPin = A1;
const int servoPin  = 3;

// ================== RFID ==================
SoftwareSerial RFID(rfidRxPin, rfidTxPin);

String tag = "";
bool readingTag = false;
String allowedTag1 = "55000BF4B11B";

// ================== HEAT DETECTION ==================
const int FRAME_DELAY_MS = 200;
const int REQUIRED_FRAMES = 1;

const float MIN_ABS_TEMP = 26.0;
const float TEMP_DIFF    = 2.0;
const int MIN_HOT_PIXELS = 3;

int consecutiveHits = 0;
int wrongIDcount = 0;
float currentMaxTemp = 0.0;
bool thermalOK = true;

// ================== SYSTEM STATE ==================
bool unlocked = false;
bool alarmActive = false;
String lastRFIDStatus = "Ready";

// %%%%%%%%%%%%%%%%%%%%%%%%%%
// MOTOR AND SERVO FUNCTION
void readEncoder() {
  int currentAState = digitalRead(pinA);

  if (currentAState != lastAState) {
    if (digitalRead(pinB) != currentAState) {
      position++;
    } else {
      position--;
    }
    lastAState = currentAState;
  }
}

void servo_unlock() {
  Serial.println("Servo unlock");
  servo.attach(servoPin);
  servo.write(180);
  delay(2000);
  servo.detach();
}

void servo_lock() {
  Serial.println("Servo lock");
  servo.attach(servoPin);
  servo.write(40);
  delay(2000);
  servo.detach();
}

void roll_up() {
  Serial.println("Rolling up...");
  position = 0;
  unsigned long startTime = millis();

  while (true) {
    float theta = (2.0 * PI / N_PPR) * (float)position;
    float s = DRUM_RADIUS_M * theta * 1000.0;

    mc.setSpeed(1, 300);

    Serial.print("UP pos = ");
    Serial.print(position);
    Serial.print("  s = ");
    Serial.println(s);

    if (s <= -38.0) break;
    if (millis() - startTime > 5000) {
      Serial.println("roll_up timeout");
      break;
    }

    delay(5);
  }

  mc.setSpeedNow(1, 0);
}

void roll_down() {
  Serial.println("Rolling down...");
  position = 0;
  unsigned long startTime = millis();

  while (true) {
    float theta = (2.0 * PI / N_PPR) * (float)position;
    float s = DRUM_RADIUS_M * theta * 1000.0;

    mc.setSpeed(1, -300);

    Serial.print("DOWN pos = ");
    Serial.print(position);
    Serial.print("  s = ");
    Serial.println(s);

    if (s >= 38.0) break;
    if (millis() - startTime > 5000) {
      Serial.println("roll_down timeout");
      break;
    }

    delay(5);
  }

  mc.setSpeedNow(1, 0);
}

void system_unlock() {
  servo_unlock();
  delay(500);
  roll_up();
  door_state = true;
  Serial.println("Door opened");
}

void system_lock() {
  servo_unlock();
  delay(500);
  roll_down();
  delay(500);
  servo_lock();
  door_state = false;
  Serial.println("Door closed and locked");
}

// ===================================================
// OLED
// ===================================================
void drawCenteredText(const char* text, int y, const uint8_t* font) {
  u8g2.setFont(font);
  int w = u8g2.getStrWidth(text);
  int x = (256 - w) / 2;
  u8g2.drawStr(x, y, text);
}

void updateOLED() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(5, 10, "Garage Security System");

  if (!thermalOK) {
    drawCenteredText("SENSOR ERROR", 34, u8g2_font_logisoso18_tr);
  } else if (alarmActive) {
    drawCenteredText("ALARM", 34, u8g2_font_logisoso24_tr);
  } else if (unlocked) {
    drawCenteredText("UNLOCKED", 34, u8g2_font_logisoso18_tr);
  } else {
    drawCenteredText("LOCKED", 34, u8g2_font_logisoso24_tr);
  }

  u8g2.setFont(u8g2_font_7x13_tr);

  if (thermalOK) {
    char tempLine[40];
    snprintf(tempLine, sizeof(tempLine), "Temp: %.1f C", currentMaxTemp);
    u8g2.drawStr(6, 50, tempLine);
  } else {
    u8g2.drawStr(6, 50, "Temp: SENSOR ERR");
  }

  char hitLine[24];
  snprintf(hitLine, sizeof(hitLine), "Hits: %d", consecutiveHits);
  u8g2.drawStr(150, 50, hitLine);

  String rfidLine = "RFID: " + lastRFIDStatus;
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(6, 63, rfidLine.c_str());

  u8g2.sendBuffer();
}

// ===================================================
// RGB
// ===================================================
void setRGB(int r, int g, int b) {
  analogWrite(redPin, r);
  analogWrite(greenPin, g);
  analogWrite(bluePin, b);
}

void showLockedState() {
  setRGB(0, 0, 255);
}

void showUnlockedState() {
  setRGB(0, 255, 0);
}

void showAlarmState() {
  setRGB(255, 0, 0);
}

void showDeniedState() {
  setRGB(255, 255, 0);
}

// ===================================================
// BUZZER
// ===================================================
void beep(int freq, int durationMs) {
  if (freq <= 0 || durationMs <= 0) return;

  unsigned long periodMicros = 1000000UL / (unsigned long)freq;
  unsigned long halfPeriodMicros = periodMicros / 2;
  unsigned long cycles = ((unsigned long)durationMs * 1000UL) / periodMicros;

  for (unsigned long i = 0; i < cycles; i++) {
    digitalWrite(buzzerPin, HIGH);
    delayMicroseconds(halfPeriodMicros);
    digitalWrite(buzzerPin, LOW);
    delayMicroseconds(halfPeriodMicros);
  }
}

void successBeep() {
  beep(2000, 120);
  delay(80);
  beep(2500, 120);
}

void failBeep() {
  beep(500, 400);
}

// ===================================================
// BUTTON
// ===================================================
bool buttonPressed() {
  return digitalRead(buttonPin) == LOW;
}

// ===================================================
// THERMAL SENSOR
// ===================================================
bool readPixels() {
  bool validFrame = true;

  for (int i = 0; i < 64; i++) {
    float t = grideye.getPixelTemperature(i);
    px[i] = t;

    if (t < -20.0 || t > 100.0) {
      validFrame = false;
    }
  }

  return validFrame;
}

float getAverageTemp() {
  float sum = 0;
  for (int i = 0; i < 64; i++) {
    sum += px[i];
  }
  return sum / 64.0;
}

float getMaxTemp() {
  float maxT = px[0];
  for (int i = 1; i < 64; i++) {
    if (px[i] > maxT) maxT = px[i];
  }
  return maxT;
}

bool detectHeat() {
  float avgT = getAverageTemp();
  float maxT = getMaxTemp();
  currentMaxTemp = maxT;

  int hotPixels = 0;

  for (int i = 0; i < 64; i++) {
    if (px[i] >= MIN_ABS_TEMP && px[i] >= (avgT + TEMP_DIFF)) {
      hotPixels++;
    }
  }

  Serial.print("Avg: ");
  Serial.print(avgT);
  Serial.print("  Max: ");
  Serial.print(maxT);
  Serial.print("  HotPixels: ");
  Serial.print(hotPixels);
  Serial.print("  Hits: ");
  Serial.print(consecutiveHits);
  Serial.print("  Alarm: ");
  Serial.print(alarmActive);
  Serial.print("  Unlocked: ");
  Serial.println(unlocked);

  return (hotPixels >= MIN_HOT_PIXELS);
}

// ===================================================
// RFID
// ===================================================
void processTag(String scannedTag) {
  scannedTag.trim();

  Serial.print("Full Card ID: ");
  Serial.println(scannedTag);

  if (scannedTag == allowedTag1) {
    successBeep();
    wrongIDcount = 0;
    consecutiveHits = 0;

    if (alarmActive) {
      Serial.println("ALARM CLEARED BY RFID");
      alarmActive = false;
      unlocked = true;
      lastRFIDStatus = "Alarm Cleared";
      showUnlockedState();

      if (!door_state) {
        system_unlock();
      }

    } else {
      unlocked = !unlocked;

      if (unlocked) {
        Serial.println("ACCESS GRANTED - SYSTEM UNLOCKED");
        lastRFIDStatus = "Access OK";
        showUnlockedState();

        if (!door_state) {
          system_unlock();
        }

      } else {
        Serial.println("SYSTEM LOCKED AGAIN");
        lastRFIDStatus = "Locked";
        showLockedState();

        if (door_state) {
          system_lock();
        }
      }
    }

  } else {
    Serial.println("ACCESS DENIED");
    failBeep();
    wrongIDcount++;
    lastRFIDStatus = "Denied";
    showDeniedState();
    delay(300);

    if (wrongIDcount >= 3) {
      alarmActive = true;
      lastRFIDStatus = "Too Many Bad IDs";
      Serial.println("ALARM TRIGGERED BY WRONG RFID");
    }

    if (alarmActive) showAlarmState();
    else if (unlocked) showUnlockedState();
    else showLockedState();
  }

  updateOLED();
}

void readRFID() {
  while (RFID.available()) {
    char c = RFID.read();

    if (c == 0x02) {
      tag = "";
      readingTag = true;
    } else if (c == 0x03) {
      readingTag = false;
      processTag(tag);
    } else if (readingTag) {
      tag += c;
    }
  }
}

// --------------------------------------------------
// Motor driver checking function
void checkCommunicationError(uint8_t errorCode)
{
  if (errorCode)
  {
    while (1)
    {
      mc.reset();
      Serial.print(F("Communication error: "));
      Serial.println(errorCode);
      delay(1000);
    }
  }
}

void checkControllerError(uint16_t status)
{
  if (status & errorMask)
  {
    while (1)
    {
      mc.reset();
      Serial.print(F("Controller error: 0x"));
      Serial.println(status, HEX);
      delay(1000);
    }
  }
}

void checkVinVoltage(uint16_t voltageMv)
{
  if (voltageMv < minVinVoltageMv)
  {
    while (1)
    {
      mc.reset();
      Serial.print(F("VIN voltage too low: "));
      Serial.println(voltageMv);
      delay(1000);
    }
  }
}

void checkForProblems()
{
  uint16_t status = mc.getStatusFlags();
  checkCommunicationError(mc.getLastError());
  checkControllerError(status);

  uint32_t voltageMv = mc.getVinVoltageMv(referenceMv, vinType);
  checkCommunicationError(mc.getLastError());
  checkVinVoltage(voltageMv);
}

// ===================================================
// SETUP
// ===================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  RFID.begin(9600);

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  u8g2.begin();

  Wire1.begin();
  grideye.begin(0x69, Wire1);

  u8g2.clearBuffer();
  drawCenteredText("Garage Security", 24, u8g2_font_logisoso18_tr);
  drawCenteredText("Starting...", 50, u8g2_font_7x13_tr);
  u8g2.sendBuffer();

  Serial.println("Sensor warming up...");
  delay(8000);

  unlocked = false;
  alarmActive = false;
  consecutiveHits = 0;
  currentMaxTemp = 0.0;
  thermalOK = true;
  wrongIDcount = 0;
  lastRFIDStatus = "Ready";

  Wire.begin();
  mc.reinitialize();
  mc.disableCrc();
  mc.clearResetFlag();
  mc.clearMotorFault();

  mc.setMaxAcceleration(1, 500);
  mc.setMaxDeceleration(1, 1500);

  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(pinA), readEncoder, CHANGE);

  showLockedState();
  updateOLED();

  Serial.println("Detection started");
  Serial.println("System is LOCKED");
}

// ===================================================
// LOOP
// ===================================================
void loop() {
  checkForProblems();
  readRFID();

  if (buttonPressed()) {
    delay(30);
    if (buttonPressed()) {
      if (!alarmActive) {
        unlocked = !unlocked;
        consecutiveHits = 0;
        wrongIDcount = 0;
        lastRFIDStatus = "Button";

        if (unlocked) {
          Serial.println("BUTTON: SYSTEM UNLOCKED");
          showUnlockedState();
          if (!door_state) {
            system_unlock();
          }
        } else {
          Serial.println("BUTTON: SYSTEM LOCKED");
          showLockedState();
          if (door_state) {
            system_lock();
          }
        }

        updateOLED();
      }

      while (buttonPressed()) {
        delay(10);
      }
    }
  }

  thermalOK = readPixels();

  if (!thermalOK) {
    Serial.println("THERMAL SENSOR READ FAILED");
    currentMaxTemp = -99.0;
    consecutiveHits = 0;

    if (!alarmActive) {
      lastRFIDStatus = "Thermal Error";
    }

    updateOLED();
    delay(FRAME_DELAY_MS);
    return;
  }

  bool heatDetected = detectHeat();

  if (!unlocked && !alarmActive && heatDetected) {
    consecutiveHits++;
  } else if (!heatDetected && !alarmActive) {
    consecutiveHits = 0;
  }

  if (!unlocked && consecutiveHits >= REQUIRED_FRAMES && !alarmActive) {
    Serial.println("ALARM TRIGGERED AND LATCHED");
    alarmActive = true;
    lastRFIDStatus = "Alarm Active";
    showAlarmState();
  }

  if (alarmActive) {
    showAlarmState();

    beep(1200, 120);
    beep(1800, 120);
    beep(2400, 120);
    delay(80);
  } else {
    if (unlocked) showUnlockedState();
    else showLockedState();

    delay(FRAME_DELAY_MS);
  }

  updateOLED();
}