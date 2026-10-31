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
// ตาราง KD ด้านหน้า
inline float get_KD_for_speed(float speed) {
  if (speed >= 100.0f) {
    return 16.5f;
  } else if (speed >= 90.0f) {
    return 14.5f;
  } else if (speed >= 80.0f) {
    return 12.9f;
  } else if (speed >= 70.0f) {
    return 10.5f;
  } else if (speed >= 60.0f) {
    return 8.8f;
  } else if (speed >= 50.0f) {
    return 7.5f;
  } else if (speed >= 40.0f) {
    return 7.5f;
  } else if (speed >= 30.0f) {
    return 5.5f;
  } else if (speed >= 20.0f) {
    return 3.2f;
  } else if (speed >= 10.0f) {
    return 1.0f;
  } else {
    return 6.10f; // ค่ามาตรฐาน
  }
}

// ตาราง KD ด้านหลัง (เซนเซอร์หลัง / วิ่งถอยหลัง)
inline float get_KD_for_speed_B(float speed) {
  if (speed >= 100.0f) {
    return 1.5f;
  } else if (speed >= 90.0f) {
    return 14.5f;
  } else if (speed >= 80.0f) {
    return 12.9f;
  } else if (speed >= 70.0f) {
    return 10.5f;
  } else if (speed >= 60.0f) {
    return 8.8f;
  } else if (speed >= 50.0f) {
    return 7.5f;
  } else if (speed >= 40.0f) {
    return 7.5f;
  } else if (speed >= 30.0f) {
    return 5.5f;
  } else if (speed >= 20.0f) {
    return 3.2f;
  } else if (speed >= 10.0f) {
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

// ตัวแปรเก็บ Error รอบก่อนหน้าสำหรับคำนวณ Derivative (D-term)
static float _line_prev_err = 0.0f;
static float _line_prev_err_B = 0.0f;

// ฟังก์ชันรีเซ็ตสถานะ PID ป้องกันอาการกระชาก (Derivative Kick) และกำหนดให้เซนเซอร์เริ่มที่จุดกึ่งกลาง (300)
inline void reset_PID_state() {
  _line_prev_err = 0.0f;
  _line_prev_err_B = 0.0f;
  _lastPosition = ((NUM_FRONT_SENSORS - 1) * 100) / 2;     // 300 (Center)
  _lastPosition_B = ((NUM_BACK_SENSORS - 1) * 100) / 2;   // 300 (Center)
}

// ด้านหน้า: lineFollow_PID_auto (คำนวณตามสมการ PID แท้ 100% สั่งการเลี้ยวเชิงรุกตามตำแหน่งเส้น)
inline void lineFollow_PID_auto(float speed) {
  float KP = line_KP; // ค่า KP (0.40f)
  float KD = (line_KD >= 0.0f) ? line_KD : get_KD_for_speed(speed);

  int present_position = readline();
  int setpoint = ((NUM_FRONT_SENSORS - 1) * 100) / 2; // 300 (Center)

  // 1. หากเซนเซอร์หน้าวิ่งทับแถบสีขวางสนาม (เซนเซอร์หลายตัวเห็นดำพร้อมกัน = Crossing)
  // หรือเซนเซอร์หน้าหลุดจากเส้นดำชั่วคราวเพราะแถบสีกลืนแสง ให้ล็อค error เป็น 0 เพื่อวิ่งตรงผ่านแถบสีไปเลย
  if (isFrontCrossing() || isFrontLineLost()) {
    present_position = setpoint;
  }

  // คำนวณ Error และ Derivative ตามสมการ PD
  float err = present_position - setpoint;

  // จำกัด Error ไม่ให้กระชากเกินขอบเขตเมื่อวิ่งแตะขอบแถบสี
  err = constrain(err, -150.0f, 150.0f);

  float deriv = err - _line_prev_err;
  deriv = constrain(deriv, -50.0f, 50.0f);
  
  float out = KP * err + KD * deriv;
  _line_prev_err = err;

  // ควบคุมกำลังมอเตอร์ซ้าย-ขวาตามสมการ PID
  float motorL = constrain(speed + out, -speed, speed);
  float motorR = constrain(speed - out, -speed, speed);

  // สั่งงานมอเตอร์ทั้ง 4 ล้อ
  drive4(motorL, motorR, motorL, motorR);
  delay(1);
}

// ด้านหลัง: lineFollow_PID_B_auto (วิ่งถอยหลังตามเส้นด้วยสมการ PID เต็มรูปแบบ)
inline void lineFollow_PID_B_auto(float speed) {
  float KP = line_KP_B; // ค่า KP (0.40f)
  float KD = (line_KD_B >= 0.0f) ? line_KD_B : get_KD_for_speed_B(speed);

  int present_position_B = readline_B();
  int setpoint_B = ((NUM_BACK_SENSORS - 1) * 100) / 2; // 300 (Center)

  float err_B = present_position_B - setpoint_B;
  float deriv_B = err_B - _line_prev_err_B;
  float out_B = KP * err_B + KD * deriv_B;
  _line_prev_err_B = err_B;

  float m1Speed = constrain(speed - out_B, -speed, speed);
  float m2Speed = constrain(speed + out_B, -speed, speed);

  drive4(-m1Speed, -m2Speed, -m1Speed, -m2Speed);
  delay(1);
}

// ============================================================================
// 3. ฟังก์ชันวิ่งข้ามเส้น (Fline_by_pass & Fline_by_pass_B)
// ============================================================================

// Fline_by_pass: วิ่งตามเส้นจนกว่าจะผ่านเส้นตัดครบตามจำนวนที่กำหนด
inline void Fline_by_pass(float speed, int pass_lines) {
  for (int count = 0; count < pass_lines; count++) {
    // หากเริ่มคำสั่งขณะที่ตัวหุ่นอยู่บนเส้นตัดอยู่แล้ว ให้วิ่งพ้นเส้นตัดเดิมออกไปก่อน
    if (count == 0 && isFrontCrossing()) {
      unsigned long t_cross = millis();
      while (isFrontCrossing() && (millis() - t_cross < 1500)) {
        lineFollow_PID_auto(speed);
      }
      unsigned long t_clear = millis();
      while (millis() - t_clear < 40) {
        lineFollow_PID_auto(speed);
      }
    }

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
      while (millis() - t_clear < 70) {
        lineFollow_PID_auto(speed);
      }
    }
  }
  stop_4w();
  delay(30);
}

// โอเวอร์โหลด Fline_by_pass แบบ 3 พารามิเตอร์ ตามสไตล์ Sisakat 4WD: Fline_by_pass(speed, count, sensor)
inline void Fline_by_pass(float speed, int pass_lines, int sensor) {
  if (sensor < 0) {
    Fline_by_pass(speed, pass_lines);
    return;
  }
  for (int count = 0; count < pass_lines; count++) {
    while (!isFrontSensorOnLine(sensor)) {
      lineFollow_PID_auto(speed);
    }
    if (count < pass_lines - 1) {
      unsigned long t_cross = millis();
      while (isFrontSensorOnLine(sensor) && (millis() - t_cross < 1500)) {
        lineFollow_PID_auto(speed);
      }
      unsigned long t_clear = millis();
      while (millis() - t_clear < 40) {
        lineFollow_PID_auto(speed);
      }
    }
  }
  stop_4w();
}

// Fline_by_pass_B: วิ่งตามเส้นถอยหลังจนกว่าจะผ่านเส้นตัดครบตามจำนวน
inline void Fline_by_pass_B(float speed, int pass_lines) {
  for (int count = 0; count < pass_lines; count++) {
    // หากเริ่มคำสั่งขณะที่ตัวหุ่นอยู่บนเส้นตัดด้านหลังอยู่แล้ว ให้วิ่งพ้นเส้นตัดเดิมออกไปก่อน
    if (count == 0 && isBackCrossing()) {
      unsigned long t_cross = millis();
      while (isBackCrossing() && (millis() - t_cross < 1500)) {
        lineFollow_PID_B_auto(speed);
      }
      unsigned long t_clear = millis();
      while (millis() - t_clear < 40) {
        lineFollow_PID_B_auto(speed);
      }
    }

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

// Fline: วิ่งตามเส้นไปข้างหน้า จนกว่าเซนเซอร์ไหนเจอ (0-6 หรือ -1 สำหรับ crossing) คือ เจอเส้นแล้วหยุดนิ่ง
inline void Fline(float speed, int sensor) {
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
  // หยุดนิ่งสนิทแบบนุ่มนวล ไม่กระตุกถอยหลัง
  stop_4w();
  delay(30);
}

// Fline_B: วิ่งถอยหลังตามเส้นจนกว่าเซนเซอร์หลังที่ระบุจะเจอเส้นแล้วหยุด
inline void Fline_B(float speed, int sensor) {
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
inline void Fline_by_phase(float speed, float distance_cm) {
  resetDistance();
  while (getDistanceCm() < distance_cm) {
    lineFollow_PID_auto(speed);
  }
  stop_4w();
}

// Fline_B_by_phase: วิ่งตามเส้นถอยหลังจนกว่าระยะจะถึงตามที่ตั้ง (ระยะเป็น ซม.)
inline void Fline_B_by_phase(float speed, float distance_cm) {
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
inline void Fline_by_time(float speed, unsigned long duration_ms) {
  unsigned long start = millis();
  while (millis() - start < duration_ms) {
    lineFollow_PID_auto(speed);
  }
  stop_4w();
}

// Fline_B_by_time: วิ่งตามเส้นถอยหลังตามเวลา (delay ms)
inline void Fline_B_by_time(float speed, unsigned long duration_ms) {
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
inline void Fline_by_Encoder(float speed, long target_pulses) {
  long initialPulses = get_pulse_Encoder(selection_Encoder);
  while (abs(get_pulse_Encoder(selection_Encoder) - initialPulses) < target_pulses) {
    lineFollow_PID_auto(speed);
  }
  stop_4w();
}

// Fline_B_by_Encoder: วิ่งตามเส้นถอยหลังตาม Encoder (pulse)
inline void Fline_B_by_Encoder(float speed, long target_pulses) {
  long initialPulses = get_pulse_Encoder(selection_Encoder);
  while (abs(get_pulse_Encoder(selection_Encoder) - initialPulses) < target_pulses) {
    lineFollow_PID_B_auto(speed);
  }
  stop_4w();
}

// ชื่อฟังก์ชันทางเลือก (Aliases)
inline void Fline_by_phase_B(float speed, float distance_cm) { Fline_B_by_phase(speed, distance_cm); }
inline void Fline_by_time_B(float speed, unsigned long duration_ms) { Fline_B_by_time(speed, duration_ms); }
inline void Fline_by_Encoder_B(float speed, long target_pulses) { Fline_B_by_Encoder(speed, target_pulses); }

// ============================================================================
// 8. วิ่งตามเส้นจนกว่าจะเจอกระป๋องด้วยเซนเซอร์ IR (Fline_until_can)
// ============================================================================
// วิ่งตามเส้นจนกว่าจะเจอกระป๋อง โดยผ่อนความเร็วลงเป็นระดับตั้งแต่เนิ่นๆ เข้าแตะนุ่มนวล ไม่ชน ไม่กระตุก
inline void Fline_until_can(float speed = 25.0f, float stop_dist_cm = 5.0f, unsigned long timeout_ms = 4500) {
  reset_PID_state();
  unsigned long startT = millis();

  while (millis() - startT < timeout_ms) {
    float dist = get_ir_distance_cm();

    // 1. ถึงระยะเป้าหมายแล้ว หยุดนิ่งสนิททันที
    if (dist <= stop_dist_cm) {
      break;
    }

    // 2. ปรับลดความเร็วตามระยะทางแบบอัตโนมัติ (ยิ่งใกล้ยิ่งช้า)
    float run_speed = speed;
    if (dist <= stop_dist_cm + 3.0f) {
      // ระยะประชิด (ห่างเป้าหมายไม่เกิน 3 cm): ย่องเบาๆ เข้าแตะ
      run_speed = 12.0f;
    } else if (dist <= stop_dist_cm + 8.0f) {
      // ระยะเริ่มเห็น (ห่างเป้าหมาย 4 - 8 cm): ผ่อนความเร็วลงครึ่งหนึ่ง
      run_speed = (speed > 18.0f) ? 18.0f : speed;
    } else if (dist <= stop_dist_cm + 14.0f) {
      // ระยะเริ่มเข้าใกล้: ผ่อนความเร็วลงเล็กน้อย
      run_speed = (speed > 22.0f) ? 22.0f : speed;
    }

    lineFollow_PID_auto(run_speed);
  }

  // หยุดนิ่งสนิท ล็อคมอเตอร์นิ่มนวล ไร้อาการสะบัด/กระตุก
  stop_4w();
  delay(40);
}

// เดินหน้าตรงๆ (ไม่ต้องเกาะเส้น) จนกว่าจะเจอกระป๋องแล้วหยุด
inline void fd_until_can(float speed = 25.0f, float stop_dist_cm = 6.0f) {
  while (get_ir_distance_cm() > stop_dist_cm) {
    fd_4w(speed);
    delay(1);
  }
  stop_4w();
}

// ============================================================================
// 8. วิ่งแบบไม่ใช้ PID (Follow_Notline & Backword_Notline)
// ============================================================================

// Follow_Notline: วิ่งไปข้างหน้าแบบ ไม่ใช้ PID ตามเวลา (delay ms)
inline void Follow_Notline(float speed, unsigned long duration_ms) {
  fd_4w(speed);
  delay(duration_ms);
  stop_4w();
}

// Backword_Notline: วิ่งถอยหลังแบบ ไม่ใช้ PID ตามเวลา (delay ms)
inline void Backword_Notline(float speed, unsigned long duration_ms) {
  bk_4w(speed);
  delay(duration_ms);
  stop_4w();
}

// Follow_Notline_by_Encoder: วิ่งไปข้างหน้าแบบ ไม่ใช้ PID ตามระยะ (pulse หรือ cm)
inline void Follow_Notline_by_Encoder(float speed, long target_pulses) {
  long initialPulses = get_pulse_Encoder(selection_Encoder);
  fd_4w(speed);
  while (abs(get_pulse_Encoder(selection_Encoder) - initialPulses) < target_pulses) {
    delay(1);
  }
  stop_4w();
}

// Backword_Notline_by_Encoder: วิ่งถอยหลังแบบ ไม่ใช้ PID ตามระยะ (pulse)
inline void Backword_Notline_by_Encoder(float speed, long target_pulses) {
  long initialPulses = get_pulse_Encoder(selection_Encoder);
  bk_4w(speed);
  while (abs(get_pulse_Encoder(selection_Encoder) - initialPulses) < target_pulses) {
    delay(1);
  }
  stop_4w();
}

// ============================================================================
// 9. การเลี้ยวและการหมุน (Turning using sl and sr)
// ============================================================================

// turnLeft: วิ่ง PID ตามเส้น พอเจอแยก/เส้นตัดแล้วสะบัดเลี้ยวซ้ายอย่างรวดเร็ว (Two-Stage Turn ตามแบบ Sisaket)
inline void turnLeft(float speed) {
  // 1. วิ่ง PID ตามเส้นจนกว่าจะเจอเส้นตัดหรือเซนเซอร์ซ้ายสุด (ADC 0) แตะเส้น
  while (!isFrontCrossing() && !isFrontSensorOnLine(0)) {
    lineFollow_PID_auto(speed);
  }
  stop_4w();
  delay(70);

  float fast_spin = (speed > 60.0f) ? speed : 60.0f; // สะบัดตัวเร็ว 75-80% เหมือน Sisaket
  float slow_spin = 25.0f;                           // ผ่อนความเร็วจับเส้นนุ่มนวล

  // 2. จังหวะที่ 1: สะบัดตัวซ้ายอย่างเร็ว (sl) หลุดจากเส้นเดิมและกวาดหาเส้นใหม่
  sl_4w(fast_spin);
  delay(60); // หลุดออกจากขอบเดิมเล็กน้อย
  unsigned long timeout = millis();
  while (!isFrontSensorOnLine(0)) {
    sl_4w(fast_spin);
    delay(1);
  }

  // 3. จังหวะที่ 2: ผ่อนความเร็วลงเพื่อจับเส้นอย่างนุ่มนวล (หยุดเมื่อเซนเซอร์ 1 หรือ 2 เจอเส้น)
  while (!isFrontSensorOnLine(1)) {
    sl_4w(slow_spin);
    delay(1);
  }
  stop_4w();
  delay(50);
}

// turnRight: วิ่ง PID ตามเส้น พอเจอแยก/เส้นตัดแล้วสะบัดเลี้ยวขวาอย่างรวดเร็ว (Two-Stage Turn ตามแบบ Sisaket)
inline void turnRight(float speed) {
  // 1. วิ่ง PID ตามเส้นจนกว่าจะเจอเส้นตัดหรือเซนเซอร์ขวาสุด (ADC 6) แตะเส้น
  while (!isFrontCrossing() && !isFrontSensorOnLine(6)) {
    lineFollow_PID_auto(speed);
  }
  stop_4w();
  delay(40);

  float fast_spin = (speed > 60.0f) ? speed : 60.0f; // สะบัดตัวเร็ว 75% เหมือน Sisaket
  float slow_spin = 25.0f;                           // ผ่อนความเร็วจับเส้นนุ่มนวล

  // 2. จังหวะที่ 1: สะบัดตัวขวาอย่างเร็ว (sr) หลุดจากเส้นเดิมและกวาดหาเส้นใหม่
  sr_4w(fast_spin);
  delay(60); // หลุดออกจากขอบเดิมเล็กน้อย
  unsigned long timeout = millis();
  while (!isFrontSensorOnLine(6) && !isFrontSensorOnLine(5) && (millis() - timeout < 2500)) {
    sr_4w(fast_spin);
    delay(1);
  }

  // 3. จังหวะที่ 2: ผ่อนความเร็วลงเพื่อจับเส้นอย่างนุ่มนวล (หยุดเมื่อเซนเซอร์ 5 หรือ 4 เจอเส้น)
  while (!isFrontSensorOnLine(5) && !isFrontSensorOnLine(4) && !isFrontSensorOnLine(FRONT_CENTER_INDEX) && (millis() - timeout < 2500)) {
    sr_4w(slow_spin);
    delay(1);
  }
  stop_4w();
  delay(50);
}

// turnLeft_B: วิ่งถอยหลัง PID ตามเส้น พอเจอเส้นตัดแล้วสะบัดเลี้ยวซ้ายอย่างรวดเร็ว
inline void turnLeft_B(float speed) {
  while (!isBackCrossing() && !isBackSensorOnLine(0)) {
    lineFollow_PID_B_auto(speed);
  }
  stop_4w();
  delay(40);

  float fast_spin = (speed > 75.0f) ? speed : 75.0f;
  float slow_spin = 25.0f;

  // ถอยหลังแล้วเลี้ยวซ้าย: ท้ายรถต้องเบี่ยงซ้าย หัวรถเบี่ยงขวา -> ใช้ sr_4w
  sr_4w(fast_spin);
  delay(60);
  unsigned long timeout = millis();
  while (!isBackSensorOnLine(0) && !isBackSensorOnLine(1) && (millis() - timeout < 2500)) {
    sr_4w(fast_spin);
    delay(1);
  }
  while (!isBackSensorOnLine(1) && !isBackSensorOnLine(2) && !isBackSensorOnLine(BACK_CENTER_INDEX) && (millis() - timeout < 2500)) {
    sr_4w(slow_spin);
    delay(1);
  }
  stop_4w();
  delay(30);
}

// turnRight_B: วิ่งถอยหลัง PID ตามเส้น พอเจอเส้นตัดแล้วสะบัดเลี้ยวขวาอย่างรวดเร็ว
inline void turnRight_B(float speed) {
  while (!isBackCrossing() && !isBackSensorOnLine(6)) {
    lineFollow_PID_B_auto(speed);
  }
  stop_4w();
  delay(40);

  float fast_spin = (speed > 75.0f) ? speed : 75.0f;
  float slow_spin = 25.0f;

  // ถอยหลังแล้วเลี้ยวขวา: ท้ายรถต้องเบี่ยงขวา หัวรถเบี่ยงซ้าย -> ใช้ sl_4w
  sl_4w(fast_spin);
  delay(60);
  unsigned long timeout = millis();
  while (!isBackSensorOnLine(6) && !isBackSensorOnLine(5) && (millis() - timeout < 2500)) {
    sl_4w(fast_spin);
    delay(1);
  }
  while (!isBackSensorOnLine(5) && !isBackSensorOnLine(4) && !isBackSensorOnLine(BACK_CENTER_INDEX) && (millis() - timeout < 2500)) {
    sl_4w(slow_spin);
    delay(1);
  }
  stop_4w();
  delay(30);
}

// turnLeft_by_stop: หมุนไปทางซ้าย (sl) จากจุดหยุดนิ่ง เข้าหาเส้นใหม่ (Two-Stage)
inline void turnLeft_by_stop(float speed = 50.0f) {
  float fast_spin = (speed > 50.0f) ? speed : 50.0f;
  float slow_spin = 25.0f;

  sl_4w(fast_spin);
  delay(60); // หลุดออกจากขอบเดิมเล็กน้อย
  unsigned long timeout = millis();
  while (!isFrontSensorOnLine(0) && !isFrontSensorOnLine(1) && (millis() - timeout < 2500)) {
    sl_4w(fast_spin);
    delay(1);
  }
  while (!isFrontSensorOnLine(1) && !isFrontSensorOnLine(2) && !isFrontSensorOnLine(FRONT_CENTER_INDEX) && (millis() - timeout < 2500)) {
    sl_4w(slow_spin);
    delay(1);
  }
  stop_4w();
  delay(20);
}

// turnRight_by_stop: หมุนไปทางขวา (sr) จากจุดหยุดนิ่ง เข้าหาเส้นใหม่ (Two-Stage)
inline void turnRight_by_stop(float speed = 50.0f) {
  float fast_spin = (speed > 50.0f) ? speed : 50.0f;
  float slow_spin = 25.0f;

  sr_4w(fast_spin);
  delay(60); // หลุดออกจากขอบเดิมเล็กน้อย
  unsigned long timeout = millis();
  while (!isFrontSensorOnLine(6) && !isFrontSensorOnLine(5) && (millis() - timeout < 2500)) {
    sr_4w(fast_spin);
    delay(1);
  }
  while (!isFrontSensorOnLine(5) && !isFrontSensorOnLine(4) && !isFrontSensorOnLine(FRONT_CENTER_INDEX) && (millis() - timeout < 2500)) {
    sr_4w(slow_spin);
    delay(1);
  }
  stop_4w();
  delay(20);
}

// ============================================================================
// 10. การจัดหุ่นยนต์เข้าสู่จุดกึ่งกลางเส้น (checkLine & checkLine_B)
// ============================================================================

// checkLine: อ่านค่าเซนเซอร์แล้วหมุนแบบ tl/tr เพื่อให้เซนเซอร์หน้าตรงกลาง (ADC 3) อยู่ที่เส้น
inline void checkLine(float speed = 30.0f) {
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
inline void checkLine_B(float speed = 30.0f) {
  if (isBackSensorOnLine(BACK_CENTER_INDEX)) {
    stop_4w();
    return;
  }

  bool lineLeft = isBackSensorOnLine(0) || isBackSensorOnLine(1) || isBackSensorOnLine(2);
  bool lineRight = isBackSensorOnLine(4) || isBackSensorOnLine(5) || isBackSensorOnLine(6);

  unsigned long startT = millis();
  if (lineLeft) {
    // เส้นอยู่ซ้ายของเซนเซอร์หลัง -> ท้ายต้องเบี่ยงซ้าย หัวต้องเบี่ยงขวา -> ล้อซ้ายเดินหน้า (tr_4w)
    while (!isBackSensorOnLine(BACK_CENTER_INDEX) && (millis() - startT < 1500)) {
      tr_4w(speed);
      delay(2);
    }
  } else if (lineRight) {
    // เส้นอยู่ขวาของเซนเซอร์หลัง -> ท้ายต้องเบี่ยงขวา หัวต้องเบี่ยงซ้าย -> ล้อขวาเดินหน้า (tl_4w)
    while (!isBackSensorOnLine(BACK_CENTER_INDEX) && (millis() - startT < 1500)) {
      tl_4w(speed);
      delay(2);
    }
  }
  stop_4w();
}

#endif // PUPPYBOT6_LINE_FOLLOW_H
