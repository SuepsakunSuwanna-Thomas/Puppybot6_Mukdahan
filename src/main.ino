#include <Arduino.h>
#include <algorithm>
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
#include "color_scanner.h"

// ============================================================================
// ฟังก์ชันภารกิจหลักในสนาม (อยู่ในไฟล์ field_mission.ino)
// ============================================================================
void runFieldMission();
void grabCan();
void releaseCan();
void up();
void down();
void keeb();
void kang();
bool checkCanPresent();

void get_Warehouse2_Spare(RobotColor color, int speed);
void get_Warehouse2_Spare(RobotColor color);

void get_Yellow(int speed);
void get_Yellow();
void get_Blue(int speed);
void get_Blue();
void get_Black(int speed);
void get_Black();
void get_Green(int speed);
void get_Green();
void get_Red(int speed);
void get_Red();
void get_White(int speed);
void get_White();

void put_from_HUB(int slot, bool is_pickup, int speed);
void put_from_HUB(int slot, bool is_pickup);
void put_from_HUB(int slot);

void executeProductRecallExact(const RobotColor order_list[6], int speed);
void executeProductRecallExact(const RobotColor order_list[6]);
void returnToStartBox(int speed);
void returnToStartBox();

// ============================================================================
// SETUP: เริ่มต้นระบบฮาร์ดแวร์และเซนเซอร์
// ============================================================================
void setup() {
   delay(1500);
   puppybot6_setup();
   Serial.begin(115200);
   Serial.println("PuppyBot6 Initializing...");

//   // 1. กำหนดค่าเริ่มต้นพินเซนเซอร์หน้า 7 ตัว (0-6) และ หลัง 7 ตัว (7-13)
   setup_sensors();

   // 2. โหลดค่าคาลิเบรตทั้งหมดจาก EEPROM (Front, Back, TCS Color, IR Distance)
   initEEPROM();

   // 3. เริ่มต้น Encoder และไจโร IMU
   init_encoder(selection_Encoder);
   set_data_for_turnDirection(1, 20, 70, 3, 5);

   // 4. สั่งเซอร์โวเข้าตำแหน่งเริ่มต้น
   servoRun(1, 10);
   servoRun(2, 165);

//   // ==========================================================================
//   // [NEW] โหมดทดสอบผ่านหน้าเว็บ (WEB TEST LAB):
//   // หากต้องการทดสอบคำสั่งต่างๆ (PID, 4WD Motor, Servo) ผ่านหน้าเว็บ Web Dashboard
//   // ให้ยกเลิกคอมเมนต์บรรทัดด้านล่างนี้:
//   // runSerialTestMode();   // ==========================================================================

   // 5. แสดงหน้าจอเริ่มต้น และตรวจจับการกดปุ่ม:
   // - กดค้าง (>= 700ms) = เข้าเมนูเลือกคาลิเบรตทีละอัน
   // - กดสั้น (< 700ms)  = เริ่มวิ่งภารกิจทันทีโดยใช้ค่าเดิมที่เคยจำไว้ใน EEPROM
   while (waitForStartOrCalibration()) {
     runCalibrationMenu();
   }
   delay(290);

   // เริ่มต้นภารกิจหลักในสนาม (เขียนคำสั่งภารกิจได้ที่ไฟล์ field_mission.ino)
   //runFieldMission();
   test_auto_scan_6colors(40, 405); // หากสีหายไปนานเกินระยะห่างช่องสี (560p) จะนับเป็น "สีขาว" ทันที



 }

// ============================================================================
// LOOP: จัดการหน้าจอแสดงผลและการกดปุ่มหลังจากจบภารกิจ
// ============================================================================
void loop() {
  updateMissionUI();
  delay(30);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
