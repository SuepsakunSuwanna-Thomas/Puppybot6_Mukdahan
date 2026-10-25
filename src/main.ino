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

void test_start_and_scan_colors(int speed = 30);

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

  // เริ่มวิ่งทดสอบตามพัลส์ (ไม่มี PID): หาเส้น -> 682p (สี 1) -> +385p x 5 (สี 2-6)
  test_start_and_scan_colors(30);
}

// ============================================================================
// โครงสร้างบันทึกข้อมูลแถบสีและสรุปผลภารกิจ
// ============================================================================
struct ColorRecord {
  char name[12];
  RobotColor color;
  long pulse;
  float cm;
  int r, g, b, lux;
  const char* sensor;
};

struct MissionSummary {
  bool completed;
  unsigned long duration_ms;
  float distance_cm;
  long pulses;
  int colorCount;
  ColorRecord records[12];
  char finishReason[24];
};

MissionSummary mission_summary = {false, 0, 0.0f, 0, 0, {}, "INIT"};

enum ScreenState {
  SCREEN_COLOR_SUMMARY,
  SCREEN_TIME_SUMMARY
};
ScreenState current_screen_state = SCREEN_COLOR_SUMMARY;
bool screen_drawn = false;

void drawColorSummaryScreen();
void drawMissionTimeOnlyScreen();

// ============================================================================
// ============================================================================
// ฟังก์ชันทดสอบ: วิ่งจากจุดเริ่มตรงไปข้างหน้า (ไม่มี PID) จนเจอเส้น
// จากนั้นวิ่งตรงไป 682 พัลส์ เก็บสีที่ 1, และวิ่งต่ออีกช่วงละ 385 พัลส์ เก็บสีที่ 2 ถึง 6
// ============================================================================
// ============================================================================
// ฟังก์ชันทดสอบ: วิ่งจากจุดเริ่ม -> เจอดำ -> วิ่ง PID ตามพัลส์ 6 ช่องแล้วหยุดอ่านสี
// ============================================================================
void test_start_and_scan_colors(int speed) {
  const long step_pulses[6] = {735, 385, 385, 385, 385, 385};
  long total_pulses = 0;

  mission_summary.colorCount = 0;
  buzzer(1500, 60);

  // 1. วิ่งตรงจนกว่าจะเจอเส้นดำขอบกล่องสตาร์ท
  while (!isFrontSensorOnLine(3)) {
    if (isButtonPressed()) { stop_4w(); return; }
    fd_4w(speed);
    delay(1);
  }

  reset_PID_state();
  resetDistance();

  // 2. วิ่งตามเส้นแบบ PID ไปทีละช่อง (682 แล้วต่อด้วย 385 x 5)
  for (int step = 0; step < 6; step++) {
    long targetP = step_pulses[step];
    total_pulses += targetP;
    long initP = get_pulse_Encoder(selection_Encoder);

    while (abs(get_pulse_Encoder(selection_Encoder) - initP) < targetP) {
      if (isButtonPressed()) { stop_4w(); return; }
      lineFollow_PID_auto(speed);
      delay(1);
    }

    // หยุดสนิท 2 วินาทีเพื่ออ่านค่าสี
    stop_4w();
    buzzer(2400, 50);

    unsigned long t0 = millis();
    while (millis() - t0 < 2000) {
      if (isButtonPressed()) break;
      delay(20);
    }

    RobotColor col = detectColorFloor();
    strncpy(mission_summary.records[step].name, getColorName(col), sizeof(mission_summary.records[step].name));
    mission_summary.records[step].color = col;
    mission_summary.records[step].pulse = total_pulses;
    mission_summary.records[step].cm = getDistanceCm();
    mission_summary.colorCount++;
    buzzer(2000, 60);
  }

  // 3. จบภารกิจ: แสดงหน้าจอสรุปสีทั้งหมดที่อ่านได้อันเดียว
  stop_4w();
  buzzer(1800, 150);
  mission_summary.completed = true;

  drawColorSummaryScreen();
}

// ============================================================================
// ฟังก์ชันวาดหน้าจอสรุปสีทั้งหมดที่ตรวจพบ (ALL SCANNED COLORS)
// ============================================================================
void drawColorSummaryScreen() {
  tft_.fillScreen(ST7735_BLACK);

  // 1. Header Bar ด้านบนสุด
  tft_.fillRoundRect(2, 2, 156, 16, 3, 0x0A22); // Emerald Dark Green
  tft_.drawRoundRect(2, 2, 156, 16, 3, ST7735_GREEN);
  printText(24, 6, "COLOR SCAN SUMMARY", 1, ST7735_GREEN, 0x0A22);

  // 2. แสดงรายการสีทั้งหมดที่บันทึกได้
  int count = mission_summary.colorCount;
  if (count == 0) {
    tft_.fillRoundRect(4, 25, 152, 74, 4, 0x0841);
    tft_.drawRoundRect(4, 25, 152, 74, 4, ST7735_CYAN);
    printText(20, 42, "No color detected!", 1, ST7735_RED, 0x0841);
    printText(14, 60, "Check TCS Sensor & Calib", 1, ST7735_WHITE, 0x0841);
  } else {
    int spacing = (count >= 7) ? 12 : 14;
    int y = (count >= 7) ? 20 : 22;

    for (int i = 0; i < count && i < 7; i++) {
      ColorRecord &rec = mission_summary.records[i];

      uint16_t badgeColor = ST7735_WHITE;
      if (rec.color == COLOR_RED) badgeColor = ST7735_RED;
      else if (rec.color == COLOR_GREEN) badgeColor = ST7735_GREEN;
      else if (rec.color == COLOR_BLUE) badgeColor = ST7735_CYAN;
      else if (rec.color == COLOR_YELLOW) badgeColor = ST7735_YELLOW;
      else if (rec.color == COLOR_BLACK) badgeColor = 0x632C; // Dark Gray

      // วาดสัญลักษณ์จุดสี (Color Badge Box)
      tft_.fillRoundRect(4, y + 1, 8, (spacing > 12 ? 9 : 8), 2, badgeColor);
      tft_.drawRoundRect(4, y + 1, 8, (spacing > 12 ? 9 : 8), 2, ST7735_WHITE);

      // ข้อความ: ลำดับ, ชื่อสี, พัลส์, ระยะทาง cm
      char itemBuf[32];
      snprintf(itemBuf, sizeof(itemBuf), "#%d %-6s %4ldp %4.1fcm",
               i + 1, rec.name, rec.pulse, rec.cm);
      printText(16, y + 1, itemBuf, 1, badgeColor, ST7735_BLACK);

      y += spacing;
    }
  }

  // 3. แถบคำสั่ง Footer ด้านล่างสุด
  tft_.fillRoundRect(2, 107, 156, 18, 3, 0x0841);
  tft_.drawRoundRect(2, 107, 156, 18, 3, ST7735_CYAN);
  if (button_mode == 3) {
    printText(17, 112, "[OK]=TIME  [B1]=START", 1, ST7735_YELLOW, 0x0841);
  } else {
    printText(14, 112, "PRESS [BTN] FOR TIME >", 1, ST7735_YELLOW, 0x0841);
  }
}

// ============================================================================
// ฟังก์ชันวาดหน้าจอแสดงผลเวลาภารกิจ (TIME ONLY)
// ============================================================================
void drawMissionTimeOnlyScreen() {
  tft_.fillScreen(ST7735_BLACK);

  // 1. Header Bar ด้านบนสุด
  tft_.fillRoundRect(4, 4, 152, 16, 3, 0x0A22); // Emerald Dark Green
  tft_.drawRoundRect(4, 4, 152, 16, 3, ST7735_GREEN);
  printText(28, 8, "MISSION TIME", 1, ST7735_GREEN, 0x0A22);

  // คำนวณเวลา: นาที (Min), วินาที (Sec), มิลลิเสค (Ms)
  unsigned long total_ms = mission_summary.duration_ms;
  unsigned long m = total_ms / 60000;
  unsigned long s = (total_ms % 60000) / 1000;
  unsigned long ms = total_ms % 1000;

  // 2. Main Hero Time Card ขนาดใหญ่ตรงกลางจอ (เน้นเฉพาะเวลาล้วนๆ)
  tft_.fillRoundRect(4, 25, 152, 74, 4, 0x0841); // Dark Tech Blue
  tft_.drawRoundRect(4, 25, 152, 74, 4, ST7735_CYAN);
  tft_.drawRoundRect(5, 26, 150, 72, 3, 0x0115); // กรอบมิติชั้นใน

  printText(10, 31, "TOTAL ELAPSED", 1, ST7735_CYAN, 0x0841);

  // ตัวเลขเวลารูปแบบนาฬิกาดิจิทัลขนาดใหญ่ (Font Size 2 สีเหลืองทองเด่นชัด)
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu.%03lu", m, s, ms);
  // ความกว้างตัวอักษรขนาด 2 รวม 9 ตัว = 108px -> กึ่งกลาง: (160 - 108)/2 = 26
  printText(26, 47, timeBuf, 2, ST7735_YELLOW, 0x0841);

  // บรรทัดย่อยแจกแจงหน่วยเวลาอย่างชัดเจน อ่านง่าย
  char subTimeBuf[36];
  if (m > 0) {
    snprintf(subTimeBuf, sizeof(subTimeBuf), "%lu min  %lu sec  %03lu ms", m, s, ms);
  } else {
    snprintf(subTimeBuf, sizeof(subTimeBuf), "%lu sec  %03lu ms", s, ms);
  }
  int subLen = strlen(subTimeBuf);
  int subX = (160 - (subLen * 6)) / 2;
  if (subX < 6) subX = 6;
  printText(subX, 71, subTimeBuf, 1, ST7735_WHITE, 0x0841);

  // ระยะทางและพัลส์รวม
  char distBuf[36];
  snprintf(distBuf, sizeof(distBuf), "Dist: %.1f cm (%ld p)", mission_summary.distance_cm, mission_summary.pulses);
  int distLen = strlen(distBuf);
  int distX = (160 - (distLen * 6)) / 2;
  if (distX < 6) distX = 6;
  printText(distX, 85, distBuf, 1, ST7735_CYAN, 0x0841);

  // 3. แถบคำสั่งด้านล่างสุดสำหรับกดเริ่มใหม่ / สลับกลับไปดูสี
  tft_.fillRoundRect(4, 105, 152, 18, 3, 0x2104);
  tft_.drawRoundRect(4, 105, 152, 18, 3, 0x4208);
  if (button_mode == 3) {
    printText(14, 110, "[OK]=COLOR  [B1]=START", 1, ST7735_YELLOW, 0x2104);
  } else {
    printText(14, 110, "PRESS BUTTON TO RESTART", 1, ST7735_YELLOW, 0x2104);
  }
}

void loop() {
  // หากยังไม่มีการรันภารกิจ ให้แสดงหน้ามอนิเตอร์เซนเซอร์ทั่วไป
  if (!mission_summary.completed) {
    displayLightSensorsFrontAndBack();
    if (checkLongPress(1200)) {
      runCalibrationMenu();
    }
    delay(50);
    return;
  }

  // วาดหน้าจอตามสถานะปัจจุบัน (วาดครั้งเดียว ไม่กระพริบ)
  if (!screen_drawn) {
    if (current_screen_state == SCREEN_COLOR_SUMMARY) {
      drawColorSummaryScreen();
    } else {
      drawMissionTimeOnlyScreen();
    }
    screen_drawn = true;
  }

  // จัดการการกดปุ่มเพื่อสลับหน้าจอหรือเริ่มใหม่
  if (button_mode == 3) {
    // โหมด 4 ปุ่ม:
    // [OK]: สลับหน้าระหว่าง รายการสี <-> หน้าเวลา
    // [Start / B1]: เริ่มวิ่งทดสอบภารกิจใหม่ทันที
    if (isBtnOkPressed()) {
      buzzer(2000, 60);
      while (isBtnOkPressed()) delay(10);
      delay(150);
      current_screen_state = (current_screen_state == SCREEN_COLOR_SUMMARY) ? SCREEN_TIME_SUMMARY : SCREEN_COLOR_SUMMARY;
      screen_drawn = false;
    } else if (isBtnStartPressed()) {
      buzzer(2400, 100);
      while (isBtnStartPressed()) delay(10);
      delay(200);
      current_screen_state = SCREEN_COLOR_SUMMARY;
      screen_drawn = false;
      test_start_and_scan_colors(30);
    }
  } else {
    // โหมดปุ่มเดี่ยว:
    // ครั้งที่ 1 (หน้าสี): กดปุ่ม -> สลับไปหน้าเวลาภารกิจ
    // ครั้งที่ 2 (หน้าเวลา): กดปุ่ม -> เริ่มวิ่งทดสอบภารกิจใหม่
    if (isButtonPressed()) {
      buzzer(2200, 80);
      while (isButtonPressed()) delay(10);
      delay(150);

      if (current_screen_state == SCREEN_COLOR_SUMMARY) {
        current_screen_state = SCREEN_TIME_SUMMARY;
        screen_drawn = false;
      } else {
        current_screen_state = SCREEN_COLOR_SUMMARY;
        screen_drawn = false;
        test_start_and_scan_colors(30);
      }
    }
  }

  delay(30);
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

