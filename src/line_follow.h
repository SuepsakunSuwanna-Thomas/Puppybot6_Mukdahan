#ifndef PUPPYBOT6_LINE_FOLLOW_H
#define PUPPYBOT6_LINE_FOLLOW_H

#include <Arduino.h>
#include <puppybot6.h>
#include "config.h"
#include "motor_drive.h"
#include "sensor_system.h"

// ============================================================================
// 1. ตาราง KD แยกตามความเร็ว (ตามข้อกำหนดของผู้ใช้)
// ============================================================================
inline float get_KD_for_speed(int speed) {
  if (speed >= 100) {
    return 12.3f;
  } else if (speed >= 90) {
    return 14.5f;
  } else if (speed >= 80) {
    return 12.9f;
  } else if (speed >= 70) {
    return 10.5f;
  } else if (speed >= 60) {
    return 8.8f;
  } else if (speed >= 50) {
    return 7.5f;
  } else if (speed >= 40) {
    return 7.5f;
  } else if (speed >= 30) {
    return 5.5f;
  } else if (speed >= 20) {
    return 3.2f;
  } else if (speed >= 10) {
    return 1.0f;
  } else {
    return 6.10f; // ค่ามาตรฐาน
  }
}

// ============================================================================
// 2. ตัวแปรและฟังก์ชันตั้งค่า KP และ KD สำหรับการวิ่งตามเส้น
// ============================================================================
// กำหนดค่า KP และ KD ได้ที่นี่ หรือเรียก setLineKP / setLineKD ได้ตลอดเวลา
inline void setLineKP(float kp) { line_KP = kp; }
inline void setLineKPB(float kp) { line_KP_B = kp; }
inline void setLineKD(float kd) { line_KD = kd; }
inline void setLineKDB(float kd) { line_KD_B = kd; }

// ============================================================================
// ฟังก์ชันแจ้งเตือนเมื่อหุ่นยนต์หลุดเส้นเกินเวลาที่กำหนด (Line Lost Alarm)
// เมื่อหลุดออกจากเส้นประมาณ 3 วินาที หุ่นยนต์จะหยุดวิ่ง
// บัซเซอร์จะดัง แล้วหยุด ดัง แล้วหยุด วนไปเรื่อยๆ ไม่มีที่สิ้นสุด
// ============================================================================
inline void handleLineLostAlarm() {
  stop_4w();
  Serial.print(F("[ALARM] Line lost > "));
  Serial.print((float)line_lost_timeout_ms / 1000.0f, 1);
  Serial.println(F("s! Robot stopped."));

  tft_.fillScreen(ST7735_BLACK);
  printText(10, 35, "LINE LOST!", 2, ST7735_RED);
  printText(15, 65, "STOPPED", 2, ST7735_YELLOW);
  String timeMsg = "Time > " + String((float)line_lost_timeout_ms / 1000.0f, 1) + "s";
  printText(15, 95, timeMsg, 1, ST7735_WHITE);

  while (true) {
    buzzer(1500, 200); // บัซเซอร์ส่งเสียงดัง 200 มิลลิวินาที
    delay(200);        // หยุดเงียบ 200 มิลลิวินาที
  }
}

// ด้านหน้า: lineFollow_PID_auto (ขับเคลื่อน 4 ล้อพร้อม Motor Trims และ Speed Scale)
inline void lineFollow_PID_auto(int speed) {
  static unsigned long last_call_time = 0;
  static int _prev_err = 0;

  unsigned long now = millis();
  // ข้อ 1: หากห่างจากการเรียกครั้งล่าสุดเกิน 100ms (เริ่มคำสั่งวิ่งใหม่) ให้เคลียร์ Error เก่า ป้องกันสะบัดตอนออกตัว
  if (now - last_call_time > 100) {
    _prev_err = 0;
  }

  // ตรวจสอบการหลุดเส้นต่อเนื่องเกินเวลาที่กำหนด (หากตั้งเป็น -1 หรือ <= 0 จะปิดการทำงาน)
  if (line_lost_timeout_ms > 0) {
    static unsigned long lost_start_time = 0;
    static bool is_lost = false;

    if (now - last_call_time > 100) {
      is_lost = false;
    }

    if (isFrontLineLost()) {
      if (!is_lost) {
        is_lost = true;
        lost_start_time = now;
      } else if (now - lost_start_time >= (unsigned long)line_lost_timeout_ms) {
        handleLineLostAlarm();
      }
    } else {
      is_lost = false;
    }
  }
  last_call_time = now;

  float KP = line_KP; // ดึงค่าจากตัวแปรส่วนกลาง (ค่าตั้งต้น 0.08)
  // หาก line_KD >= 0 ให้ใช้ค่าที่ผู้ใช้ตั้งจริง (รวมถึง 0.00 ปิด D), ถ้า < 0 (-1) ถึงจะใช้ auto ตารางความเร็ว
  float KD = (line_KD >= 0.0f) ? line_KD : get_KD_for_speed(speed);

  int present_position = readline();
  int setpoint = ((NUM_FRONT_SENSORS - 1) * 100) / 2; // (6 * 100) / 2 = 300

  // หากกำลังอยู่บนเส้นตัด ป้องกันไม่ให้ตำแหน่งเพี้ยนและรีเซ็ต error เพื่อไม่ให้เกิดการส่ายกระชาก
  if (isFrontCrossing()) {
    present_position = setpoint;
    _prev_err = 0;
  }

  float err = present_position - setpoint;
  float deriv = err - _prev_err;
  float out = KP * err + KD * deriv;
  _prev_err = err;

  // สูตรขับเคลื่อน: ล้อด้านนอกถูกบล็อกไว้ไม่เกิน speed (ไม่เร่งเกินสปีด)
  // ล้อด้านในหมุนถอยหลังช่วยเลี้ยวได้ถึง -speed (ที่ speed 100 ถอยหลังได้ถึง -100 เลี้ยววงแคบได้คมมาก)
  int motorL = constrain((int)(speed + out), -speed, speed);
  int motorR = constrain((int)(speed - out), -speed, speed);

  // สั่งงานมอเตอร์ทั้ง 4 ล้อพร้อมกัน (M1, M2, M3, M4)
  drive4(motorL, motorR, motorL, motorR);
  delay(1);
}

// ด้านหลัง: lineFollow_PID_B_auto (วิ่งถอยหลังตามเส้นด้วยเซนเซอร์หลัง)
inline void lineFollow_PID_B_auto(int speed) {
  static unsigned long last_call_time_B = 0;
  static int _prev_err_B = 0;

  unsigned long now = millis();
  // ข้อ 1: หากห่างจากการเรียกครั้งล่าสุดเกิน 100ms (เริ่มคำสั่งวิ่งใหม่) ให้เคลียร์ Error เก่า ป้องกันสะบัดตอนออกตัว
  if (now - last_call_time_B > 100) {
    _prev_err_B = 0;
  }

  // ตรวจสอบการหลุดเส้นต่อเนื่องด้านหลังเกินเวลาที่กำหนด (หากตั้งเป็น -1 หรือ <= 0 จะปิดการทำงาน)
  if (line_lost_timeout_ms > 0) {
    static unsigned long lost_start_time_B = 0;
    static bool is_lost_B = false;

    if (now - last_call_time_B > 100) {
      is_lost_B = false;
    }

    if (isBackLineLost()) {
      if (!is_lost_B) {
        is_lost_B = true;
        lost_start_time_B = now;
      } else if (now - lost_start_time_B >= (unsigned long)line_lost_timeout_ms) {
        handleLineLostAlarm();
      }
    } else {
      is_lost_B = false;
    }
  }
  last_call_time_B = now;

  float KP = line_KP_B; // ดึงค่าจากตัวแปรส่วนกลาง (ค่าตั้งต้น 0.40)
  float KD = (line_KD_B >= 0.0f) ? line_KD_B : get_KD_for_speed(speed);

  int present_position_B = readline_B();
  int setpoint_B = ((NUM_BACK_SENSORS - 1) * 100) / 2; // (6 * 100) / 2 = 300

  // หากกำลังอยู่บนเส้นตัดด้านหลัง
  if (isBackCrossing()) {
    present_position_B = setpoint_B;
    _prev_err_B = 0;
  }

  float err_B = present_position_B - setpoint_B;
  float deriv_B = err_B - _prev_err_B;
  float out_B = KP * err_B + KD * deriv_B;
  _prev_err_B = err_B;

  // วิ่งถอยหลัง: ล้อด้านนอกถูกบล็อกไม่เกิน speed, ล้อด้านในหมุนกลับทิศทางช่วยเลี้ยวได้ถึง -speed
  int motorL = -constrain((int)(speed - out_B), -speed, speed);
  int motorR = -constrain((int)(speed + out_B), -speed, speed);

  drive4(motorL, motorR, motorL, motorR);
  delay(1);
}

// ============================================================================
// 3. ฟังก์ชันวิ่งข้ามเส้น (Fline_by_pass & Fline_by_pass_B)
// ============================================================================

// Fline_by_pass: วิ่งตามเส้นจนกว่าจะผ่านเส้นตัดครบตามจำนวนที่กำหนด
inline void Fline_by_pass(int speed, int pass_lines) {
  for (int count = 0; count < pass_lines; count++) {
    // 1. วิ่งตามเส้นจนกว่าจะเจอเส้นตัด (Crossing >= ครึ่งหนึ่ง)
    while (!isFrontCrossing()) {
      lineFollow_PID_auto(speed);
    }

    // 2. หากยังไม่ถึงเส้นสุดท้ายที่ต้องการหยุด ให้วิ่งจนกว่าเซนเซอร์จะพ้นเส้นตัดจริง
    if (count < pass_lines - 1) {
      unsigned long t_cross = millis();
      // วิ่งจนกว่าจะพ้นเส้นตัดจริง (พร้อม safety timeout 1500ms)
      while (isFrontCrossing() && (millis() - t_cross < 1500)) {
        lineFollow_PID_auto(speed);
      }
      // วิ่งหน่วงอีก 40ms เพื่อป้องกันการสะท้อนขอบเส้นตัดนับซ้ำ
      unsigned long t_clear = millis();
      while (millis() - t_clear < 40) {
        lineFollow_PID_auto(speed);
      }
    }
  }
  stop_4w();
}

// Fline_by_pass_B: วิ่งตามเส้นถอยหลังจนกว่าจะผ่านเส้นตัดครบตามจำนวน
inline void Fline_by_pass_B(int speed, int pass_lines) {
  for (int count = 0; count < pass_lines; count++) {
    while (!isBackCrossing()) {
      lineFollow_PID_B_auto(speed);
    }

    if (count < pass_lines - 1) {
      unsigned long t_cross = millis();
      while (isBackCrossing() && (millis() - t_cross < 1500)) {
        lineFollow_PID_B_auto(speed);
      }
      unsigned long t_clear = millis();
      while (millis() - t_clear < 40) {
        lineFollow_PID_B_auto(speed);
      }
    }
  }
  stop_4w();
}

// ============================================================================
// 4. ฟังก์ชันวิ่งจนกว่าเซนเซอร์ที่กำหนดจะเจอเส้นแล้วหยุด (Fline & Fline_B)
// ============================================================================

// Fline: วิ่งตามเส้นไปข้างหน้า จนกว่าเซนเซอร์ไหนเจอ (0-6 หรือ -1 สำหรับ crossing) คือ เจอเส้นแล้วหยุด
inline void Fline(int speed, int sensor) {
  if (sensor >= 0 && sensor < NUM_FRONT_SENSORS) {
    while (!isFrontSensorOnLine(sensor)) {
      lineFollow_PID_auto(speed);
    }
  } else {
    // หากระบุ sensor เป็น -1 หรือค่าอื่น หมายถึงรอให้เจอเส้นตัด
    while (!isFrontCrossing()) {
      lineFollow_PID_auto(speed);
    }
  }
  stop_4w();
}

// Fline_B: วิ่งถอยหลังตามเส้นจนกว่าเซนเซอร์หลังที่ระบุจะเจอเส้นแล้วหยุด
inline void Fline_B(int speed, int sensor) {
  if (sensor >= 0 && sensor < NUM_BACK_SENSORS) {
    while (!isBackSensorOnLine(sensor)) {
      lineFollow_PID_B_auto(speed);
    }
  } else {
    while (!isBackCrossing()) {
      lineFollow_PID_B_auto(speed);
    }
  }
  stop_4w();
}

// ============================================================================
// 5. วิ่งตามเส้นตามระยะทางเป็นเซนติเมตร (Fline_by_phase & Fline_B_by_phase)
// ============================================================================

// Fline_by_phase: วิ่งตามเส้นจนกว่าระยะจะถึงตามที่ตั้ง (ระยะเป็น ซม.)
inline void Fline_by_phase(int speed, float distance_cm) {
  resetDistance();
  while (getDistanceCm() < distance_cm) {
    lineFollow_PID_auto(speed);
  }
  stop_4w();
}

// Fline_B_by_phase: วิ่งตามเส้นถอยหลังจนกว่าระยะจะถึงตามที่ตั้ง (ระยะเป็น ซม.)
inline void Fline_B_by_phase(int speed, float distance_cm) {
  resetDistance();
  while (getDistanceCm() < distance_cm) {
    lineFollow_PID_B_auto(speed);
  }
  stop_4w();
}

// ============================================================================
// 6. วิ่งตามเส้นตามเวลา (Fline_by_time & Fline_B_by_time)
// ============================================================================

// Fline_by_time: วิ่งตามเส้นตามเวลา (delay ms)
inline void Fline_by_time(int speed, unsigned long duration_ms) {
  unsigned long start = millis();
  while (millis() - start < duration_ms) {
    lineFollow_PID_auto(speed);
  }
  stop_4w();
}

// Fline_B_by_time: วิ่งตามเส้นถอยหลังตามเวลา (delay ms)
inline void Fline_B_by_time(int speed, unsigned long duration_ms) {
  unsigned long start = millis();
  while (millis() - start < duration_ms) {
    lineFollow_PID_B_auto(speed);
  }
  stop_4w();
}

// ============================================================================
// 7. วิ่งตามเส้นตามระยะ Encoder pulses (Fline_by_Encoder & Fline_B_by_Encoder)
// ============================================================================

// Fline_by_Encoder: วิ่งตามเส้นตามระยะของ Encoder (pulse)
inline void Fline_by_Encoder(int speed, long target_pulses) {
  long initialPulses = abs(get_pulse_Encoder(selection_Encoder));
  while (abs(get_pulse_Encoder(selection_Encoder)) - initialPulses < target_pulses) {
    lineFollow_PID_auto(speed);
  }
  stop_4w();
}

// Fline_B_by_Encoder: วิ่งตามเส้นถอยหลังตาม Encoder (pulse)
inline void Fline_B_by_Encoder(int speed, long target_pulses) {
  long initialPulses = abs(get_pulse_Encoder(selection_Encoder));
  while (abs(get_pulse_Encoder(selection_Encoder)) - initialPulses < target_pulses) {
    lineFollow_PID_B_auto(speed);
  }
  stop_4w();
}

// ชื่อฟังก์ชันทางเลือก (Aliases)
inline void Fline_by_phase_B(int speed, float distance_cm) { Fline_B_by_phase(speed, distance_cm); }
inline void Fline_by_time_B(int speed, unsigned long duration_ms) { Fline_B_by_time(speed, duration_ms); }
inline void Fline_by_Encoder_B(int speed, long target_pulses) { Fline_B_by_Encoder(speed, target_pulses); }

// ============================================================================
// 8. วิ่งแบบไม่ใช้ PID (Follow_Notline & Backword_Notline)
// ============================================================================

// Follow_Notline: วิ่งไปข้างหน้าแบบ ไม่ใช้ PID ตามเวลา (delay ms)
inline void Follow_Notline(int speed, unsigned long duration_ms) {
  fd_4w(speed);
  delay(duration_ms);
  stop_4w();
}

// Backword_Notline: วิ่งถอยหลังแบบ ไม่ใช้ PID ตามเวลา (delay ms)
inline void Backword_Notline(int speed, unsigned long duration_ms) {
  bk_4w(speed);
  delay(duration_ms);
  stop_4w();
}

// Follow_Notline_by_Encoder: วิ่งไปข้างหน้าแบบ ไม่ใช้ PID ตามระยะ (pulse หรือ cm)
inline void Follow_Notline_by_Encoder(int speed, long target_pulses) {
  long initialPulses = abs(get_pulse_Encoder(selection_Encoder));
  fd_4w(speed);
  while (abs(get_pulse_Encoder(selection_Encoder)) - initialPulses < target_pulses) {
    delay(1);
  }
  stop_4w();
}

// Backword_Notline_by_Encoder: วิ่งถอยหลังแบบ ไม่ใช้ PID ตามระยะ (pulse)
inline void Backword_Notline_by_Encoder(int speed, long target_pulses) {
  long initialPulses = abs(get_pulse_Encoder(selection_Encoder));
  bk_4w(speed);
  while (abs(get_pulse_Encoder(selection_Encoder)) - initialPulses < target_pulses) {
    delay(1);
  }
  stop_4w();
}

// ============================================================================
// 9. การเลี้ยวและการหมุน (Turning using sl and sr)
// ============================================================================

// turnLeft: วิ่ง PID ตามเส้น พอเจอ crossing แล้วทำการหมุนซ้าย (sl) ทันที (ไม่ต้องเดินหน้า) จนกว่าเซนเซอร์หน้าจะเจอเส้นใหม่
inline void turnLeft(int speed) {
  // 1. วิ่ง PID ตามเส้นจนกว่าจะเจอเส้นตัด
  while (!isFrontCrossing()) {
    lineFollow_PID_auto(speed);
  }
  stop_4w();
  // 2. หมุนซ้าย (sl) ทันที ไม่ต้องเดินหน้า ออกจากเส้นเดิมก่อน
  sl_4w(speed);
  delay(140);

  // 3. หมุนซ้ายต่อจนกว่าเซนเซอร์หน้าตรงกลาง (ADC 3) จะเจอเส้นใหม่
  unsigned long timeout = millis();
  while (!isFrontSensorOnLine(FRONT_CENTER_INDEX) && (millis() - timeout < 3500)) {
    sl_4w(speed);
    delay(1);
  }
  stop_4w();
}

// turnRight: วิ่ง PID ตามเส้น พอเจอ crossing แล้วทำการหมุนขวา (sr) ทันที (ไม่ต้องเดินหน้า) จนกว่าเซนเซอร์หน้าจะเจอเส้นใหม่
inline void turnRight(int speed) {
  while (!isFrontCrossing()) {
    lineFollow_PID_auto(speed);
  }
  stop_4w();
  // หมุนขวา (sr) ทันที ไม่ต้องเดินหน้า ออกจากเส้นเดิมก่อน
  sr_4w(speed);
  delay(140);

  unsigned long timeout = millis();
  while (!isFrontSensorOnLine(FRONT_CENTER_INDEX) && (millis() - timeout < 3500)) {
    sr_4w(speed);
    delay(1);
  }
  stop_4w();
}

// turnLeft_B: วิ่งถอยหลัง PID ตามเส้น พอเจอเส้นแล้วหมุนขวา (sr) สลับกันทันทีจนกว่าเซนเซอร์หลังจะเจอเส้นใหม่
inline void turnLeft_B(int speed) {
  while (!isBackCrossing()) {
    lineFollow_PID_B_auto(speed);
  }

  // สลับทิศทางการหมุนเมื่อถอยหลัง: ใช้ sr ทันที
  sr_4w(speed);
  delay(140);

  unsigned long timeout = millis();
  while (!isBackSensorOnLine(BACK_CENTER_INDEX) && (millis() - timeout < 3500)) {
    sr_4w(speed);
    delay(1);
  }
  stop_4w();
}

// turnRight_B: วิ่งถอยหลัง PID ตามเส้น พอเจอเส้นแล้วหมุนซ้าย (sl) สลับกันทันทีจนกว่าเซนเซอร์หลังจะเจอเส้นใหม่
inline void turnRight_B(int speed) {
  while (!isBackCrossing()) {
    lineFollow_PID_B_auto(speed);
  }

  // สลับทิศทางการหมุนเมื่อถอยหลัง: ใช้ sl ทันที
  sl_4w(speed);
  delay(140);

  unsigned long timeout = millis();
  while (!isBackSensorOnLine(BACK_CENTER_INDEX) && (millis() - timeout < 3500)) {
    sl_4w(speed);
    delay(1);
  }
  stop_4w();
}

// turnLeft_by_stop: หมุนไปทางซ้าย (sl) จนกว่าเซนเซอร์หน้าจะเจอเส้น แล้วหยุด
inline void turnLeft_by_stop(int speed) {
  sl_4w(speed);
  delay(100); // พ้นตำแหน่งเดิม
  unsigned long timeout = millis();
  while (!isFrontSensorOnLine(FRONT_CENTER_INDEX) && (millis() - timeout < 3500)) {
    sl_4w(speed);
    delay(1);
  }
  stop_4w();
}

// turnRight_by_stop: หมุนไปทางขวา (sr) จนกว่าเซนเซอร์หน้าจะเจอเส้น แล้วหยุด
inline void turnRight_by_stop(int speed) {
  sr_4w(speed);
  delay(100);
  unsigned long timeout = millis();
  while (!isFrontSensorOnLine(FRONT_CENTER_INDEX) && (millis() - timeout < 3500)) {
    sr_4w(speed);
    delay(1);
  }
  stop_4w();
}

// ============================================================================
// 10. การจัดหุ่นยนต์เข้าสู่จุดกึ่งกลางเส้น (checkLine & checkLine_B)
// ============================================================================

// checkLine: อ่านค่าเซนเซอร์แล้วหมุนแบบ tl/tr เพื่อให้เซนเซอร์หน้าตรงกลาง (ADC 3) อยู่ที่เส้น
inline void checkLine(int speed = 30) {
  // หากเซนเซอร์ตรงกลางอยู่บนเส้นแล้ว ให้หยุด
  if (isFrontSensorOnLine(FRONT_CENTER_INDEX)) {
    stop_4w();
    return;
  }

  // เช็คว่าเส้นอยู่ทางซ้าย (เซนเซอร์ 0, 1, 2)
  bool lineLeft = isFrontSensorOnLine(0) || isFrontSensorOnLine(1) || isFrontSensorOnLine(2);
  // เช็คว่าเส้นอยู่ทางขวา (เซนเซอร์ 4, 5, 6)
  bool lineRight = isFrontSensorOnLine(4) || isFrontSensorOnLine(5) || isFrontSensorOnLine(6);

  unsigned long startT = millis();
  if (lineLeft) {
    while (!isFrontSensorOnLine(FRONT_CENTER_INDEX) && (millis() - startT < 1500)) {
      tl_4w(speed);
      delay(2);
    }
  } else if (lineRight) {
    while (!isFrontSensorOnLine(FRONT_CENTER_INDEX) && (millis() - startT < 1500)) {
      tr_4w(speed);
      delay(2);
    }
  }
  stop_4w();
}

// checkLine_B: อ่านค่าเซนเซอร์แล้วหมุนแบบ tl/tr เพื่อให้เซนเซอร์หลังตรงกลาง (ADC 10) อยู่ที่เส้น
inline void checkLine_B(int speed = 30) {
  if (isBackSensorOnLine(BACK_CENTER_INDEX)) {
    stop_4w();
    return;
  }

  bool lineLeft = isBackSensorOnLine(0) || isBackSensorOnLine(1) || isBackSensorOnLine(2);
  bool lineRight = isBackSensorOnLine(4) || isBackSensorOnLine(5) || isBackSensorOnLine(6);

  unsigned long startT = millis();
  if (lineLeft) {
    while (!isBackSensorOnLine(BACK_CENTER_INDEX) && (millis() - startT < 1500)) {
      tl_4w(speed);
      delay(2);
    }
  } else if (lineRight) {
    while (!isBackSensorOnLine(BACK_CENTER_INDEX) && (millis() - startT < 1500)) {
      tr_4w(speed);
      delay(2);
    }
  }
  stop_4w();
}

#endif // PUPPYBOT6_LINE_FOLLOW_H
