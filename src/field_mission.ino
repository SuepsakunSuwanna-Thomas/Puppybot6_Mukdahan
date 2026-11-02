// ============================================================================
// ============================================================================
// LOGISTICS ROBOT COMPETITION (ภารกิจหุ่นยนต์ Logistics Robot รุ่น 19)
// โค้ดแนวทางแบบ Sisakat 4WD 6 ปรับใช้กับ PuppyBot 6 RP2350 / RP2040
// โครงสร้างฟังก์ชันแยก void รายสี และควบคุมจากจุด HUB 100% ตามข้อกำหนด
// ============================================================================
// ============================================================================

#include <Arduino.h>
#include "config.h"
#include "motor_drive.h"
#include "sensor_system.h"
#include "eeprom_calib.h"
#include "line_follow.h"
#include "color_scanner.h"

// ============================================================================
// 1. การตั้งค่าเซอร์โวมือจับ แขนยก ระยะเอ็นโค้ดเดอร์ และความเร็ว
// ============================================================================
const int SERVO_ARM_CH     = 1; // เซอร์โวยกแขนขึ้น-ลง (ช่อง 1)
const int SERVO_GRIPPER_CH = 2; // เซอร์โวมือจับหนีบกระป๋อง (ช่อง 2)

// องศาเซอร์โว (สามารถปรับจูนตามกลไกของหุ่นได้ที่นี่)
int GRIPPER_OPEN_ANG = 165;  // องศากางมือจับออกสุด
int GRIPPER_GRAB_ANG = 86; // องศาหนีบกระป๋องแน่น
int ARM_DOWN_ANG     = 110;  // องศาลดแขนลงระดับคีบ/วางลงหลุมแท่น 5 cm
int ARM_LIFT_ANG     = 60; // องศายกแขนขึ้นสูงพ้นแท่น 5 cm

// ระยะพัลส์เอ็นโค้ดเดอร์สำหรับเข้าไปคีบและวางกระป๋อง
long ENCODER_CAN_APPROACH_PULSES = 100; // ระยะ PID เดินหน้าเข้าไปเอากระป๋องนิดนึง
long ENCODER_CAN_BACKUP_PULSES   = 180; // ระยะถอยหลังตามเอ็นโค้ดเดอร์นิดนึงหลังยก
long ENCODER_CONSUMER_PULSES     = 200; // ระยะ PID เข้าไปวาง/ยกที่แท่นหลุมลูกค้า

// ความเร็วมาตรฐานในการวิ่งภารกิจ
int SPEED_SCAN       = 40;  // ความเร็วตอนสแกนสีแถบคำสั่งซื้อ
int SPEED_NAV        = 30;  // ความเร็ววิ่งนำทางระหว่างจุด
int SPEED_WAREHOUSE  = 28;  // ความเร็วเข้าคลังสินค้า
int SPEED_CONSUMER   = 30;  // ความเร็วเข้าส่งลูกค้า
int SPEED_RECALL     = 28;  // ความเร็วภารกิจเรียกคืนสินค้า
int SPEED_RETURN     = 32;  // ความเร็ววิ่งกลับจุดสตาร์ท
int speedl           = 45;  // ความเร็วเลี้ยงซ้าย
int speedr           = 45;  // ความเร็วเลี้ยวขวา
int speed1           = 60;  // ความเร็วก่อนเลี้ยว

// กำหนดสีที่หมดสต็อก หรือสีชำรุดล่วงหน้า (หากต้องการบังคับทดสอบ)
RobotColor OUT_OF_STOCK_COLOR = COLOR_UNKNOWN; // เช่น COLOR_RED หรือ COLOR_UNKNOWN (เช็คจริงด้วย IR)
bool ENABLE_PRODUCT_RECALL    = true;          // เปิด/ปิดภารกิจเรียกคืนสินค้าชำรุด
RobotColor RECALLED_COLOR     = COLOR_UNKNOWN; // เช่น COLOR_BLUE หรือ COLOR_UNKNOWN (อ่านจากจุดเช็ค)

// ============================================================================
// 2. ฟังก์ชันควบคุมมือจับและแขนยก สไตล์ Sisakat 4WD
// ============================================================================

// คีบกระป๋องและยกขึ้นข้ามแท่น (Grab Can)
inline void grabCan() {
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง
  delay(300);
  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG);         // ลดแขนลงครอบตัวกระป๋อง
  delay(250);
  servoRun(SERVO_GRIPPER_CH, GRIPPER_GRAB_ANG); // หนีบกระป๋องแน่น
  delay(250);
  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG);         // ยกแขนข้ามแท่น 5 cm
  delay(200);
}

// วางกระป๋องลงในหลุมแท่นลูกค้าให้ตั้งตรง (Release Can into 5 cm Hole Platform)
inline void releaseCan() {
   servoRun(SERVO_ARM_CH, 90);         // ลดระดับลงใกล้ปากหลุม
  delay(220);
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับ ปล่อยกระป๋องลงหลุม
  delay(220);
  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG);         // ยกแขนขึ้นหลบขอบแท่น ไม่ให้เกี่ยวชน
  delay(150);
}

// คำสั่งย่อสไตล์ Sisakat
inline void up()   { servoRun(SERVO_ARM_CH, ARM_LIFT_ANG); delay(180); }
inline void down() { servoRun(SERVO_ARM_CH, ARM_DOWN_ANG); delay(180); }
inline void keeb() { servoRun(SERVO_GRIPPER_CH, GRIPPER_GRAB_ANG); delay(200); }
inline void kang() { servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); delay(200); }

// ============================================================================
// 3. ตรวจสอบสินค้าในช่องคลังด้วยเซนเซอร์วัดระยะทาง IR (GP2Y0A41SK0F ADC 14)
// ============================================================================
inline bool checkCanPresent() {
  float dist_cm = get_ir_distance_cm();
  Serial.print("[IR Sensor] Measured Distance: ");
  Serial.print(dist_cm, 1);
  Serial.print(" cm");

  // หากมีกระป๋องอยู่ในช่อง ระยะตรวจจับจะอยู่ในช่วง 3.0 ถึง 16.0 ซม.
  if (dist_cm > 3.0f && dist_cm <= 16.0f) {
    Serial.println(" -> [OK] CAN FOUND! (มีสินค้าในคลัง)");
    buzzer(2400, 40);
    return true;
  } else {
    Serial.println(" -> [EMPTY] OUT OF STOCK! (สินค้าหมดสต็อก -> ไปคลังสำรอง W2)");
    buzzer(1200, 150);
    return false;
  }
}

// ============================================================================
// 4. ฟังก์ชันค้นหาว่า สีนี้ ต้องนำไปส่งให้ลูกค้าช่องที่เท่าไหร่ (1-6)
// ============================================================================
inline int findConsumerForColor(RobotColor color, const RobotColor* order_list) {
  for (int i = 0; i < 6; i++) {
    if (order_list[i] == color) {
      return (i + 1);
    }
  }
  return 1;
}

// ----------------------------------------------------------------------------
// จัดการกรณีกระป๋องหมดสต็อกที่ Warehouse 1 -> ไปคลังสำรอง Warehouse 2
// แยก switch-case ตามสีที่หมด เพราะตำแหน่งที่หุ่นยนต์ไปเช็คแต่ละสียืนอยู่คนละจุด
// ----------------------------------------------------------------------------
inline void get_Warehouse2_Spare(RobotColor color, int speed) {
  Serial.print("\n[Warehouse 2] Heading to Warehouse 2 for Spare Can (Color: ");
  Serial.print(getColorName(color));
  Serial.println(") ...");

  switch (color) {
    // ------------------------------------------------------------------------
    // กรณีสีเหลืองหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง Yellow (เส้นกึ่งกลาง หน้า HUB)
    // ------------------------------------------------------------------------
    case COLOR_YELLOW:
      Serial.println("[W2 Route] Out of stock at YELLOW -> Heading to Warehouse 2...");
      // 2. เลี้ยวขวามุ่งหน้าสู่คลังสำรอง W2
      turnRight(40);
      Fline_by_pass(speed1,2);
     Fline(20,0);
     turnLeft_by_stop(35);
     Fline_until_can(35, 7);


      grabCan();
      Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
      // 4. หันหลังกลับและวิ่งกลับมาตั้งลำที่จุด HUB (หันหน้าเข้าหา Consumer)
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight(50);
      Fline_by_pass(speed, 2);
      break;

    // ------------------------------------------------------------------------
    // กรณีสีน้ำเงินหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง Blue (เส้นบน)
    // ------------------------------------------------------------------------
    case COLOR_BLUE:
      Serial.println("[W2 Route] Out of stock at BLUE -> Heading to Warehouse 2...");
      // 1. ถอย/กลับตัวออกจากช่อง Blue กลับเข้าสู่จุด HUB
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      turnRight_by_stop(50);
      // 2. จาก HUB เลี้ยวขวามุ่งหน้าสู่คลังสำรอง W2
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      // 3. วิ่งตามเอ็นโค้ดเดอร์เข้าไปคีบกระป๋องสำรอง
      Fline_by_Encoder(speed, ENCODER_CAN_APPROACH_PULSES);
      grabCan();
      Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
      // 4. หันหลังกลับและวิ่งกลับมาตั้งลำที่จุด HUB (หันหน้าเข้าหา Consumer)
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight(50);
      Fline_by_pass(speed, 2);
      break;

    // ------------------------------------------------------------------------
    // กรณีสีดำหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง Black (เส้นล่าง)
    // ------------------------------------------------------------------------
    case COLOR_BLACK:
      Serial.println("[W2 Route] Out of stock at BLACK -> Heading to Warehouse 2...");
      // 1. ถอย/กลับตัวออกจากช่อง Black กลับเข้าสู่จุด HUB
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      turnRight_by_stop(50);
      turnLeft_by_stop(50);
      // 2. จาก HUB เลี้ยวขวามุ่งหน้าสู่คลังสำรอง W2
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      // 3. วิ่งตามเอ็นโค้ดเดอร์เข้าไปคีบกระป๋องสำรอง
      Fline_by_Encoder(speed, ENCODER_CAN_APPROACH_PULSES);
      grabCan();
      Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
      // 4. หันหลังกลับและวิ่งกลับมาตั้งลำที่จุด HUB (หันหน้าเข้าหา Consumer)
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight(50);
      Fline_by_pass(speed, 2);
      break;

    // ------------------------------------------------------------------------
    // กรณีสีเขียวหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง Green (เส้นกึ่งกลาง ถัดจากเหลือง)
    // ------------------------------------------------------------------------
    case COLOR_GREEN:
      Serial.println("[W2 Route] Out of stock at GREEN -> Heading to Warehouse 2...");
      // 1. หันหลังกลับ วิ่งผ่าน 2 เส้น เลี้ยวขวา ผ่าน 1 เส้น เข้าคลัง W2
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 2);
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      // 2. วิ่งตามเอ็นโค้ดเดอร์เข้าไปคีบกระป๋องสำรอง
      Fline_by_Encoder(speed, ENCODER_CAN_APPROACH_PULSES);
      grabCan();
      Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
      // 3. หันหลังกลับและวิ่งกลับมาตั้งลำที่จุด HUB (หันหน้าเข้าหา Consumer)
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight(50);
      Fline_by_pass(speed, 2);
      break;

    // ------------------------------------------------------------------------
    // กรณีสีแดงหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง Red (เส้นบน ถัดจากน้ำเงิน)
    // ------------------------------------------------------------------------
    case COLOR_RED:
      Serial.println("[W2 Route] Out of stock at RED -> Heading to Warehouse 2...");
      // 1. ถอย/กลับตัวออกจากช่อง Red กลับเข้าสู่จุด HUB
      turnRight_by_stop(50);
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      // 2. จาก HUB เลี้ยวขวามุ่งหน้าสู่คลังสำรอง W2
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      // 3. วิ่งตามเอ็นโค้ดเดอร์เข้าไปคีบกระป๋องสำรอง
      Fline_by_Encoder(speed, ENCODER_CAN_APPROACH_PULSES);
      grabCan();
      Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
      // 4. หันหลังกลับและวิ่งกลับมาตั้งลำที่จุด HUB (หันหน้าเข้าหา Consumer)
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight(50);
      Fline_by_pass(speed, 2);
      break;

    // ------------------------------------------------------------------------
    // กรณีสีขาวหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง White (เส้นล่าง ถัดจากดำ)
    // ------------------------------------------------------------------------
    case COLOR_WHITE:
      Serial.println("[W2 Route] Out of stock at WHITE -> Heading to Warehouse 2...");
      // 1. ถอย/กลับตัวออกจากช่อง White กลับเข้าสู่จุด HUB
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      // 2. จาก HUB เลี้ยวขวามุ่งหน้าสู่คลังสำรอง W2
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      // 3. วิ่งตามเอ็นโค้ดเดอร์เข้าไปคีบกระป๋องสำรอง
      Fline_by_Encoder(speed, ENCODER_CAN_APPROACH_PULSES);
      grabCan();
      Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
      // 4. หันหลังกลับและวิ่งกลับมาตั้งลำที่จุด HUB (หันหน้าเข้าหา Consumer)
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight(50);
      Fline_by_pass(speed, 2);
      break;

    default:
      Serial.println("[Warehouse 2] Unknown color! Using default route...");
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 2);
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      Fline_by_Encoder(speed, ENCODER_CAN_APPROACH_PULSES);
      grabCan();
      Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight(50);
      Fline_by_pass(speed, 2);
      break;
  }

  Serial.println("[Warehouse 2] Spare can secured and robot returned to HUB!");
}
inline void get_Warehouse2_Spare(RobotColor color) { get_Warehouse2_Spare(color, SPEED_WAREHOUSE); }

// ============================================================================
// 5. ฟังก์ชันแยกสำหรับวิ่งไปหยิบสินค้าแต่ละสี (Warehouse 1)
// ============================================================================

// [5.1] สีเหลือง: วิ่งจากการอ่านค่าแถบสีเสร็จ เลี้ยวซ้าย ซ้าย ผ่าน 1 เลี้ยวขวา ผ่าน 4 เส้น
//       -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น -> ถอยหลังตาม encoder นิดนึง -> หมุนตัวเพื่อให้หน้าอยู่ตรง HUB
inline void get_Yellow(int speed) {
  Serial.println("\n[Action] >>> Step 1: Going to YELLOW from Color Scan Line <<<");

  // 1. เลี้ยวซ้าย ซ้าย ผ่าน 1 เลี้ยวขวา ผ่าน 4 เส้น
  // turnLeft(40);
  // turnLeft(35);
  // Fline_by_pass(50, 1);
  // turnRight(35);
  Fline_by_pass(100, 2);
  Fline_by_pass(65, 1);             
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง
  delay(300);
  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG);         // ลดแขนลงครอบตัวกระป๋อง
  Fline(35,6), 
  Fline_until_can(25, 7);
  // 2. เช็คว่ามีกระป๋องไหม
  bool has_can = (OUT_OF_STOCK_COLOR == COLOR_YELLOW) ? false : checkCanPresent();
  if (!has_can) {
    // หากไม่มี ไปเอาที่คลังสำรอง แล้ว return ทันที ป้องกันคำสั่งด้านล่างทำงานซ้ำ!
    get_Warehouse2_Spare(COLOR_YELLOW, speed);
    return;
  } else {
  
    // หากมีกระป๋อง:

    // ยกขึ้น
    grabCan();

    // ถอยหลังตามเอ็นโค้ดเดอร์นิดนึง
    // Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
    Fline_B_by_time(40, 30);
    // // หมุนตัวเพื่อให้หน้าอยู่ตรง HUB
    turnLeft_by_stop(40);
    Fline_B_by_time(40, 40);
    turnLeft_by_stop(40);
    delay(50);
  }

  Serial.println("[Action] Yellow can ready at HUB!");
}
inline void get_Yellow() { get_Yellow(SPEED_WAREHOUSE); }

// [5.2] สีน้ำเงิน: พอหุ่นยนต์ไปวางเหลืองเสร็จแล้วหันหน้าออกมาที่จุด HUB
//       หมุนซ้าย เลี้ยวขวา -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น -> ถอยหลังตาม encoder นิดนึง -> หมุน เลี้ยวซ้าย เลี้ยวขวา หน้าจะอยู่ตรง HUB
inline void get_Blue(int speed) {
  Serial.println("\n[Action] >>> Step 2: Going to BLUE from HUB <<<");

  // 1. หมุนซ้าย เลี้ยวขวา
  servoRun(SERVO_ARM_CH, 50); 
  turnLeft(45);
  turnRight(45);
  delay(250);             
  Fline_B_by_time(50, 270);
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง
  delay(300);
  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG);         // ลดแขนลงครอบตัวกระป๋อง
  delay(250);
  Fline_until_can(25, 7);   


  // 2. เช็คว่ามีกระป๋องไหม
  bool has_can = (OUT_OF_STOCK_COLOR == COLOR_BLUE) ? false : checkCanPresent();
  if (!has_can) {
    // หากไม่มี ไปเอาที่คลังสำรอง แล้ว return ทันที ป้องกันคำสั่งด้านล่างทำงานซ้ำ!
    get_Warehouse2_Spare(COLOR_BLUE, speed);
    return;
  } else {
  
    // หากมีกระป๋อง:

    // ยกขึ้น
    grabCan();

    //  ถอย
    Fline_B_by_time(20, 100);

    // หมุน เลี้ยวซ้าย เลี้ยวขวา หน้าจะอยู่ตรง HUB
    turnRight_by_stop(45);
    turnRight(45);
    delay(50);
  }

  Serial.println("[Action] Blue can ready at HUB!");
}
inline void get_Blue() { get_Blue(SPEED_WAREHOUSE); }

// [5.3] สีดำ: หมุนขวา เลี้ยวซ้าย -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น -> ถอยหลังตาม encoder นิดนึง -> หมุน เลี้ยวขวา เลี้ยวซ้าย หน้าจะอยู่ที่ HUB
inline void get_Black(int speed) {
  Serial.println("\n[Action] >>> Step 3: Going to BLACK from HUB <<<");

  // 1. หมุนขวา เลี้ยวซ้าย
  servoRun(SERVO_ARM_CH, 50); 
  turnRight(45);
  turnLeft(45);
  delay(250);             
  Fline_B_by_time(50,300);
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง
  delay(300);
  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG);         // ลดแขนลงครอบตัวกระป๋อง
  delay(250);
  Fline_until_can(25, 7);  

  // 2. เช็คว่ามีกระป๋องไหม
  bool has_can = (OUT_OF_STOCK_COLOR == COLOR_BLACK) ? false : checkCanPresent();
  if (!has_can) {
    get_Warehouse2_Spare(COLOR_BLACK, speed);
    return;
  }

    // หากมีกระป๋อง:

    // ยกขึ้น
    grabCan();

    //  ถอย
    Fline_B_by_time(20, 100);

    // หมุน เลี้ยวขวา เลี้ยวซ้าย หน้าจะอยู่ที่ HUB
    turnLeft_by_stop(55);
    turnLeft(40);
    delay(50);                          

  Serial.println("[Action] Black can ready at HUB!");
}
inline void get_Black() { get_Black(SPEED_WAREHOUSE); }

// [5.4] สีเขียว: วิ่งผ่าน 1 เส้น -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น -> ถอยหลังตาม encoder นิดนึง -> หมุน ผ่าน 2 เส้น หน้าจะอยู่ที่ HUB
inline void get_Green(int speed) {
  Serial.println("\n[Action] >>> Step 4: Going to GREEN from HUB <<<");

  // 1. วิ่งผ่าน 1 เส้นถอยหลังตามเอ็นโค้ดเดอร์นิดนึง
  Fline_by_pass(65, 1);             
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง
  delay(300);
  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG);         // ลดแขนลงครอบตัวกระป๋อง
  Fline(40,6), 
  Fline_until_can(15, 7);
  // 2. เช็คว่ามีกระป๋องไหม

  bool has_can = (OUT_OF_STOCK_COLOR == COLOR_GREEN) ? false : checkCanPresent();
  if (!has_can) {
    get_Warehouse2_Spare(COLOR_GREEN, speed);
    return;
  }

    // หากมีกระป๋อง:

    // ยกขึ้น
    grabCan();

    // ถอยหลังตามเอ็นโค้ดเดอร์นิดนึง
    // Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
    Fline_B_by_time(40, 30);
    // // หมุนตัวเพื่อให้หน้าอยู่ตรง HUB
    turnLeft_by_stop(40);
    // Backword_Notline_by_Encoder(40, 160);
    Fline_B_by_time(40,50);
    turnLeft_by_stop(40);
    Fline_by_pass(60,1);
    delay(50);

  Serial.println("[Action] Green can ready at HUB!");
}
inline void get_Green() { get_Green(SPEED_WAREHOUSE); }

// [5.5] สีแดง: ผ่าน 1 เส้น เลี้ยวซ้าย เลี้ยวขวา -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น -> ถอยหลังตาม encoder นิดนึง -> หมุนขวา เลี้ยวขวา ผ่าน 1 เส้น เรียก HUB
inline void get_Red(int speed) {
  Serial.println("\n[Action] >>> Step 5: Going to RED from HUB <<<");

  // 1. หมุนซ้าย เลี้ยวขวา ผ่าน1
  servoRun(SERVO_ARM_CH, 50); 
  turnLeft(45);
  turnRight(45);
  Fline(40,6),
  delay(250);             
  Fline_B_by_time(50, 115);
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง
  delay(300);
  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG);         // ลดแขนลงครอบตัวกระป๋อง
  delay(250);
  Fline_until_can(25, 7);   


  // 2. เช็คว่ามีกระป๋องไหม
  bool has_can = (OUT_OF_STOCK_COLOR == COLOR_RED) ? false : checkCanPresent();
  if (!has_can) {
    get_Warehouse2_Spare(COLOR_RED, speed);
    return;
  }

    // หากมีกระป๋อง:

    // ยกขึ้น
    grabCan();

    //  ถอย
    Fline_B_by_time(20, 100);

    // หมุน เลี้ยวซ้าย เลี้ยวขวา หน้าจะอยู่ตรง HUB
    turnRight_by_stop(45);
    turnRight(45);
    Fline_by_pass(60,1);
    delay(50);

  Serial.println("[Action] Red can ready at HUB!");
}
inline void get_Red() { get_Red(SPEED_WAREHOUSE); }

// [5.6] สีขาว: เลี้ยวขวา เลี้ยวซ้าย -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น -> ถอยหลังตาม encoder นิดนึง -> หมุนซ้าย เลี้ยวซ้าย ผ่าน 1 เส้น เรียก HUB
inline void get_White(int speed) {
  Serial.println("\n[Action] >>> Step 6: Going to WHITE from HUB <<<");

  // 1. หมุนซ้าย เลี้ยวขวา ผ่าน1
  servoRun(SERVO_ARM_CH, 50); 
  turnRight(45);
  turnLeft(45);
  Fline(40,6),
  delay(250);             
  Fline_B_by_time(50, 100);
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง
  delay(300);
  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG);         // ลดแขนลงครอบตัวกระป๋อง
  delay(250);
  Fline_until_can(25, 7);   
  
  // 2. เช็คว่ามีกระป๋องไหม
  bool has_can = (OUT_OF_STOCK_COLOR == COLOR_WHITE) ? false : checkCanPresent();
  if (!has_can) {
    get_Warehouse2_Spare(COLOR_WHITE, speed);
    return;
  }

    // หากมีกระป๋อง:

    // ยกขึ้น
    grabCan();

    //  ถอย
    Fline_B_by_time(20, 100);

    // หมุน เลี้ยวขวา เลี้ยวซ้าย หน้าจะอยู่ที่ HUB
    turnLeft_by_stop(55);
    turnLeft(40);
    Fline_by_pass(60,1);
    delay(50);    

  Serial.println("[Action] White can ready at HUB!");
}
inline void get_White() { get_White(SPEED_WAREHOUSE); }

// ============================================================================
// 6. ฟังก์ชัน HUB แบบสวิตช์เคส ช่อง 1 ถึง 6 (มี if เช็คทุกช่องว่าจะเรียกวางหรือยก)
// ============================================================================
// is_pickup = false : วิ่งไปวางกระป๋อง (ตอนส่งของปกติ)
// is_pickup = true  : วิ่งไปยกกระป๋อง (ตอน Recall เรียกคืนสินค้าชำรุด)
inline void put_from_HUB(int slot, bool is_pickup, int speed) {
  Serial.print("\n[HUB] >>> Navigating from HUB to Consumer Slot #");
  Serial.print(slot);
  Serial.print(" (Action: ");
  Serial.print(is_pickup ? "PICK UP / ยก" : "PLACE / วาง");
  Serial.println(") <<<");

  switch (slot) {
    // ------------------------------------------------------------------------
    // ช่อง 1: ผ่าน 2 เลี้ยวซ้าย เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนซ้าย ผ่าน 3 เส้น
    // ------------------------------------------------------------------------
    case 1:
      Fline_by_pass(100, 1);
      Fline_by_pass(50, 1);
      turnLeft(36);
      delay(200);
       Fline_B_by_time(60, 150);
       Fline_until_can(45, 5);

      if (is_pickup) {
        grabCan();   // ยกขึ้น
      } else {
        releaseCan(); // วางลงหลุม
      }

      Fline_B_by_time(40, 100);
      turnLeft_by_stop(55);
      Fline_by_pass(80, 2);
      delay(50);
      break;

    // ------------------------------------------------------------------------
    // ช่อง 2: ผ่าน 2 เลี้ยวขวา เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนขวา ผ่าน 3 เส้น
    // ------------------------------------------------------------------------
    case 2:
      Fline_by_pass(100, 1);
      Fline_by_pass(50, 1);
      turnRight(32);
      delay(200);
       Fline_B_by_time(60, 100);
       Fline_until_can(60, 5);

      if (is_pickup) {
        grabCan();   // ยกขึ้น
      } else {
        releaseCan(); // วางลงหลุม
      }

      Fline_B_by_time(40, 115);
      turnRight_by_stop(55);
      Fline_by_pass(80, 2);
      delay(50);
      break;

    // ------------------------------------------------------------------------
    // ช่อง 3: ผ่าน 1 เลี้ยวซ้าย เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนซ้าย ผ่าน 2 เส้น
    // ------------------------------------------------------------------------
    case 3:
      Fline_by_pass(50, 1);
      turnLeft(35);
      delay(200);
       Fline_B_by_time(52, 150);
       Fline_until_can(45, 5);

      if (is_pickup) {
        grabCan();   // ยกขึ้น
      } else {
        releaseCan(); // วางลงหลุม
      }

      Fline_B_by_time(40, 100);
      turnLeft_by_stop(55);
      Fline_by_pass(80, 1);
      delay(50);
      
      break;


    // ------------------------------------------------------------------------
    // ช่อง 4: ผ่าน 1 เลี้ยวขวา เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนขวา ผ่าน 2 เส้น
    // ------------------------------------------------------------------------
    case 4:
      Fline_by_pass(50, 1);
      turnRight(33);
      delay(200);
       Fline_B_by_time(60, 100);
       Fline_until_can(60, 5);

      if (is_pickup) {
        grabCan();   // ยกขึ้น
      } else {
        releaseCan(); // วางลงหลุม
      }

      Fline_B_by_time(40, 105);
      turnRight_by_stop(55);
      Fline_by_pass(80, 1);
      delay(50);
      break;

    // ------------------------------------------------------------------------
    // ช่อง 5: เลี้ยวซ้าย เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนซ้าย ผ่าน 1 เส้น
    // ------------------------------------------------------------------------
    case 5:
      turnLeft(35);
      delay(200);
       Fline_B_by_time(52, 150);
       Fline_until_can(45, 5);

      if (is_pickup) {
        grabCan();   // ยกขึ้น
      } else {
        releaseCan(); // วางลงหลุม
      }

      Fline_B_by_time(40, 100);
      turnLeft_by_stop(48);
      delay(50);
      break;

    // ------------------------------------------------------------------------
    // ช่อง 6: เลี้ยวขวา เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนขวา ผ่าน 1 เส้น
    // ------------------------------------------------------------------------
    case 6:
       turnRight(35);
      delay(200);
       Fline_B_by_time(52, 150);
       Fline_until_can(45, 5);

      if (is_pickup) {
        grabCan();   // ยกขึ้น
      } else {
        releaseCan(); // วางลงหลุม
      }

      Fline_B_by_time(40, 90);
      turnRight_by_stop(48);
      delay(50);
      break;

    default:
      Serial.println("[HUB] Invalid slot number!");
      break;
  }

  buzzer(2000, 60);
  Serial.println("[HUB] Operation completed. Robot is now back at HUB!");
}
inline void put_from_HUB(int slot, bool is_pickup) { put_from_HUB(slot, is_pickup, SPEED_CONSUMER); }
inline void put_from_HUB(int slot) { put_from_HUB(slot, false, SPEED_CONSUMER); }

// ============================================================================
// 7. ภารกิจเรียกคืนสินค้าชำรุด (Product Recall Execution)
// เมื่อเอาไปวางครบทั้งหมด:
// อยู่ที่ HUB -> เลี้ยวซ้าย ผ่าน 1 เลี้ยวซ้าย -> เช็คสีตัวที่ 2 -> รู้สี -> ไป HUB
// -> ไปช่องเรียกคืน ยกขึ้นมา -> ไป HUB
// -> เลี้ยวขวา ผ่าน 1 เลี้ยวซ้าย วิ่งตามเอ็นโค้ดเดอร์ -> วางลง
// -> ถอยหลัง PID ตามเอ็นโค้ดเดอร์ -> หมุนซ้าย ผ่าน 3 เส้น เลี้ยวขวา วิ่งเข้าไปตามเอ็นโค้ดเดอร์ -> ยก
// -> ถอยหลังตามเอ็นโค้ดเดอร์ -> หมุนขวา ผ่าน 1 เลี้ยวขวา ผ่าน 1 -> ถึง HUB -> เรียก HUB ไปวาง!
// ============================================================================
inline void executeProductRecallExact(const RobotColor order_list[6], int speed) {
  Serial.println("\n***************************************************");
  Serial.println(">>> [PRODUCT RECALL] Starting Defective Recall Mission <<<");
  Serial.println("***************************************************");

  // 1. ตอนนี้หุ่นยนต์อยู่ที่จุด HUB
  //    เลี้ยวซ้าย ผ่าน 1 เลี้ยวซ้าย
  turnLeft_by_stop(50);
  Fline_by_pass(speed, 1);
  turnLeft_by_stop(50);

  // 2. เช็คสีตัวที่ 2 (อ่านบล็อกเลโก้สีชำรุดที่จุดตรวจเช็ค)
  RobotColor recalled_color = detectColorFront();
  if (RECALLED_COLOR != COLOR_UNKNOWN) {
    recalled_color = RECALLED_COLOR; // หากมีการตั้งค่าทดสอบไว้ล่วงหน้า
  }
  if (recalled_color == COLOR_UNKNOWN) {
    recalled_color = COLOR_RED; // ตัวอย่าง default กรณีอ่านไม่ชัดเจน
  }
  Serial.print("[Recall] Detected Defective Lego Color: ");
  Serial.println(getColorName(recalled_color));

  // 3. รู้สีแล้ว เช็คว่าสีนั้นอยู่ช่องไหน
  int recall_slot = findConsumerForColor(recalled_color, order_list);
  Serial.print("[Recall] Defective item is in Consumer Slot #");
  Serial.println(recall_slot);

  // 4. วิ่งไปที่ HUB ก่อน
  turnLeft_by_stop(50);
  turnLeft_by_stop(50);
  Fline_by_pass(speed, 1);
  turnRight_by_stop(50); // หน้าหันตรงเข้าหา Consumer ที่จุด HUB

  // 5. วิ่งไปที่ช่องเรียกคืน ก็ยกขึ้นมา แล้วไปที่ HUB
  put_from_HUB(recall_slot, true, SPEED_CONSUMER); // is_pickup = true (ยกของเสียออกมาแล้วกลับมา HUB)

  // 6. เลี้ยวขวา ผ่าน 1 เลี้ยวซ้าย วิ่งตามเอ็นโค้ดเดอร์ แล้ววางลง
  turnRight_by_stop(50);
  Fline_by_pass(speed, 1);
  turnLeft_by_stop(50);
  Fline_by_Encoder(speed, ENCODER_CAN_APPROACH_PULSES);
  releaseCan(); // วางของชำรุดทิ้งลง

  // 7. ถอยหลัง PID ตามเอ็นโค้ดเดอร์ หมุนซ้าย ผ่าน 3 เส้น เลี้ยวขวา วิ่งเข้าไปตามเอ็นโค้ดเดอร์ แล้วยก
  Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
  turnLeft_by_stop(50);
  Fline_by_pass(speed, 3);
  turnRight_by_stop(50);
  Fline_by_Encoder(speed, ENCODER_CAN_APPROACH_PULSES);
  grabCan(); // ยกของชิ้นใหม่ทดแทนขึ้นมา

  // 8. ถอยหลังตามเอ็นโค้ดเดอร์ หมุนขวา ผ่าน 1 เลี้ยวขวา ผ่าน 1 ก็เรียก HUB
  Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);
  turnRight_by_stop(50);
  Fline_by_pass(speed, 1);
  turnRight_by_stop(50);
  Fline_by_pass(speed, 1); // มาถึง HUB

  // 9. เรียก HUB ไปวางที่ช่องเดิม!
  put_from_HUB(recall_slot, false, SPEED_CONSUMER); // is_pickup = false (วางกระป๋องใหม่ลงหลุม)

  Serial.println("[Recall] Product Recall Completed 100%!");
  buzzer(2400, 200);
}

// ============================================================================
// 8. เดินทางกลับเข้าจุด START (Return to Start Box) รับคะแนนโบนัส 10 คะแนน
// ============================================================================
inline void returnToStartBox(int speed) {
  Serial.println("[Nav] Mission Complete -> Returning to START Box (10 Bonus Points)...");
  // จากจุด Hub หันหน้าไปทางทิศตะวันตก มุ่งหน้าสู่แนวแกนของกล่อง START
  Fline_by_pass(speed, 3);
  turnRight_by_stop(50);
  Fline_by_pass(speed, 1);
  turnLeft_by_stop(50);
  // วิ่งเข้าจอดอย่างสง่างามในกล่อง START
  Fline_by_phase(speed, 25.0f);
  stop_4w();

  buzzer(2200, 300);
  delay(150);
  buzzer(2600, 400);
}
inline void returnToStartBox() { returnToStartBox(SPEED_RETURN); }
inline void executeProductRecallExact(const RobotColor order_list[6]) { executeProductRecallExact(order_list, SPEED_RECALL); }

// ============================================================================
// 9. ฟังก์ชันหลักสำหรับรันภารกิจทั้งหมดในสนาม (MASTER FIELD MISSION LOOP)
// ============================================================================
inline void runFieldMission() {
  unsigned long mission_start_time = millis();

  Serial.println("\n=======================================================");
  Serial.println("  STARTING LOGISTICS ROBOT FIELD MISSION (รุ่น 19)");
  Serial.println("  Sequential Color Delivery Architecture on PuppyBot 6");
  Serial.println("=======================================================");

  // --------------------------------------------------------------------------
  // ขั้นตอนที่ 1: สแกนและบันทึกคำสั่งซื้อทั้ง 6 ช่อง (Order Scanning)
  // --------------------------------------------------------------------------
  Serial.println("\n>>> [PHASE 1] Scanning 6 Order Colors from Start Line <<<");
  test_auto_scan_6colors(SPEED_SCAN, 405);

  // บันทึกรายการคำสั่งซื้อลงในหน่วยความจำ
  RobotColor order_list[6];
  Serial.println("\n--- RECORDED 6 ORDERS ---");
  for (int i = 0; i < 6; i++) {
    order_list[i] = getColorAtSlot(i);
    char buf[64];
    snprintf(buf, sizeof(buf), "Order #%d (Customer #%d) = %s",
             i + 1, i + 1, getColorName(order_list[i]));
    Serial.println(buf);
  }

  // --------------------------------------------------------------------------
  // ขั้นตอนที่ 2 & 3: วิ่งไปเอาเรียงลำดับ: เหลือง -> น้ำเงิน -> ดำ -> เขียว -> แดง -> ขาว
  // --------------------------------------------------------------------------
  Serial.println("\n>>> [PHASE 2 & 3] Sequential Delivery: Yellow -> Blue -> Black -> Green -> Red -> White <<<");
 
  // [1] สีเหลือง: วิ่งจากการอ่านค่าแถบสีเสร็จ ไปเอาเหลือง -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)
  get_Yellow(SPEED_WAREHOUSE);
  put_from_HUB(findConsumerForColor(COLOR_YELLOW, order_list), false, SPEED_CONSUMER);

  // // [2] สีน้ำเงิน: จาก HUB วิ่งไปเอาน้ำเงิน -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)
  // get_Blue(SPEED_WAREHOUSE);
  // put_from_HUB(findConsumerForColor(COLOR_BLUE, order_list), false, SPEED_CONSUMER);

  // // [3] สีดำ: จาก HUB วิ่งไปเอาดำ -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)
  // get_Black(SPEED_WAREHOUSE);
  // put_from_HUB(findConsumerForColor(COLOR_BLACK, order_list), false, SPEED_CONSUMER);

  // // [4] สีเขียว: จาก HUB วิ่งไปเอาเขียว -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)
  // get_Green(SPEED_WAREHOUSE);
  // put_from_HUB(findConsumerForColor(COLOR_GREEN, order_list), false, SPEED_CONSUMER);

  // // [5] สีแดง: จาก HUB วิ่งไปเอาแดง -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)
  // get_Red(SPEED_WAREHOUSE);
  // put_from_HUB(findConsumerForColor(COLOR_RED, order_list), false, SPEED_CONSUMER);

  // // [6] สีขาว: จาก HUB วิ่งไปเอาขาว -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)
  // get_White(SPEED_WAREHOUSE);
  // put_from_HUB(findConsumerForColor(COLOR_WHITE, order_list), false, SPEED_CONSUMER);

  // // --------------------------------------------------------------------------
  // // ขั้นตอนที่ 4: ภารกิจเรียกคืนสินค้าชำรุด (Product Recall)
  // // --------------------------------------------------------------------------
  // if (ENABLE_PRODUCT_RECALL) {
  //   executeProductRecallExact(order_list, SPEED_RECALL);
  // }

  // // --------------------------------------------------------------------------
  // // ขั้นตอนที่ 5: กลับเข้าจุดเริ่มต้น (Return to Start) รับคะแนนโบนัส 10 คะแนน
  // // --------------------------------------------------------------------------
  // Serial.println("\n>>> [PHASE 5] Returning to START Box <<<");
  // returnToStartBox(SPEED_RETURN);

  // // บันทึกสรุปผลภารกิจและแสดงบนจอ TFT
  // mission_summary.duration_ms = millis() - mission_start_time;
  // mission_summary.distance_cm = getDistanceCm();
  // mission_summary.pulses      = abs(get_pulse_Encoder(selection_Encoder));
  // mission_summary.completed   = true;
  // snprintf(mission_summary.finishReason, sizeof(mission_summary.finishReason), "MISSION FINISHED");

  // buzzer(2400, 200);
  // delay(100);
  // buzzer(2400, 200);

  // drawColorSummaryScreen();
}
