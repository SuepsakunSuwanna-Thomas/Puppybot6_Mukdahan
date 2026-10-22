#ifndef PUPPYBOT6_EEPROM_CALIB_H
#define PUPPYBOT6_EEPROM_CALIB_H

#include <Arduino.h>
#include <EEPROM.h>
#include <hardware/watchdog.h>
#include <puppybot6.h>
#include "config.h"
#include "sensor_system.h"

// ============================================================================
// รีเซ็ต/รีบูตบอร์ด (Hardware Reboot Microcontroller)
// ============================================================================
inline void rebootBoard() {
  Serial.println(">> Rebooting board...");
  buzzer(2400, 200);
  delay(200);
  watchdog_reboot(0, 0, 0);
  while (1) {
    delay(1);
  }
}

// ============================================================================
// ฟังก์ชันอ่านค่าเฉลี่ยของ ADC Pin
// ============================================================================
inline int readAdcAverage(int pin, int samples = 40) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += ADC(pin);
    delay(5);
  }
  return (int)(sum / samples);
}

// ============================================================================
// บันทึกค่าลง EEPROM (SAVE)
// ============================================================================
inline void saveCalibrationToEEPROM() {
  calib.magic = EEPROM_MAGIC;
  EEPROM.put(EEPROM_ADDR_BASE, calib);
  EEPROM.commit();

  // ส่งค่าที่บันทึกแล้วเข้า library puppybot6
  setSensorMin(calib.front_min);
  setSensorMax(calib.front_max);
  setSensorMin_B(calib.back_min);
  setSensorMax_B(calib.back_max);

  Serial.println(">> Calibration Saved to EEPROM successfully!");
}

// ============================================================================
// โหลดค่าจาก EEPROM (LOAD)
// ============================================================================
inline void loadCalibrationFromEEPROM() {
  EEPROM.get(EEPROM_ADDR_BASE, calib);

  if (calib.magic != EEPROM_MAGIC) {
    Serial.println(">> EEPROM magic mismatch or uninitialized! Setting defaults...");
    calib.magic = EEPROM_MAGIC;
    
    // ค่าตั้งต้นสำหรับเซนเซอร์หน้าและหลัง
    for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
      calib.front_min[i] = 120;
      calib.front_max[i] = 950;
    }
    for (int i = 0; i < NUM_BACK_SENSORS; i++) {
      calib.back_min[i] = 120;
      calib.back_max[i] = 950;
    }

    // ค่าตั้งต้น IR GP2Y0A41SK0F (5cm, 10cm)
    calib.ir_raw_5cm = 620;
    calib.ir_raw_10cm = 330;

    // ค่าตั้งต้นสำหรับ TCS RGB ให้ตรงกับสเกล Read_Color_TCS (สเกล Normalized Average 100)
    // RED
    calib.tcs_front_rgb[0][0] = 180; calib.tcs_front_rgb[0][1] = 60; calib.tcs_front_rgb[0][2] = 60;
    // GREEN
    calib.tcs_front_rgb[1][0] = 60;  calib.tcs_front_rgb[1][1] = 180; calib.tcs_front_rgb[1][2] = 60;
    // BLUE
    calib.tcs_front_rgb[2][0] = 60;  calib.tcs_front_rgb[2][1] = 75;  calib.tcs_front_rgb[2][2] = 165;
    // YELLOW
    calib.tcs_front_rgb[3][0] = 135; calib.tcs_front_rgb[3][1] = 135; calib.tcs_front_rgb[3][2] = 30;
    // WHITE
    calib.tcs_front_rgb[4][0] = 100; calib.tcs_front_rgb[4][1] = 100; calib.tcs_front_rgb[4][2] = 100;
    // BLACK
    calib.tcs_front_rgb[5][0] = 95;  calib.tcs_front_rgb[5][1] = 105; calib.tcs_front_rgb[5][2] = 100;

    for (int i = 0; i < NUM_CALIB_COLORS; i++) {
      for (int k = 0; k < 3; k++) {
        calib.tcs_back_rgb[i][k] = calib.tcs_front_rgb[i][k];
      }
    }

    saveCalibrationToEEPROM();
  } else {
    Serial.println(">> Loaded valid Calibration data from EEPROM!");
    // อัปเกรดสเกลอัตโนมัติหากใน EEPROM เดิมเป็นสเกลเก่า (R < 100 สำหรับสีแดง)
    if (calib.tcs_front_rgb[0][0] < 100) {
      Serial.println(">> Auto-upgrading TCS color reference to calibrated scale...");
      calib.tcs_front_rgb[0][0] = 180; calib.tcs_front_rgb[0][1] = 60; calib.tcs_front_rgb[0][2] = 60;
      calib.tcs_front_rgb[1][0] = 60;  calib.tcs_front_rgb[1][1] = 180; calib.tcs_front_rgb[1][2] = 60;
      calib.tcs_front_rgb[2][0] = 60;  calib.tcs_front_rgb[2][1] = 75;  calib.tcs_front_rgb[2][2] = 165;
      calib.tcs_front_rgb[3][0] = 135; calib.tcs_front_rgb[3][1] = 135; calib.tcs_front_rgb[3][2] = 30;
      calib.tcs_front_rgb[4][0] = 100; calib.tcs_front_rgb[4][1] = 100; calib.tcs_front_rgb[4][2] = 100;
      calib.tcs_front_rgb[5][0] = 95;  calib.tcs_front_rgb[5][1] = 105; calib.tcs_front_rgb[5][2] = 100;
      for (int i = 0; i < NUM_CALIB_COLORS; i++) {
        for (int k = 0; k < 3; k++) {
          calib.tcs_back_rgb[i][k] = calib.tcs_front_rgb[i][k];
        }
      }
      saveCalibrationToEEPROM();
    }
    setSensorMin(calib.front_min);
    setSensorMax(calib.front_max);
    setSensorMin_B(calib.back_min);
    setSensorMax_B(calib.back_max);
  }
}

// เริ่มต้นระบบ EEPROM
inline void initEEPROM() {
  EEPROM.begin(1024);
  loadCalibrationFromEEPROM();
}

// ============================================================================
// 1. คาลิเบรตเซนเซอร์หน้า (Front Sensors ADC 0 - 6)
// ============================================================================
// UI Helper: ส่วนหัวเรื่อง กรอบการ์ด และแถบคำสั่งด้านล่าง (Modern Tech Style)
// ============================================================================
inline void drawHeaderBar(const char* title, uint16_t borderColor = ST7735_CYAN, uint16_t titleColor = ST7735_CYAN) {
  tft_.fillRoundRect(2, 2, 156, 13, 2, 0x0841);
  tft_.drawRoundRect(2, 2, 156, 13, 2, borderColor);
  int len = strlen(title);
  int x = (160 - (len * 6)) / 2;
  if (x < 6) x = 6;
  printText(x, 4, title, 1, titleColor, 0x0841);
}

inline void drawStepBadge(const char* badgeText, uint16_t bgColor, uint16_t textColor, uint16_t borderColor) {
  tft_.fillRoundRect(2, 16, 156, 12, 2, bgColor);
  tft_.drawRoundRect(2, 16, 156, 12, 2, borderColor);
  int len = strlen(badgeText);
  int x = (160 - (len * 6)) / 2;
  if (x < 6) x = 6;
  printText(x, 18, badgeText, 1, textColor, bgColor);
}

inline void drawFooterBar(const char* footerText, uint16_t bgColor = 0x0A22, uint16_t textColor = ST7735_WHITE, uint16_t borderColor = ST7735_GREEN) {
  tft_.fillRoundRect(2, 112, 156, 14, 2, bgColor);
  tft_.drawRoundRect(2, 112, 156, 14, 2, borderColor);
  int len = strlen(footerText);
  int x = (160 - (len * 6)) / 2;
  if (x < 6) x = 6;
  printText(x, 115, footerText, 1, textColor, bgColor);
}

// ============================================================================
// 1. คาลิเบรตเซนเซอร์หน้า (Front Sensors ADC 0 - 6) สไตล์โมเดิร์น
// ============================================================================
inline void calibrateFrontSensors() {
  // -------------------------------------------------------------
  // ขั้นตอนที่ 1: อ่านพื้นสีขาว (White Floor)
  // -------------------------------------------------------------
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("[1/4] FRONT LIGHT CALIB", ST7735_CYAN, ST7735_CYAN);
  drawStepBadge("STEP 1/2: WHITE FLOOR", 0xCE79, ST7735_BLACK, ST7735_WHITE);
  drawFooterBar("PRESS [B3/OK] TO CAPTURE", 0x0A22, ST7735_WHITE, ST7735_GREEN);

  // วาดกรอบมิเตอร์สำหรับ 7 เซนเซอร์ (เว้นระยะด้านบน 8px ไม่ชนกับ Badge)
  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    int y = 34 + (i * 10);
    tft_.drawRect(48, y, 108, 8, 0x2965);
  }

  // วนลูปแสดงผลค่า ADC สดพร้อมแถบเกจวัดแบบเรียลไทม์
  while (!isButtonPressed()) {
    for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
      int y = 34 + (i * 10);
      int raw = ADC(FRONT_PINS[i]);
      char buf[12];
      snprintf(buf, sizeof(buf), "A%d:%4d", i, raw);
      printText(4, y, buf, 1, ST7735_CYAN, ST7735_BLACK);

      int bw = map(constrain(raw, 0, 1023), 0, 1023, 0, 104);
      if (bw > 0) tft_.fillRect(50, y + 2, bw, 4, ST7735_CYAN);
      if (bw < 104) tft_.fillRect(50 + bw, y + 2, 104 - bw, 4, ST7735_BLACK);
    }
    delay(30);
  }

  buzzer(1200, 120);
  while (isButtonPressed()) delay(10);
  delay(80);

  // อ่านค่าเฉลี่ยสีขาว
  int whiteVals[NUM_FRONT_SENSORS];
  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    whiteVals[i] = readAdcAverage(FRONT_PINS[i], 35);
  }

  // -------------------------------------------------------------
  // ขั้นตอนที่ 2: อ่านเส้นสีดำ (Black Line)
  // -------------------------------------------------------------
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("[1/4] FRONT LIGHT CALIB", ST7735_CYAN, ST7735_CYAN);
  drawStepBadge("STEP 2/2: BLACK LINE", 0x2104, ST7735_YELLOW, ST7735_YELLOW);
  drawFooterBar("PRESS [B3/OK] TO CAPTURE", 0x0A22, ST7735_WHITE, ST7735_GREEN);

  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    int y = 34 + (i * 10);
    tft_.drawRect(48, y, 108, 8, 0x2965);
  }

  while (!isButtonPressed()) {
    for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
      int y = 34 + (i * 10);
      int raw = ADC(FRONT_PINS[i]);
      char buf[12];
      snprintf(buf, sizeof(buf), "A%d:%4d", i, raw);
      printText(4, y, buf, 1, ST7735_YELLOW, ST7735_BLACK);

      int bw = map(constrain(raw, 0, 1023), 0, 1023, 0, 104);
      if (bw > 0) tft_.fillRect(50, y + 2, bw, 4, ST7735_YELLOW);
      if (bw < 104) tft_.fillRect(50 + bw, y + 2, 104 - bw, 4, ST7735_BLACK);
    }
    delay(30);
  }

  buzzer(1500, 120);
  while (isButtonPressed()) delay(10);
  delay(80);

  // คำนวณ Min/Max และบันทึก
  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    int bVal = readAdcAverage(FRONT_PINS[i], 35);
    calib.front_min[i] = min(whiteVals[i], bVal);
    calib.front_max[i] = max(whiteVals[i], bVal);
  }
  saveCalibrationToEEPROM();

  // -------------------------------------------------------------
  // ตารางสรุปผลคาลิเบรตเซนเซอร์หน้า (Summary Table Card)
  // -------------------------------------------------------------
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("FRONT SAVED (EEPROM)", ST7735_GREEN, ST7735_GREEN);

  tft_.drawRoundRect(2, 17, 156, 92, 3, 0x03EF);
  printText(6, 20, "CH   WHITE   BLACK   MID", 1, ST7735_CYAN, ST7735_BLACK);
  tft_.drawFastHLine(4, 29, 152, 0x31A6);

  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    int y = 31 + (i * 10);
    char buf[32];
    snprintf(buf, sizeof(buf), "A%d    %4d    %4d   %4d", i, calib.front_min[i], calib.front_max[i], (calib.front_min[i] + calib.front_max[i]) / 2);
    printText(6, y, buf, 1, ST7735_WHITE, ST7735_BLACK);
  }

  drawFooterBar("PRESS [B3/OK] -> MENU", 0x18C3, ST7735_YELLOW, ST7735_YELLOW);
  buzzer(2000, 150);

  while (!isButtonPressed()) delay(10);
  buzzer(1200, 80);
  while (isButtonPressed()) delay(10);
  delay(120);
}

// ============================================================================
// 2. คาลิเบรตเซนเซอร์หลัง (Back Sensors ADC 7 - 13) สไตล์โมเดิร์น
// ============================================================================
inline void calibrateBackSensors() {
  // -------------------------------------------------------------
  // ขั้นตอนที่ 1: อ่านพื้นสีขาว
  // -------------------------------------------------------------
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("[2/4] BACK LIGHT CALIB", ST7735_MAGENTA, ST7735_MAGENTA);
  drawStepBadge("STEP 1/2: WHITE FLOOR", 0xCE79, ST7735_BLACK, ST7735_WHITE);
  drawFooterBar("PRESS [B3/OK] TO CAPTURE", 0x0A22, ST7735_WHITE, ST7735_GREEN);

  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    int y = 34 + (i * 10);
    tft_.drawRect(48, y, 108, 8, 0x2965);
  }

  while (!isButtonPressed()) {
    for (int i = 0; i < NUM_BACK_SENSORS; i++) {
      int y = 34 + (i * 10);
      int raw = ADC(BACK_PINS[i]);
      char buf[12];
      snprintf(buf, sizeof(buf), "A%-2d:%4d", i + 7, raw);
      printText(4, y, buf, 1, ST7735_MAGENTA, ST7735_BLACK);

      int bw = map(constrain(raw, 0, 1023), 0, 1023, 0, 104);
      if (bw > 0) tft_.fillRect(50, y + 2, bw, 4, ST7735_MAGENTA);
      if (bw < 104) tft_.fillRect(50 + bw, y + 2, 104 - bw, 4, ST7735_BLACK);
    }
    delay(30);
  }

  buzzer(1200, 120);
  while (isButtonPressed()) delay(10);
  delay(80);

  int whiteVals[NUM_BACK_SENSORS];
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    whiteVals[i] = readAdcAverage(BACK_PINS[i], 35);
  }

  // -------------------------------------------------------------
  // ขั้นตอนที่ 2: อ่านเส้นสีดำ
  // -------------------------------------------------------------
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("[2/4] BACK LIGHT CALIB", ST7735_MAGENTA, ST7735_MAGENTA);
  drawStepBadge("STEP 2/2: BLACK LINE", 0x2104, ST7735_YELLOW, ST7735_YELLOW);
  drawFooterBar("PRESS [B3/OK] TO CAPTURE", 0x0A22, ST7735_WHITE, ST7735_GREEN);

  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    int y = 34 + (i * 10);
    tft_.drawRect(48, y, 108, 8, 0x2965);
  }

  while (!isButtonPressed()) {
    for (int i = 0; i < NUM_BACK_SENSORS; i++) {
      int y = 34 + (i * 10);
      int raw = ADC(BACK_PINS[i]);
      char buf[12];
      snprintf(buf, sizeof(buf), "A%-2d:%4d", i + 7, raw);
      printText(4, y, buf, 1, ST7735_YELLOW, ST7735_BLACK);

      int bw = map(constrain(raw, 0, 1023), 0, 1023, 0, 104);
      if (bw > 0) tft_.fillRect(50, y + 2, bw, 4, ST7735_YELLOW);
      if (bw < 104) tft_.fillRect(50 + bw, y + 2, 104 - bw, 4, ST7735_BLACK);
    }
    delay(30);
  }

  buzzer(1500, 120);
  while (isButtonPressed()) delay(10);
  delay(80);

  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    int bVal = readAdcAverage(BACK_PINS[i], 35);
    calib.back_min[i] = min(whiteVals[i], bVal);
    calib.back_max[i] = max(whiteVals[i], bVal);
  }
  saveCalibrationToEEPROM();

  // -------------------------------------------------------------
  // ตารางสรุปผลคาลิเบรตเซนเซอร์หลัง
  // -------------------------------------------------------------
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("BACK SAVED (EEPROM)", ST7735_GREEN, ST7735_GREEN);

  tft_.drawRoundRect(2, 17, 156, 92, 3, 0x03EF);
  printText(6, 20, "CH   WHITE   BLACK   MID", 1, ST7735_MAGENTA, ST7735_BLACK);
  tft_.drawFastHLine(4, 29, 152, 0x31A6);

  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    int y = 31 + (i * 10);
    char buf[32];
    snprintf(buf, sizeof(buf), "A%-2d   %4d    %4d   %4d", i + 7, calib.back_min[i], calib.back_max[i], (calib.back_min[i] + calib.back_max[i]) / 2);
    printText(6, y, buf, 1, ST7735_WHITE, ST7735_BLACK);
  }

  drawFooterBar("PRESS [B3/OK] -> MENU", 0x18C3, ST7735_YELLOW, ST7735_YELLOW);
  buzzer(2000, 150);

  while (!isButtonPressed()) delay(10);
  buzzer(1200, 80);
  while (isButtonPressed()) delay(10);
  delay(120);
}

// ============================================================================
// 3. คาลิเบรตเซนเซอร์สี TCS34725 (หน้าและหลัง)
// ============================================================================
inline void calibrateTCSColors() {
  const char* colorNames[NUM_CALIB_COLORS] = {
    "RED", "GREEN", "BLUE", "YELLOW", "WHITE", "BLACK"
  };
  uint16_t colorThemes[NUM_CALIB_COLORS] = {
    ST7735_RED, ST7735_GREEN, ST7735_BLUE, ST7735_YELLOW, ST7735_WHITE, ST7735_MAGENTA
  };

  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    tft_.fillScreen(ST7735_BLACK);
    drawHeaderBar("[3/4] CALIB TCS COLOR", ST7735_CYAN, ST7735_CYAN);

    // Color Swatch Badge
    char titleBuf[24];
    snprintf(titleBuf, sizeof(titleBuf), "COLOR %d/6: %s", c + 1, colorNames[c]);
    drawStepBadge(titleBuf, colorThemes[c], (c == 4 || c == 3) ? ST7735_BLACK : ST7735_WHITE, ST7735_WHITE);
    drawFooterBar("PRESS [B3/OK] TO CAPTURE", 0x0A22, ST7735_WHITE, ST7735_GREEN);

    // กรอบแสดงแถบสี
    tft_.drawRoundRect(6, 34, 148, 72, 3, 0x2965);

    while (!isButtonPressed()) {
      int r = Read_Color_TCS(0);
      int g = Read_Color_TCS(1);
      int b = Read_Color_TCS(2);

      // แถบค่า R, G, B สด
      char rBuf[16], gBuf[16], bBuf[16];
      snprintf(rBuf, sizeof(rBuf), "R:%3d", r);
      snprintf(gBuf, sizeof(gBuf), "G:%3d", g);
      snprintf(bBuf, sizeof(bBuf), "B:%3d", b);
      printText(12, 42, rBuf, 1, ST7735_RED, ST7735_BLACK);
      printText(12, 62, gBuf, 1, ST7735_GREEN, ST7735_BLACK);
      printText(12, 82, bBuf, 1, ST7735_BLUE, ST7735_BLACK);

      // แถบ Meter
      tft_.drawRect(55, 42, 90, 8, 0x2965);
      tft_.drawRect(55, 62, 90, 8, 0x2965);
      tft_.drawRect(55, 82, 90, 8, 0x2965);

      int br = map(constrain(r, 0, 100), 0, 100, 0, 86);
      int bg = map(constrain(g, 0, 100), 0, 100, 0, 86);
      int bb = map(constrain(b, 0, 100), 0, 100, 0, 86);

      if (br > 0) tft_.fillRect(57, 44, br, 4, ST7735_RED);
      if (br < 86) tft_.fillRect(57 + br, 44, 86 - br, 4, ST7735_BLACK);

      if (bg > 0) tft_.fillRect(57, 64, bg, 4, ST7735_GREEN);
      if (bg < 86) tft_.fillRect(57 + bg, 64, 86 - bg, 4, ST7735_BLACK);

      if (bb > 0) tft_.fillRect(57, 84, bb, 4, ST7735_BLUE);
      if (bb < 86) tft_.fillRect(57 + bb, 84, 86 - bb, 4, ST7735_BLACK);

      delay(50);
    }

    buzzer(1200, 120);
    while (isButtonPressed()) delay(10);
    delay(80);

    calib.tcs_front_rgb[c][0] = Read_Color_TCS(0);
    calib.tcs_front_rgb[c][1] = Read_Color_TCS(1);
    calib.tcs_front_rgb[c][2] = Read_Color_TCS(2);
  }

  saveCalibrationToEEPROM();

  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("TCS SAVED (EEPROM)", ST7735_GREEN, ST7735_GREEN);

  tft_.drawRoundRect(2, 17, 156, 92, 3, 0x03EF);
  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    int y = 20 + (c * 14);
    char buf[32];
    snprintf(buf, sizeof(buf), "%-7s R:%2d G:%2d B:%2d", colorNames[c],
             calib.tcs_front_rgb[c][0], calib.tcs_front_rgb[c][1], calib.tcs_front_rgb[c][2]);
    printText(8, y, buf, 1, colorThemes[c], ST7735_BLACK);
  }

  drawFooterBar("PRESS [B3/OK] -> MENU", 0x18C3, ST7735_YELLOW, ST7735_YELLOW);
  buzzer(2000, 150);

  while (!isButtonPressed()) delay(10);
  buzzer(1200, 80);
  while (isButtonPressed()) delay(10);
  delay(120);
}

// ============================================================================
// 4. คาลิเบรตเซนเซอร์วัดระยะห่าง Infrared GP2Y0A41SK0F (ADC 14)
// ============================================================================
inline void calibrateIRDistance() {
  // ขั้นที่ 1: วางวัตถุที่ระยะ 5 ซม.
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("[4/4] CALIB IR DISTANCE", ST7735_CYAN, ST7735_CYAN);
  drawStepBadge("STEP 1/2: OBJECT AT 5CM", 0x2104, ST7735_YELLOW, ST7735_YELLOW);
  drawFooterBar("PRESS [B3/OK] TO READ 5CM", 0x0A22, ST7735_WHITE, ST7735_GREEN);

  while (!isButtonPressed()) {
    int raw = ADC(IR_DISTANCE_PIN);
    char buf[30];
    snprintf(buf, sizeof(buf), "Raw ADC: %4d     ", raw);
    printText(15, 60, buf, 2, ST7735_YELLOW, ST7735_BLACK);
    delay(50);
  }
  buzzer(1200, 120);
  while (isButtonPressed()) delay(10);
  delay(80);
  calib.ir_raw_5cm = readAdcAverage(IR_DISTANCE_PIN, 40);

  // ขั้นที่ 2: วางวัตถุที่ระยะ 10 ซม.
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("[4/4] CALIB IR DISTANCE", ST7735_CYAN, ST7735_CYAN);
  drawStepBadge("STEP 2/2: OBJECT AT 10CM", 0x2104, ST7735_YELLOW, ST7735_YELLOW);
  drawFooterBar("PRESS [B3/OK] TO READ 10CM", 0x0A22, ST7735_WHITE, ST7735_GREEN);

  while (!isButtonPressed()) {
    int raw = ADC(IR_DISTANCE_PIN);
    char buf[30];
    snprintf(buf, sizeof(buf), "Raw ADC: %4d     ", raw);
    printText(15, 60, buf, 2, ST7735_YELLOW, ST7735_BLACK);
    delay(50);
  }
  buzzer(1500, 120);
  while (isButtonPressed()) delay(10);
  delay(80);
  calib.ir_raw_10cm = readAdcAverage(IR_DISTANCE_PIN, 40);

  saveCalibrationToEEPROM();

  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("IR DISTANCE SAVED", ST7735_GREEN, ST7735_GREEN);
  tft_.drawRoundRect(2, 17, 156, 92, 3, 0x03EF);
  
  char buf1[30], buf2[30], buf3[30];
  snprintf(buf1, sizeof(buf1), "Raw @ 5cm  = %d", calib.ir_raw_5cm);
  snprintf(buf2, sizeof(buf2), "Raw @ 10cm = %d", calib.ir_raw_10cm);
  printText(12, 28, buf1, 1, ST7735_WHITE, ST7735_BLACK);
  printText(12, 44, buf2, 1, ST7735_WHITE, ST7735_BLACK);

  float testDist = readDistanceIR();
  snprintf(buf3, sizeof(buf3), "Test: %.1f cm", testDist);
  printText(12, 66, buf3, 2, ST7735_CYAN, ST7735_BLACK);

  drawFooterBar("PRESS [B3/OK] -> MENU", 0x18C3, ST7735_YELLOW, ST7735_YELLOW);
  buzzer(2000, 150);

  while (!isButtonPressed()) delay(10);
  buzzer(1200, 80);
  while (isButtonPressed()) delay(10);
  delay(120);
}

// ============================================================================
// เมนูคาลิเบรตหลัก (Interactive Calibration Menu Dashboard)
// ============================================================================
inline void renderMenuItem(int index, const char* label, bool isSelected) {
  int y = 18 + ((index - 1) * 18);
  if (isSelected) {
    uint16_t borderCol = (index == 5) ? ST7735_GREEN : ST7735_CYAN;
    uint16_t textCol = (index == 5) ? ST7735_GREEN : ST7735_WHITE;
    tft_.fillRoundRect(3, y, 154, 16, 3, 0x028F); // พื้นหลังการ์ดสีน้ำเงินเงางาม
    tft_.drawRoundRect(3, y, 154, 16, 3, borderCol);
    char buf[32];
    snprintf(buf, sizeof(buf), "> %s", label);
    printText(8, y + 4, buf, 1, textCol, 0x028F);
  } else {
    tft_.fillRoundRect(3, y, 154, 16, 3, ST7735_BLACK);
    tft_.drawRoundRect(3, y, 154, 16, 3, 0x2124); // ขอบการ์ดบางสีเทา
    char buf[32];
    snprintf(buf, sizeof(buf), "  %s", label);
    printText(8, y + 4, buf, 1, 0x94B2, ST7735_BLACK);
  }
}

inline void runCalibrationMenu() {
  int currentOption = 1;
  const int totalOptions = 5;
  const char* menuLabels[5] = {
    "1. FRONT SENSORS",
    "2. BACK SENSORS",
    "3. TCS COLOR SENSOR",
    "4. IR DISTANCE",
    "5. EXIT & START ROBOT"
  };

  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("CALIBRATION MENU", ST7735_CYAN, ST7735_WHITE);
  
  if (button_mode == 3) {
    drawFooterBar("[B2]:UP  [B4]:DN  [B3]:OK", 0x0841, ST7735_WHITE, 0x39E7);
  } else {
    drawFooterBar("[Click]:Next  [Hold]:OK", 0x0841, ST7735_WHITE, 0x39E7);
  }

  for (int i = 1; i <= totalOptions; i++) {
    renderMenuItem(i, menuLabels[i - 1], i == currentOption);
  }

  while (true) {
    if (button_mode == 3) {
      // โหมด 3: B2=UP, B4=DOWN, B3=OK
      while (true) {
        if (isBtnUpPressed()) {
          buzzer(800, 30);
          while (isBtnUpPressed()) delay(10);
          int prev = currentOption;
          currentOption--;
          if (currentOption < 1) currentOption = totalOptions;
          renderMenuItem(prev, menuLabels[prev - 1], false);
          renderMenuItem(currentOption, menuLabels[currentOption - 1], true);
          delay(60);
          break;
        }

        if (isBtnDownPressed()) {
          buzzer(800, 30);
          while (isBtnDownPressed()) delay(10);
          int prev = currentOption;
          currentOption++;
          if (currentOption > totalOptions) currentOption = 1;
          renderMenuItem(prev, menuLabels[prev - 1], false);
          renderMenuItem(currentOption, menuLabels[currentOption - 1], true);
          delay(60);
          break;
        }

        if (isBtnOkPressed()) {
          buzzer(2200, 120);
          while (isBtnOkPressed()) delay(10);
          delay(80);

          if (currentOption == 1) {
            calibrateFrontSensors();
          } else if (currentOption == 2) {
            calibrateBackSensors();
          } else if (currentOption == 3) {
            calibrateTCSColors();
          } else if (currentOption == 4) {
            calibrateIRDistance();
          } else if (currentOption == 5) {
            tft_.fillScreen(ST7735_BLACK);
            tft_.fillRoundRect(8, 42, 144, 44, 4, 0x0A22);
            tft_.drawRoundRect(8, 42, 144, 44, 4, ST7735_GREEN);
            printText(16, 56, "STARTING ROBOT...", 1, ST7735_GREEN, 0x0A22);
            buzzer(1500, 120);
            delay(400);
            return;
          }

          // หลังทำรายการเสร็จ วาดหน้าเมนูใหม่
          tft_.fillScreen(ST7735_BLACK);
          drawHeaderBar("== CALIBRATION MENU ==", ST7735_CYAN, ST7735_WHITE);
          drawFooterBar("[B2]:UP  [B4]:DN  [B3]:OK", 0x0841, ST7735_WHITE, 0x39E7);
          for (int i = 1; i <= totalOptions; i++) {
            renderMenuItem(i, menuLabels[i - 1], i == currentOption);
          }
          break;
        }

        delay(10);
      }
    } else {
      // โหมด 0 และ 1: คลิก = Next, ค้าง = OK
      while (!isButtonPressed()) {
        delay(10);
      }

      unsigned long pressStart = millis();
      bool isLong = false;
      while (isButtonPressed()) {
        if (!isLong && (millis() - pressStart >= 600)) {
          isLong = true;
          buzzer(1800, 100);
        }
        delay(10);
      }
      delay(80);

      if (!isLong) {
        int prev = currentOption;
        currentOption++;
        if (currentOption > totalOptions) currentOption = 1;
        buzzer(800, 30);
        renderMenuItem(prev, menuLabels[prev - 1], false);
        renderMenuItem(currentOption, menuLabels[currentOption - 1], true);
      } else {
        buzzer(2200, 120);
        if (currentOption == 1) {
          calibrateFrontSensors();
        } else if (currentOption == 2) {
          calibrateBackSensors();
        } else if (currentOption == 3) {
          calibrateTCSColors();
        } else if (currentOption == 4) {
          calibrateIRDistance();
        } else if (currentOption == 5) {
          tft_.fillScreen(ST7735_BLACK);
          tft_.fillRoundRect(8, 42, 144, 44, 4, 0x0A22);
          tft_.drawRoundRect(8, 42, 144, 44, 4, ST7735_GREEN);
          printText(16, 56, "STARTING ROBOT...", 1, ST7735_GREEN, 0x0A22);
          buzzer(1500, 120);
          delay(400);
          return;
        }

        tft_.fillScreen(ST7735_BLACK);
        drawHeaderBar("== CALIBRATION MENU ==", ST7735_CYAN, ST7735_WHITE);
        drawFooterBar("[Click]:Next  [Hold]:OK", 0x0841, ST7735_WHITE, 0x39E7);
        for (int i = 1; i <= totalOptions; i++) {
          renderMenuItem(i, menuLabels[i - 1], i == currentOption);
        }
      }
    }
    delay(60);
  }
}

// ฟังก์ชันชื่อเดิมเพื่อความเข้ากันได้ย้อนหลัง
inline void autoCalibrate() {
  runCalibrationMenu();
}

#endif // PUPPYBOT6_EEPROM_CALIB_H
