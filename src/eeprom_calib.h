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

    // ค่าตั้งต้นสำหรับ TCS RGB (R, G, B)
    // RED
    calib.tcs_front_rgb[0][0] = 60; calib.tcs_front_rgb[0][1] = 20; calib.tcs_front_rgb[0][2] = 20;
    // GREEN
    calib.tcs_front_rgb[1][0] = 20; calib.tcs_front_rgb[1][1] = 60; calib.tcs_front_rgb[1][2] = 20;
    // BLUE
    calib.tcs_front_rgb[2][0] = 20; calib.tcs_front_rgb[2][1] = 25; calib.tcs_front_rgb[2][2] = 55;
    // YELLOW
    calib.tcs_front_rgb[3][0] = 45; calib.tcs_front_rgb[3][1] = 45; calib.tcs_front_rgb[3][2] = 10;
    // WHITE
    calib.tcs_front_rgb[4][0] = 33; calib.tcs_front_rgb[4][1] = 33; calib.tcs_front_rgb[4][2] = 33;
    // BLACK
    calib.tcs_front_rgb[5][0] = 30; calib.tcs_front_rgb[5][1] = 35; calib.tcs_front_rgb[5][2] = 35;

    for (int i = 0; i < NUM_CALIB_COLORS; i++) {
      for (int k = 0; k < 3; k++) {
        calib.tcs_back_rgb[i][k] = calib.tcs_front_rgb[i][k];
      }
    }

    saveCalibrationToEEPROM();
  } else {
    Serial.println(">> Loaded valid Calibration data from EEPROM!");
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
inline void calibrateFrontSensors() {
  // ขั้นตอนที่ 1: อ่านพื้นสีขาว
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 8, "CALIB FRONT", 2, ST7735_CYAN);
  printText(5, 45, "1. Put on WHITE", 1, ST7735_WHITE);
  printText(5, 75, "Click to Read", 1, ST7735_YELLOW);
  waitForButton("Press to READ");

  // อ่านค่าสีขาว
  int whiteVals[NUM_FRONT_SENSORS];
  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    whiteVals[i] = readAdcAverage(FRONT_PINS[i]);
  }
  // อ่านเสร็จส่งเสียงบัซเซอร์ดัง
  buzzer(1200, 150);

  // แจ้งว่าอ่านขาวเสร็จแล้ว และให้รอกดปุ่มเพื่ออ่านดำ
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 8, "CALIB FRONT", 2, ST7735_CYAN);
  printText(5, 35, "WHITE Done!", 1, ST7735_GREEN);
  printText(5, 60, "2. Put on BLACK", 1, ST7735_WHITE);
  printText(5, 85, "Click to Read", 1, ST7735_YELLOW);
  // รอกดปุ่มก่อนเริ่มอ่านอันถัดไป
  waitForButton("Press to READ");

  // ขั้นตอนที่ 2: อ่านเส้นสีดำ
  int blackVals[NUM_FRONT_SENSORS];
  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    blackVals[i] = readAdcAverage(FRONT_PINS[i]);
  }
  // อ่านเสร็จส่งเสียงบัซเซอร์ดัง
  buzzer(1500, 150);

  // คำนวณ Min และ Max
  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    calib.front_min[i] = min(whiteVals[i], blackVals[i]);
    calib.front_max[i] = max(whiteVals[i], blackVals[i]);
  }

  // บันทึกลง EEPROM
  saveCalibrationToEEPROM();

  // โชว์ค่าที่บันทึกไปทั้งหมดบนจอก่อน แล้วรอกดปุ่มอีกรอบเพื่อรีเซ็ตบอร์ด
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 2, "FRONT SAVED (ADC0-6)", 1, ST7735_GREEN);
  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    String text = "A" + String(i) + " Min:" + String(calib.front_min[i]) + " Max:" + String(calib.front_max[i]);
    printText(5, 15 + (i * 12), text, 1, ST7735_WHITE);
  }
  printText(5, 108, "> Click to REBOOT <", 1, ST7735_YELLOW);
  buzzer(2000, 200);

  // รอกดปุ่มอีกรอบเพื่อรีเซ็ตบอร์ด
  waitForButton("Click to REBOOT");
  rebootBoard();
}

// ============================================================================
// 2. คาลิเบรตเซนเซอร์หลัง (Back Sensors ADC 7 - 13)
// ============================================================================
inline void calibrateBackSensors() {
  // ขั้นตอนที่ 1: อ่านพื้นสีขาว
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 8, "CALIB BACK", 2, ST7735_CYAN);
  printText(5, 45, "1. Put on WHITE", 1, ST7735_WHITE);
  printText(5, 75, "Click to Read", 1, ST7735_YELLOW);
  waitForButton("Press to READ");

  int whiteVals[NUM_BACK_SENSORS];
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    whiteVals[i] = readAdcAverage(BACK_PINS[i]);
  }
  buzzer(1200, 150);

  // แจ้งว่าอ่านขาวเสร็จแล้ว และรอกดปุ่มเพื่ออ่านดำ
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 8, "CALIB BACK", 2, ST7735_CYAN);
  printText(5, 35, "WHITE Done!", 1, ST7735_GREEN);
  printText(5, 60, "2. Put on BLACK", 1, ST7735_WHITE);
  printText(5, 85, "Click to Read", 1, ST7735_YELLOW);
  waitForButton("Press to READ");

  // ขั้นตอนที่ 2: อ่านเส้นสีดำ
  int blackVals[NUM_BACK_SENSORS];
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    blackVals[i] = readAdcAverage(BACK_PINS[i]);
  }
  buzzer(1500, 150);

  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    calib.back_min[i] = min(whiteVals[i], blackVals[i]);
    calib.back_max[i] = max(whiteVals[i], blackVals[i]);
  }

  saveCalibrationToEEPROM();

  // โชว์ค่าที่บันทึกบนหน้าจอ
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 2, "BACK SAVED (ADC7-13)", 1, ST7735_GREEN);
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    String text = "A" + String(i + 7) + " Min:" + String(calib.back_min[i]) + " Max:" + String(calib.back_max[i]);
    printText(5, 15 + (i * 12), text, 1, ST7735_WHITE);
  }
  printText(5, 108, "> Click to REBOOT <", 1, ST7735_YELLOW);
  buzzer(2000, 200);

  waitForButton("Click to REBOOT");
  rebootBoard();
}

// ============================================================================
// 3. คาลิเบรตเซนเซอร์สี TCS34725 (หน้าและหลัง)
// ============================================================================
inline void calibrateTCSColors() {
  const char* colorNames[NUM_CALIB_COLORS] = {
    "RED", "GREEN", "BLUE", "YELLOW", "WHITE", "BLACK"
  };

  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    tft_.fillScreen(ST7735_BLACK);
    printText(5, 5, "CALIB TCS COLOR", 1, ST7735_CYAN);
    printText(5, 25, String("Color [") + (c + 1) + "/6]", 1, ST7735_WHITE);
    printText(5, 45, colorNames[c], 2, ST7735_YELLOW);
    printText(5, 80, "Put sensor & Click", 1, ST7735_WHITE);
    waitForButton("Read Color");

    // อ่านค่าจากเซนเซอร์สีหน้า (TCS Front)
    calib.tcs_front_rgb[c][0] = Read_Color_TCS(0); // R ratio
    calib.tcs_front_rgb[c][1] = Read_Color_TCS(1); // G ratio
    calib.tcs_front_rgb[c][2] = Read_Color_TCS(2); // B ratio

    buzzer(1200, 150);

    // ถ้ารอบนี้ยังไม่ถึงสีสุดท้าย ให้หยุดรอกดปุ่มก่อนอ่านสีถัดไป
    if (c < NUM_CALIB_COLORS - 1) {
      tft_.fillScreen(ST7735_BLACK);
      printText(5, 15, String(colorNames[c]) + " READ OK!", 2, ST7735_GREEN);
      printText(5, 60, String("Next: ") + colorNames[c + 1], 1, ST7735_YELLOW);
      printText(5, 85, "Click to Continue", 1, ST7735_WHITE);
      waitForButton("Click Next");
    }
  }

  saveCalibrationToEEPROM();

  // โชว์ค่าสีที่บันทึกลงจอ
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 2, "TCS COLORS SAVED", 1, ST7735_GREEN);
  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    String text = String(colorNames[c]) + " R:" + String(calib.tcs_front_rgb[c][0]) + 
                  " G:" + String(calib.tcs_front_rgb[c][1]) + 
                  " B:" + String(calib.tcs_front_rgb[c][2]);
    printText(5, 15 + (c * 14), text, 1, ST7735_WHITE);
  }
  printText(5, 108, "> Click to REBOOT <", 1, ST7735_YELLOW);
  buzzer(2000, 200);

  waitForButton("Click to REBOOT");
  rebootBoard();
}

// ============================================================================
// 4. คาลิเบรตเซนเซอร์วัดระยะห่าง Infrared GP2Y0A41SK0F (ADC 14)
// ============================================================================
inline void calibrateIRDistance() {
  // ขั้นที่ 1: วางวัตถุที่ระยะ 5 ซม.
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 10, "CALIB IR DIST", 2, ST7735_CYAN);
  printText(5, 40, "Step 1: Put at 5cm", 1, ST7735_WHITE);
  printText(5, 65, "Target = 5 cm", 2, ST7735_YELLOW);
  printText(5, 95, "Click to Read", 1, ST7735_WHITE);
  waitForButton("Press to READ 5cm");

  calib.ir_raw_5cm = readAdcAverage(IR_DISTANCE_PIN, 50);
  buzzer(1200, 150);

  // แสดงผลว่า 5 ซม. อ่านเสร็จแล้ว และรอกดปุ่มเพื่ออ่าน 10 ซม.
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 10, "CALIB IR DIST", 2, ST7735_CYAN);
  printText(5, 35, "5 cm Done!", 1, ST7735_GREEN);
  printText(5, 60, "Step 2: Put at 10cm", 1, ST7735_WHITE);
  printText(5, 85, "Click to Read", 1, ST7735_YELLOW);
  waitForButton("Press to READ 10cm");

  // ขั้นที่ 2: อ่านค่าระยะ 10 ซม.
  calib.ir_raw_10cm = readAdcAverage(IR_DISTANCE_PIN, 50);
  buzzer(1500, 150);

  saveCalibrationToEEPROM();

  // โชว์ค่าที่บันทึกและระยะทดสอบสด
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 5, "IR DIST SAVED", 2, ST7735_GREEN);
  printText(5, 30, "Raw @ 5cm  = " + String(calib.ir_raw_5cm), 1, ST7735_WHITE);
  printText(5, 45, "Raw @ 10cm = " + String(calib.ir_raw_10cm), 1, ST7735_WHITE);
  
  float testDist = readDistanceIR();
  printText(5, 65, "Test Dist: " + String(testDist, 1) + " cm", 2, ST7735_CYAN);
  printText(5, 108, "> Click to REBOOT <", 1, ST7735_YELLOW);
  buzzer(2000, 200);

  waitForButton("Click to REBOOT");
  rebootBoard();
}

// ============================================================================
// เมนูคาลิเบรตหลัก (Interactive Calibration Menu 1-4)
// เข้าเมนูนี้เมื่อกดปุ่มค้าง (Long Press)
// ============================================================================
inline void runCalibrationMenu() {
  int currentOption = 1;
  const int totalOptions = 5; // 1=หน้า, 2=หลัง, 3=สีTCS, 4=IR Distance, 5=Cancel/Exit

  while (true) {
    tft_.fillScreen(ST7735_BLACK);
    printText(5, 5, "== SELECT CALIB ==", 1, ST7735_CYAN);

    // เมนูเลือกคาลิเบรตทีละอัน
    printText(5, 22, (currentOption == 1) ? "> 1. FRONT SENSOR" : "  1. FRONT SENSOR", 1, 
              (currentOption == 1) ? ST7735_YELLOW : ST7735_WHITE);
    printText(5, 38, (currentOption == 2) ? "> 2. BACK SENSOR" : "  2. BACK SENSOR", 1, 
              (currentOption == 2) ? ST7735_YELLOW : ST7735_WHITE);
    printText(5, 54, (currentOption == 3) ? "> 3. TCS COLORS" : "  3. TCS COLORS", 1, 
              (currentOption == 3) ? ST7735_YELLOW : ST7735_WHITE);
    printText(5, 70, (currentOption == 4) ? "> 4. IR DISTANCE" : "  4. IR DISTANCE", 1, 
              (currentOption == 4) ? ST7735_YELLOW : ST7735_WHITE);
    printText(5, 86, (currentOption == 5) ? "> 5. CANCEL & START" : "  5. CANCEL & START", 1, 
              (currentOption == 5) ? ST7735_GREEN : ST7735_MAGENTA);

    printText(5, 110, "[Click]=Next [Hold]=OK", 1, ST7735_WHITE);

    // รอการกดปุ่ม
    while (!isButtonPressed()) {
      delay(10);
    }

    // วัดระยะเวลาที่กดปุ่ม:
    // กดสั้น (< 600ms) = เลื่อนไปเมนูถัดไป
    // กดยาว (>= 600ms) = เลือกเมนูนั้นทันที
    unsigned long pressStart = millis();
    bool isLong = false;
    while (isButtonPressed()) {
      if (!isLong && (millis() - pressStart >= 600)) {
        isLong = true;
        buzzer(1800, 100); // ปี๊บเตือนทันทีเมื่อกดค้างถึงเวลา
      }
      delay(10);
    }
    delay(80);

    if (!isLong) {
      // กดสั้น: เลื่อนเมนู
      currentOption++;
      if (currentOption > totalOptions) currentOption = 1;
      buzzer(800, 30);
    } else {
      // กดยาว: เข้าไปคาลิเบรตอันนั้น
      buzzer(2200, 120);
      if (currentOption == 1) {
        calibrateFrontSensors(); // พอเสร็จ โชว์ค่า และกดปุ่มจะรีเซ็ตบอร์ด
      } else if (currentOption == 2) {
        calibrateBackSensors();  // พอเสร็จ โชว์ค่า และกดปุ่มจะรีเซ็ตบอร์ด
      } else if (currentOption == 3) {
        calibrateTCSColors();    // พอเสร็จ โชว์ค่า และกดปุ่มจะรีเซ็ตบอร์ด
      } else if (currentOption == 4) {
        calibrateIRDistance();   // พอเสร็จ โชว์ค่า และกดปุ่มจะรีเซ็ตบอร์ด
      } else if (currentOption == 5) {
        // Cancel & Start: ออกจากเมนูไปวิ่งทันทีด้วยค่าเดิม
        tft_.fillScreen(ST7735_BLACK);
        printText(10, 45, "EXITING MENU...", 2, ST7735_WHITE);
        buzzer(1500, 100);
        delay(400);
        break;
      }
    }
    delay(100);
  }
}

// ฟังก์ชันชื่อเดิมเพื่อความเข้ากันได้ย้อนหลัง
inline void autoCalibrate() {
  runCalibrationMenu();
}

#endif // PUPPYBOT6_EEPROM_CALIB_H
