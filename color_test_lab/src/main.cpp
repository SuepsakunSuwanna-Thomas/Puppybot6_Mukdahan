#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_TCS34725.h>

// ============================================================================
// 1. PIN DEFINITIONS & HARDWARE CONFIGURATION
// ============================================================================
// Display ST7735 (160x128 SPI TFT)
#define TFT_CS   13
#define TFT_DC   8
#define TFT_MOSI 11
#define TFT_SCLK 10
#define TFT_RST  9

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// Buzzer Pin
#define BUZZER_PIN 2

// External 4 Buttons
#define BTN_START_PIN   15   // ปุ่ม 1: Pin 15 / ADC 15
#define BTN_UP_PIN      37   // ปุ่ม 2: เลื่อนขึ้น (GPIO 37)
#define BTN_OK_PIN      38   // ปุ่ม 3: ตกลง / คาลิเบรต (GPIO 38)
#define BTN_DOWN_PIN    39   // ปุ่ม 4: เลื่อนลง (GPIO 39)
#define ONBOARD_BTN_PIN 3    // ปุ่มบนบอร์ด (สำรอง)

// I2C Pins for TCS34725 (หลัก: SDA=4, SCL=5)
#define FRONT_SDA 4
#define FRONT_SCL 5

#define SIDE_SDA  6
#define SIDE_SCL  7

// Adafruit TCS34725 Sensors
// ใช้ Integration Time 2.4ms หรือ 24ms และ Gain 4X
Adafruit_TCS34725 tcsFront = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_24MS, TCS34725_GAIN_4X);
Adafruit_TCS34725 tcsSide  = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_24MS, TCS34725_GAIN_4X);

uint8_t tcs_front_addr = 0x29;
uint8_t tcs_side_addr  = 0x29;
bool tcs_front_found = false;
bool tcs_side_found  = false;

// Selected active sensor: 0 = Front (Wire SDA=4, SCL=5), 1 = Side (Wire1 SDA=6, SCL=7)
int activeSensor = 0; 

// ============================================================================
// 2. COLOR DATA STRUCTURES & EEPROM
// ============================================================================
enum RobotColor {
  COLOR_UNKNOWN = 0,
  COLOR_RED     = 1,
  COLOR_GREEN   = 2,
  COLOR_BLUE    = 3,
  COLOR_YELLOW  = 4,
  COLOR_WHITE   = 5,
  COLOR_BLACK   = 6
};

#define NUM_CALIB_COLORS 6
const char* COLOR_NAMES[NUM_CALIB_COLORS] = {
  "RED", "GREEN", "BLUE", "YELLOW", "WHITE", "BLACK"
};

const uint16_t COLOR_HEX[NUM_CALIB_COLORS] = {
  ST7735_RED,     // Red
  ST7735_GREEN,   // Green
  ST7735_BLUE,    // Blue
  ST7735_YELLOW,  // Yellow
  ST7735_WHITE,   // White
  0x39E7          // Black
};

#define EEPROM_MAGIC 0x434F4C38 // "COL8"
#define EEPROM_SIZE  512

struct ColorCalibData {
  uint32_t magic;
  int front_rgb[NUM_CALIB_COLORS][3];    // [Color][R, G, B normalized 0-250]
  int front_clear[NUM_CALIB_COLORS];     // [Color] Raw Clear intensity
  int side_rgb[NUM_CALIB_COLORS][3];
  int side_clear[NUM_CALIB_COLORS];
};

ColorCalibData calibData;

// ค่าตั้งต้นมาตรฐานสำหรับเซนเซอร์สี (Normalized RGB สเกล 100)
const int DEFAULT_FRONT_RGB[NUM_CALIB_COLORS][3] = {
  { 180, 60,  60  }, // RED: R สูงเด่น
  { 60,  180, 60  }, // GREEN: G สูงเด่น
  { 55,  80,  165 }, // BLUE: B สูงเด่น
  { 140, 130, 30  }, // YELLOW: R & G สูงเด่น, B ต่ำ
  { 95,  115, 90  }, // WHITE: สมดุลใต้แสง LED ขาว (G จะสูงกว่า R,B เล็กน้อยตามสเปกตรัมหลอด)
  { 100, 100, 100 }  // BLACK: สมดุลเท่ากันแต่มืด
};

const int DEFAULT_FRONT_CLEAR[NUM_CALIB_COLORS] = {
  1500, // RED
  1800, // GREEN
  1200, // BLUE
  2500, // YELLOW
  3000, // WHITE (สะท้อนแสงสูง)
  200   // BLACK (ดูดกลืนแสง Clear ต่ำมาก)
};

// ============================================================================
// 3. BUZZER & SOUND EFFECTS
// ============================================================================
void beep(int freq, int duration_ms) {
  pinMode(BUZZER_PIN, OUTPUT);
  tone(BUZZER_PIN, freq);
  delay(duration_ms);
  tone(BUZZER_PIN, 0);
}

void soundClick() {
  beep(1800, 40);
}

void soundConfirm() {
  beep(1500, 60);
  delay(30);
  beep(2200, 90);
}

void soundColorFound(RobotColor c) {
  switch (c) {
    case COLOR_RED:    beep(1000, 60); break;
    case COLOR_GREEN:  beep(1400, 60); break;
    case COLOR_BLUE:   beep(1800, 60); break;
    case COLOR_YELLOW: beep(2200, 60); break;
    case COLOR_WHITE:  beep(2600, 60); break;
    case COLOR_BLACK:  beep(700, 60); break;
    default: break;
  }
}

// ============================================================================
// 4. BUTTON HANDLING (EXTERNAL 4 BUTTONS)
// ============================================================================
void setupButtons() {
  pinMode(BTN_UP_PIN, INPUT_PULLUP);    // ปุ่ม 2 (ขา 37)
  pinMode(BTN_OK_PIN, INPUT_PULLUP);    // ปุ่ม 3 (ขา 38)
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);  // ปุ่ม 4 (ขา 39)
  pinMode(BTN_START_PIN, INPUT_PULLUP); // ปุ่ม 1 (ขา 15)
  pinMode(ONBOARD_BTN_PIN, INPUT_PULLUP);
}

// ปุ่ม 1 (ขา 15 / ADC 15)
bool isBtn1StartRaw() {
  if (digitalRead(BTN_START_PIN) == LOW) return true;
  int adcVal = analogRead(32 + BTN_START_PIN);
  if (adcVal < 500 && adcVal >= 0) return true;
  return false;
}

// ปุ่ม 2 (ขา 37 - UP)
bool isBtnUpRaw() {
  return (digitalRead(BTN_UP_PIN) == LOW);
}

// ปุ่ม 3 (ขา 38 - OK / CALIB)
bool isBtnOkRaw() {
  return (digitalRead(BTN_OK_PIN) == LOW) || isBtn1StartRaw() || (digitalRead(ONBOARD_BTN_PIN) == LOW);
}

// ปุ่ม 4 (ขา 39 - DOWN)
bool isBtnDownRaw() {
  return (digitalRead(BTN_DOWN_PIN) == LOW);
}

bool isAnyButtonPressed() {
  return isBtnOkRaw() || isBtnUpRaw() || isBtnDownRaw() || isBtn1StartRaw();
}

void waitButtonRelease() {
  while (isAnyButtonPressed()) {
    delay(10);
  }
  delay(60);
}

// ============================================================================
// 5. EEPROM MANAGEMENT
// ============================================================================
void loadCalibration() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, calibData);

  if (calibData.magic != EEPROM_MAGIC) {
    Serial.println("[EEPROM] Initializing defaults...");
    calibData.magic = EEPROM_MAGIC;
    for (int i = 0; i < NUM_CALIB_COLORS; i++) {
      for (int k = 0; k < 3; k++) {
        calibData.front_rgb[i][k] = DEFAULT_FRONT_RGB[i][k];
        calibData.side_rgb[i][k]  = DEFAULT_FRONT_RGB[i][k];
      }
      calibData.front_clear[i] = DEFAULT_FRONT_CLEAR[i];
      calibData.side_clear[i]  = DEFAULT_FRONT_CLEAR[i];
    }
    EEPROM.put(0, calibData);
    EEPROM.commit();
  } else {
    Serial.println("[EEPROM] Loaded calibration data successfully!");
  }
}

void saveCalibration() {
  calibData.magic = EEPROM_MAGIC;
  EEPROM.put(0, calibData);
  EEPROM.commit();
  Serial.println("[EEPROM] Calibration saved to EEPROM!");
}

// ============================================================================
// 6. SENSOR INITIALIZATION & READING
// ============================================================================
uint8_t probeAndInitTCS(Adafruit_TCS34725 &sensor, TwoWire &bus, bool &foundFlag) {
  const uint8_t addrs[] = {0x29, 0x39, 0x49, 0x10};
  for (uint8_t a : addrs) {
    if (sensor.begin(a, &bus)) {
      foundFlag = true;
      sensor.setInterrupt(false); // [CRITICAL FIX] สั่งเปิดไฟ LED สีขาวของบอร์ดเซนเซอร์!
      return a;
    }
  }
  foundFlag = false;
  return 0x29;
}

void setupColorSensors() {
  // Wire: Front Sensor (SDA=4, SCL=5)
  Wire.setSDA(FRONT_SDA);
  Wire.setSCL(FRONT_SCL);
  Wire.begin();
  delay(50);

  tcs_front_addr = probeAndInitTCS(tcsFront, Wire, tcs_front_found);
  if (tcs_front_found) {
    tcsFront.setInterrupt(false); // เปิดไฟ LED
    Serial.printf("[TCS Front] Connected on Wire (SDA=4, SCL=5) Addr: 0x%02X\n", tcs_front_addr);
  } else {
    Serial.println("[TCS Front] Warning: NOT detected on Wire (SDA=4, SCL=5)");
  }

  // Wire1: Side Sensor (SDA=6, SCL=7)
  Wire1.setSDA(SIDE_SDA);
  Wire1.setSCL(SIDE_SCL);
  Wire1.begin();
  delay(30);

  tcs_side_addr = probeAndInitTCS(tcsSide, Wire1, tcs_side_found);
  if (tcs_side_found) {
    tcsSide.setInterrupt(false);
    Serial.printf("[TCS Side] Connected on Wire1 (SDA=6, SCL=7) Addr: 0x%02X\n", tcs_side_addr);
  } else {
    Serial.println("[TCS Side] Note: No sensor on Wire1 (Single sensor mode)");
  }

  // กำหนดเซนเซอร์เริ่มต้น: หากพบเซนเซอร์หน้า (ขา 4, 5) ให้ใช้เป็นหลักเสมอ
  if (tcs_front_found) {
    activeSensor = 0;
  } else if (tcs_side_found) {
    activeSensor = 1;
  } else {
    activeSensor = 0;
  }
}

// โครงสร้างเก็บข้อมูลสีอ่านได้
struct ColorReading {
  uint16_t raw_r;
  uint16_t raw_g;
  uint16_t raw_b;
  uint16_t raw_c;
  int norm_r;
  int norm_g;
  int norm_b;
  int lux;
  int cct;
  bool valid;
};

// [ULTRA FAST & ACCURATE] ฟังก์ชันอ่าน C, R, G, B ตรงจากรีจิสเตอร์แบบ Burst Read (ไม่มี delay บล็อก CPU)
bool readColorNoWait(TwoWire &wireBus, uint8_t addr, uint16_t &r, uint16_t &g, uint16_t &b, uint16_t &c) {
  wireBus.beginTransmission(addr);
  wireBus.write(0xA0 | 0x14); // 0xA0 = Command bit | Auto-increment, 0x14 = CDATAL
  if (wireBus.endTransmission() != 0) return false;
  if (wireBus.requestFrom(addr, (uint8_t)8) != 8) return false;
  c = wireBus.read(); c |= (uint16_t)wireBus.read() << 8;
  r = wireBus.read(); r |= (uint16_t)wireBus.read() << 8;
  g = wireBus.read(); g |= (uint16_t)wireBus.read() << 8;
  b = wireBus.read(); b |= (uint16_t)wireBus.read() << 8;
  return true;
}

// อ่านค่าเซนเซอร์ที่เลือก โดยใช้การอ่านตรงแบบไม่บล็อก + สุ่มตัวอย่าง 3 ครั้งแบบรวดเร็ว
ColorReading readActiveSensor(int sensorIdx) {
  ColorReading res = {0, 0, 0, 0, 0, 0, 0, 0, 0, false};
  TwoWire* bus = (sensorIdx == 0) ? &Wire : &Wire1;
  uint8_t addr = (sensorIdx == 0) ? tcs_front_addr : tcs_side_addr;
  bool found   = (sensorIdx == 0) ? tcs_front_found : tcs_side_found;

  if (!found) return res;

  uint32_t sum_r = 0, sum_g = 0, sum_b = 0, sum_c = 0;
  int validSamples = 0;

  for (int i = 0; i < 3; i++) {
    uint16_t r = 0, g = 0, b = 0, c = 0;
    if (readColorNoWait(*bus, addr, r, g, b, c)) {
      sum_r += r;
      sum_g += g;
      sum_b += b;
      sum_c += c;
      validSamples++;
    }
    delayMicroseconds(500); // ดีเลย์สั้นมากแค่ 0.5ms ไม่บล็อก CPU
  }

  if (validSamples == 0) return res;

  res.raw_r = (uint16_t)(sum_r / validSamples);
  res.raw_g = (uint16_t)(sum_g / validSamples);
  res.raw_b = (uint16_t)(sum_b / validSamples);
  res.raw_c = (uint16_t)(sum_c / validSamples);

  // คำนวณความสว่างรวมที่แท้จริงจาก Clear Channel
  res.lux = res.raw_c; 
  Adafruit_TCS34725* s = (sensorIdx == 0) ? &tcsFront : &tcsSide;
  res.cct = s->calculateColorTemperature(res.raw_r, res.raw_g, res.raw_b);

  float avg = ((float)res.raw_r + (float)res.raw_g + (float)res.raw_b) / 3.0f;
  if (avg < 1.0f) avg = 1.0f;

  res.norm_r = (int)(((float)res.raw_r / avg) * 100.0f);
  res.norm_g = (int)(((float)res.raw_g / avg) * 100.0f);
  res.norm_b = (int)(((float)res.raw_b / avg) * 100.0f);
  res.valid = true;
  return res;
}

// [CRITICAL FIX] จำแนกสีด้วย Euclidean Distance ครอบคลุมทั้ง 5 สี (RED, GREEN, BLUE, YELLOW, WHITE)
// และแยกสีดำ (BLACK) ด้วยระดับความเข้มแสงสะท้อน Clear Channel
RobotColor classifyColor(const ColorReading &rd, int sensorIdx) {
  if (!rd.valid || rd.raw_c == 0) {
    return COLOR_UNKNOWN;
  }

  // ดึงค่าเทียบสีที่บันทึกไว้ใน EEPROM
  int (*ref_rgb)[3] = (sensorIdx == 0) ? calibData.front_rgb : calibData.side_rgb;
  int *ref_clear    = (sensorIdx == 0) ? calibData.front_clear : calibData.side_clear;

  // 1. ตรวจสอบสีดำ (BLACK): วัตถุดูดกลืนแสง ทำให้ Clear Channel ต่ำมาก
  int black_thresh = ref_clear[5] + 120;
  if (black_thresh < 120) black_thresh = 150;
  if (rd.raw_c < black_thresh) {
    return COLOR_BLACK;
  }

  // 2. ตรวจจับแม่สีและสีขาว (0=RED, 1=GREEN, 2=BLUE, 3=YELLOW, 4=WHITE) ด้วย Euclidean Distance
  int bestIdx = -1;
  long minDiff = 1000000;

  for (int c = 0; c < 5; c++) { // ตรวจสอบครบทั้ง 5 สี รวมทั้ง WHITE
    long dr = rd.norm_r - ref_rgb[c][0];
    long dg = rd.norm_g - ref_rgb[c][1];
    long db = rd.norm_b - ref_rgb[c][2];
    long diff = dr * dr + dg * dg + db * db;

    // ถ่วงน้ำหนักพิเศษสำหรับสีเขียว (Green Dominance):
    // ถ้าสีเขียวจริง ค่า G ต้องเด่นกว่า R และ B ชัดเจน
    if (c == 1) { // GREEN
      if (rd.norm_g < rd.norm_r + 20 || rd.norm_g < rd.norm_b + 20) {
        diff += 15000; // หาก G ไม่ได้เด่นจริง จะไม่ให้แมตช์เป็นสีเขียว
      }
    }

    if (diff < minDiff) {
      minDiff = diff;
      bestIdx = c;
    }
  }

  if (bestIdx >= 0) {
    return (RobotColor)(bestIdx + 1); // 1=RED, 2=GREEN, 3=BLUE, 4=YELLOW, 5=WHITE
  }

  return COLOR_UNKNOWN;
}

const char* getColorNameStr(RobotColor c) {
  switch (c) {
    case COLOR_RED:     return "RED";
    case COLOR_GREEN:   return "GREEN";
    case COLOR_BLUE:    return "BLUE";
    case COLOR_YELLOW:  return "YELLOW";
    case COLOR_WHITE:   return "WHITE";
    case COLOR_BLACK:   return "BLACK";
    default:            return "UNKNOWN";
  }
}

uint16_t getColorHexVal(RobotColor c) {
  switch (c) {
    case COLOR_RED:     return ST7735_RED;
    case COLOR_GREEN:   return ST7735_GREEN;
    case COLOR_BLUE:    return ST7735_BLUE;
    case COLOR_YELLOW:  return ST7735_YELLOW;
    case COLOR_WHITE:   return ST7735_WHITE;
    case COLOR_BLACK:   return 0x31A6;
    default:            return 0x52AA;
  }
}

// ============================================================================
// 7. UI DRAWING HELPERS
// ============================================================================
void drawHeader(const char* title, uint16_t color = ST7735_CYAN) {
  tft.fillRect(0, 0, 160, 16, 0x0113);
  tft.drawFastHLine(0, 16, 160, color);
  tft.setCursor(4, 4);
  tft.setTextColor(color);
  tft.setTextSize(1);
  tft.print(title);

  // Sensor badge
  tft.setCursor(118, 4);
  tft.setTextColor(activeSensor == 0 ? ST7735_GREEN : ST7735_YELLOW);
  tft.print(activeSensor == 0 ? "[FRONT]" : "[SIDE]");
}

void drawFooter(const char* hint) {
  tft.fillRect(0, 116, 160, 12, 0x0841);
  tft.drawFastHLine(0, 115, 160, 0x2124);
  tft.setCursor(4, 118);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  tft.print(hint);
}

// ============================================================================
// 8. SCREEN: LIVE COLOR TEST MODE
// ============================================================================
void runLiveColorTest() {
  tft.fillScreen(ST7735_BLACK);
  drawHeader("LIVE COLOR TEST");
  drawFooter("[B3/B1] -> EXIT MENU");

  RobotColor lastDetected = COLOR_UNKNOWN;
  unsigned long lastPrint = 0;

  while (true) {
    if (isBtnOkRaw()) {
      soundClick();
      waitButtonRelease();
      break;
    }

    ColorReading rd = readActiveSensor(activeSensor);
    RobotColor detected = classifyColor(rd, activeSensor);

    if (detected != lastDetected && detected != COLOR_UNKNOWN) {
      soundColorFound(detected);
      lastDetected = detected;
    }

    // 1. Big Color Swatch Card
    uint16_t badgeBg = getColorHexVal(detected);
    uint16_t textCol = (detected == COLOR_WHITE || detected == COLOR_YELLOW) ? ST7735_BLACK : ST7735_WHITE;
    tft.fillRoundRect(6, 22, 148, 28, 4, badgeBg);
    tft.drawRoundRect(6, 22, 148, 28, 4, ST7735_WHITE);

    tft.setTextSize(2);
    tft.setTextColor(textCol, badgeBg);
    const char* colName = getColorNameStr(detected);
    int textX = 80 - (strlen(colName) * 6);
    tft.setCursor(textX, 29);
    tft.print(colName);

    // 2. Normalized RGB Bars & Values
    tft.setTextSize(1);

    // Red Bar
    char rBuf[24]; snprintf(rBuf, sizeof(rBuf), "R:%3d (Raw:%4d)", rd.norm_r, rd.raw_r);
    tft.setTextColor(ST7735_RED, ST7735_BLACK);
    tft.setCursor(6, 54); tft.print(rBuf);
    int barR = map(constrain(rd.norm_r, 0, 200), 0, 200, 0, 140);
    tft.fillRect(6, 63, barR, 4, ST7735_RED);
    tft.fillRect(6 + barR, 63, 140 - barR, 4, 0x18C3);

    // Green Bar
    char gBuf[24]; snprintf(gBuf, sizeof(gBuf), "G:%3d (Raw:%4d)", rd.norm_g, rd.raw_g);
    tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
    tft.setCursor(6, 70); tft.print(gBuf);
    int barG = map(constrain(rd.norm_g, 0, 200), 0, 200, 0, 140);
    tft.fillRect(6, 79, barG, 4, ST7735_GREEN);
    tft.fillRect(6 + barG, 79, 140 - barG, 4, 0x18C3);

    // Blue Bar
    char bBuf[24]; snprintf(bBuf, sizeof(bBuf), "B:%3d (Raw:%4d)", rd.norm_b, rd.raw_b);
    tft.setTextColor(ST7735_BLUE, ST7735_BLACK);
    tft.setCursor(6, 86); tft.print(bBuf);
    int barB = map(constrain(rd.norm_b, 0, 200), 0, 200, 0, 140);
    tft.fillRect(6, 95, barB, 4, ST7735_BLUE);
    tft.fillRect(6 + barB, 95, 140 - barB, 4, 0x18C3);

    // 3. Clear Intensity & CCT
    char metaBuf[32];
    snprintf(metaBuf, sizeof(metaBuf), "Clear:%-5d CCT:%-5dK ", rd.raw_c, rd.cct);
    tft.setTextColor(0x94B2, ST7735_BLACK);
    tft.setCursor(6, 103);
    tft.print(metaBuf);

    // ส่งออกทาง Serial ให้ดูด้วย
    if (millis() - lastPrint >= 200) {
      lastPrint = millis();
      Serial.printf("[TCS] DETECT: %-7s | NORM R:%3d G:%3d B:%3d | RAW R:%4d G:%4d B:%4d C:%5d\n",
                    colName, rd.norm_r, rd.norm_g, rd.norm_b, rd.raw_r, rd.raw_g, rd.raw_b, rd.raw_c);
    }

    delay(30);
  }
}

// ============================================================================
// 9. SCREEN: CALIBRATION MODE (เทียบค่าสีทีละสี)
// ============================================================================
void runCalibrationRoutine() {
  tft.fillScreen(ST7735_BLACK);

  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    tft.fillScreen(ST7735_BLACK);
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "CALIB %s (%d/%d)", COLOR_NAMES[c], c + 1, NUM_CALIB_COLORS);
    drawHeader(hdr, COLOR_HEX[c]);
    drawFooter("PRESS [B3/B1] TO CAPTURE");

    // Color Sample Frame
    tft.fillRoundRect(6, 20, 148, 26, 4, COLOR_HEX[c]);
    tft.drawRoundRect(6, 20, 148, 26, 4, ST7735_WHITE);
    tft.setTextColor((c == 3 || c == 4) ? ST7735_BLACK : ST7735_WHITE, COLOR_HEX[c]);
    tft.setTextSize(1);
    tft.setCursor(12, 28);
    tft.print("วางเซนเซอร์บน: ");
    tft.print(COLOR_NAMES[c]);

    // Live Readout Meter
    while (!isBtnOkRaw()) {
      ColorReading rd = readActiveSensor(activeSensor);

      // Red Bar
      char rBuf[24]; snprintf(rBuf, sizeof(rBuf), "R:%3d (Raw:%4d)", rd.norm_r, rd.raw_r);
      tft.setTextColor(ST7735_RED, ST7735_BLACK);
      tft.setCursor(6, 52); tft.print(rBuf);
      int barR = map(constrain(rd.norm_r, 0, 200), 0, 200, 0, 140);
      tft.fillRect(6, 62, barR, 4, ST7735_RED);
      tft.fillRect(6 + barR, 62, 140 - barR, 4, 0x18C3);

      // Green Bar
      char gBuf[24]; snprintf(gBuf, sizeof(gBuf), "G:%3d (Raw:%4d)", rd.norm_g, rd.raw_g);
      tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
      tft.setCursor(6, 70); tft.print(gBuf);
      int barG = map(constrain(rd.norm_g, 0, 200), 0, 200, 0, 140);
      tft.fillRect(6, 80, barG, 4, ST7735_GREEN);
      tft.fillRect(6 + barG, 80, 140 - barG, 4, 0x18C3);

      // Blue Bar
      char bBuf[24]; snprintf(bBuf, sizeof(bBuf), "B:%3d (Raw:%4d)", rd.norm_b, rd.raw_b);
      tft.setTextColor(ST7735_BLUE, ST7735_BLACK);
      tft.setCursor(6, 88); tft.print(bBuf);
      int barB = map(constrain(rd.norm_b, 0, 200), 0, 200, 0, 140);
      tft.fillRect(6, 98, barB, 4, ST7735_BLUE);
      tft.fillRect(6 + barB, 98, 140 - barB, 4, 0x18C3);

      // Clear Status
      char lBuf[32]; snprintf(lBuf, sizeof(lBuf), "Clear:%-5d CCT:%-5dK ", rd.raw_c, rd.cct);
      tft.setTextColor(0x94B2, ST7735_BLACK);
      tft.setCursor(6, 105); tft.print(lBuf);

      delay(30);
    }

    // Capture Color (อ่านซ้ำเพื่อความแม่นยำสูง)
    soundConfirm();
    ColorReading captured = readActiveSensor(activeSensor);
    if (activeSensor == 0) {
      calibData.front_rgb[c][0] = captured.norm_r;
      calibData.front_rgb[c][1] = captured.norm_g;
      calibData.front_rgb[c][2] = captured.norm_b;
      calibData.front_clear[c]  = captured.raw_c;
    } else {
      calibData.side_rgb[c][0] = captured.norm_r;
      calibData.side_rgb[c][1] = captured.norm_g;
      calibData.side_rgb[c][2] = captured.norm_b;
      calibData.side_clear[c]  = captured.raw_c;
    }

    Serial.printf("[CALIB] %s Saved: NORM R:%d G:%d B:%d (Clear:%d)\n",
                  COLOR_NAMES[c], captured.norm_r, captured.norm_g, captured.norm_b, captured.raw_c);

    waitButtonRelease();
    delay(100);
  }

  // บันทึกลง EEPROM
  saveCalibration();

  // แสดงผลลัพธ์การบันทึก
  tft.fillScreen(ST7735_BLACK);
  drawHeader("CALIB FINISHED!", ST7735_GREEN);
  drawFooter("[B3/B1] -> MENU");

  tft.drawRoundRect(4, 20, 152, 90, 3, 0x0A22);
  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    int y = 24 + (c * 14);
    char buf[32];
    int r = (activeSensor == 0) ? calibData.front_rgb[c][0] : calibData.side_rgb[c][0];
    int g = (activeSensor == 0) ? calibData.front_rgb[c][1] : calibData.side_rgb[c][1];
    int b = (activeSensor == 0) ? calibData.front_rgb[c][2] : calibData.side_rgb[c][2];
    snprintf(buf, sizeof(buf), "%-6s R:%2d G:%2d B:%2d", COLOR_NAMES[c], r, g, b);
    tft.setTextColor(COLOR_HEX[c], ST7735_BLACK);
    tft.setCursor(8, y);
    tft.print(buf);
  }

  soundConfirm();
  while (!isAnyButtonPressed()) delay(10);
  soundClick();
  waitButtonRelease();
}

// ============================================================================
// 10. SCREEN: VIEW SAVED CALIBRATION
// ============================================================================
void runViewCalibTable() {
  tft.fillScreen(ST7735_BLACK);
  drawHeader("SAVED CALIBRATION");
  drawFooter("[B3/B1] -> MENU");

  tft.drawRoundRect(4, 20, 152, 92, 3, 0x2124);
  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    int y = 23 + (c * 15);
    char buf[32];
    int r = (activeSensor == 0) ? calibData.front_rgb[c][0] : calibData.side_rgb[c][0];
    int g = (activeSensor == 0) ? calibData.front_rgb[c][1] : calibData.side_rgb[c][1];
    int b = (activeSensor == 0) ? calibData.front_rgb[c][2] : calibData.side_rgb[c][2];
    snprintf(buf, sizeof(buf), "%-6s R:%3d G:%3d B:%3d", COLOR_NAMES[c], r, g, b);
    tft.setTextColor(COLOR_HEX[c], ST7735_BLACK);
    tft.setCursor(8, y);
    tft.print(buf);
  }

  while (!isAnyButtonPressed()) delay(10);
  soundClick();
  waitButtonRelease();
}

// ============================================================================
// 11. SCREEN: RAW SENSOR INSPECTOR
// ============================================================================
void runRawInspector() {
  tft.fillScreen(ST7735_BLACK);
  drawHeader("RAW TCS INSPECT");
  drawFooter("[B3/B1] -> EXIT MENU");

  while (!isAnyButtonPressed()) {
    ColorReading rd = readActiveSensor(activeSensor);

    char buf[32];
    tft.setTextColor(ST7735_WHITE, ST7735_BLACK);

    snprintf(buf, sizeof(buf), "Raw Clear : %-6u", rd.raw_c);
    tft.setCursor(10, 24); tft.print(buf);

    snprintf(buf, sizeof(buf), "Raw Red   : %-6u", rd.raw_r);
    tft.setTextColor(ST7735_RED, ST7735_BLACK);
    tft.setCursor(10, 40); tft.print(buf);

    snprintf(buf, sizeof(buf), "Raw Green : %-6u", rd.raw_g);
    tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
    tft.setCursor(10, 56); tft.print(buf);

    snprintf(buf, sizeof(buf), "Raw Blue  : %-6u", rd.raw_b);
    tft.setTextColor(ST7735_BLUE, ST7735_BLACK);
    tft.setCursor(10, 72); tft.print(buf);

    tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
    snprintf(buf, sizeof(buf), "Intensity : %-6d", rd.raw_c);
    tft.setCursor(10, 88); tft.print(buf);

    snprintf(buf, sizeof(buf), "Color Temp: %-5d K", rd.cct);
    tft.setCursor(10, 102); tft.print(buf);

    delay(50);
  }

  soundClick();
  waitButtonRelease();
}

// ============================================================================
// 12. SCREEN: SWITCH SENSOR (FRONT / SIDE)
// ============================================================================
void toggleSensor() {
  activeSensor = (activeSensor == 0) ? 1 : 0;
  soundConfirm();
}

// ============================================================================
// 13. MAIN MENU
// ============================================================================
const int TOTAL_MENU_ITEMS = 5;
const char* MENU_ITEMS[TOTAL_MENU_ITEMS] = {
  "1. LIVE COLOR TEST",
  "2. CALIBRATE 6 COLORS",
  "3. VIEW CALIB TABLE",
  "4. RAW INSPECTOR",
  "5. SWITCH FRONT/SIDE"
};

void drawMainMenu(int currentIdx) {
  drawHeader("PUPPYBOT COLOR LAB");
  drawFooter("[B2]:UP  [B4]:DN  [B3/B1]:OK");

  for (int i = 0; i < TOTAL_MENU_ITEMS; i++) {
    int y = 20 + (i * 19);
    bool selected = (i == currentIdx);

    if (selected) {
      tft.fillRoundRect(4, y, 152, 17, 3, 0x028F);
      tft.drawRoundRect(4, y, 152, 17, 3, ST7735_CYAN);
      char buf[32]; snprintf(buf, sizeof(buf), "> %s", MENU_ITEMS[i]);
      tft.setTextColor(ST7735_WHITE, 0x028F);
      tft.setCursor(8, y + 5);
      tft.print(buf);
    } else {
      tft.fillRoundRect(4, y, 152, 17, 3, ST7735_BLACK);
      tft.drawRoundRect(4, y, 152, 17, 3, 0x2124);
      char buf[32]; snprintf(buf, sizeof(buf), "  %s", MENU_ITEMS[i]);
      tft.setTextColor(0x94B2, ST7735_BLACK);
      tft.setCursor(8, y + 5);
      tft.print(buf);
    }
  }
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n========================================");
  Serial.println("  PuppyBot6 Standalone Color Lab Started");
  Serial.println("  I2C Pins: SDA=Pin 4, SCL=Pin 5");
  Serial.println("  Buttons : B1=Pin15, B2=Pin37, B3=Pin38, B4=Pin39");
  Serial.println("========================================");

  // Setup Hardware
  setupButtons();
  pinMode(BUZZER_PIN, OUTPUT);

  // Setup ST7735 Display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3); // แนวนอน 160x128
  tft.fillScreen(ST7735_BLACK);

  // Startup Splash Screen
  tft.fillRoundRect(10, 20, 140, 70, 6, 0x0155);
  tft.drawRoundRect(10, 20, 140, 70, 6, ST7735_CYAN);
  tft.setTextColor(ST7735_WHITE, 0x0155);
  tft.setTextSize(1);
  tft.setCursor(20, 30); tft.print("PUPPYBOT COLOR LAB");
  tft.setTextColor(ST7735_GREEN, 0x0155);
  tft.setCursor(26, 46); tft.print("I2C: SDA=4, SCL=5");
  tft.setTextColor(ST7735_YELLOW, 0x0155);
  tft.setCursor(22, 66); tft.print("EXT BUTTONS: B1-B4");

  beep(1200, 80);
  delay(100);
  beep(1800, 100);

  // Setup Sensors & EEPROM
  setupColorSensors();
  loadCalibration();

  delay(600);
  tft.fillScreen(ST7735_BLACK);
}

int menuIndex = 0;
bool needRedrawMenu = true;

void loop() {
  if (needRedrawMenu) {
    drawMainMenu(menuIndex);
    needRedrawMenu = false;
  }

  // 1. ปุ่ม 2 (Pin 37 - เลื่อนขึ้น UP)
  if (isBtnUpRaw()) {
    soundClick();
    menuIndex--;
    if (menuIndex < 0) menuIndex = TOTAL_MENU_ITEMS - 1;
    needRedrawMenu = true;
    waitButtonRelease();
    return;
  }

  // 2. ปุ่ม 4 (Pin 39 - เลื่อนลง DOWN)
  if (isBtnDownRaw()) {
    soundClick();
    menuIndex++;
    if (menuIndex >= TOTAL_MENU_ITEMS) menuIndex = 0;
    needRedrawMenu = true;
    waitButtonRelease();
    return;
  }

  // 3. ปุ่ม 3 (Pin 38 - ตกลง OK) หรือ ปุ่ม 1 (Pin 15 / ADC 15)
  if (isBtnOkRaw()) {
    soundConfirm();
    waitButtonRelease();

    switch (menuIndex) {
      case 0: runLiveColorTest(); break;
      case 1: runCalibrationRoutine(); break;
      case 2: runViewCalibTable(); break;
      case 3: runRawInspector(); break;
      case 4: toggleSensor(); break;
    }

    tft.fillScreen(ST7735_BLACK);
    needRedrawMenu = true;
    return;
  }

  delay(20);
}
