// ============================================================================
// ============================================================================
// LOGISTICS ROBOT COMPETITION (กติกาการแข่งขันหุ่นยนต์ "Logistics Robot" รุ่น 19)
// ============================================================================
// ============================================================================
// กฎเหล็กตามคำสั่ง:
// 1. "ห้ามใช้ไจโร (NO GYRO 100%)" -> เรียกใช้ฟังก์ชันจับเส้นโดยตรงจาก line_follow.h:
//    - turnLeft_by_stop() / turnRight_by_stop()
//    - Fline_by_pass() / Fline_by_pass_B()
//    - Fline_by_phase() / Fline_B_by_phase()
//    - Fline()
// 2. สแกนสี Order 1-6 จากแถบสีที่จุดเริ่มต้น แล้วจำค่าไว้
// 3. เลี้ยวซ้ายวิ่งตามเส้นรอบล่างไปยัง "จุดเริ่มต้นที่วงสีแดง" (Hub Intersection ทางเข้าคลัง 1)
// 4. วิ่งไปหยิบกระป๋องตามสีคำสั่งซื้อ (เช่น ดำ, เหลือง, น้ำเงิน, ...) ในคลัง 1 (Warehouse 1)
//    - เช็คด้วยเซนเซอร์วัดระยะทาง IR (GP2Y0A41SK0F) ว่ามีกระป๋องอยู่หรือไม่
//    - หากมีกระป๋อง -> คีบกระป๋องและนำกลับมาที่ "จุดเริ่มต้นที่วงสีแดง"
//    - หากสินค้าหมดสต็อก -> วิ่งไปเอาคลังสำรอง (Warehouse 2) บนแท่น 5 cm กลับมาที่จุดวงสีแดง
// 5. จากจุดวงสีแดง นำกระป๋องไปส่งลูกค้า (Consumer 1 ถึง 6) ด้วย switch-case ตามตำแหน่งลูกค้า
//    - วางกระป๋องลงช่องลูกค้า (สูง 5 cm) แล้ววิ่งกลับมาที่ "จุดเริ่มต้นที่วงสีแดง" เพื่อไปเอาชิ้นถัดไป
//    - มีฟังก์ชันทางลัด (Shortcut) ให้เขียนเสริมได้หากมีเส้นทางที่ใกล้กว่า
// 6. ส่งครบทุกคำสั่งซื้อ -> วิ่งกลับเข้าจุด START เพื่อรับคะแนนโบนัส 10 คะแนน
// ============================================================================

#include <Arduino.h>
#include "config.h"
#include "motor_drive.h"
#include "sensor_system.h"
#include "eeprom_calib.h"
#include "line_follow.h"
#include "color_scanner.h"

// ----------------------------------------------------------------------------
// 1. การตั้งค่าเซอร์โวมือจับและแขนยก (Gripper & Arm Configuration)
// ----------------------------------------------------------------------------
const int SERVO_GRIPPER_CH = 1; // เซอร์โวมือจับหนีบกระป๋อง (ช่อง 1)
const int SERVO_ARM_CH     = 2; // เซอร์โวยกแขนขึ้น-ลง (ช่อง 2)

int GRIPPER_OPEN_ANG = 55;  // องศากางมือจับออก
int GRIPPER_GRAB_ANG = 145; // องศาหนีบกระป๋องแน่น
int ARM_DOWN_ANG     = 62;  // องศาลดแขนลงระดับคีบกระป๋อง
int ARM_LIFT_ANG     = 125; // องศายกแขนขึ้นข้ามแท่น 5 cm ตามกติกา

// ฟังก์ชันสั่งคีบกระป๋อง (Grab Can)
inline void grabCan() {
  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG);
  delay(180);
  servoRun(SERVO_GRIPPER_CH, GRIPPER_GRAB_ANG);
  delay(250);
  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG); // ยกกระป๋องขึ้นข้ามแท่น 5 cm
  delay(200);
}

// ฟังก์ชันสั่งวางกระป๋องลงช่องลูกค้า (Release Can onto 5 cm Platform)
inline void releaseCan() {
  servoRun(SERVO_ARM_CH, ARM_DOWN_ANG);
  delay(200);
  servoRun(SERVO_GRIPPER_CH, GRIPPER_OPEN_ANG); // คลายมือจับ
  delay(200);
  servoRun(SERVO_ARM_CH, ARM_LIFT_ANG);         // ยกแขนกลับเพื่อไม่ชนสิ่งกีดขวาง
  delay(150);
}

// ----------------------------------------------------------------------------
// 2. การตรวจสอบการมีอยู่ของกระป๋องด้วยเซนเซอร์ IR (GP2Y0A41SK0F ADC 14)
// ----------------------------------------------------------------------------
inline bool checkCanPresent() {
  float dist_cm = get_ir_distance_cm();
  Serial.print("[IR Sensor] Measured Distance: ");
  Serial.print(dist_cm, 1);
  Serial.print(" cm");

  // หากมีกระป๋องวางอยู่ ระยะทางสะท้อนจะอยู่ในช่วง 3.5 ถึง 16.0 ซม.
  // หากสินค้าหมดสต็อก (ถูกหยิบออกตามกติกา 6.5) ระยะจะมากกว่า 20 ซม.
  if (dist_cm > 3.0f && dist_cm <= 16.0f) {
    Serial.println(" -> [OK] CAN FOUND! (มีสินค้า)");
    buzzer(2400, 40);
    return true;
  } else {
    Serial.println(" -> [WARNING] OUT OF STOCK! (สินค้าหมด -> ไปคลังสำรอง W2)");
    buzzer(1200, 150);
    return false;
  }
}

// ----------------------------------------------------------------------------
// 3. เส้นทางจากเส้นสแกนสี (Order Line) เลี้ยวซ้ายไปยังจุดเริ่มต้นวงสีแดง (Hub)
// ----------------------------------------------------------------------------
inline void navigateFromOrderLineToHub(int speed = 30) {
  Serial.println("[Nav] Heading from Order Line to Hub (Red Circle)...");
  // วิ่งตรงจนเจอเส้นตัดขอบล่างสุดของสนาม
  Fline(speed, -1);
  Fline_by_phase(speed, 5.0f); // ขยับตัวล้อให้อยู่กลางจุดตัด
  // เลี้ยวซ้ายจับเส้นขอบล่างสุด
  turnLeft_by_stop(50);

  // วิ่งตามเส้นด้านล่างผ่านแนว Consumer (เสาล่าง 1, 3, 5) ไปยังแนวแกนของจุด Hub
  Fline_by_pass(speed, 3);

  // เลี้ยวซ้าย ขึ้นสู่เส้นแกนกลาง
  turnLeft_by_stop(50);

  // วิ่งขึ้นมา 1 เส้นตัด ถึง "จุดเริ่มต้นที่วงสีแดง" (Hub Intersection ทางเข้าคลัง 1)
  Fline_by_pass(speed, 1);

  // หันหน้าไปทางทิศตะวันออก (หันหน้าเข้าหา Warehouse 1)
  turnRight_by_stop(50);
  Serial.println("[Nav] Arrived at HUB (Red Circle) - Facing East to Warehouse 1");
}

// ----------------------------------------------------------------------------
// 4. เส้นทางจากจุดวงสีแดง (HUB) ไปหยิบกระป๋องสีต่างๆ ใน Warehouse 1
// ----------------------------------------------------------------------------
// ตำแหน่งกระป๋องในคลัง 1 ตามผังสนาม 3D:
//  - แถวกลาง (Center): Yellow (ช่อง 1), Green (ช่อง 2)
//  - แถวบน   (North) : Blue (ช่อง 1), Red (ช่อง 2)
//  - แถวล่าง  (South) : Black (ช่อง 1), White (ช่อง 2)
inline void routeHubToWarehouse1Can(RobotColor col, int speed = 28) {
  Serial.print("[Nav] Hub -> Warehouse 1 Color: ");
  Serial.println(getColorName(col));

  switch (col) {
    case COLOR_YELLOW: // สีเหลือง: ตรงไปข้างหน้า 1 เส้นตัด (แถวกลาง ช่อง 1)
      Fline_by_pass(speed, 1);
      Fline_by_phase(25, 6.0f);
      break;

    case COLOR_GREEN: // สีเขียว: ตรงไปข้างหน้า 2 เส้นตัด (แถวกลาง ช่อง 2)
      Fline_by_pass(speed, 2);
      Fline_by_phase(25, 6.0f);
      break;

    case COLOR_BLUE: // สีน้ำเงิน: เลี้ยวซ้ายขึ้นแถวบน 1 เส้นตัด แล้วเลี้ยวขวา
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight_by_stop(50);
      Fline_by_phase(25, 6.0f);
      break;

    case COLOR_RED: // สีแดง: เลี้ยวซ้ายขึ้นแถวบน เดินหน้า 1 ช่อง แล้วเลี้ยวขวา
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      Fline_by_phase(25, 6.0f);
      break;

    case COLOR_BLACK: // สีดำ: เลี้ยวขวาลงแถวล่าง 1 เส้นตัด แล้วเลี้ยวซ้าย
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      turnLeft_by_stop(50);
      Fline_by_phase(25, 6.0f);
      break;

    case COLOR_WHITE: // สีขาว: เลี้ยวขวาลงแถวล่าง เดินหน้า 1 ช่อง แล้วเลี้ยวซ้าย
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      Fline_by_phase(25, 6.0f);
      break;

    default:
      Fline_by_pass(speed, 1);
      Fline_by_phase(25, 6.0f);
      break;
  }
}

// วิ่งย้อนกลับจากจุดกระป๋องใน Warehouse 1 กลับมาที่ "จุดวงสีแดง" (หันหน้าไปทิศตะวันตกหาลูกค้า)
inline void routeWarehouse1CanToHub(RobotColor col, int speed = 28) {
  Serial.print("[Nav] Warehouse 1 -> Returning to Hub from: ");
  Serial.println(getColorName(col));

  switch (col) {
    case COLOR_YELLOW:
      Fline_B_by_phase(25, 6.0f);
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      break;

    case COLOR_GREEN:
      Fline_B_by_phase(25, 6.0f);
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 2);
      break;

    case COLOR_BLUE:
      Fline_B_by_phase(25, 6.0f);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight_by_stop(50);
      break;

    case COLOR_RED:
      Fline_B_by_phase(25, 6.0f);
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight_by_stop(50);
      break;

    case COLOR_BLACK:
      Fline_B_by_phase(25, 6.0f);
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      turnLeft_by_stop(50);
      break;

    case COLOR_WHITE:
      Fline_B_by_phase(25, 6.0f);
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      turnLeft_by_stop(50);
      break;

    default:
      Fline_B_by_phase(25, 6.0f);
      turnLeft_by_stop(50);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      break;
  }
  // ขณะนี้หุ่นยนต์กลับมาอยู่ที่จุดเริ่มต้นวงสีแดง และหันหน้าไปทางทิศตะวันตก (เข้าหาโซนลูกค้า Consumer)
  Serial.println("[Nav] Back at Hub (Facing West to Consumers)");
}

// ----------------------------------------------------------------------------
// 5. เส้นทางไปยังคลังสินค้าสำรอง (Warehouse 2 Spare Can บนแท่นสูง 5 cm)
// ----------------------------------------------------------------------------
// วิ่งจากจุดกระป๋องที่หมดสต็อก ไปยังแท่นคลังสำรองที่มุมสนาม
inline void routeToWarehouse2Spare(RobotColor empty_col, int speed = 28) {
  Serial.println("[Nav] Heading to Warehouse 2 (Spare Can)...");
  Fline_B_by_phase(25, 6.0f);
  turnLeft_by_stop(50);
  Fline_by_pass(speed, 1);
  turnRight_by_stop(50);
  Fline_by_pass(speed, 2);
  Fline_by_phase(25, 8.0f);
}

// กลับจากคลังสำรอง Warehouse 2 มายังจุดวงสีแดง (Hub)
inline void routeWarehouse2SpareToHub(int speed = 28) {
  Serial.println("[Nav] Returning from Warehouse 2 to Hub...");
  Fline_B_by_phase(25, 8.0f);
  turnLeft_by_stop(50);
  turnLeft_by_stop(50);
  Fline_by_pass(speed, 2);
  turnLeft_by_stop(50);
  Fline_by_pass(speed, 1);
  turnRight_by_stop(50);
}

// ----------------------------------------------------------------------------
// 6. เส้นทางจากจุดเริ่มต้นวงสีแดง (HUB) ไปส่งลูกค้า (Consumer 1 ถึง 6) ด้วย switch-case
// ----------------------------------------------------------------------------
// หุ่นยนต์เริ่มจากจุดวงสีแดง หันหน้าไปทางทิศตะวันตก (เข้าหาลูกค้า)
inline void routeHubToConsumer(int consumer_id, int speed = 30) {
  Serial.print("[Nav] Hub -> Delivering to Consumer #");
  Serial.println(consumer_id);

  switch (consumer_id) {
    case 5: // ลูกค้า 5 (แถวล่าง ขวา): เดินหน้า 1 เส้นตัด แล้วเลี้ยวซ้าย (ใต้)
      Fline_by_pass(speed, 1);
      turnLeft_by_stop(50);
      Fline_by_phase(25, 8.0f);
      break;

    case 6: // ลูกค้า 6 (แถวบน ขวา): เดินหน้า 1 เส้นตัด แล้วเลี้ยวขวา (เหนือ)
      Fline_by_pass(speed, 1);
      turnRight_by_stop(50);
      Fline_by_phase(25, 8.0f);
      break;

    case 3: // ลูกค้า 3 (แถวล่าง กลาง): เดินหน้า 2 เส้นตัด แล้วเลี้ยวซ้าย (ใต้)
      Fline_by_pass(speed, 2);
      turnLeft_by_stop(50);
      Fline_by_phase(25, 8.0f);
      break;

    case 4: // ลูกค้า 4 (แถวบน กลาง): เดินหน้า 2 เส้นตัด แล้วเลี้ยวขวา (เหนือ)
      Fline_by_pass(speed, 2);
      turnRight_by_stop(50);
      Fline_by_phase(25, 8.0f);
      break;

    case 1: // ลูกค้า 1 (แถวล่าง ซ้าย): เดินหน้า 3 เส้นตัด แล้วเลี้ยวซ้าย (ใต้)
      Fline_by_pass(speed, 3);
      turnLeft_by_stop(50);
      Fline_by_phase(25, 8.0f);
      break;

    case 2: // ลูกค้า 2 (แถวบน ซ้าย): เดินหน้า 3 เส้นตัด แล้วเลี้ยวขวา (เหนือ)
      Fline_by_pass(speed, 3);
      turnRight_by_stop(50);
      Fline_by_phase(25, 8.0f);
      break;

    default:
      Serial.println("[Nav] Unknown Consumer ID!");
      break;
  }
}

// เดินทางย้อนกลับจากจุดวางสินค้าของลูกค้า Consumer กลับมายัง "จุดเริ่มต้นวงสีแดง" (Hub)
inline void routeConsumerToHub(int consumer_id, int speed = 30) {
  Serial.print("[Nav] Consumer #");
  Serial.print(consumer_id);
  Serial.println(" -> Returning to Hub (Red Circle)...");

  switch (consumer_id) {
    case 5: // จาก Consumer 5 กลับ Hub
      Fline_B_by_phase(25, 8.0f);
      turnRight_by_stop(50);
      Fline_by_pass(speed, 1);
      break;

    case 6: // จาก Consumer 6 กลับ Hub
      Fline_B_by_phase(25, 8.0f);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 1);
      break;

    case 3: // จาก Consumer 3 กลับ Hub
      Fline_B_by_phase(25, 8.0f);
      turnRight_by_stop(50);
      Fline_by_pass(speed, 2);
      break;

    case 4: // จาก Consumer 4 กลับ Hub
      Fline_B_by_phase(25, 8.0f);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 2);
      break;

    case 1: // จาก Consumer 1 กลับ Hub
      Fline_B_by_phase(25, 8.0f);
      turnRight_by_stop(50);
      Fline_by_pass(speed, 3);
      break;

    case 2: // จาก Consumer 2 กลับ Hub
      Fline_B_by_phase(25, 8.0f);
      turnLeft_by_stop(50);
      Fline_by_pass(speed, 3);
      break;

    default:
      break;
  }
  // เมื่อกลับมาถึงจุดวงสีแดง จะหันหน้าไปทางทิศตะวันออก (เข้าหาคลัง) พร้อมสำหรับคำสั่งซื้อถัดไป
  Serial.println("[Nav] Back at Hub (Facing East to Warehouse 1)");
}

// ----------------------------------------------------------------------------
// 7. ทางลัดพิเศษ (Custom Shortcuts Optimization Hook)
// ----------------------------------------------------------------------------
// หากผู้ใช้ต้องการเขียนทางลัดโดยตรง เช่น จาก Consumer หนึ่งข้ามไปอีกคลังหนึ่ง
// สามารถใส่โค้ดในฟังก์ชันนี้ได้ เมื่อ return true หุ่นจะไม่วิ่งกลับจุดวงสีแดง
inline bool tryExecuteShortcut(int from_consumer, RobotColor next_col) {
  // พื้นที่สำหรับเขียนคำสั่งทางลัดของคุณ:
  // ตัวอย่าง:
  /*
  if (from_consumer == 5 && next_col == COLOR_BLACK) {
    Fline_B_by_phase(25, 8.0f);
    turnRight_by_stop(50);
    Fline_by_pass(30, 1);
    return true; // วิ่งทางลัดสำเร็จ
  }
  */
  return false; // ค่าตั้งต้น: คืนค่า false เพื่อวิ่งกลับมาตั้งหลักที่จุดวงสีแดงตามมาตรฐาน
}

// เดินทางกลับเข้าจุด START เมื่อส่งครบทุกคำสั่งซื้อ (รับคะแนนโบนัส 10 คะแนนตามกติกา 6.10.4)
inline void returnToStartBox(int speed = 32) {
  Serial.println("[Nav] Mission Complete -> Returning to START Box...");
  // จากจุด Hub หันหน้าไปทางทิศตะวันตก มุ่งหน้าสู่กล่อง START
  Fline_by_pass(speed, 3);
  turnRight_by_stop(50);
  Fline_by_pass(speed, 1);
  turnLeft_by_stop(50);
  Fline_by_phase(speed, 25.0f); // วิ่งเข้าจอดในกล่อง START
  stop_4w();
  buzzer(2200, 300);
}

// ----------------------------------------------------------------------------
// 8. ฟังก์ชันหลักสำหรับรันภารกิจทั้งหมดในสนาม (MASTER FIELD MISSION LOOP)
// ----------------------------------------------------------------------------
inline void runFieldMission() {
  unsigned long mission_start_time = millis();
  Serial.println("\n=======================================================");
  Serial.println("  STARTING LOGISTICS ROBOT FIELD MISSION (กติกา รุ่น 19)");
  Serial.println("=======================================================");

  // --------------------------------------------------------------------------
  // ขั้นตอนที่ 1: สแกนแถบสีคำสั่งซื้อทั้ง 6 ช่อง (Order 1 - 6) แบบ Non-Stop
  // --------------------------------------------------------------------------
  Serial.println("\n[PHASE 1] Scanning 6 Order Colors from Start Line...");
  test_start_and_scan_colors(30);

  // คัดลอกรายการสีคำสั่งซื้อทั้ง 6 รายการที่อ่านได้
  RobotColor order_list[6];
  Serial.println("\n--- ORDER LIST FROM SCAN ---");
  for (int i = 0; i < 6; i++) {
    order_list[i] = getColorAtSlot(i);
    char buf[64];
    snprintf(buf, sizeof(buf), "Order #%d (Deliver to Consumer #%d) = %s",
             i + 1, i + 1, getColorName(order_list[i]));
    Serial.println(buf);
  }

  // --------------------------------------------------------------------------
  // ขั้นตอนที่ 2: วิ่งนำทางจากเส้นแถบสี เลี้ยวซ้ายไปยังจุดเริ่มต้นวงสีแดง (Hub)
  // --------------------------------------------------------------------------
  Serial.println("\n[PHASE 2] Moving to HUB Intersection (Red Circle)...");
  navigateFromOrderLineToHub(30);

  // --------------------------------------------------------------------------
  // ขั้นตอนที่ 3: วนลูปส่งสินค้าทีละชิ้นตามคำสั่งซื้อ (Order 1 ถึง 6)
  // --------------------------------------------------------------------------
  Serial.println("\n[PHASE 3] Starting Pick & Deliver Loop for 6 Orders...");
  int completed_deliveries = 0;

  for (int order_idx = 0; order_idx < 6; order_idx++) {
    RobotColor target_color = order_list[order_idx];
    int target_consumer     = order_idx + 1; // Order 1 ส่ง Consumer 1, Order 2 ส่ง Consumer 2 ...

    char startMsg[80];
    snprintf(startMsg, sizeof(startMsg), "\n>>> Processing Order #%d: Color %s -> Consumer #%d <<<",
             order_idx + 1, getColorName(target_color), target_consumer);
    Serial.println(startMsg);

    // 3.1 จากจุดวงสีแดง วิ่งไปยังตำแหน่งกระป๋องใน Warehouse 1
    routeHubToWarehouse1Can(target_color, 28);

    // 3.2 ตรวจสอบว่ามีกระป๋องอยู่ในช่องหรือไม่ ด้วยเซนเซอร์วัดระยะทาง IR
    bool can_found = checkCanPresent();
    if (can_found) {
      // คลังมีสินค้าปกติ -> คีบกระป๋องและยกขึ้น
      grabCan();
      // วิ่งกลับมายังจุดวงสีแดง (Hub)
      routeWarehouse1CanToHub(target_color, 28);
    } else {
      // สินค้าหมดสต็อกตามกติกา 6.3.7 / 6.5! -> ไปคลังสำรอง Warehouse 2
      routeToWarehouse2Spare(target_color, 28);
      // คีบกระป๋องสำรองที่ Warehouse 2 บนแท่น 5 cm
      grabCan();
      // นำกระป๋องสำรองกลับมายังจุดวงสีแดง (Hub)
      routeWarehouse2SpareToHub(28);
    }

    // 3.3 จากจุดวงสีแดง นำกระป๋องไปส่งที่ช่องลูกค้า (Consumer 1 ถึง 6) ตาม switch-case
    routeHubToConsumer(target_consumer, 30);

    // 3.4 วางกระป๋องลงในช่องลูกค้า
    releaseCan();
    completed_deliveries++;
    buzzer(2000, 60);

    // 3.5 ตรวจสอบทางลัด (Shortcut):
    bool used_shortcut = false;
    if (order_idx < 5) {
      used_shortcut = tryExecuteShortcut(target_consumer, order_list[order_idx + 1]);
    }

    // หากไม่ได้ใช้ทางลัด ให้วิ่งกลับมาตั้งหลักที่จุดวงสีแดงตามมาตรฐาน
    if (!used_shortcut && order_idx < 5) {
      routeConsumerToHub(target_consumer, 30);
    }
  }

  // --------------------------------------------------------------------------
  // ขั้นตอนที่ 4: สิ้นสุดภารกิจ วิ่งกลับเข้าจุด START (รับคะแนนโบนัส 10 คะแนน)
  // --------------------------------------------------------------------------
  Serial.println("\n[PHASE 4] All orders processed! Returning to START box...");
  returnToStartBox(32);

  // สรุปข้อมูลภารกิจ
  mission_summary.duration_ms = millis() - mission_start_time;
  mission_summary.distance_cm = getDistanceCm();
  mission_summary.pulses      = abs(get_pulse_Encoder(selection_Encoder));
  mission_summary.completed   = true;
  snprintf(mission_summary.finishReason, sizeof(mission_summary.finishReason), "LOGISTICS FINISHED");

  buzzer(2400, 200);
  delay(100);
  buzzer(2400, 200);

  // แสดงหน้าจอสรุปผลบนจอ TFT
  drawColorSummaryScreen();
}
