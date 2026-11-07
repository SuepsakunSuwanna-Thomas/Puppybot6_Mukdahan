// ============================================================================

// ============================================================================

// LOGISTICS ROBOT COMPETITION (ภารกิจหุ่นยนต์ Logistics Robot รุ่น 19)

// โค้ดแนวทางแบบ Sisakat 4WD 6 ปรับใช้กับ PuppyBot 6 RP2350 / RP2040

// โครงสร้างฟังก์ชันแยก void รายสี และควบคุมจากจุด HUB 100% ตามข้อกำหนด

// ============================================================================

// ============================================================================

#include "color_scanner.h"

#include "config.h"

#include "eeprom_calib.h"

#include "line_follow.h"

#include "motor_drive.h"

#include "sensor_system.h"

#include <Arduino.h>

// ============================================================================

// 1. การตั้งค่าเซอร์โวมือจับ แขนยก ระยะเอ็นโค้ดเดอร์ และความเร็ว

// ============================================================================

const int SERVO_ARM_CH = 1; // เซอร์โวยกแขนขึ้น-ลง (ช่อง 1)

const int SERVO_GRIPPER_CH = 2; // เซอร์โวมือจับหนีบกระป๋อง (ช่อง 2)

const int SERVO_SIDE_CH = 3; // เซอร์โวกางแขนอ่านสีด้านข้าง (ช่อง 3)

// ============================================================================
// องศาเซอร์โว (สามารถปรับจูนตามกลไกของหุ่นได้ที่นี่ - แก้จุดนี้ที่เดียวมีผลทั้งระบบ)
// ============================================================================

int GRIPPER_OPEN_ANG = 75; // องศากางมือจับออกสุด

int GRIPPER_GRAB_ANG = 20; // องศาหนีบกระป๋องแน่น

int ARM_DOWN_ANG =
    100; // องศาลดแขนลงระดับคีบ/วางลงหลุมแท่น 5 cm (ปรับยกขึ้น 15 องศาจากเดิม 115)

int ARM_LIFT_ANG = 30; // องศายกแขนขึ้นสูงพ้นแท่น (ยกขึ้นอีก 20 องศา จากเดิม 50)

// ****************************************************************************
// [จุดแก้ที่เดียว] องศาเซอร์โวภารกิจเรียกคืนสินค้าชำรุด (Product Recall)
// ปรับแก้ค่าตรงนี้ที่เดียว แยกชัดเจนทุกสเต็ป (คีบ / ยก / เอาไปทิ้ง):
// ****************************************************************************

// --- 1. สเต็ปไปหยิบสินค้าชำรุดที่ช่องลูกค้า (Step 1) ---
int ARM_PICKUP_APPROACH_ANG = 65; // องศาเตรียมแขนตอนวิ่งเข้าไปหยิบสินค้าชำรุด (ไม่บังเซนเซอร์ IR)
int ARM_PICKUP_GRAB_ANG     = 65; // องศาลดแขนลงคีบสินค้าชำรุด (ตอนคีบ)
int ARM_PICKUP_LIFT_ANG     = 0;  // องศายกแขนขึ้นหลังคีบเสร็จ (ตอนยก - ทำแยกกัน)

// --- 2. สเต็ปเอาสินค้าชำรุดไปทิ้งที่จุดทิ้ง (Step 2 - ทำแยกต่างหาก) ---
int ARM_DISPOSE_APPROACH_ANG = 80; // องศาแขนตอนถือของชำรุดวิ่งเข้าไปจุดทิ้ง (แก้ที่เดียว)
int ARM_DISPOSE_LIFT_ANG     = 0;  // องศายกแขนขึ้นหลังปล่อยของเสียลงจุดทิ้งแล้ว (แก้ที่เดียว)

int SIDE_ARM_DEPLOY_ANG = 48; // องศากางแขนเซนเซอร์ข้างออกเพื่ออ่านสี (45 องศา)

int SIDE_ARM_RETRACT_ANG = 95; // องศาหุบแขนเซนเซอร์ข้างเก็บ (95 องศา)

// ระยะพัลส์เอ็นโค้ดเดอร์สำหรับเข้าไปคีบและวางกระป๋อง

long ENCODER_CAN_APPROACH_PULSES = 100; // ระยะ PID เดินหน้าเข้าไปเอากระป๋องนิดนึง

long ENCODER_CAN_BACKUP_PULSES = 180; // ระยะถอยหลังตามเอ็นโค้ดเดอร์นิดนึงหลังยก

long ENCODER_CONSUMER_PULSES = 200; // ระยะ PID เข้าไปวาง/ยกที่แท่นหลุมลูกค้า

// ความเร็วมาตรฐานในการวิ่งภารกิจ

int SPEED_SCAN = 40; // ความเร็วตอนสแกนสีแถบคำสั่งซื้อ

int SPEED_NAV = 30; // ความเร็ววิ่งนำทางระหว่างจุด

int SPEED_WAREHOUSE = 28; // ความเร็วเข้าคลังสินค้า

int SPEED_CONSUMER = 30; // ความเร็วเข้าส่งลูกค้า

int SPEED_RECALL = 28; // ความเร็วภารกิจเรียกคืนสินค้า

int SPEED_RETURN = 32; // ความเร็ววิ่งกลับจุดสตาร์ท

int speedl = 18; // ความเร็วเลี้ยงซ้าย

int speedr = 45; // ความเร็วเลี้ยวขวา

int speed1 = 14; // ความเร็วก่อนเลี้ยว

int toy = 180; // ถอย

int de = 200; // ดีเลก่อนเดินไปวาง

int rv1 = 48; // เลี้ยวก่อนวาง
int rv2 = 48; // เลี้ยวก่อนวาง
int rv3 = 48; // เลี้ยวก่อนวาง

// การตั้งค่าระยะ IR สำหรับตรวจสอบกระป๋องในคลัง (ปรับจูนง่ายจากจุดนี้)

float can_check_max_cm =

    10.0f; // ระยะสูงสุดที่นับว่าเจอกระป๋อง (ซม.) ตั้งไว้ 12.0 cm เพื่อให้อยู่ระหว่างกระป๋องเหลือง
           // (~11 cm) และกระป๋องเขียว (~13 cm)

float ir1 = 6.4f; // ระยะหยุดเมื่อเดินหน้าเข้าหากระป๋อง W1

float ir2 = 7.0f; // ระยะหยุดเมื่อเดินหน้าเข้าหากระป๋อง W2

float ir3 = 5.9f;

// กำหนดสีที่หมดสต็อก หรือสีชำรุดล่วงหน้า (หากต้องการบังคับทดสอบ)

RobotColor OUT_OF_STOCK_COLOR =

    COLOR_UNKNOWN; // เช่น COLOR_RED หรือ COLOR_UNKNOWN (เช็คจริงด้วย IR)

bool ENABLE_PRODUCT_RECALL = true; // เปิด/ปิดภารกิจเรียกคืนสินค้าชำรุด

RobotColor RECALLED_COLOR =

    COLOR_UNKNOWN; // เช่น COLOR_BLUE หรือ COLOR_UNKNOWN (อ่านจากจุดเช็ค)

// ============================================================================

// 2. ฟังก์ชันควบคุมมือจับและแขนยก สไตล์ Sisakat 4WD

// ============================================================================

// คีบกระป๋องและยกขึ้นข้ามแท่น (Grab Can)

inline void grabCan() {

  delay(30);

  servoRun(SERVO_GRIPPER_CH, GRIPPER_GRAB_ANG); // หนีบกระป๋องแน่น

  delay(250);

  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG); // ยกแขนข้ามแท่น 5 cm

  delay(200);
}

// วางกระป๋องลงในหลุมแท่นลูกค้าให้ตั้งตรง (Release Can into 5 cm Hole Platform)

inline void releaseCan() {

  servoRun(SERVO_ARM_CH, 65); // ลดระดับลงใกล้ปากหลุม

  delay(220);

  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับ ปล่อยกระป๋องลงหลุม

  delay(220);

  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG); // ยกแขนขึ้นหลบขอบแท่น ไม่ให้เกี่ยวชน

  delay(150);
}

// ----------------------------------------------------------------------------
// ฟังก์ชันกลางสำหรับการหยิบสินค้าชำรุด (ปรับแก้ตรรกะที่นี่ที่เดียว มีผลทุกช่อง 1-6)
// ----------------------------------------------------------------------------

// 1. เตรียมแขนตอนวิ่งเข้าไปหยิบสินค้าชำรุด (กางมือจับ + ยกแขนไม่บัง IR)
inline void prepareArmForDefectivePickup() {
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับออกเตรียมครอบกระป๋อง
  delay(150);
  servoRun(SERVO_ARM_CH, ARM_PICKUP_APPROACH_ANG); // ยกแขนตามค่าที่ตั้งไว้ (ARM_PICKUP_APPROACH_ANG)
  delay(150);
}

// 2. คำสั่งคีบสินค้าชำรุด (ลดแขนลงระดับคีบ + หนีบมือจับแน่น)
inline void keeb_defective_can() {
  servoRun(SERVO_ARM_CH, ARM_PICKUP_GRAB_ANG); // ลดแขนลงระดับคีบของชำรุด (ARM_PICKUP_GRAB_ANG)
  delay(200);
  servoRun(SERVO_GRIPPER_CH, GRIPPER_GRAB_ANG); // หนีบกระป๋องแน่น
  delay(250);
}

// 3. คำสั่งยกสินค้าชำรุดหลังคีบ (ทำแยกกัน ไม่รวมกับตอนคีบ)
inline void yok_defective_can() {
  servoRun(SERVO_ARM_CH, ARM_PICKUP_LIFT_ANG); // สั่งยกแขนขึ้นพ้นแท่นหลุม (ARM_PICKUP_LIFT_ANG)
  delay(250);
}

// 4. ฟังก์ชันรวม: คีบ แล้วตามด้วย ยก (ทำแยกกันเป็น 2 สเต็ป)
inline void pickupDefectiveCan() {
  keeb_defective_can(); // สเต็ป 1: คีบ
  yok_defective_can();  // สเต็ป 2: หลังคีบต้องยกด้วย (ทำแยกกัน)
}

// ----------------------------------------------------------------------------
// ฟังก์ชันสำหรับนำสินค้าชำรุดไปทิ้งที่จุดทิ้ง (Step 2 - ทำแยกต่างหาก)
// ----------------------------------------------------------------------------

// คำสั่งปล่อยสินค้าชำรุดลงจุดทิ้ง แล้วยกแขนหลบ (ทำแยกกัน)
inline void releaseDefectiveToDisposal() {
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // กางมือจับ ปล่อยของเสียลงจุดทิ้ง
  delay(250);
  servoRun(SERVO_ARM_CH, ARM_DISPOSE_LIFT_ANG); // สั่งยกแขนขึ้นหลบขอบจุดทิ้ง (ARM_DISPOSE_LIFT_ANG)
  delay(200);
}

inline void to_hub() {

  // 4. หันหลังกลับและวิ่งกลับมาตั้งลำที่จุด HUB (หันหน้าเข้าหา Consumer)

  Fline_B_by_time(40, 150);

  turnRight_by_stop(12);

  delay(200);

  Fline_by_time(40, 250);

  turnRight(40);

  delay(50);

  Fline_by_pass(80, 1);

  turnLeft(40);

  delay(200);
}

// คำสั่งย่อสไตล์ Sisakat

inline void up() {

  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG);

  delay(180);
}

inline void down() {

  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG);

  delay(180);
}

inline void keeb() {

  servoRun(SERVO_GRIPPER_CH, GRIPPER_GRAB_ANG);

  delay(200);
}

inline void kang() {

  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG);

  delay(200);
}

// ============================================================================

// 3. ตรวจสอบสินค้าในช่องคลังด้วยเซนเซอร์วัดระยะทาง IR (GP2Y0A41SK0F ADC 14)

// ============================================================================

inline bool checkCanPresent() {

  stop_4w();

  delay(250); // หน่วงเวลาให้หุ่นยนต์หยุดนิ่งสนิทก่อนอ่านค่า

  // อ่านค่าเฉลี่ยหลายครั้งเพื่อกรอง noise และความผันผวน

  float sum_dist = 0.0f;

  long sum_raw = 0;

  const int SAMPLES = 7;

  for (int i = 0; i < SAMPLES; i++) {

    sum_dist += get_ir_distance_cm();

    sum_raw += readRawIR();

    delay(15);
  }

  float dist_cm = sum_dist / (float)SAMPLES;

  int avg_raw = sum_raw / SAMPLES;

  Serial.print("[IR Sensor] Measured Distance: ");

  Serial.print(dist_cm, 1);

  Serial.print(" cm (Raw ADC: ");

  Serial.print(avg_raw);

  Serial.print(")");

  // เกณฑ์ตรวจจับกระป๋อง:
  // - มีกระป๋อง: ระยะต้องไม่เกิน can_check_max_cm (ขยายระยะเป็น 13.5 ซม.)
  // - ไม่มีกระป๋อง: แสงจะยิงทะลุเกินระยะนี้ -> ถือว่าสินค้าหมดสต็อก ไป W2
  bool has_can = (dist_cm <= can_check_max_cm);

  if (has_can) {

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

int findConsumerForColor(RobotColor color,

                         const RobotColor *order_list) {

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

    Serial.println(

        "[W2 Route] Out of stock at YELLOW -> Heading to Warehouse 2...");

    // 2. เลี้ยวขวามุ่งหน้าสู่คลังสำรอง W2

    Fline_by_time(70, 250);

    turnRight(40);

    Fline_by_pass(65, 1);

    Fline(20, 0);

    turnLeft_by_stop(90);

    servoRun(SERVO_GRIPPER_CH,

             GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

    servoRun(1, 48); // ยกแขนครอบตัวกระป๋อง

    Fline_until_can(35, ir3);

    grabCan();

    to_hub();

    break;

    // ------------------------------------------------------------------------

    // กรณีสีน้ำเงินหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง Blue (เส้นบน)

    // ------------------------------------------------------------------------

  case COLOR_BLUE:

    Serial.println(

        "[W2 Route] Out of stock at BLUE -> Heading to Warehouse 2...");

    // 2. เลี้ยวขวามุ่งหน้าสู่คลังสำรอง W2

   Fline_by_time(90, 180); 

    turnRight(40);

    Fline_by_pass(65, 2);

    Fline(20, 0);

    turnLeft_by_stop(90);

    servoRun(SERVO_GRIPPER_CH,

             GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

    servoRun(1, 48); // ยกแขนครอบตัวกระป๋อง

    Fline_until_can(35, ir3);

    grabCan();

    to_hub();

    break;

    // ------------------------------------------------------------------------

    // กรณีสีดำหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง Black (เส้นล่าง)

    // ------------------------------------------------------------------------

  case COLOR_BLACK:

    Serial.println(

        "[W2 Route] Out of stock at BLACK -> Heading to Warehouse 2...");

    // 1. ถอย/กลับตัวออกจากช่อง Black กลับเข้าสู่จุด HUB

    Fline_by_time(90, 180);

    delay(50);

    turnRight(40);

    Fline(20, 0);

    turnLeft_by_stop(90);

    servoRun(SERVO_GRIPPER_CH,

             GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

    servoRun(1, 48); // ยกแขนครอบตัวกระป๋อง

    Fline_until_can(35, ir3);

    to_hub();

    break;

    // ------------------------------------------------------------------------

    // กรณีสีเขียวหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง Green (เส้นกึ่งกลาง ถัดจากเหลือง)

    // ------------------------------------------------------------------------

  case COLOR_GREEN:

    Serial.println(

        "[W2 Route] Out of stock at GREEN -> Heading to Warehouse 2...");

    // 2. เลี้ยวขวามุ่งหน้าสู่คลังสำรอง W2

    turnRight_by_stop(55);

    delay(50);

    Fline_by_pass(65, 1);

    Fline(20, 0);

    turnLeft_by_stop(90);

    servoRun(SERVO_GRIPPER_CH,

             GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

    servoRun(1, 48); // ยกแขนครอบตัวกระป๋อง

    Fline_until_can(35, ir3);

    grabCan();

    to_hub();

    break;

    // ------------------------------------------------------------------------

    // กรณีสีแดงหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง Red (เส้นบน ถัดจากน้ำเงิน)

    // ------------------------------------------------------------------------

  case COLOR_RED:

    Serial.println(

        "[W2 Route] Out of stock at RED -> Heading to Warehouse 2...");

    turnRight_by_stop(55);

    delay(50);

    Fline_by_pass(65, 2);

    Fline(20, 0);

    turnLeft_by_stop(90);

    servoRun(SERVO_GRIPPER_CH,

             GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

    servoRun(1, 48); // ยกแขนครอบตัวกระป๋อง

    Fline_until_can(35, ir3);

    grabCan();

    to_hub();

    break;

    // ------------------------------------------------------------------------

    // กรณีสีขาวหมดสต็อก: หุ่นยนต์หยุดตรวจจับที่ช่อง White (เส้นล่าง ถัดจากดำ)

    // ------------------------------------------------------------------------

  case COLOR_WHITE:

    Serial.println(

        "[W2 Route] Out of stock at WHITE -> Heading to Warehouse 2...");

    // 2. เลี้ยวขวามุ่งหน้าสู่คลังสำรอง W2

    turnRight_by_stop(55);

    delay(50);

    Fline(20, 0);

    turnLeft_by_stop(90);

    servoRun(SERVO_GRIPPER_CH,

             GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

    servoRun(1, 48); // ยกแขนครอบตัวกระป๋อง

    Fline_until_can(35, ir3);

    grabCan();

    to_hub();

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

inline void get_Warehouse2_Spare(RobotColor color) {

  get_Warehouse2_Spare(color, SPEED_WAREHOUSE);
}

// ============================================================================

// 5. ฟังก์ชันแยกสำหรับวิ่งไปหยิบสินค้าแต่ละสี (Warehouse 1)

// ============================================================================

// [5.1] สีเหลือง: วิ่งจากการอ่านค่าแถบสีเสร็จ เลี้ยวซ้าย ซ้าย ผ่าน 1 เลี้ยวขวา ผ่าน 4 เส้น

//       -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น -> ถอยหลังตาม encoder

//       นิดนึง -> หมุนตัวเพื่อให้หน้าอยู่ตรง HUB

inline void get_Yellow(int speed) {

  Serial.println(

      "\n[Action] >>> Step 1: Going to YELLOW from Color Scan Line <<<");

  // 1. เลี้ยวซ้าย ซ้าย ผ่าน 1 เลี้ยวขวา ผ่าน 4 เส้น

  turnLeft(40);

  turnLeft(35);

  Fline_by_pass(50, 1);

  turnRight(35);

  Fline_by_pass(100, 2);

  Fline_by_pass(65, 1);

  Fline(35, 6);

  delay(300);

  // 2. เช็คว่ามีกระป๋องไหม

  bool has_can =

      (OUT_OF_STOCK_COLOR == COLOR_YELLOW) ? false : checkCanPresent();

  if (!has_can) {

    // หากไม่มี ไปเอาที่คลังสำรอง แล้ว return ทันที ป้องกันคำสั่งด้านล่างทำงานซ้ำ!

    get_Warehouse2_Spare(COLOR_YELLOW, speed);

    return;

  } else {

    // หากมีกระป๋อง:

    Fline_B_by_time(35, 120);

    servoRun(SERVO_GRIPPER_CH,

             GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

    delay(300);

    servoRun(SERVO_ARM_CH, ARM_DOWN_ANG); // ลดแขนลงครอบตัวกระป๋อง

    Fline_until_can(speed1, ir1);

    // ยกขึ้น

    grabCan();

    // ถอยหลังตามเอ็นโค้ดเดอร์นิดนึง

    // Backword_Notline_by_Encoder(speed, ENCODER_CAN_BACKUP_PULSES);

    Fline_B_by_time(40, 220);
    delay(70);

    // // หมุนตัวเพื่อให้หน้าอยู่ตรง HUB

    turnLeft_by_stop(40);


    delay(50);
  }

  Serial.println("[Action] Yellow can ready at HUB!");
}

inline void get_Yellow() { get_Yellow(SPEED_WAREHOUSE); }

// [5.2] สีน้ำเงิน: พอหุ่นยนต์ไปวางเหลืองเสร็จแล้วหันหน้าออกมาที่จุด HUB

//       หมุนซ้าย เลี้ยวขวา -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น ->

//       ถอยหลังตาม encoder นิดนึง -> หมุน เลี้ยวซ้าย เลี้ยวขวา หน้าจะอยู่ตรง HUB

inline void get_Blue(int speed) {

  Serial.println("\n[Action] >>> Step 2: Going to BLUE from HUB <<<");

  // 1. หมุนซ้าย เลี้ยวขวา

  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG);

  turnLeft(40);

  turnRight(45);

  delay(250);

  Fline_B_by_time(50, 50);
  delay(100);
  Fline_by_time(50, 50);

  delay(100);

  // 2. เช็คว่ามีกระป๋องไหม

  bool has_can = (OUT_OF_STOCK_COLOR == COLOR_BLUE) ? false : checkCanPresent();

  if (!has_can) {

    // หากไม่มี ไปเอาที่คลังสำรอง แล้ว return ทันที ป้องกันคำสั่งด้านล่างทำงานซ้ำ!

    get_Warehouse2_Spare(COLOR_BLUE, speed);

    return;

  } else {

    // หากมีกระป๋อง:
    Fline_B_by_time(25, 230);

    servoRun(SERVO_GRIPPER_CH,

             GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

    delay(300);

    servoRun(SERVO_ARM_CH, ARM_DOWN_ANG); // ลดแขนลงครอบตัวกระป๋อง

    delay(300);

    Fline_until_can(speed1, ir1);

    // ยกขึ้น

    grabCan();

    //  ถอย

    Fline_B_by_time(20, 100);

    // หมุน เลี้ยวซ้าย เลี้ยวขวา หน้าจะอยู่ตรง HUB

    turnRight_by_stop(40);

    turnRight(43);

    delay(50);
  }

  Serial.println("[Action] Blue can ready at HUB!");
}

inline void get_Blue() { get_Blue(SPEED_WAREHOUSE); }

// [5.3] สีดำ: หมุนขวา เลี้ยวซ้าย -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น ->

// ถอยหลังตาม encoder นิดนึง -> หมุน เลี้ยวขวา เลี้ยวซ้าย หน้าจะอยู่ที่ HUB

inline void get_Black(int speed) {

  Serial.println("\n[Action] >>> Step 3: Going to BLACK from HUB <<<");

  // 1. หมุนขวา เลี้ยวซ้าย

  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG);

  turnRight(40);

  turnLeft(43);

  delay(250);

  Fline_B_by_time(50, 60);
  delay(100);
  Fline_by_time(50, 60);

  delay(100);

  // 2. เช็คว่ามีกระป๋องไหม

  bool has_can =

      (OUT_OF_STOCK_COLOR == COLOR_BLACK) ? false : checkCanPresent();

  if (!has_can) {

    get_Warehouse2_Spare(COLOR_BLACK, speed);

    return;
  }

  // หากมีกระป๋อง:

  Fline_B_by_time(40, 175);

  servoRun(SERVO_GRIPPER_CH,

           GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

  delay(300);

  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG); // ลดแขนลงครอบตัวกระป๋อง

  Fline_until_can(speed1, ir1);

  // ยกขึ้น

  grabCan();

  //  ถอย

  Fline_B_by_time(20, 100);

  // หมุน เลี้ยวขวา เลี้ยวซ้าย หน้าจะอยู่ที่ HUB

  turnLeft_by_stop(55);

  turnLeft(37);

  delay(50);

  Serial.println("[Action] Black can ready at HUB!");
}

inline void get_Black() { get_Black(SPEED_WAREHOUSE); }

// [5.4] สีเขียว: วิ่งผ่าน 1 เส้น -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น ->

// ถอยหลังตาม encoder นิดนึง -> หมุน ผ่าน 2 เส้น หน้าจะอยู่ที่ HUB

inline void get_Green(int speed) {

  Serial.println("\n[Action] >>> Step 4: Going to GREEN from HUB <<<");

  // 1. วิ่งผ่าน 1 เส้นถอยหลังตามเอ็นโค้ดเดอร์นิดนึง

  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG);

  Fline_by_pass(65, 1);

  Fline(40, 6);

  delay(500);

  // 2. เช็คว่ามีกระป๋องไหม

  bool has_can =

      (OUT_OF_STOCK_COLOR == COLOR_GREEN) ? false : checkCanPresent();

  if (!has_can) {

    get_Warehouse2_Spare(COLOR_GREEN, speed);

    return;
  }

  // หากมีกระป๋อง:

  Fline_B_by_time(40, 170);

  servoRun(SERVO_GRIPPER_CH,

           GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

  delay(300);

  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG); // ลดแขนลงครอบตัวกระป๋อง

  Fline_until_can(speed1, ir1);

  // ยกขึ้น

  grabCan();

  Fline_B_by_time(40, 220);
  delay(70);

  // // หมุนตัวเพื่อให้หน้าอยู่ตรง HUB

  turnLeft_by_stop(40);

  // Backword_Notline_by_Encoder(40, 160);


  delay(50);

  Serial.println("[Action] Green can ready at HUB!");
}

inline void get_Green() { get_Green(SPEED_WAREHOUSE); }

// [5.5] สีแดง: ผ่าน 1 เส้น เลี้ยวซ้าย เลี้ยวขวา -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง

// -> ยกขึ้น -> ถอยหลังตาม encoder นิดนึง -> หมุนขวา เลี้ยวขวา ผ่าน 1 เส้น เรียก HUB

inline void get_Red(int speed) {

  Serial.println("\n[Action] >>> Step 5: Going to RED from HUB <<<");

  // 1. หมุนซ้าย เลี้ยวขวา ผ่าน1

  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG);

  turnLeft(40);

  turnRight(45);

  Fline(40, 6), delay(250);

  // 2. เช็คว่ามีกระป๋องไหม

  bool has_can = (OUT_OF_STOCK_COLOR == COLOR_RED) ? false : checkCanPresent();

  if (!has_can) {

    get_Warehouse2_Spare(COLOR_RED, speed);

    return;
  }

  // หากมีกระป๋อง:

  Fline_B_by_time(50, 115);

  servoRun(SERVO_GRIPPER_CH,

           GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

  delay(300);

  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG); // ลดแขนลงครอบตัวกระป๋อง

  Fline_until_can(speed1, ir1);

  // ยกขึ้น

  grabCan();

  //  ถอย

  Fline_B_by_time(20, 100);

  // หมุน เลี้ยวซ้าย เลี้ยวขวา หน้าจะอยู่ตรง HUB

  turnRight_by_stop(45);

  Fline_B_by_time(50, 50);

  turnRight(45);

  Fline_by_pass(60, 1);

  delay(50);

  Serial.println("[Action] Red can ready at HUB!");
}

inline void get_Red() { get_Red(SPEED_WAREHOUSE); }

// [5.6] สีขาว: เลี้ยวขวา เลี้ยวซ้าย -> เช็คกระป๋อง -> วิ่ง PID ตาม encoder นิดนึง -> ยกขึ้น

// -> ถอยหลังตาม encoder นิดนึง -> หมุนซ้าย เลี้ยวซ้าย ผ่าน 1 เส้น เรียก HUB

inline void get_White(int speed) {

  Serial.println("\n[Action] >>> Step 6: Going to WHITE from HUB <<<");

  // 1. หมุนซ้าย เลี้ยวขวา ผ่าน1

  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG);

  turnRight(45);

  turnLeft(45);

  Fline(40, 6), delay(250);

  // 2. เช็คว่ามีกระป๋องไหม

  bool has_can =

      (OUT_OF_STOCK_COLOR == COLOR_WHITE) ? false : checkCanPresent();

  if (!has_can) {

    get_Warehouse2_Spare(COLOR_WHITE, speed);

    return;
  }

  // หากมีกระป๋อง:

  Fline_B_by_time(50, 115);

  servoRun(SERVO_GRIPPER_CH,

           GRIPPER_OPEN_ANG); // กางมือจับออกก่อนเพื่อเตรียมครอบกระป๋อง

  delay(300);

  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG); // ลดแขนลงครอบตัวกระป๋อง

  Fline_until_can(speed1, ir1);

  // ยกขึ้น

  grabCan();

  //  ถอย

  Fline_B_by_time(20, 100);

  // หมุน เลี้ยวขวา เลี้ยวซ้าย หน้าจะอยู่ที่ HUB

  turnLeft_by_stop(55);

  Fline_B_by_time(50, 50);

  turnLeft(40);

  Fline_by_pass(60, 1);

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

    // ช่อง 1: ผ่าน 2 เลี้ยวซ้าย เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนซ้าย ผ่าน
    // 3

    // เส้น

    // ------------------------------------------------------------------------

  case 1:

    delay(50);

    Fline_by_pass(100, 1);

    Fline_by_pass(50, 1);

    delay(20);

    turnLeft(rv1);

    delay(de);

    Fline_B_by_time(35, toy);

    if (is_pickup) {
      prepareArmForDefectivePickup();
    }

    Fline_until_can(30, ir2);

    delay(150);

    if (is_pickup) {
      keeb_defective_can(); // 1. คีบสินค้าชำรุด
      yok_defective_can();  // 2. หลังคีบต้องยกด้วย (ทำแยกกัน)
    } else {
      releaseCan();
    }

    Fline_B_by_time(40, 100);
    turnLeft_by_stop(55);

    Fline_by_pass(80, 2);

    delay(50);

    break;

    // ------------------------------------------------------------------------

    // ช่อง 2: ผ่าน 2 เลี้ยวขวา เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนขวา ผ่าน
    // 3

    // เส้น

    // ------------------------------------------------------------------------

  case 2:

    delay(50);

    Fline_by_pass(100, 1);

    Fline_by_pass(50, 1);

    delay(20);

    turnRight(rv1);

    delay(de);

    Fline_B_by_time(35, toy);

    if (is_pickup) {
      prepareArmForDefectivePickup();
    }

    Fline_until_can(30, ir2);

    delay(150);

    if (is_pickup) {
      keeb_defective_can(); // 1. คีบสินค้าชำรุด
      yok_defective_can();  // 2. หลังคีบต้องยกด้วย (ทำแยกกัน)
    } else {
      releaseCan();
    }

    Fline_B_by_time(40, 100);
    turnLeft_by_stop(55);

    Fline_by_pass(80, 2);

    delay(50);

    break;

    // ------------------------------------------------------------------------

    // ช่อง 3: ผ่าน 1 เลี้ยวซ้าย เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนซ้าย ผ่าน
    // 2

    // เส้น

    // ------------------------------------------------------------------------

  case 3:

    delay(50);

    Fline_by_pass(50, 1);

    turnLeft(rv2);

    delay(de);

    Fline_B_by_time(35, toy);

    if (is_pickup) {
      prepareArmForDefectivePickup();
    }

    Fline_until_can(30, ir2);

    delay(150);

    if (is_pickup) {
      keeb_defective_can(); // 1. คีบสินค้าชำรุด
      yok_defective_can();  // 2. หลังคีบต้องยกด้วย (ทำแยกกัน)
    } else {
      releaseCan();
    }

    Fline_B_by_time(40, 100);
    turnLeft_by_stop(55);

    Fline_by_pass(80, 1);

    delay(50);

    break;

    // ------------------------------------------------------------------------

    // ช่อง 4: ผ่าน 1 เลี้ยวขวา เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนขวา ผ่าน
    // 2

    // เส้น

    // ------------------------------------------------------------------------

  case 4:

    delay(50);

    Fline_by_pass(50, 1);

    turnRight(rv2);

    delay(de);

    Fline_B_by_time(35, toy);

    if (is_pickup) {
      prepareArmForDefectivePickup();
    }

    Fline_until_can(30, ir2);

    delay(150);

    if (is_pickup) {
      keeb_defective_can(); // 1. คีบสินค้าชำรุด
      yok_defective_can();  // 2. หลังคีบต้องยกด้วย (ทำแยกกัน)
    } else {
      releaseCan();
    }

    Fline_B_by_time(40, 100);
    turnLeft_by_stop(55);

    Fline_by_pass(80, 1);

    delay(50);

    break;

    // ------------------------------------------------------------------------

    // ช่อง 5: เลี้ยวซ้าย เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนซ้าย ผ่าน 1 เส้น

    // ------------------------------------------------------------------------

  case 5:

    delay(50);

    turnLeft(rv3);

    delay(de);

    Fline_B_by_time(35, toy);

    if (is_pickup) {
      prepareArmForDefectivePickup();
    }

    Fline_until_can(30, ir2);

    delay(150);

    if (is_pickup) {
      keeb_defective_can(); // 1. คีบสินค้าชำรุด
      yok_defective_can();  // 2. หลังคีบต้องยกด้วย (ทำแยกกัน)
    } else {
      releaseCan();
    }

    Fline_B_by_time(40, 100);
    turnLeft_by_stop(48);

    delay(50);

    break;

    // ------------------------------------------------------------------------

    // ช่อง 6: เลี้ยวขวา เดินหน้าแบบ PID ตามเอ็นโค้ดเดอร์ แล้ววาง/ยก ถอย หมุนขวา ผ่าน 1 เส้น

    // ------------------------------------------------------------------------

  case 6:

    delay(50);

    turnRight(rv3);

    delay(de);

    Fline_B_by_time(35, toy);

    if (is_pickup) {
      prepareArmForDefectivePickup();
    }

    Fline_until_can(30, ir2);

    delay(150);

    if (is_pickup) {
      keeb_defective_can(); // 1. คีบสินค้าชำรุด
      yok_defective_can();  // 2. หลังคีบต้องยกด้วย (ทำแยกกัน)
    } else {
      releaseCan();
    }

    Fline_B_by_time(40, 100);
    turnLeft_by_stop(48);

    delay(50);

    break;

  default:

    Serial.println("[HUB] Invalid slot number!");

    break;
  }

  buzzer(2000, 60);

  Serial.println("[HUB] Operation completed. Robot is now back at HUB!");
}

inline void put_from_HUB(int slot, bool is_pickup) {

  put_from_HUB(slot, is_pickup, SPEED_CONSUMER);
}

inline void put_from_HUB(int slot) {

  put_from_HUB(slot, false, SPEED_CONSUMER);
}

// ============================================================================
// ฟังก์ชันแยกขั้นตอนสำหรับทดสอบภารกิจเรียกคืนสินค้าชำรุด (Modular Product Recall)
// ============================================================================

// [ขั้นตอนที่ 1] จาก HUB วิ่งไปคีบสินค้าชำรุดที่ช่องลูกค้า (1-6) แล้วพากลับมาที่ HUB
inline void recall_step1_pickup_from_consumer(int slot) {
  Serial.printf(
      "\n>>> [RECALL STEP 1] Pickup defective can from slot #%d <<<\n", slot);
  put_from_HUB(slot, true, SPEED_CONSUMER);
  Serial.printf(
      ">>> [RECALL STEP 1] Completed! Defective can is now at HUB <<<\n\n");
  buzzer(2000, 100);
}

// [ขั้นตอนที่ 2] จาก HUB นำสินค้าชำรุดไปทิ้งลงจุดทิ้ง แล้วไปคีบกระป๋องสำรองอันใหม่ กลับมาที่ HUB
inline void recall_step2_dispose_and_get_spare() {
  Serial.println(
      "\n>>> [RECALL STEP 2] Dispose defective can & Grab spare can <<<");

  // 6. เลี้ยวขวา ผ่าน 1 เลี้ยวซ้าย วิ่งตามเอ็นโค้ดเดอร์ แล้ววางลง
  Fline(40, 0);
  Fline_by_time(70, 200);
  turnRight(40);
  Fline_by_pass(65, 1);
  Fline(20, 0);
  turnLeft_by_stop(90);

  servoRun(SERVO_ARM_CH, ARM_DISPOSE_APPROACH_ANG); // ปรับองศาแขนถือของชำรุดเข้าจุดทิ้ง (แก้ที่เดียว)
  Fline_until_can(35, ir3);
  delay(300);
  releaseDefectiveToDisposal(); // ปล่อยของเสียลงจุดทิ้ง แล้วยกแขนหลบ (ทำแยกกัน)

  // 7. ถอยหลัง PID ตามเอ็นโค้ดเดอร์ หมุนซ้าย ผ่าน 3 เส้น เลี้ยวขวา วิ่งเข้าไปตามเอ็นโค้ดเดอร์
  // แล้วยก
  Fline_B(40, 0);
  turnLeft_by_stop(20);
  Fline_by_pass(40, 3);
  delay(70);
  turnRight(45);
  prepareArmForDefectivePickup(); // กางมือจับและยกแขนเตรียมครอบกระป๋อง ไม่บัง IR (แก้จุดเดียวที่ ARM_PICKUP_APPROACH_ANG)
  Fline_until_can(35, ir3);
  grabCan(); // ยกของชิ้นใหม่ทดแทนขึ้นมา

  // 8. ถอยหลังตามเอ็นโค้ดเดอร์ หมุนขวา ผ่าน 1 เลี้ยวขวา ผ่าน 1 ก็เรียก HUB
  Fline_B(40, 0);
  turnRight_by_stop(20);
  Fline_by_pass(50, 1);
  delay(70);
  turnRight(30);
  Fline_by_pass(40, 1); // มาถึง HUB

  Serial.println(
      ">>> [RECALL STEP 2] Completed! Replacement can is now at HUB <<<\n");
  buzzer(2200, 100);
}

// [ขั้นตอนที่ 3] จาก HUB นำกระป๋องใหม่ไปวางคืนที่ช่องลูกค้า (1-6) แล้วกลับมาที่ HUB
inline void recall_step3_deliver_spare(int slot) {
  Serial.printf(
      "\n>>> [RECALL STEP 3] Deliver replacement can to slot #%d <<<\n", slot);
  put_from_HUB(slot, false, SPEED_CONSUMER);
  Serial.printf(">>> [RECALL STEP 3] Completed! Replacement delivered to slot "
                "#%d <<<\n\n",
                slot);
  buzzer(2400, 150);
}

// ฟังก์ชันรวมทดสอบ Product Recall ครบทั้ง 3 สเต็ปจากจุด HUB โดยระบุช่องเป้าหมายได้โดยตรง
// (ไม่ต้องรอสแกนสี)
inline void test_recall_mission(int slot) {
  Serial.println("\n=======================================================");
  Serial.printf("  STARTING TEST RECALL FROM HUB (Target Slot: %d)\n", slot);
  Serial.println("=======================================================");
  recall_step1_pickup_from_consumer(slot);
  recall_step2_dispose_and_get_spare();
  recall_step3_deliver_spare(slot);
  Serial.println(">>> [ALL RECALL STEPS COMPLETED 100%] <<<");
  buzzer(2600, 300);
}

// ============================================================================
// ฟังก์ชันทดสอบอ่านเซนเซอร์สีด้านข้าง (TCS Side Color Testing)
// ============================================================================

// [ทดสอบแบบที่ 1: ส่องอ่านค่าสีสดๆ อยู่กับที่ (Live Reading)]
// กางแขนข้างออก (Servo 3 -> 45 องศา) อ่านค่า RGB และระบุชื่อสีขึ้นจอ TFT + Serial แบบ Real-Time
// กดปุ่มบนบอร์ดเพื่อสิ้นสุดการทดสอบและหุบแขนกลับ
inline void test_side_color_live() {
  Serial.println("\n=======================================================");
  Serial.println("  LIVE SIDE COLOR SENSOR TEST (TCS Side Arm)");
  Serial.println("  Deploying arm (Servo 3 -> 45 deg)...");
  Serial.println("  Press onboard button to exit.");
  Serial.println("=======================================================");

  servoRun(SERVO_SIDE_CH, SIDE_ARM_DEPLOY_ANG); // กางแขนเซนเซอร์ข้างออกมาที่ 45 องศา
  delay(300);

  tft_.fillScreen(ST7735_BLACK);
  drawHeaderBar("TEST SIDE COLOR", ST7735_YELLOW, ST7735_WHITE);
  drawFooterBar("PRESS BTN TO EXIT", 0x0A22, ST7735_WHITE, ST7735_GREEN);

  unsigned long last_print = 0;
  while (!isButtonPressed()) {
    int r = Read_Color_TCS_B(0);
    int g = Read_Color_TCS_B(1);
    int b = Read_Color_TCS_B(2);
    RobotColor col = detectColorSide();

    if (millis() - last_print >= 80) {
      last_print = millis();

      char rgbBuf[32];
      snprintf(rgbBuf, sizeof(rgbBuf), "R:%3d G:%3d B:%3d", r, g, b);
      printText(10, 42, rgbBuf, 1, ST7735_WHITE, ST7735_BLACK);

      uint16_t tftColor = ST7735_WHITE;
      switch (col) {
        case COLOR_RED:    tftColor = ST7735_RED; break;
        case COLOR_GREEN:  tftColor = ST7735_GREEN; break;
        case COLOR_BLUE:   tftColor = ST7735_BLUE; break;
        case COLOR_YELLOW: tftColor = ST7735_YELLOW; break;
        case COLOR_WHITE:  tftColor = ST7735_WHITE; break;
        case COLOR_BLACK:  tftColor = 0x7BEF; break;
        default:           tftColor = ST7735_MAGENTA; break;
      }

      char colBuf[32];
      snprintf(colBuf, sizeof(colBuf), "COLOR: %-8s", getColorName(col));
      printText(10, 68, colBuf, 2, tftColor, ST7735_BLACK);

      Serial.printf("[SIDE TCS] R:%3d G:%3d B:%3d => Detected: %s\n",
                    r, g, b, getColorName(col));
    }
    delay(10);
  }

  buzzer(2000, 100);
  servoRun(3, 95); // หุบแขนเก็บ
  delay(300);
  Serial.println("Side arm retracted. Live test exited.");
}

// [ทดสอบแบบที่ 2: วิ่งสแกนสีด้านข้างตามเส้นจริงจากจุด HUB]
inline RobotColor test_recall_scan_side_color() {
  Serial.println("\n***************************************************");
  Serial.println(">>> [TEST SIDE SCAN] Scanning Side Color from HUB <<<");
  Serial.println("***************************************************");

  // 1. ตอนนี้หุ่นยนต์อยู่ที่จุด HUB -> เลี้ยวซ้าย ผ่าน 1 เลี้ยวซ้าย
  turnLeft(40);
  Fline_by_pass(45, 1);
  delay(70);
  turnLeft(40);
  Fline(35, 0);

  // 2. กางแขนเซนเซอร์ข้างออกมาก่อนเริ่มวิ่งสแกน (45 องศา)
  servoRun(SERVO_SIDE_CH, SIDE_ARM_DEPLOY_ANG);
  delay(250);

  int non_white_counts[7] = {0};
  int white_count = 0;
  unsigned long scan_timeout = millis();
  unsigned long last_sample_time = 0;

  // วิ่งตามเส้นจนเซนเซอร์หน้า (เซนเซอร์ 0) แตะเจอเส้นดำ
  while (!isFrontSensorOnLine(0) && (millis() - scan_timeout < 4000)) {
    if (millis() - last_sample_time >= 4) {
      last_sample_time = millis();
      RobotColor c = detectColorSide();
      if (c != COLOR_UNKNOWN) {
        if (c != COLOR_WHITE) {
          non_white_counts[c]++;
          Serial.printf(">> [SIDE SCAN] Detected: %s\n", getColorName(c));
        } else {
          white_count++;
        }
      }
    }
    lineFollow_PID_auto(30);
    delay(2);
  }

  // วิ่งพร้อมสแกนต่อมาอีกนิดนึง (Overrun Scan)
  long overrun_start_pulse = get_pulse_Encoder(selection_Encoder);
  long extra_scan_pulses = 150;
  unsigned long overrun_timeout = millis();
  while (abs(get_pulse_Encoder(selection_Encoder) - overrun_start_pulse) <
             extra_scan_pulses &&
         (millis() - overrun_timeout < 1500)) {
    if (millis() - last_sample_time >= 4) {
      last_sample_time = millis();
      RobotColor c = detectColorSide();
      if (c != COLOR_UNKNOWN) {
        if (c != COLOR_WHITE) {
          non_white_counts[c]++;
          Serial.printf(">> [SIDE SCAN OVERRUN] Detected: %s\n", getColorName(c));
        } else {
          white_count++;
        }
      }
    }
    lineFollow_PID_auto(18);
    delay(2);
  }

  stop_4w();
  servoRun(3, 95); // หุบแขนเก็บ
  delay(100);

  RobotColor recalled_color = COLOR_WHITE;
  int max_non_white = 0;
  for (int c = 1; c <= 6; c++) {
    if (c != COLOR_WHITE && non_white_counts[c] > max_non_white) {
      max_non_white = non_white_counts[c];
      recalled_color = (RobotColor)c;
    }
  }

  if (max_non_white >= 3) {
    Serial.printf("\n>> Found cutting color: %s (%d samples)\n",
                  getColorName(recalled_color), max_non_white);
  } else {
    recalled_color = COLOR_WHITE;
    Serial.printf("\n>> Classified as WHITE! (%d white samples)\n", white_count);
  }

  buzzer(2400, 200);
  return recalled_color;
}

// ============================================================================

// 7. ภารกิจเรียกคืนสินค้าชำรุด (Product Recall Execution)

// เมื่อเอาไปวางครบทั้งหมด:

// อยู่ที่ HUB -> เลี้ยวซ้าย ผ่าน 1 เลี้ยวซ้าย -> เช็คสีตัวที่ 2 -> รู้สี -> ไป HUB

// -> ไปช่องเรียกคืน ยกขึ้นมา -> ไป HUB

// -> เลี้ยวขวา ผ่าน 1 เลี้ยวซ้าย วิ่งตามเอ็นโค้ดเดอร์ -> วางลง

// -> ถอยหลัง PID ตามเอ็นโค้ดเดอร์ -> หมุนซ้าย ผ่าน 3 เส้น เลี้ยวขวา วิ่งเข้าไปตามเอ็นโค้ดเดอร์

// -> ยก

// -> ถอยหลังตามเอ็นโค้ดเดอร์ -> หมุนขวา ผ่าน 1 เลี้ยวขวา ผ่าน 1 -> ถึง HUB -> เรียก HUB

// ไปวาง!

// ============================================================================

inline void executeProductRecallExact(const RobotColor order_list[6],

                                      int speed) {

  Serial.println("\n***************************************************");

  Serial.println(">>> [PRODUCT RECALL] Starting Defective Recall Mission <<<");

  Serial.println("***************************************************");

  // 1 & 2. จากจุด HUB เลี้ยวซ้ายไปสแกนอ่านสีชิ้นงานชำรุดที่ด้านข้าง
  RobotColor recalled_color = test_recall_scan_side_color();

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
  turnRight_by_stop(50);

  Fline_by_pass(40, 1);

  delay(70);

  turnRight(40);

  Fline_by_pass(40, 1);

  delay(70);

  turnRight(40);

  delay(50);
  // หน้าหันตรงเข้าหา Consumer ที่จุด HUB

  // 5. วิ่งไปที่ช่องเรียกคืน ยกของเสียขึ้นมา แล้วนำกลับมาที่ HUB
  recall_step1_pickup_from_consumer(recall_slot);

  // 6, 7, 8: นำของเสียไปทิ้งจุดทิ้ง แล้วไปคีบของใหม่ทดแทน นำกลับมาที่ HUB
  recall_step2_dispose_and_get_spare();

  // 9. เรียก HUB นำของใหม่ไปวางที่ช่องเดิม!
  recall_step3_deliver_spare(recall_slot);

  Serial.println("[Recall] Product Recall Completed 100%!");

  buzzer(2400, 200);
}

// ============================================================================

// 8. เดินทางกลับเข้าจุด START (Return to Start Box) รับคะแนนโบนัส 10 คะแนน

// ============================================================================

inline void returnToStartBox(int speed) {

  Serial.println(

      "[Nav] Mission Complete -> Returning to START Box (10 Bonus Points)...");

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

inline void executeProductRecallExact(const RobotColor order_list[6]) {

  executeProductRecallExact(order_list, SPEED_RECALL);
}

// ============================================================================

// 9. ฟังก์ชันหลักสำหรับรันภารกิจทั้งหมดในสนาม (MASTER FIELD MISSION LOOP)

// ============================================================================

inline void runFieldMission() {
  
  Follow_Notline(30, 300); 

  unsigned long mission_start_time = millis();

  Serial.println("\n=======================================================");

  Serial.println("  STARTING LOGISTICS ROBOT FIELD MISSION (รุ่น 19)");

  Serial.println("  Sequential Color Delivery Architecture on PuppyBot 6");

  Serial.println("=======================================================");

  // --------------------------------------------------------------------------

  // ขั้นตอนที่ 1: สแกนและบันทึกคำสั่งซื้อทั้ง 6 ช่อง (Order Scanning)

  // --------------------------------------------------------------------------

  Serial.println("\n>>> [PHASE 1] Scanning 6 Order Colors from Start Line <<<");

  test_auto_scan_6colors();

  // บันทึกรายการคำสั่งซื้อลงในหน่วยความจำ

  RobotColor order_list[6];

  Serial.println("\n--- RECORDED 6 ORDERS ---");

  for (int i = 0; i < 6; i++) {

    order_list[i] = getColorAtSlot(i);

    char buf[64];

    snprintf(buf, sizeof(buf), "Order #%d (Customer #%d) = %s", i + 1, i + 1,

             getColorName(order_list[i]));

    Serial.println(buf);
  }

  // --------------------------------------------------------------------------

  // ขั้นตอนที่ 2 & 3: วิ่งไปเอาเรียงลำดับ: เหลือง -> น้ำเงิน -> ดำ -> เขียว -> แดง -> ขาว

  // --------------------------------------------------------------------------

  Serial.println("\n>>> [PHASE 2 & 3] Sequential Delivery: Yellow -> Blue -> "

                 "Black -> Green -> Red -> White <<<");

  // [1] สีเหลือง: วิ่งจากการอ่านค่าแถบสีเสร็จ ไปเอาเหลือง -> นำกลับมา HUB -> ไปวางช่องลูกค้า

  // (1-6)

  // Fline_by_time(70, 120);

  // servoRun(SERVO_ARM_CH, ARM_LIFT_ANG);

  // get_Yellow(SPEED_WAREHOUSE);

  // put_from_HUB(findConsumerForColor(COLOR_YELLOW, order_list), false,

  //              SPEED_CONSUMER);

  // // [2] สีน้ำเงิน: จาก HUB วิ่งไปเอาน้ำเงิน -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)

  // get_Blue(SPEED_WAREHOUSE);

  // put_from_HUB(findConsumerForColor(COLOR_BLUE, order_list), false,

  //              SPEED_CONSUMER);

  // // [3] สีดำ: จาก HUB วิ่งไปเอาดำ -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)

  // get_Black(SPEED_WAREHOUSE);

  // put_from_HUB(findConsumerForColor(COLOR_BLACK, order_list), false,

  //              SPEED_CONSUMER);

  // // [4] สีเขียว: จาก HUB วิ่งไปเอาเขียว -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)

  // get_Green(SPEED_WAREHOUSE);

  // put_from_HUB(findConsumerForColor(COLOR_GREEN, order_list), false,

  //              SPEED_CONSUMER);

  // // [5] สีแดง: จาก HUB วิ่งไปเอาแดง -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)

  // get_Red(SPEED_WAREHOUSE);

  // put_from_HUB(findConsumerForColor(COLOR_RED, order_list), false,

  //              SPEED_CONSUMER);

  // // [6] สีขาว: จาก HUB วิ่งไปเอาขาว -> นำกลับมา HUB -> ไปวางช่องลูกค้า (1-6)

  // get_White(SPEED_WAREHOUSE);

  // put_from_HUB(findConsumerForColor(COLOR_WHITE, order_list), false,

  //              SPEED_CONSUMER);

  // --------------------------------------------------------------------------

  // ขั้นตอนที่ 4: ภารกิจเรียกคืนสินค้าชำรุด (Product Recall)

  // --------------------------------------------------------------------------
  sw();
  if (ENABLE_PRODUCT_RECALL) {

    executeProductRecallExact(order_list, SPEED_RECALL);

  }

  //

  // --------------------------------------------------------------------------

  // ขั้นตอนที่ 5: กลับเข้าจุดเริ่มต้น (Return to Start) รับคะแนนโบนัส 10 คะแนน

  //

  --------------------------------------------------------------------------

  // Serial.println("\n>>> [PHASE 5] Returning to START Box <<<");

  // returnToStartBox(SPEED_RETURN);

  // บันทึกสรุปผลภารกิจและแสดงบนจอ TFT

  mission_summary.duration_ms = millis() - mission_start_time;

  mission_summary.distance_cm = getDistanceCm();

  mission_summary.pulses      = abs(get_pulse_Encoder(selection_Encoder));

  mission_summary.completed   = true;

  snprintf(mission_summary.finishReason,

  sizeof(mission_summary.finishReason), "MISSION FINISHED");

  buzzer(2400, 200);

  delay(100);

  buzzer(2400, 200);

  drawColorSummaryScreen();
}
