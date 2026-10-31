#ifndef PUPPYBOT6_MOTOR_DRIVE_H
#define PUPPYBOT6_MOTOR_DRIVE_H

#include <Arduino.h>
#include <puppybot6.h>
#include "config.h"

// ============================================================================
// ฟังก์ชันคำนวณความเร็วพร้อมชดเชย Trim และ Speed Scale
// ============================================================================
// s: ความเร็วที่ต้องการ (-100.0 ถึง 100.0)
// trim: ค่าชดเชยของมอเตอร์ตัวนั้นๆ (-100 ถึง 100)
static inline int calc_motor_output(float s, int trim) {
  if (fabs(s) < 0.001f) return 0;

  float sign = (s > 0.0f) ? 1.0f : -1.0f;
  float mag = fabs(s);

  // 1. ชดเชยเปอร์เซ็นต์ความเร็วแบตเตอรี่ (speed_offset_pct)
  // หากปกติ 100 แล้วบวก 5 จะถูก clamp ที่ 100 สูงสุด
  mag = mag + speed_offset_pct;
  mag = constrain(mag, 0.0f, 100.0f);

  // 2. ชดเชยค่า Trim ของแต่ละล้อเพื่อให้วิ่งตรง
  mag = mag + trim;
  mag = constrain(mag, 0.0f, 100.0f);

  return (int)roundf(sign * mag);
}

// ============================================================================
// สั่งการมอเตอร์ทั้ง 4 ตัวพร้อมกัน (4WD Motor Drive)
// M1: ซ้ายหน้า (Left Front)
// M2: ขวาหน้า (Right Front)
// M3: ซ้ายหลัง (Left Back)
// M4: ขวาหลัง (Right Back)
// ============================================================================
inline void drive4(float m1_target, float m2_target, float m3_target, float m4_target) {
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
inline void fd_4w(float speed) {
  drive4(speed, speed, speed, speed);
}

// ถอยหลัง 4 ล้อ
inline void bk_4w(float speed) {
  drive4(-speed, -speed, -speed, -speed);
}

// หมุนซ้ายอยู่กับที่ (Spin Left) 4 ล้อ: ซ้ายถอย หลัง/ขวาเดินหน้า
inline void sl_4w(float speed) {
  drive4(-speed, speed, -speed, speed);
}

// หมุนขวาอยู่กับที่ (Spin Right) 4 ล้อ: ซ้ายเดินหน้า ขวาถอยหลัง
inline void sr_4w(float speed) {
  drive4(speed, -speed, speed, -speed);
}

// เลี้ยวซ้ายแบบเบี้ยว (Turn Left Pivot)
inline void tl_4w(float speed) {
  drive4(0, speed, 0, speed);
}

// เลี้ยวขวาแบบเบี้ยว (Turn Right Pivot)
inline void tr_4w(float speed) {
  drive4(speed, 0, speed, 0);
}

// หยุดมอเตอร์ทั้งหมดแบบ Free Run
inline void stop_4w() {
  motor(1, 0);
  motor(2, 0);
  motor(3, 0);
  motor(4, 0);
  ao();
}

// เบรกมอเตอร์แบบ Active Brake (ชอร์ตขั้วมอเตอร์) ล็อคล้อให้นิ่งสนิททันที
inline void brake_4w(int brake_ms = 40) {
  motorBreak(10); // H-Bridge High-Side/Low-Side clamp ทุกมอเตอร์
  if (brake_ms > 0) {
    delay(brake_ms);
  }
  ao();
}

// หยุดแบบสวนแรงเฉื่อย (Reverse Pulse Brake) แล้วล็อคล้อ - หยุดได้นิ่งและไวที่สุด
inline void brake_reverse_4w(float forward_speed, int pulse_ms = 25, int hold_brake_ms = 40) {
  if (forward_speed > 0.0f) {
    // จังหวะถอยสวนสั้นๆ ซับแรงเฉื่อย
    bk_4w(constrain(forward_speed, 20.0f, 70.0f));
    delay(pulse_ms);
  } else if (forward_speed < 0.0f) {
    // หากก่อนหน้านี้กำลังถอยหลัง ให้เดินหน้าสวนสั้นๆ
    fd_4w(constrain(fabs(forward_speed), 20.0f, 70.0f));
    delay(pulse_ms);
  }
  motorBreak(10);
  if (hold_brake_ms > 0) {
    delay(hold_brake_ms);
  }
  ao();
}

#endif // PUPPYBOT6_MOTOR_DRIVE_H
