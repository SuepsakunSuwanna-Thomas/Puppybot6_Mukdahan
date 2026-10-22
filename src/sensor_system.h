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
  setFrontLineColor(0); // 0 = เส้นดำ พื้นขาว (Black line on white floor, map ADC min->100, max->0)

  // ตั้งค่าเซนเซอร์หลัง 7 ตัว (ADC 7 ถึง 13)
  setSensorPins_B(BACK_PINS, NUM_BACK_SENSORS);
  setBackLineColor(0); // 0 = เส้นดำ พื้นขาว (Black line on white floor, map ADC min->100, max->0)

  // กำหนดโหมดปุ่มบนบอร์ด
  pinMode(ONBOARD_BUTTON_PIN, INPUT_PULLUP);

  // กรณีเป็น button_mode 3 (ปุ่มภายนอก 4 ตัว: 1=ADC15, 2=37, 3=38, 4=39)
  // ปลด Servo 4 เพื่อป้องกันไม่ให้สัญญาณชนกับปุ่มกดขา 37, 38, 39
  if (button_mode == 3) {
    servo4.detach();
    pinMode(BTN_UP_PIN, INPUT_PULLUP);   // ขา 37 = เลื่อนเมนูขึ้น
    pinMode(BTN_OK_PIN, INPUT_PULLUP);   // ขา 38 = เข้าคาลิเบรต / ตกลง
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP); // ขา 39 = เลื่อนเมนูลง
  }
}

// ============================================================================
// 2. การจัดการปุ่มกด (Button Management)
// ============================================================================
// ตรวจสอบสถานะการกดของแต่ละปุ่ม:
inline bool isBtnStartPressed() {
  if (button_mode == 3 || button_mode == 1) {
    return (ADC(EXTERNAL_BUTTON_PIN) < EXTERNAL_BUTTON_PRESS_THRESHOLD);
  } else {
    return (digitalRead(ONBOARD_BUTTON_PIN) == LOW);
  }
}

inline bool isBtnUpPressed() {
  if (button_mode == 3) {
    return (digitalRead(BTN_UP_PIN) == LOW);
  }
  return false;
}

inline bool isBtnOkPressed() {
  if (button_mode == 3) {
    return (digitalRead(BTN_OK_PIN) == LOW);
  }
  return isBtnStartPressed();
}

inline bool isBtnDownPressed() {
  if (button_mode == 3) {
    return (digitalRead(BTN_DOWN_PIN) == LOW);
  }
  return false;
}

// button_mode: 0 = ปุ่มบนบอร์ด (pin 3), 1 = ปุ่มภายนอก ADC(15) < 500, 3 = ปุ่มภายนอก 4 ตัว
inline bool isButtonPressed() {
  if (button_mode == 3) {
    // ในโหมด 4 ปุ่ม: ปุ่มตกลง (ขา 38) หรือปุ่มเริ่ม (ADC 15)
    return (digitalRead(BTN_OK_PIN) == LOW) || (ADC(EXTERNAL_BUTTON_PIN) < EXTERNAL_BUTTON_PRESS_THRESHOLD);
  } else if (button_mode == 1) {
    return (ADC(EXTERNAL_BUTTON_PIN) < EXTERNAL_BUTTON_PRESS_THRESHOLD);
  } else {
    return (digitalRead(ONBOARD_BUTTON_PIN) == LOW);
  }
}

// รอจนกว่าจะกดปุ่มแล้วปล่อย
inline void waitForButton(const char* promptText = "Press Button") {
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 50, promptText, 2, ST7735_WHITE);
  
  // แสดงสถานะว่าใช้ปุ่มไหน
  if (button_mode == 3) {
    printText(5, 80, "OK: PIN 38 / B1", 1, ST7735_YELLOW);
  } else if (button_mode == 1) {
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
inline bool isFrontSensorOnLine(int index);
inline bool isBackSensorOnLine(int index);
inline int countFrontSensorsOnLine();
inline int countBackSensorsOnLine();
inline float readDistanceIR();
inline RobotColor detectColorFront();
inline RobotColor detectColorBack();
inline const char* getColorName(RobotColor c);
inline void displayColorSensorsOnly();

// ============================================================================
// แสดงผลค่าแสงเซนเซอร์หน้า 7 ตัว (A0-A6) และหลัง 7 ตัว (A7-A13) แบบ Real-time
// แสดงค่าตัวเลข (0-100) พร้อมแถบกราฟิกสวยงาม ชัดเจน ไม่มีข้อความซ้อนทับ
// ============================================================================
inline void displayLightSensorsFrontAndBack() {
  // หัวข้อคอลัมน์เซนเซอร์หน้าและหลัง
  printText(4, 18,  "FRONT (A0-A6)", 1, ST7735_CYAN, ST7735_BLACK);
  printText(84, 18, "BACK (A7-A13)", 1, ST7735_MAGENTA, ST7735_BLACK);
  tft_.drawFastHLine(0, 27, 160, ST7735_BLUE);

  // วาดค่าและบาร์กราฟิกของเซนเซอร์หน้า 7 ตัว และหลัง 7 ตัว
  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    int y = 29 + (i * 12);

    // --- ด้านหน้า (Front ADC 0 ถึง 6) ---
    int valF = ReadLightSensor(i); // ค่าคำนวณ 0-100%
    bool onF = isFrontSensorOnLine(i);
    char bufF[10];
    snprintf(bufF, sizeof(bufF), "A%d:%3d", i, valF);
    printText(2, y, bufF, 1, onF ? ST7735_GREEN : ST7735_WHITE, ST7735_BLACK);

    int barW_F = map(valF, 0, 100, 0, 26);
    barW_F = constrain(barW_F, 0, 26);
    tft_.fillRect(45, y + 1, barW_F, 6, onF ? ST7735_GREEN : ST7735_CYAN);
    tft_.fillRect(45 + barW_F, y + 1, 26 - barW_F, 6, ST7735_BLACK);
    tft_.drawRect(44, y, 28, 8, ST7735_BLUE);

    // --- ด้านหลัง (Back ADC 7 ถึง 13) ---
    int valB = ReadLightSensor_B(i);
    bool onB = isBackSensorOnLine(i);
    char bufB[10];
    snprintf(bufB, sizeof(bufB), "A%d:%3d", i + 7, valB);
    printText(82, y, bufB, 1, onB ? ST7735_YELLOW : ST7735_WHITE, ST7735_BLACK);

    int barW_B = map(valB, 0, 100, 0, 26);
    barW_B = constrain(barW_B, 0, 26);
    tft_.fillRect(128, y + 1, barW_B, 6, onB ? ST7735_YELLOW : ST7735_MAGENTA);
    tft_.fillRect(128 + barW_B, y + 1, 26 - barW_B, 6, ST7735_BLACK);
    tft_.drawRect(127, y, 28, 8, ST7735_BLUE);
  }

  // เส้นแบ่งแนวตั้งระหว่างเซนเซอร์หน้าและหลัง
  tft_.drawFastVLine(78, 18, 97, ST7735_BLUE);

  // แถบสรุปสถานะด้านล่างสุด
  tft_.drawFastHLine(0, 115, 160, ST7735_BLUE);
  int cntF = countFrontSensorsOnLine();
  int cntB = countBackSensorsOnLine();
  char statusBuf[32];
  snprintf(statusBuf, sizeof(statusBuf), "OnLine F:%d/7  B:%d/7 ", cntF, cntB);
  printText(5, 118, statusBuf, 1, ST7735_YELLOW, ST7735_BLACK);
}

// ============================================================================
// รอเริ่มการทำงาน พร้อมแสดงผลค่าแสงเซ็นเซอร์หน้าและหลังแบบ Real-time บนหน้าจอ
// - โหมด 3 (4 ปุ่ม): ปุ่ม 1 (ADC 15) = เริ่มวิ่งทันที | ปุ่ม 3 (ขา 38) = เข้าเมนูคาลิเบรต
// - โหมด 0/1 (ปุ่มเดี่ยว): กดสั้น (< 700ms) = เริ่มวิ่ง | กดค้าง (>= 700ms) = เข้าเมนูคาลิเบต
// ============================================================================
inline bool waitForStartOrCalibration() {
  tft_.fillScreen(ST7735_BLACK);
  if (button_mode == 3) {
    printText(10, 4, "[B1]=START  [B3]=CALIB", 1, ST7735_YELLOW, ST7735_BLACK);
  } else {
    printText(8, 4, "[Click]=RUN  [Hold]=CALIB", 1, ST7735_YELLOW, ST7735_BLACK);
  }
  tft_.drawFastHLine(0, 15, 160, ST7735_BLUE);

  unsigned long lastUpdate = 0;

  // วนลูปแสดงผลค่าแสงเซนเซอร์หน้าและหลังแบบสดๆ จนกว่าจะมีการกดปุ่ม
  while (true) {
    unsigned long now = millis();
    if (now - lastUpdate >= 100) {
      lastUpdate = now;
      displayLightSensorsFrontAndBack();
    }

    if (button_mode == 3) {
      // 1. ปุ่ม 1 (ADC 15): เริ่มทำงานของหุ่นยนต์อย่างเดียว ไม่เกี่ยวกับคาลิเบรต
      if (isBtnStartPressed()) {
        buzzer(1200, 100);
        while (isBtnStartPressed()) delay(10); // รอปล่อยปุ่ม
        delay(80);
        return false; // เริ่มวิ่งภารกิจทันที!
      }

      // 2. ปุ่ม 3 (ขา 38): กดเข้าหน้าคาลิเบรต
      if (isBtnOkPressed()) {
        buzzer(2000, 150);
        while (isBtnOkPressed()) delay(10); // รอปล่อยปุ่ม
        delay(80);
        return true; // เข้าสู่หน้าเมนูคาลิเบรตทันที!
      }
    } else {
      // โหมด 0 หรือ 1: ทำตามโค้ดเดิม (กดสั้น = วิ่ง, กดค้าง >= 700ms = คาลิเบรต)
      if (isButtonPressed()) {
        unsigned long startPress = millis();
        bool isLong = false;
        while (isButtonPressed()) {
          if (!isLong && (millis() - startPress >= 700)) {
            isLong = true;
            buzzer(2000, 150);
          }
          delay(10);
        }
        delay(80);

        if (isLong) {
          return true; // ผู้ใช้กดค้าง -> เข้าสู่เมนูคาลิเบรต
        } else {
          buzzer(1200, 100);
          return false; // ผู้ใช้กดสั้น -> เริ่มวิ่งภารกิจทันที
        }
      }
    }

    delay(10);
  }
}

// ตรวจสอบการกดปุ่มเพื่อเข้าเมนูคาลิเบรตใน loop()
inline bool checkLongPress(unsigned long hold_ms = 1500) {
  if (button_mode == 3) {
    // โหมด 3: ปุ่ม 3 (ขา 38) คือปุ่มเข้าหน้าคาลิเบรตโดยตรง
    if (isBtnOkPressed()) {
      buzzer(2000, 200);
      while (isBtnOkPressed()) delay(10);
      return true;
    }
    return false;
  }

  // โหมด 0 และ 1: ต้องกดปุ่มค้าง
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

// ตรวจสอบว่าหลุดออกจากเส้นหรือไม่ (เซนเซอร์ไม่เจอเส้นเลยสักตัว = 0)
inline bool isFrontLineLost() {
  return (countFrontSensorsOnLine() == 0);
}

inline bool isBackLineLost() {
  return (countBackSensorsOnLine() == 0);
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
  int lux = Read_Color_TCS(4);

  // หากไม่ได้เชื่อมต่อเซนเซอร์
  if (!tcs_front_found || (r == 0 && g == 0 && b == 0)) {
    return COLOR_UNKNOWN;
  }

  // 1. แยกแยะสีดำ: แสงสะท้อนมืดมาก
  if (lux < 30) {
    return COLOR_BLACK;
  }

  // 2. แยกแยะสีขาว: สว่างสูง และสัดส่วน R, G, B สมดุลใกล้เคียงกัน (ไม่เอียงไปทางสีใด)
  if (lux >= 60 && abs(r - g) <= 25 && abs(g - b) <= 25 && abs(r - b) <= 25) {
    return COLOR_WHITE;
  }

  // 3. ตรวจจับแม่สี (RED, GREEN, BLUE, YELLOW) ด้วยระยะ Euclidean Distance
  int bestColor = COLOR_UNKNOWN;
  long minDiff = 1000000;

  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    // ข้าม White และ Black เพื่อไม่ให้ค่าสมดุลดูดสีแม่สีไป
    if (c == 4 || c == 5) continue;
    long dr = r - calib.tcs_front_rgb[c][0];
    long dg = g - calib.tcs_front_rgb[c][1];
    long db = b - calib.tcs_front_rgb[c][2];
    long diff = dr * dr + dg * dg + db * db;
    if (diff < minDiff) {
      minDiff = diff;
      bestColor = c + 1; // 1-based enum
    }
  }

  // ถ้าสีไม่ชัดเจนและสว่างพอ ให้เป็นสีขาว
  if (minDiff > 12000 && lux >= 60) {
    return COLOR_WHITE;
  }

  return (RobotColor)bestColor;
}

inline RobotColor detectColorBack() {
  if (!tcs_back_found) return COLOR_UNKNOWN;
  int r = Read_Color_TCS_B(0);
  int g = Read_Color_TCS_B(1);
  int b = Read_Color_TCS_B(2);
  int lux = Read_Color_TCS_B(4);

  if (lux < 30) return COLOR_BLACK;
  if (lux >= 60 && abs(r - g) <= 25 && abs(g - b) <= 25 && abs(r - b) <= 25) return COLOR_WHITE;

  int bestColor = COLOR_UNKNOWN;
  long minDiff = 1000000;

  for (int c = 0; c < NUM_CALIB_COLORS; c++) {
    if (c == 4 || c == 5) continue;
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

// ============================================================================
// ชื่อฟังก์ชันระบุตำแหน่งชัดเจน: Floor (พื้นสนาม) และ Side (ยื่นด้านข้าง)
// ============================================================================
inline RobotColor detectColorFloor() { return detectColorFront(); }
inline RobotColor detectColorSide()  { return detectColorBack(); }

inline int Read_Color_TCS_Floor(int ch) { return Read_Color_TCS(ch); }
inline int Read_Color_TCS_Side(int ch)  { return Read_Color_TCS_B(ch); }

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
