#include <Arduino.h>
#include <puppybot6.h>

int selection_Encoder = 1;
#include "PuppyBotIMU.h"
#include "PuppyBotTurnPID.h"

#include "config.h"
#include "motor_drive.h"
#include "sensor_system.h"
#include "eeprom_calib.h"
#include "line_follow.h"
#include "serial_tester.h"

void setup() {
  delay(1500);
  puppybot6_setup();
  Serial.begin(115200);
  Serial.println("PuppyBot6 Initializing...");

  // 1. กำหนดค่าเริ่มต้นพินเซนเซอร์หน้า 7 ตัว (0-6) และ หลัง 7 ตัว (7-13)
  setup_sensors();

  // 2. โหลดค่าคาลิเบรตทั้งหมดจาก EEPROM (Front, Back, TCS Color, IR Distance)
  initEEPROM();

  // 3. เริ่มต้น Encoder และไจโร
  init_encoder(selection_Encoder);
  set_data_for_turnDirection(1, 20, 70, 3, 5);

  // 4. สั่งเซอร์โวเข้าตำแหน่งเริ่มต้น
  servoRun(1, 145);
  servoRun(2, 62);

  // ==========================================================================
  // [NEW] โหมดทดสอบผ่านหน้าเว็บ (WEB TEST LAB):
  // หากต้องการทดสอบคำสั่งต่างๆ (PID, 4WD Motor, Servo) ผ่านหน้าเว็บ Web Dashboard
  // ให้ยกเลิกคอมเมนต์บรรทัดด้านล่างนี้ (เมื่อเข้าไปแล้วจะวนลูปรับคำสั่งไม่มีที่สิ้นสุด):
  // runSerialTestMode();
  // ==========================================================================

  // 5. แสดงหน้าจอเริ่มต้น และตรวจจับการกดปุ่ม:
  // - กดค้าง (>= 700ms) = เข้าเมนูเลือกคาลิเบรตทีละอัน
  // - กดสั้น (< 700ms)  = เริ่มวิ่งภารกิจทันทีโดยใช้ค่าเดิมที่เคยจำไว้ใน EEPROM
  if (waitForStartOrCalibration()) {
    runCalibrationMenu();
  }
  delay(300);
// Fline_by_pass(30,5);
  // ==========================================================================
  // [TEST CODE] วิ่งจากจุดสตาร์ท -> พอแตะเส้นดำสลับเข้า PID ทันที
  // วิ่งช้าๆ อ่านพัลส์ (Pulse) และ cm จริง พร้อมบันทึกสรุปสีทั้งหมดค้างไว้บนจอ
  // พารามิเตอร์: speed (25-35), max_pulses (พัลส์สูงสุดป้องกันหลุด), max_dist_cm (ระยะ cm)
  // ==========================================================================
  test_start_and_scan_colors(30, 1500, 75.0f);
}

// ============================================================================
// โครงสร้างบันทึกข้อมูลแถบสีที่ตรวจพบ
// ============================================================================
struct ColorRecord {
  char name[12];
  RobotColor color;
  long pulse;
  float cm;
  int r, g, b, lux;
  const char* sensor;
};

// ============================================================================
// ฟังก์ชันทดสอบ: วิ่งจากจุดสตาร์ท -> วิ่งตรงจนเจอเส้น -> สลับเข้า PID ทันที
// พร้อมอ่านค่าพัลส์ (Pulse) และ cm พร้อมทั้งสรุปรายการสีทั้งหมดค้างไว้บนจอ
// ============================================================================
void test_start_and_scan_colors(int speed, long max_pulses, float max_dist_cm) {
  Serial.println(F("\n======================================================="));
  Serial.println(F("  🚀 [TEST] START -> INSTANT PID -> COLOR SCAN & SUMMARY"));
  Serial.println(F("======================================================="));
  Serial.print(F("Speed: ")); Serial.print(speed);
  Serial.print(F(" | Max Pulses: ")); Serial.print(max_pulses);
  Serial.print(F(" | Max Dist: ")); Serial.print(max_dist_cm); Serial.println(F(" cm"));

  // โครงสร้างบันทึกสี
  const int MAX_RECORDS = 12;
  ColorRecord records[MAX_RECORDS];
  int recordCount = 0;

  // เตรียมหน้าจอ TFT แสดงสถานะช่วงค้นหาเส้น
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 4, "TEST: SCAN COLORS", 1, ST7735_CYAN, ST7735_BLACK);
  tft_.drawFastHLine(0, 16, 160, ST7735_BLUE);
  printText(5, 30, "1. DRIVING TO LINE...", 1, ST7735_WHITE, ST7735_BLACK);
  printText(5, 50, "Searching for black line", 1, ST7735_YELLOW, ST7735_BLACK);

  // --------------------------------------------------------------------------
  // ขั้นตอนที่ 1: วิ่งตรงออกจากจุดสตาร์ทจนกว่าจะเจอเส้นดำ -> เริ่มวิ่ง PID ทันที!
  // --------------------------------------------------------------------------
  resetDistance();
  reset_PID_state();
  buzzer(1500, 60);

  // หากเซนเซอร์ยังไม่แตะเส้น ให้วิ่งตรงไปข้างหน้าช้าๆ จนกว่าจะแตะเส้นดำ
  unsigned long search_start = millis();
  while (countFrontSensorsOnLine() == 0) {
    if (millis() - search_start > 8000) { // Safety timeout 8 วินาที
      Serial.println(F("[WARNING] Search line timeout! Stopped."));
      stop_4w();
      buzzer(1000, 400);
      return;
    }
    fd_4w(speed); // วิ่งตรงไปข้างหน้า
    delay(1);
  }

  // >>> วินาทีที่แตะเส้นดำ: หยุดนิ่งสั้นๆ และส่งเสียงบัซเซอร์เตือนดังชัดเจน 2 ปี๊บ! <<<
  // เพื่อให้ทราบทันทีว่า "ตรวจจับเส้นดำได้แล้ว และกำลังเริ่มวิ่งด้วยสมการ PID!"
  stop_4w();
  buzzer(1800, 100);
  delay(50);
  buzzer(2400, 150);
  delay(60);

  // แตะเส้นดำแล้ว: เริ่มต้นวิ่ง PID ตามสมการทันที
  reset_PID_state();
  lineFollow_PID_auto(speed);
  resetDistance(); // เริ่มนับ Encoder pulse และ cm จากจุดที่เริ่มเกาะเส้นนำทาง

  // เตรียมหน้าจอแสดงผลสดระหว่างวิ่ง PID
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 4, "PID TRACKING & SCAN", 1, ST7735_GREEN, ST7735_BLACK);
  tft_.drawFastHLine(0, 16, 160, ST7735_BLUE);

  RobotColor lastFloorColor = COLOR_UNKNOWN;
  RobotColor lastSideColor = COLOR_UNKNOWN;
  long lastRecordPulse = -999;
  unsigned long lastLogTime = 0;
  unsigned long lastScreenTime = 0;

  // --------------------------------------------------------------------------
  // ขั้นตอนที่ 2: วิ่ง PID ตามเส้นแบบช้าๆ พร้อมตรวจวัด Pulse, cm และสแกนแถบสี
  // --------------------------------------------------------------------------
  while (true) {
    // 1. กดปุ่มหยุดฉุกเฉินได้ตลอดเวลา
    if (isButtonPressed()) {
      Serial.println(F(">>> [STOPPED] ผู้ใช้กดปุ่มหยุดหุ่นยนต์"));
      break;
    }

    // 2. วิ่งเกาะเส้นด้วย PID ความเร็วคงที่ ช้านิ่ง
    lineFollow_PID_auto(speed);

    // 3. อ่านค่าระยะพัลส์จริง และ cm (ทั้ง 2 ค่าเพื่อให้เปรียบเทียบได้)
    long curPulse = abs(get_pulse_Encoder(selection_Encoder));
    float curCm = getDistanceCm();
    unsigned long now = millis();

    // เช็คเงื่อนไขระยะสูงสุด (ป้องกันวิ่งเลยหรือหลุด)
    if (curPulse >= max_pulses || curCm >= max_dist_cm) {
      Serial.println(F(">>> [LIMIT REACHED] ครบระยะสูงสุดที่ตั้งไว้"));
      break;
    }

    // 4. อ่านค่าสี Floor Sensor (พื้นสนาม)
    int rF = Read_Color_TCS(0);
    int gF = Read_Color_TCS(1);
    int bF = Read_Color_TCS(2);
    int clrF = Read_Color_TCS(4);
    RobotColor floorCol = detectColorFloor();

    // 5. อ่านค่าสี Side Sensor (ด้านข้าง)
    int rS = Read_Color_TCS_B(0);
    int gS = Read_Color_TCS_B(1);
    int bS = Read_Color_TCS_B(2);
    int clrS = Read_Color_TCS_B(4);
    RobotColor sideCol = detectColorSide();

    // 6. บันทึกแถบสีใหม่ (Floor Sensor): เมื่อตรวจพบสีและอยู่ห่างจากจุดบันทึกก่อนหน้าอย่างน้อย 15 พัลส์
    if (floorCol != COLOR_UNKNOWN && floorCol != lastFloorColor && (curPulse - lastRecordPulse >= 15)) {
      if (recordCount < MAX_RECORDS) {
        strncpy(records[recordCount].name, getColorName(floorCol), sizeof(records[recordCount].name));
        records[recordCount].color = floorCol;
        records[recordCount].pulse = curPulse;
        records[recordCount].cm = curCm;
        records[recordCount].r = rF;
        records[recordCount].g = gF;
        records[recordCount].b = bF;
        records[recordCount].lux = clrF;
        records[recordCount].sensor = "Floor";
        recordCount++;

        lastRecordPulse = curPulse;
        lastFloorColor = floorCol;

        buzzer(2400, 20); // Beep สั้นๆ ไม่หน่วง PID

        Serial.println(F("-------------------------------------------------------"));
        Serial.print(F("🎯 [NEW COLOR #")); Serial.print(recordCount);
        Serial.print(F("] สี: ")); Serial.print(getColorName(floorCol));
        Serial.print(F(" | Pulse: ")); Serial.print(curPulse);
        Serial.print(F(" p | ระยะ: ")); Serial.print(curCm, 1);
        Serial.print(F(" cm | RGB: ("));
        Serial.print(rF); Serial.print(F(", "));
        Serial.print(gF); Serial.print(F(", "));
        Serial.print(bF); Serial.print(F(") Clear: "));
        Serial.println(clrF);
        Serial.println(F("-------------------------------------------------------"));
      }
    } else if (floorCol != lastFloorColor) {
      lastFloorColor = floorCol;
    }

    // ตรวจสอบ Side Sensor (กรณีเซนเซอร์สีติดด้านข้าง)
    if (sideCol != COLOR_UNKNOWN && sideCol != lastSideColor && (curPulse - lastRecordPulse >= 15)) {
      if (recordCount < MAX_RECORDS) {
        strncpy(records[recordCount].name, getColorName(sideCol), sizeof(records[recordCount].name));
        records[recordCount].color = sideCol;
        records[recordCount].pulse = curPulse;
        records[recordCount].cm = curCm;
        records[recordCount].r = rS;
        records[recordCount].g = gS;
        records[recordCount].b = bS;
        records[recordCount].lux = clrS;
        records[recordCount].sensor = "Side";
        recordCount++;

        lastRecordPulse = curPulse;
        lastSideColor = sideCol;

        buzzer(2600, 20);

        Serial.print(F("👉 [SIDE COLOR #")); Serial.print(recordCount);
        Serial.print(F("] สี: ")); Serial.print(getColorName(sideCol));
        Serial.print(F(" | Pulse: ")); Serial.print(curPulse);
        Serial.print(F(" p | cm: ")); Serial.print(curCm, 1);
        Serial.println(F(" cm"));
      }
    } else if (sideCol != lastSideColor) {
      lastSideColor = sideCol;
    }

    // หากอ่านครบ 6 แถบสี และวิ่งพ้นแถบสุดท้ายไปสักระยะ (~60 พัลส์) ให้หยุดอัตโนมัติ
    if (recordCount >= 6 && (curPulse - records[5].pulse > 60)) {
      Serial.println(F(">>> [ALL 6 COLORS FOUND] ตรวจพบครบทั้ง 6 แถบสีแล้ว!"));
      break;
    }

    // 7. พิมพ์ Telemetry ออก Serial ทุก 80 ms
    if (now - lastLogTime >= 80) {
      lastLogTime = now;
      Serial.print(F("[LIVE] Pulse: "));
      Serial.print(curPulse);
      Serial.print(F(" p ("));
      Serial.print(curCm, 1);
      Serial.print(F(" cm) | Floor: "));
      Serial.print(getColorName(floorCol));
      Serial.print(F(" (R:")); Serial.print(rF);
      Serial.print(F(" G:")); Serial.print(gF);
      Serial.print(F(" B:")); Serial.print(bF);
      Serial.print(F(") | Found: "));
      Serial.print(recordCount);
      Serial.println(F(" colors"));
    }

    // 8. อัปเดตหน้าจอ TFT LCD สดๆ ทุก 120 ms
    if (now - lastScreenTime >= 120) {
      lastScreenTime = now;
      char pBuf[32], cBuf[32], cntBuf[32];
      snprintf(pBuf, sizeof(pBuf), "Pulse: %-5ld (%4.1fcm)", curPulse, curCm);
      snprintf(cBuf, sizeof(cBuf), "Color: %-7s", getColorName(floorCol));
      snprintf(cntBuf, sizeof(cntBuf), "Recorded: %d strips", recordCount);

      printText(5, 26, pBuf, 1, ST7735_GREEN, ST7735_BLACK);
      printText(5, 46, cBuf, 2, ST7735_YELLOW, ST7735_BLACK);
      printText(5, 72, cntBuf, 1, ST7735_CYAN, ST7735_BLACK);
      printText(5, 96, "Press Btn to FINISH", 1, 0x94B2, ST7735_BLACK);
    }
  }

  // --------------------------------------------------------------------------
  // จบการทดสอบ: หยุดมอเตอร์ 4 ล้อทันที
  // --------------------------------------------------------------------------
  stop_4w();
  buzzer(1600, 120);
  delay(60);
  buzzer(2200, 180);

  // --------------------------------------------------------------------------
  // พิมพ์ตารางสรุปผลทั้งหมดอย่างละเอียดออก Serial Monitor
  // --------------------------------------------------------------------------
  Serial.println(F("\n========================================================================"));
  Serial.println(F("📊 สรุปผลการสแกนแถบสีและระยะทาง (COLOR & ENCODER SUMMARY)"));
  Serial.println(F("========================================================================"));
  Serial.println(F(" #  | สีที่ตรวจพบ  |  พัลส์ (Pulse)  | ระยะทาง (cm) | ค่าสี RGB (R,G,B) | เซนเซอร์"));
  Serial.println(F("----+------------+-----------------+--------------+-------------------+---------"));
  for (int i = 0; i < recordCount; i++) {
    char sLine[90];
    snprintf(sLine, sizeof(sLine), " %-2d | %-10s | %6ld pulses | %6.1f cm   | (%3d, %3d, %3d)   | %s",
             i + 1, records[i].name, records[i].pulse, records[i].cm,
             records[i].r, records[i].g, records[i].b, records[i].sensor);
    Serial.println(sLine);
  }
  Serial.println(F("========================================================================"));
  Serial.println(F("💡 สามารถนำตัวเลขพัลส์ (Pulse) หรือ cm ในตารางนี้ไปสร้าง void ใช้งานจริงได้เลย!"));
  Serial.println(F("========================================================================\n"));

  // --------------------------------------------------------------------------
  // แสดงผลตารางสรุปค้างไว้บนหน้าจอ TFT LCD (ไม่เด้งหาย ค้างไว้จนกว่าจะกดปุ่ม)
  // --------------------------------------------------------------------------
  tft_.fillScreen(ST7735_BLACK);
  tft_.fillRect(0, 0, 160, 14, 0x0A22); // Header สีเขียวเข้ม
  printText(4, 3, "COLOR SCAN SUMMARY", 1, ST7735_GREEN, 0x0A22);

  if (recordCount == 0) {
    printText(10, 35, "No color detected!", 1, ST7735_RED, ST7735_BLACK);
    printText(10, 55, "Check TCS Sensor /", 1, ST7735_WHITE, ST7735_BLACK);
    printText(10, 70, "Calibration.", 1, ST7735_WHITE, ST7735_BLACK);
  } else {
    int y = 17;
    for (int i = 0; i < recordCount && i < 7; i++) {
      uint16_t rowColor = ST7735_WHITE;
      if (records[i].color == COLOR_RED) rowColor = ST7735_RED;
      else if (records[i].color == COLOR_GREEN) rowColor = ST7735_GREEN;
      else if (records[i].color == COLOR_BLUE) rowColor = ST7735_CYAN;
      else if (records[i].color == COLOR_YELLOW) rowColor = ST7735_YELLOW;
      else if (records[i].color == COLOR_BLACK) rowColor = 0x8410; // Dark Gray

      char itemBuf[32];
      // รูปแบบ: "1.RED    185p 14.2cm"
      snprintf(itemBuf, sizeof(itemBuf), "%d.%-6s %4ldp %4.1fcm",
               i + 1, records[i].name, records[i].pulse, records[i].cm);
      printText(2, y, itemBuf, 1, rowColor, ST7735_BLACK);
      y += 13;
    }
  }

  // Footer แสดงจำนวนสีที่พบ และปุ่มกด
  tft_.fillRect(0, 114, 160, 14, 0x0841);
  char footBuf[32];
  snprintf(footBuf, sizeof(footBuf), "Found: %d colors  [OK]", recordCount);
  printText(4, 117, footBuf, 1, ST7735_YELLOW, 0x0841);

  // >>> ค้างหน้าจอไว้ตลอดเวลา ไม่มีทางหลุดหรือโดนทับ <<<
  // จนกว่าผู้ใช้จะกดปุ่ม OK/SW อีกครั้ง
  while (!isButtonPressed()) {
    delay(20);
  }
  buzzer(1800, 100);
  while (isButtonPressed()) delay(10);
  delay(100);
}

void loop() {
  static bool screen_init = false;
  // if (!screen_init) {
  //   tft_.fillScreen(ST7735_BLACK);
  //   if (button_mode == 3) {
  //     printText(5, 4, "MONITOR: [B3]=CALIB", 1, ST7735_YELLOW, ST7735_BLACK);
  //   } else {
  //     printText(5, 4, "MONITOR: [Hold]=CALIB", 1, ST7735_YELLOW, ST7735_BLACK);
  //   }
  //   tft_.drawFastHLine(0, 15, 160, ST7735_BLUE);
  //   screen_init = true;
  // }

  // // แสดงผลค่าแสงเซนเซอร์หน้าและหลังแบบ Real-time บนหน้าจอ
  // displayLightSensorsFrontAndBack();

  // // หากกดปุ่มจะเข้าสู่เมนูคาลิเบรตได้ทันที
  // if (checkLongPress(1200)) {
  //   runCalibrationMenu();
  //   screen_init = false; // หลังออกจากเมนู ให้วาดหัวข้อหน้าจอใหม่
  // }

  // delay(100);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ห้ามแก้ไข
// ห้ามแก้ไข
// ห้ามแก้ไข
// ห้ามแก้ไข
// ห้ามแก้ไข
// ฟังก์ชันสั่งอ่านค่าสีดิบเซนเซอร์หลังเมื่อต้องการใช้งาน (On-Demand Call)
inline void update_TCS_B() {
  TCS_R = Read_Color_TCS_B_Raw(0);
  TCS_G = Read_Color_TCS_B_Raw(1);
  TCS_B = Read_Color_TCS_B_Raw(2);
  TCS_T = Read_Color_TCS_B_Raw(3);
  TCS_L = Read_Color_TCS_B_Raw(4);
}

void setup1(){
  delay(1000);
  initIMU();
  set_TCS_back();
  updateIMU_fine_offset_Yaw(200);
  offset_yaw_loop1 = getOffsetYaw();
}
void loop1(){
  yaw_loop1 = getYaw();
  Con_yaw_loop1 = getContinuousYaw();
  if(status_resetYaw == 1){
    status_resetYaw = 0;
    resetContinuousYaw();
  }
}

