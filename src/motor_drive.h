#ifndef PUPPYBOT6_MOTOR_DRIVE_H
#define PUPPYBOT6_MOTOR_DRIVE_H

#include <Arduino.h>
#include <puppybot6.h>
#include "config.h"

// ============================================================================
// ฟังก์ชันคำนวณความเร็วพร้อมชดเชย Trim และ Speed Scale
// ============================================================================
// s: ความเร็วที่ต้องการ (-100 ถึง 100)
// trim: ค่าชดเชยของมอเตอร์ตัวนั้นๆ (-100 ถึง 100)
static inline int calc_motor_output(int s, int trim) {
  if (s == 0) return 0;

  int sign = (s > 0) ? 1 : -1;
  int mag = abs(s);

  // 1. ชดเชยเปอร์เซ็นต์ความเร็วแบตเตอรี่ (speed_offset_pct)
  // หากปกติ 100 แล้วบวก 5 จะถูก clamp ที่ 100 สูงสุด
  mag = mag + speed_offset_pct;
  mag = constrain(mag, 0, 100);

  // 2. ชดเชยค่า Trim ของแต่ละล้อเพื่อให้วิ่งตรง
  mag = mag + trim;
  mag = constrain(mag, 0, 100);

  return sign * mag;
}

// ============================================================================
// สั่งการมอเตอร์ทั้ง 4 ตัวพร้อมกัน (4WD Motor Drive)
// M1: ซ้ายหน้า (Left Front)
// M2: ขวาหน้า (Right Front)
// M3: ซ้ายหลัง (Left Back)
// M4: ขวาหลัง (Right Back)
// ============================================================================
inline void drive4(int m1_target, int m2_target, int m3_target, int m4_target) {
  int out1 = calc_motor_output(m1_target, motor_trim_M1);
  int out2 = calc_motor_output(m2_target, motor_trim_M2);
  int out3 = calc_motor_output(m3_target, motor_trim_M3);
  int out4 = calc_motor_output(m4_target, motor_trim_M4);

  motor(1, out1);
  motor(2, out2);
  motor(3, out3);
  motor(4, out4);
}

// ============================================================================
// การเคลื่อนที่พื้นฐาน 4 ล้อ
// ============================================================================

// เดินหน้า 4 ล้อ
inline void fd_4w(int speed) {
  drive4(speed, speed, speed, speed);
}

// ถอยหลัง 4 ล้อ
inline void bk_4w(int speed) {
  drive4(-speed, -speed, -speed, -speed);
}

// หมุนซ้ายอยู่กับที่ (Spin Left) 4 ล้อ: ซ้ายถอย หลัง/ขวาเดินหน้า
inline void sl_4w(int speed) {
  drive4(-speed, speed, -speed, speed);
}

// หมุนขวาอยู่กับที่ (Spin Right) 4 ล้อ: ซ้ายเดินหน้า ขวาถอยหลัง
inline void sr_4w(int speed) {
  drive4(speed, -speed, speed, -speed);
}

// เลี้ยวซ้ายแบบเบี้ยว (Turn Left Pivot)
inline void tl_4w(int speed) {
  drive4(0, speed, 0, speed);
}

// เลี้ยวขวาแบบเบี้ยว (Turn Right Pivot)
inline void tr_4w(int speed) {
  drive4(speed, 0, speed, 0);
}

// หยุดมอเตอร์ทั้งหมด
inline void stop_4w() {
  motor(1, 0);
  motor(2, 0);
  motor(3, 0);
  motor(4, 0);
  ao();
}

#endif // PUPPYBOT6_MOTOR_DRIVE_H
