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
  servoRun(1, 50);
  servoRun(2, 146);

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

  // ==========================================================================
  // ตัวอย่างการเรียกใช้งานฟังก์ชันการเคลื่อนที่ที่สร้างไว้ครบทั้งหมด:
  // ==========================================================================
  // Fline_by_pass(50, 2);                // วิ่งผ่าน 2 เส้นตัดแล้วหยุด
  // turnLeft(40);                        // วิ่งจนเจอเส้นตัดแล้วหมุนซ้าย (sl) จนเจอเส้นใหม่
  // Fline_by_phase(50, 30.0);            // วิ่งตามเส้น 30 ซม. (บอกระยะด้วย Encoder)
  // turnRight(40);                       // วิ่งจนเจอเส้นตัดแล้วหมุนขวา (sr) จนเจอเส้นใหม่
  // Fline_by_time(50, 1000);             // วิ่งตามเส้น 1 วินาที
  // checkLine();                         // จัดเซนเซอร์หน้าตัวกลาง (ADC 3) ตรงเส้น
  // Fline_by_pass_B(40, 1);              // วิ่งถอยหลังผ่าน 1 เส้น
  // turnLeft_B(40);                      // ถอยหลังเจอเส้นแล้วหมุนขวา (sr) สลับกัน
  // Follow_Notline(50, 500);             // วิ่งตรงแบบไม่ใช้ PID 500 ms (ปรับ Trim ทั้ง 4 ล้อ)
  // stop_4w();
}

void loop() {
  static bool screen_init = false;
  if (!screen_init) {
    tft_.fillScreen(ST7735_BLACK);
    printText(5, 4, "--- COLOR MONITOR ---", 1, ST7735_YELLOW);
    tft_.drawFastHLine(0, 16, 160, ST7735_BLUE);
    screen_init = true;
  }

  // แสดงผลเฉพาะค่าสีเซนเซอร์หน้าและหลัง (Front & Back TCS)
  displayColorSensorsOnly();

  // หากกดปุ่มค้างในขณะ loop() จะเข้าสู่เมนูคาลิเบรตได้ทันที
  if (checkLongPress(1200)) {
    runCalibrationMenu();
    screen_init = false; // หลังออกจากเมนู ให้วาดหัวข้อหน้าจอใหม่
  }

  delay(120);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ห้ามแก้ไข
// ห้ามแก้ไข
// ห้ามแก้ไข
// ห้ามแก้ไข
// ห้ามแก้ไข
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
  TCS_R = Read_Color_TCS_B_Raw(0);
  TCS_G = Read_Color_TCS_B_Raw(1);
  TCS_B = Read_Color_TCS_B_Raw(2);
  TCS_T = Read_Color_TCS_B_Raw(3);
  TCS_L = Read_Color_TCS_B_Raw(4);
}
