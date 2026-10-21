#ifndef PUPPYBOT6_SENSOR_SYSTEM_H
#define PUPPYBOT6_SENSOR_SYSTEM_H

#include <Arduino.h>
#include <puppybot6.h>
#include "config.h"

// ============================================================================
// 1. การเริ่มต้นเซนเซอร์ (Setup Sensors)
// ============================================================================
inline void setup_sensors() {
  // ตั้งค่าเซนเซอร์หน้า 7 ตัว (ADC 0 ถึง 6)
  setSensorPins(FRONT_PINS, NUM_FRONT_SENSORS);
  setFrontLineColor(1); // 1 = เส้นดำ (Black line)

  // ตั้งค่าเซนเซอร์หลัง 7 ตัว (ADC 7 ถึง 13)
  setSensorPins_B(BACK_PINS, NUM_BACK_SENSORS);
  setBackLineColor(1); // 1 = เส้นดำ (Black line)

  // กำหนดโหมดปุ่มบนบอร์ด
  pinMode(ONBOARD_BUTTON_PIN, INPUT_PULLUP);
}

// ============================================================================
// 2. การจัดการปุ่มกด (Button Management)
// ============================================================================
// button_mode: 0 = ปุ่มบนบอร์ด (pin 3), 1 = ปุ่มภายนอก ADC(15) < 500
inline bool isButtonPressed() {
  if (button_mode == 1) {
    // อ่านจากภายนอก ADC(15)
    return (ADC(EXTERNAL_BUTTON_PIN) < EXTERNAL_BUTTON_PRESS_THRESHOLD);
  } else {
    // อ่านจากบนบอร์ด
    return (digitalRead(ONBOARD_BUTTON_PIN) == LOW);
  }
}

// รอจนกว่าจะกดปุ่มแล้วปล่อย
inline void waitForButton(const char* promptText = "Press Button") {
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 50, promptText, 2, ST7735_WHITE);
  
  // แสดงสถานะว่าใช้ปุ่มไหน
  if (button_mode == 1) {
    printText(5, 80, "EXT: ADC15", 1, ST7735_YELLOW);
  } else {
    printText(5, 80, "BOARD: PIN 3", 1, ST7735_YELLOW);
  }

  // รอจนกระทั่งกดปุ่ม
  while (!isButtonPressed()) {
    delay(10);
  }
  buzzer(1200, 80);

  // รอจนกระทั่งปล่อยปุ่ม
  while (isButtonPressed()) {
    delay(10);
  }
  delay(150);
}

// Forward declarations
inline float readDistanceIR();
inline RobotColor detectColorFront();
inline RobotColor detectColorBack();
inline const char* getColorName(RobotColor c);
inline void displayColorSensorsOnly();

// ============================================================================
// แสดงผลเฉพาะค่าสีเซนเซอร์หน้า (Front TCS34725) แบบสดๆ
// ============================================================================
inline void displayColorSensorsOnly() {
  int rF = Read_Color_TCS(0);
  int gF = Read_Color_TCS(1);
  int bF = Read_Color_TCS(2);
  RobotColor colF = detectColorFront();
  
  printText(5, 26, "FRONT COLOR SENSOR:", 1, ST7735_CYAN, ST7735_BLACK);
  String fColName = String(getColorName(colF)) + "        ";
  printText(5, 45, fColName, 2, ST7735_GREEN, ST7735_BLACK);
  String fRgb = "R:" + String(rF) + " G:" + String(gF) + " B:" + String(bF) + "       ";
  printText(5, 75, fRgb, 1, ST7735_WHITE, ST7735_BLACK);
}

// ============================================================================
// รอเริ่มการทำงาน พร้อมแสดงผลค่าสีเซ็นเซอร์หน้าและหลังแบบ Real-time บนหน้าจอ
// - หากไม่มีการกดปุ่มใดๆ: แสดงเฉพาะค่าสีหน้า/หลังไปเรื่อยๆ
// - หากกดสั้น (< 700ms): เริ่มวิ่งภารกิจทันทีด้วยค่าเดิมที่จำไว้ใน EEPROM
// - หากกดค้าง (>= 700ms): เข้าสู่หน้าเมนูเลือกคาลิเบรต
// ============================================================================
inline bool waitForStartOrCalibration() {
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 4, "[Hold]=CALIB  [Click]=START", 1, ST7735_YELLOW);
  tft_.drawFastHLine(0, 16, 160, ST7735_BLUE);

  unsigned long lastUpdate = 0;

  // วนลูปแสดงผลเฉพาะค่าสีเซนเซอร์หน้าและหลังแบบสดๆ จนกว่าจะมีการกดปุ่ม
  while (!isButtonPressed()) {
    unsigned long now = millis();
    if (now - lastUpdate >= 120) {
      lastUpdate = now;
      displayColorSensorsOnly();
    }
    delay(10);
  }

  // ผู้ใช้เริ่มกดปุ่ม -> ตรวจจับระยะเวลาที่กด
  unsigned long startPress = millis();
  bool isLong = false;
  while (isButtonPressed()) {
    if (!isLong && (millis() - startPress >= 700)) {
      isLong = true;
      buzzer(2000, 150); // ส่งเสียงปี๊บเตือนว่าเข้าสู่โหมดกดค้างแล้ว
    }
    delay(10);
  }
  delay(80);

  if (isLong) {
    return true; // ผู้ใช้กดค้าง -> เข้าสู่เมนูคาลิเบรต
  } else {
    buzzer(1200, 100);
    return false; // ผู้ใช้กดสั้น -> เริ่มวิ่งภารกิจทันทีด้วยค่าเดิม
  }
}

// ตรวจสอบการกดปุ่มค้าง (Long Press) เพื่อเข้าเมนูคาลิเบรต
inline bool checkLongPress(unsigned long hold_ms = 1500) {
  if (!isButtonPressed()) return false;

  unsigned long startPress = millis();
  while (isButtonPressed()) {
    if (millis() - startPress >= hold_ms) {
      buzzer(2000, 200);
      while (isButtonPressed()) delay(10); // รอปล่อยปุ่ม
      return true;
    }
    delay(10);
  }
  return false;
}

// ============================================================================
// 3. การตรวจจับเส้นและเส้นตัด (Line & Cross Line Detection)
// ============================================================================
// เซนเซอร์หน้า: index 0 ถึง 6
inline bool isFrontSensorOnLine(int index) {
  if (index < 0 || index >= NUM_FRONT_SENSORS) return false;
  return (ReadLightSensor(index) > _Sensitive);
}

// เซนเซอร์หลัง: index 0 ถึง 6 (ตรงกับ ADC 7 ถึง 13)
inline bool isBackSensorOnLine(int index) {
  if (index < 0 || index >= NUM_BACK_SENSORS) return false;
  return (ReadLightSensor_B(index) > _Sensitive_B);
}

// นับจำนวนเซนเซอร์หน้าที่เห็นเส้นดำ
inline int countFrontSensorsOnLine() {
  int count = 0;
  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    if (isFrontSensorOnLine(i)) count++;
  }
  return count;
}

// นับจำนวนเซนเซอร์หลังที่เห็นเส้นดำ
inline int countBackSensorsOnLine() {
  int count = 0;
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    if (isBackSensorOnLine(i)) count++;
  }
  return count;
}

// Cross Line: อ่านเซนเซอร์ทุกตัว ถ้า >= ครึ่งหนึ่งเห็นเส้นดำ = ผ่านเส้นตัด
// เซนเซอร์ 7 ตัว: ครึ่งหนึ่งคือ >= 4 ตัว
inline bool isFrontCrossing() {
  return (countFrontSensorsOnLine() >= 4);
}

inline bool isBackCrossing() {
  return (countBackSensorsOnLine() >= 4);
}

// ============================================================================
// 4. เซนเซอร์วัดระยะห่าง Infrared GP2Y0A41SK0F (ADC 14)
// ============================================================================
// อ่านค่าดิบ ADC(14)
inline int readRawIR() {
  return ADC(IR_DISTANCE_PIN);
}

// อ่านค่าเฉลี่ยดิบหลายรอบเพื่อความเสถียร
inline int readRawIRAverage(int samples = 15) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += ADC(IR_DISTANCE_PIN);
    delay(3);
  }
  return (int)(sum / samples);
}

// คำนวณและ Return ระยะห่างเป็น "เซนติเมตร" (cm)
// ใช้การแปลงความสัมพันธ์ 1/d ที่แม่นยำตามคุณสมบัติทางแสงของเซนเซอร์ Sharp IR
inline float readDistanceIR() {
  int raw = readRawIRAverage(10);
  
  // ตรวจสอบความถูกต้องของค่า Calibrate ใน EEPROM
  int c5 = calib.ir_raw_5cm;
  int c10 = calib.ir_raw_10cm;

  // หากยังไม่ได้คาลิเบรตหรือค่าผิดปกติ ให้ใช้ค่าตั้งต้นมาตรฐาน
  if (c5 <= 0 || c10 <= 0 || c5 <= c10) {
    c5 = 620;   // ค่าเฉลี่ยทั่วไปของ GP2Y0A41SK0F ที่ 5 ซม.
    c10 = 330;  // ค่าเฉลี่ยทั่วไปของ GP2Y0A41SK0F ที่ 10 ซม.
  }

  // หากระยะไกลเกินหรือวัตถุไม่อยู่ในระยะตรวจจับ
  if (raw <= 60) return 30.0f;

  // Sharp GP2Y0A41SK0F: 1/distance เป็นสัดส่วนเชิงเส้นตรงกับแรงดันไฟฟ้า ADC
  float inv_5 = 1.0f / 5.0f;   // 0.20
  float inv_10 = 1.0f / 10.0f; // 0.10
  
  float slope = (inv_5 - inv_10) / (float)(c5 - c10);
  float inv_d = inv_10 + slope * (float)(raw - c10);

  if (inv_d <= 0.033f) return 30.0f; // เกิน 30 ซม.
  
  float distance_cm = 1.0f / inv_d;
  return constrain(distance_cm, 3.5f, 32.0f);
}

// ตัวแปร return ระยะทาง IR เป็นเซนติเมตร
inline float get_ir_distance_cm() {
  return readDistanceIR();
}

// ============================================================================
// 5. ตัวแปร return ตัวเลขที่เป็น "ซม." ออกมาสำหรับบอกระยะ (Encoder Distance)
// ============================================================================
// แปลง Encoder pulses เป็น เซนติเมตร
inline float encoderToCm(long pulses) {
  if (ENCODER_PULSES_PER_CM <= 0.001f) return (float)pulses;
  return (float)pulses / ENCODER_PULSES_PER_CM;
}

// แปลง เซนติเมตร เป็น Encoder pulses
inline long cmToEncoder(float cm) {
  return (long)(cm * ENCODER_PULSES_PER_CM);
}

// รีเซ็ตค่าระยะทางของ Encoder
inline void resetDistance() {
  resetEncoder(selection_Encoder);
}

// อ่านระยะทางสะสมปัจจุบันเป็น เซนติเมตร (cm)
inline float getDistanceCm() {
  long pulse = abs(get_pulse_Encoder(selection_Encoder));
  return encoderToCm(pulse);
}

// ฟังก์ชันทางเลือกชื่อเดียวกัน
inline float get_distance_cm() {
  return getDistanceCm();
}

// ============================================================================
// 6. ตรวจจับสี (TCS34725 Color Detection)
// ============================================================================
inline RobotColor detectColorFront() {
  int r = Read_Color_TCS(0);
  int g = Read_Color_TCS(1);
  int b = Read_Color_TCS(2);

  int bestColor = COLOR_UNKNOWN;
  long minDiff = 1000000;

  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    long dr = r - calib.tcs_front_rgb[c][0];
    long dg = g - calib.tcs_front_rgb[c][1];
    long db = b - calib.tcs_front_rgb[c][2];
    long diff = dr * dr + dg * dg + db * db;
    if (diff < minDiff) {
      minDiff = diff;
      bestColor = c + 1; // 1-based enum
    }
  }
  return (RobotColor)bestColor;
}

inline RobotColor detectColorBack() {
  int r = Read_Color_TCS_B(0);
  int g = Read_Color_TCS_B(1);
  int b = Read_Color_TCS_B(2);

  int bestColor = COLOR_UNKNOWN;
  long minDiff = 1000000;

  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    long dr = r - calib.tcs_back_rgb[c][0];
    long dg = g - calib.tcs_back_rgb[c][1];
    long db = b - calib.tcs_back_rgb[c][2];
    long diff = dr * dr + dg * dg + db * db;
    if (diff < minDiff) {
      minDiff = diff;
      bestColor = c + 1;
    }
  }
  return (RobotColor)bestColor;
}

inline const char* getColorName(RobotColor c) {
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

#endif // PUPPYBOT6_SENSOR_SYSTEM_H
