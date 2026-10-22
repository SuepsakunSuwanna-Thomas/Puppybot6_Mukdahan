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

    // ค่าตั้งต้น Encoder Pulses per CM
    calib.encoder_pulses_per_cm = 50.0f;
    ENCODER_PULSES_PER_CM = 50.0f;

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

    // ตรวจสอบค่า Encoder Pulses per CM
    if (calib.encoder_pulses_per_cm <= 1.0f || isnan(calib.encoder_pulses_per_cm)) {
      calib.encoder_pulses_per_cm = 50.0f;
      saveCalibrationToEEPROM();
    }
    ENCODER_PULSES_PER_CM = calib.encoder_pulses_per_cm;

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
// 3. คาลิเบรตเซนเซอร์สี TCS34725 (แยกเป็น: สีพื้นสนาม FLOOR และ สียื่นด้านข้าง SIDE)
// ============================================================================
inline void calibrateTCSColorSensor(bool isSideSensor) {
  const char* colorNames[NUM_CALIB_COLORS] = {
    "RED", "GREEN", "BLUE", "YELLOW", "WHITE", "BLACK"
  };
  uint16_t colorThemes[NUM_CALIB_COLORS] = {
    ST7735_RED, ST7735_GREEN, ST7735_BLUE, ST7735_YELLOW, ST7735_WHITE, ST7735_MAGENTA
  };

  const char* headerTitle = isSideSensor ? "CALIB TCS SIDE (CANS)" : "CALIB TCS FLOOR (FLD)";
  const char* placeHint   = isSideSensor ? "SIDE OBJECT/CAN" : "FLOOR SURFACE";

  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    tft_.fillScreen(ST7735_BLACK);
    drawHeaderBar(headerTitle, isSideSensor ? ST7735_YELLOW : ST7735_CYAN, ST7735_WHITE);

    // Color Swatch Badge
    char titleBuf[32];
    snprintf(titleBuf, sizeof(titleBuf), "%s: %s", colorNames[c], placeHint);
    drawStepBadge(titleBuf, colorThemes[c], (c == 4 || c == 3) ? ST7735_BLACK : ST7735_WHITE, ST7735_WHITE);
    drawFooterBar("PRESS [B3/OK] TO CAPTURE", 0x0A22, ST7735_WHITE, ST7735_GREEN);

    // กรอบแสดงแถบสี
    tft_.drawRoundRect(6, 34, 148, 72, 3, 0x2965);

    while (!isButtonPressed()) {
      int r = isSideSensor ? Read_Color_TCS_B(0) : Read_Color_TCS(0);
      int g = isSideSensor ? Read_Color_TCS_B(1) : Read_Color_TCS(1);
      int b = isSideSensor ? Read_Color_TCS_B(2) : Read_Color_TCS(2);

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

    if (isSideSensor) {
      calib.tcs_back_rgb[c][0] = Read_Color_TCS_B(0);
      calib.tcs_back_rgb[c][1] = Read_Color_TCS_B(1);
      calib.tcs_back_rgb[c][2] = Read_Color_TCS_B(2);
    } else {
      calib.tcs_front_rgb[c][0] = Read_Color_TCS(0);
      calib.tcs_front_rgb[c][1] = Read_Color_TCS(1);
      calib.tcs_front_rgb[c][2] = Read_Color_TCS(2);
    }
  }

  saveCalibrationToEEPROM();

  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar(isSideSensor ? "TCS SIDE SAVED!" : "TCS FLOOR SAVED!", ST7735_GREEN, ST7735_GREEN);

  tft_.drawRoundRect(2, 17, 156, 92, 3, 0x03EF);
  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    int y = 20 + (c * 14);
    char buf[32];
    int r = isSideSensor ? calib.tcs_back_rgb[c][0] : calib.tcs_front_rgb[c][0];
    int g = isSideSensor ? calib.tcs_back_rgb[c][1] : calib.tcs_front_rgb[c][1];
    int b = isSideSensor ? calib.tcs_back_rgb[c][2] : calib.tcs_front_rgb[c][2];
    snprintf(buf, sizeof(buf), "%-7s R:%2d G:%2d B:%2d", colorNames[c], r, g, b);
    printText(8, y, buf, 1, colorThemes[c], ST7735_BLACK);
  }

  drawFooterBar("PRESS [B3/OK] -> MENU", 0x18C3, ST7735_YELLOW, ST7735_YELLOW);
  buzzer(2000, 150);

  while (!isButtonPressed()) delay(10);
  buzzer(1200, 80);
  while (isButtonPressed()) delay(10);
  delay(120);
}

// คาลิเบรตเซนเซอร์สีพื้นสนาม (Floor Ground)
inline void calibrateTCSColorsFloor() {
  calibrateTCSColorSensor(false);
}

// คาลิเบรตเซนเซอร์สียื่นด้านข้าง (Side Objects/Cans)
inline void calibrateTCSColorsSide() {
  calibrateTCSColorSensor(true);
}

// เพื่อความเข้ากันได้ย้อนหลัง
inline void calibrateTCSColors() {
  calibrateTCSColorsFloor();
}

// เมนูย่อยเลือกคาลิเบรตเซนเซอร์สี (Floor หรือ Side)
inline void selectTCSColorSubmenu() {
  int currentOption = 1;
  const int totalOptions = 3;
  const char* menuLabels[3] = {
    "1. FLOOR COLOR (พื้น)",
    "2. SIDE COLOR (ข้าง)",
    "3. BACK TO MENU"
  };

  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("SELECT COLOR SENSOR", ST7735_CYAN, ST7735_WHITE);
  if (button_mode == 3) {
    drawFooterBar("[B2]:UP  [B4]:DN  [B3]:OK", 0x0841, ST7735_WHITE, 0x39E7);
  } else {
    drawFooterBar("[Click]:Next  [Hold]:OK", 0x0841, ST7735_WHITE, 0x39E7);
  }

  for (int i = 1; i <= totalOptions; i++) {
    int y = 25 + ((i - 1) * 26);
    if (i == currentOption) {
      tft_.fillRoundRect(4, y, 152, 22, 3, 0x028F);
      tft_.drawRoundRect(4, y, 152, 22, 3, ST7735_CYAN);
      char buf[32]; snprintf(buf, sizeof(buf), "> %s", menuLabels[i - 1]);
      printText(10, y + 6, buf, 1, ST7735_WHITE, 0x028F);
    } else {
      tft_.fillRoundRect(4, y, 152, 22, 3, ST7735_BLACK);
      tft_.drawRoundRect(4, y, 152, 22, 3, 0x2124);
      char buf[32]; snprintf(buf, sizeof(buf), "  %s", menuLabels[i - 1]);
      printText(10, y + 6, buf, 1, 0x94B2, ST7735_BLACK);
    }
  }

  while (true) {
    if (button_mode == 3) {
      if (isBtnUpPressed()) {
        buzzer(800, 30);
        while (isBtnUpPressed()) delay(10);
        currentOption--;
        if (currentOption < 1) currentOption = totalOptions;
        for (int i = 1; i <= totalOptions; i++) {
          int y = 25 + ((i - 1) * 26);
          if (i == currentOption) {
            tft_.fillRoundRect(4, y, 152, 22, 3, 0x028F);
            tft_.drawRoundRect(4, y, 152, 22, 3, ST7735_CYAN);
            char buf[32]; snprintf(buf, sizeof(buf), "> %s", menuLabels[i - 1]);
            printText(10, y + 6, buf, 1, ST7735_WHITE, 0x028F);
          } else {
            tft_.fillRoundRect(4, y, 152, 22, 3, ST7735_BLACK);
            tft_.drawRoundRect(4, y, 152, 22, 3, 0x2124);
            char buf[32]; snprintf(buf, sizeof(buf), "  %s", menuLabels[i - 1]);
            printText(10, y + 6, buf, 1, 0x94B2, ST7735_BLACK);
          }
        }
        delay(60);
      }
      if (isBtnDownPressed()) {
        buzzer(800, 30);
        while (isBtnDownPressed()) delay(10);
        currentOption++;
        if (currentOption > totalOptions) currentOption = 1;
        for (int i = 1; i <= totalOptions; i++) {
          int y = 25 + ((i - 1) * 26);
          if (i == currentOption) {
            tft_.fillRoundRect(4, y, 152, 22, 3, 0x028F);
            tft_.drawRoundRect(4, y, 152, 22, 3, ST7735_CYAN);
            char buf[32]; snprintf(buf, sizeof(buf), "> %s", menuLabels[i - 1]);
            printText(10, y + 6, buf, 1, ST7735_WHITE, 0x028F);
          } else {
            tft_.fillRoundRect(4, y, 152, 22, 3, ST7735_BLACK);
            tft_.drawRoundRect(4, y, 152, 22, 3, 0x2124);
            char buf[32]; snprintf(buf, sizeof(buf), "  %s", menuLabels[i - 1]);
            printText(10, y + 6, buf, 1, 0x94B2, ST7735_BLACK);
          }
        }
        delay(60);
      }
      if (isBtnOkPressed()) {
        buzzer(2200, 120);
        while (isBtnOkPressed()) delay(10);
        delay(80);
        if (currentOption == 1) {
          calibrateTCSColorsFloor();
        } else if (currentOption == 2) {
          calibrateTCSColorsSide();
        }
        return;
      }
      delay(10);
    } else {
      if (isButtonPressed()) {
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
          currentOption++;
          if (currentOption > totalOptions) currentOption = 1;
          buzzer(800, 30);
          for (int i = 1; i <= totalOptions; i++) {
            int y = 25 + ((i - 1) * 26);
            if (i == currentOption) {
              tft_.fillRoundRect(4, y, 152, 22, 3, 0x028F);
              tft_.drawRoundRect(4, y, 152, 22, 3, ST7735_CYAN);
              char buf[32]; snprintf(buf, sizeof(buf), "> %s", menuLabels[i - 1]);
              printText(10, y + 6, buf, 1, ST7735_WHITE, 0x028F);
            } else {
              tft_.fillRoundRect(4, y, 152, 22, 3, ST7735_BLACK);
              tft_.drawRoundRect(4, y, 152, 22, 3, 0x2124);
              char buf[32]; snprintf(buf, sizeof(buf), "  %s", menuLabels[i - 1]);
              printText(10, y + 6, buf, 1, 0x94B2, ST7735_BLACK);
            }
          }
        } else {
          buzzer(2200, 120);
          if (currentOption == 1) {
            calibrateTCSColorsFloor();
          } else if (currentOption == 2) {
            calibrateTCSColorsSide();
          }
          return;
        }
      }
      delay(10);
    }
  }
}
// ============================================================================
// 4. คาลิเบรตตัวเอ็นโค้ดเดอร์ (Encoder Distance Calibration)
// ============================================================================
inline void calibrateEncoderDistance() {
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("[4] ENCODER CALIB", ST7735_GREEN, ST7735_GREEN);

  // คำแนะนำขั้นตอนที่ 1
  tft_.drawRoundRect(4, 18, 152, 90, 3, 0x028F);
  printText(8, 22, "1. PLACE ROBOT AT", 1, ST7735_WHITE, 0x028F);
  printText(8, 34, "   START MARK (0 cm)", 1, ST7735_YELLOW, 0x028F);
  printText(8, 48, "2. PRESS [OK] TO ZERO", 1, ST7735_WHITE, 0x028F);
  printText(8, 60, "   & ROLL 20 CM", 1, ST7735_CYAN, 0x028F);

  char curScaleBuf[32];
  snprintf(curScaleBuf, sizeof(curScaleBuf), "Current: %.1f p/cm", ENCODER_PULSES_PER_CM);
  printText(8, 86, curScaleBuf, 1, 0x94B2, 0x028F);

  drawFooterBar("PRESS [B3/OK] TO ZERO", 0x0A22, ST7735_WHITE, ST7735_GREEN);
  buzzer(1500, 100);

  while (!isButtonPressed()) delay(10);
  buzzer(2000, 120);
  while (isButtonPressed()) delay(10);
  delay(120);

  // เคลียร์ค่าเริ่มต้น
  resetEncoder(selection_Encoder);
  delay(60);

  // ขั้นตอนที่ 2: เข็นหุ่นยนต์ไปข้างหน้า 20 cm
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("ROLL FORWARD 20 CM", ST7735_YELLOW, ST7735_YELLOW);
  drawFooterBar("PRESS [OK] AT 20 CM", 0x0A22, ST7735_WHITE, ST7735_GREEN);

  tft_.drawRoundRect(4, 18, 152, 90, 3, 0x2965);
  printText(8, 22, "TARGET: 20.0 cm", 1, ST7735_YELLOW, ST7735_BLACK);

  long lastPulses = -999999;
  long pulses = 0;

  while (!isButtonPressed()) {
    pulses = abs(get_pulse_Encoder(selection_Encoder));
    if (pulses != lastPulses) {
      lastPulses = pulses;

      char pBuf[32];
      snprintf(pBuf, sizeof(pBuf), "Pulse: %-6ld", pulses);
      printText(8, 38, pBuf, 2, ST7735_GREEN, ST7735_BLACK);

      float calcRatio = (float)pulses / 20.0f;
      char rBuf[32];
      snprintf(rBuf, sizeof(rBuf), "Scale: %.2f p/cm ", calcRatio);
      printText(8, 60, rBuf, 1, ST7735_CYAN, ST7735_BLACK);

      // บาร์กราฟิกความคืบหน้า (ประมาณ 1000 pulse)
      int barW = map(constrain((int)pulses, 0, 1500), 0, 1500, 0, 140);
      tft_.fillRect(8, 76, barW, 6, ST7735_CYAN);
      tft_.fillRect(8 + barW, 76, 140 - barW, 6, ST7735_BLACK);
      tft_.drawRect(7, 75, 142, 8, 0x2965);

      printText(8, 90, "ROLL -> REACH 20 CM", 1, 0x94B2, ST7735_BLACK);
    }
    delay(30);
  }

  buzzer(2200, 150);
  while (isButtonPressed()) delay(10);
  delay(100);

  // บันทึก Scale ใหม่
  if (pulses > 10) {
    float newScale = (float)pulses / 20.0f;
    calib.encoder_pulses_per_cm = newScale;
    ENCODER_PULSES_PER_CM = newScale;
    saveCalibrationToEEPROM();

    tft_.fillScreen(ST7735_BLACK);
    drawHeaderBar("ENCODER SAVED!", ST7735_GREEN, ST7735_GREEN);
    tft_.drawRoundRect(4, 20, 152, 88, 3, 0x0A22);

    printText(10, 26, "CALIBRATION SUCCESS!", 1, ST7735_GREEN, 0x0A22);
    char res1[32], res2[32];
    snprintf(res1, sizeof(res1), "Total: %ld pulses", pulses);
    snprintf(res2, sizeof(res2), "Scale: %.2f p/cm", newScale);
    printText(10, 44, res1, 1, ST7735_WHITE, 0x0A22);
    printText(10, 60, res2, 1, ST7735_YELLOW, 0x0A22);
    printText(10, 78, "Saved to EEPROM!", 1, ST7735_CYAN, 0x0A22);

    buzzer(1800, 100);
    delay(100);
    buzzer(2400, 150);

    drawFooterBar("PRESS [B3/OK] -> MENU", 0x18C3, ST7735_YELLOW, ST7735_YELLOW);
    while (!isButtonPressed()) delay(10);
    while (isButtonPressed()) delay(10);
    delay(120);
  }
}

// ============================================================================
// 5. คาลิเบรตเซนเซอร์วัดระยะห่าง Infrared GP2Y0A41SK0F (ADC 14)
// ============================================================================
inline void calibrateIRDistance() {
  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("[5] CALIB IR DISTANCE", ST7735_CYAN, ST7735_CYAN);
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
  drawHeaderBar("[5] CALIB IR DISTANCE", ST7735_CYAN, ST7735_CYAN);
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
// เมนูคาลิเบรตหลัก (Interactive Calibration Menu Dashboard 6 Options)
// ============================================================================
inline void renderMenuItem(int index, const char* label, bool isSelected) {
  int y = 16 + ((index - 1) * 16);
  if (isSelected) {
    uint16_t borderCol = (index == 6) ? ST7735_GREEN : ST7735_CYAN;
    uint16_t textCol   = (index == 6) ? ST7735_GREEN : ST7735_WHITE;
    tft_.fillRoundRect(3, y, 154, 14, 2, 0x028F); // พื้นหลังการ์ดสีน้ำเงินเงางาม
    tft_.drawRoundRect(3, y, 154, 14, 2, borderCol);
    char buf[32];
    snprintf(buf, sizeof(buf), "> %s", label);
    printText(6, y + 3, buf, 1, textCol, 0x028F);
  } else {
    tft_.fillRoundRect(3, y, 154, 14, 2, ST7735_BLACK);
    tft_.drawRoundRect(3, y, 154, 14, 2, 0x2124); // ขอบการ์ดบางสีเทา
    char buf[32];
    snprintf(buf, sizeof(buf), "  %s", label);
    printText(6, y + 3, buf, 1, 0x94B2, ST7735_BLACK);
  }
}

inline void runCalibrationMenu() {
  int currentOption = 1;
  const int totalOptions = 6;
  const char* menuLabels[6] = {
    "1. FRONT SENSORS",
    "2. BACK SENSORS",
    "3. TCS COLOR (FLR/SIDE)",
    "4. ENCODER CALIB",
    "5. IR DISTANCE SENSOR",
    "6. EXIT & START ROBOT"
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
            selectTCSColorSubmenu();
          } else if (currentOption == 4) {
            calibrateEncoderDistance();
          } else if (currentOption == 5) {
            calibrateIRDistance();
          } else if (currentOption == 6) {
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
          selectTCSColorSubmenu();
        } else if (currentOption == 4) {
          calibrateEncoderDistance();
        } else if (currentOption == 5) {
          calibrateIRDistance();
        } else if (currentOption == 6) {
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
