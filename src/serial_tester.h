#ifndef PUPPYBOT6_SERIAL_TESTER_H
#define PUPPYBOT6_SERIAL_TESTER_H

#include <Arduino.h>
#include <puppybot6.h>
#include "config.h"
#include "motor_drive.h"
#include "sensor_system.h"
#include "line_follow.h"

// ============================================================================
// PUPPYBOT6 SERIAL TESTER & WEB CONTROLLER
// รองรับการรับคำสั่งจาก Web Dashboard ผ่าน Serial USB (115200 Baud)
// - วนลูปทดสอบไม่มีที่สิ้นสุด
// - ส่ง Telemetry เซนเซอร์ทุกอย่างแบบสดๆ กลับไปแสดงผลบนหน้าเว็บ
// - เมื่อได้รับคำสั่งทดสอบ (PID, 4WD Motor, Servo) จะรอกดสวิตช์ที่ตัวหุ่นก่อนเริ่ม
// - เมื่อทำงานเสร็จ จะหยุดมอเตอร์ และรอรับคำสั่งถัดไปจากหน้าเว็บต่อทันที
// ============================================================================

namespace SerialTester {

// ฟังก์ชันช่วยแยกค่า String จากข้อความ JSON
inline String getJsonStr(const String& json, const String& key) {
  int keyIndex = json.indexOf("\"" + key + "\"");
  if (keyIndex == -1) return "";
  int colonIndex = json.indexOf(':', keyIndex);
  if (colonIndex == -1) return "";
  int start = colonIndex + 1;
  while (start < (int)json.length() && (json[start] == ' ' || json[start] == '\"')) start++;
  int end = start;
  while (end < (int)json.length() && json[end] != ',' && json[end] != '}' && json[end] != '\"' && json[end] != '\r' && json[end] != '\n') end++;
  return json.substring(start, end);
}

inline float getJsonFloat(const String& json, const String& key, float defaultVal = 0.0f) {
  String val = getJsonStr(json, key);
  if (val.length() == 0) return defaultVal;
  return val.toFloat();
}

inline int getJsonInt(const String& json, const String& key, int defaultVal = 0) {
  String val = getJsonStr(json, key);
  if (val.length() == 0) return defaultVal;
  return val.toInt();
}

// ส่งแพ็กเก็ต Telemetry ข้อมูลเซนเซอร์ทั้งหมดกลับไปยังหน้าเว็บ
inline void sendTelemetry() {
  // อ่านค่าเซนเซอร์สีหน้า (TCS Front ตัวเดียว)
  int rF = Read_Color_TCS(0);
  int gF = Read_Color_TCS(1);
  int bF = Read_Color_TCS(2);
  RobotColor colF = detectColorFront();

  float irDist = readDistanceIR();
  float encDist = getDistanceCm();
  int btnPressed = isButtonPressed() ? 1 : 0;

  // ประกอบ JSON ส่งผ่าน Serial
  Serial.print("{\"type\":\"TELEM\"");
  
  // สีหน้า (Front Color Sensor ตัวเดียว)
  Serial.print(",\"fc\":"); Serial.print((int)colF);
  Serial.print(",\"fn\":\""); Serial.print(getColorName(colF)); Serial.print("\"");
  Serial.print(",\"frgb\":["); Serial.print(rF); Serial.print(","); Serial.print(gF); Serial.print(","); Serial.print(bF); Serial.print("]");

  // ระยะทางและไจโร
  Serial.print(",\"ir\":"); Serial.print(irDist, 1);
  Serial.print(",\"enc\":"); Serial.print(encDist, 1);
  Serial.print(",\"yaw\":"); Serial.print(yaw_loop1, 1);
  Serial.print(",\"btn\":"); Serial.print(btnPressed);

  // เซนเซอร์หน้า 7 ตัว
  Serial.print(",\"fl\":[");
  for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
    Serial.print(isFrontSensorOnLine(i) ? 1 : 0);
    if (i < NUM_FRONT_SENSORS - 1) Serial.print(",");
  }
  Serial.print("]");

  // เซนเซอร์หลัง 7 ตัว
  Serial.print(",\"bl\":[");
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    Serial.print(isBackSensorOnLine(i) ? 1 : 0);
    if (i < NUM_BACK_SENSORS - 1) Serial.print(",");
  }
  Serial.print("]");

  Serial.println("}");
}

// แสดงหน้าจอสถานะ Serial Test Mode บนหน้าจอ TFT
inline void drawIdleScreen() {
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 4, "== WEB TEST LAB ==", 1, ST7735_CYAN);
  tft_.drawFastHLine(0, 16, 160, ST7735_BLUE);

  printText(5, 22, "STATUS: READY (WAIT CMD)", 1, ST7735_GREEN, ST7735_BLACK);
  printText(5, 36, "Serial: 115200 Baud", 1, ST7735_YELLOW, ST7735_BLACK);

  tft_.drawFastHLine(0, 50, 160, ST7735_BLUE);
  printText(5, 56, "Live Sensor Preview:", 1, ST7735_WHITE, ST7735_BLACK);
}

// อัปเดตข้อมูลบนหน้าจอ TFT ระหว่างรอคำสั่ง
inline void updateIdleDisplay() {
  RobotColor colF = detectColorFront();
  float irDist = readDistanceIR();
  float encDist = getDistanceCm();

  String fColStr = "Color: " + String(getColorName(colF)) + "         ";
  printText(5, 72, fColStr, 1, ST7735_CYAN, ST7735_BLACK);

  String distStr = "IR: " + String(irDist, 1) + "cm  Enc:" + String(encDist, 0) + "cm   ";
  printText(5, 92, distStr, 1, ST7735_GREEN, ST7735_BLACK);

  String yawStr = "Yaw: " + String(yaw_loop1, 1) + " deg    ";
  printText(5, 110, yawStr, 1, ST7735_YELLOW, ST7735_BLACK);
}

// ประมวลผลคำสั่งที่ส่งมาจากหน้าเว็บ Web Dashboard
inline void processCommand(const String& cmdLine) {
  String cmd = getJsonStr(cmdLine, "cmd");
  cmd.trim();

  // 1. PING เช็คการเชื่อมต่อ
  if (cmd.equalsIgnoreCase("PING")) {
    Serial.println("{\"type\":\"PONG\",\"status\":\"READY\",\"msg\":\"PuppyBot6 Online\"}");
    return;
  }

  // 2. STOP คำสั่งหยุดฉุกเฉิน (Emergency Stop)
  if (cmd.equalsIgnoreCase("STOP")) {
    stop_4w();
    buzzer(800, 100);
    tft_.fillScreen(ST7735_RED);
    printText(15, 50, "EMERGENCY STOP", 2, ST7735_WHITE);
    delay(500);
    drawIdleScreen();
    Serial.println("{\"type\":\"RES\",\"status\":\"STOPPED\",\"msg\":\"Emergency Stop Triggered\"}");
    return;
  }

  // 3. SET_CONFIG ตั้งค่า KP, KD, Trim และ Battery Offset แบบ Real-time
  if (cmd.equalsIgnoreCase("SET_CONFIG")) {
    if (cmdLine.indexOf("\"kp\"") != -1) line_KP = getJsonFloat(cmdLine, "kp", line_KP);
    if (cmdLine.indexOf("\"kp_b\"") != -1) line_KP_B = getJsonFloat(cmdLine, "kp_b", line_KP_B);
    if (cmdLine.indexOf("\"kd\"") != -1) line_KD = getJsonFloat(cmdLine, "kd", line_KD);
    if (cmdLine.indexOf("\"kd_b\"") != -1) line_KD_B = getJsonFloat(cmdLine, "kd_b", line_KD_B);
    if (cmdLine.indexOf("\"m1\"") != -1) motor_trim_M1 = getJsonInt(cmdLine, "m1", motor_trim_M1);
    if (cmdLine.indexOf("\"m2\"") != -1) motor_trim_M2 = getJsonInt(cmdLine, "m2", motor_trim_M2);
    if (cmdLine.indexOf("\"m3\"") != -1) motor_trim_M3 = getJsonInt(cmdLine, "m3", motor_trim_M3);
    if (cmdLine.indexOf("\"m4\"") != -1) motor_trim_M4 = getJsonInt(cmdLine, "m4", motor_trim_M4);
    if (cmdLine.indexOf("\"offset\"") != -1) speed_offset_pct = getJsonInt(cmdLine, "offset", speed_offset_pct);

    buzzer(1800, 80);
    Serial.println("{\"type\":\"RES\",\"status\":\"CONFIG_SAVED\",\"msg\":\"Configurations Updated\"}");
    return;
  }

  // 4. SERVO สั่งงานมุมเซอร์โว 1 และ 2
  if (cmd.equalsIgnoreCase("SERVO")) {
    int s1 = getJsonInt(cmdLine, "s1", 50);
    int s2 = getJsonInt(cmdLine, "s2", 146);
    int waitBtn = getJsonInt(cmdLine, "wait_btn", 0);

    if (waitBtn == 1) {
      tft_.fillScreen(ST7735_BLACK);
      printText(5, 5, "== SERVO TEST ==", 1, ST7735_YELLOW);
      printText(5, 30, "S1: " + String(s1) + " deg", 2, ST7735_CYAN);
      printText(5, 55, "S2: " + String(s2) + " deg", 2, ST7735_MAGENTA);
      printText(5, 95, "> PRESS BUTTON <", 1, ST7735_GREEN);

      buzzer(1500, 100);
      Serial.println("{\"type\":\"STATUS\",\"state\":\"WAIT_BTN\",\"msg\":\"Press switch on robot to move servos\"}");
      waitForButton("Press for Servo");
    }

    servoRun(1, s1);
    servoRun(2, s2);
    buzzer(2000, 150);

    Serial.println("{\"type\":\"RES\",\"status\":\"DONE\",\"msg\":\"Servo Position Set\"}");
    delay(400);
    drawIdleScreen();
    return;
  }

  // 5. TEST_MOTOR ทดสอบมอเตอร์ 4 ล้ออิสระ (M1, M2, M3, M4)
  if (cmd.equalsIgnoreCase("TEST_MOTOR")) {
    int m1 = getJsonInt(cmdLine, "m1", 0);
    int m2 = getJsonInt(cmdLine, "m2", 0);
    int m3 = getJsonInt(cmdLine, "m3", 0);
    int m4 = getJsonInt(cmdLine, "m4", 0);
    int duration = getJsonInt(cmdLine, "time", 1000);
    int waitBtn = getJsonInt(cmdLine, "wait_btn", 1);

    if (waitBtn == 1) {
      tft_.fillScreen(ST7735_BLACK);
      printText(5, 5, "== 4WD MOTOR TEST ==", 1, ST7735_YELLOW);
      printText(5, 25, "M1:" + String(m1) + "  M2:" + String(m2), 1, ST7735_WHITE);
      printText(5, 45, "M3:" + String(m3) + "  M4:" + String(m4), 1, ST7735_WHITE);
      printText(5, 65, "Time: " + String(duration) + " ms", 1, ST7735_CYAN);
      printText(5, 95, "> PRESS BUTTON <", 2, ST7735_GREEN);

      buzzer(1500, 100);
      Serial.println("{\"type\":\"STATUS\",\"state\":\"WAIT_BTN\",\"msg\":\"Press switch on robot to start motor test\"}");
      waitForButton("Press for Motor");
    }

    tft_.fillScreen(ST7735_BLACK);
    printText(5, 45, "MOTORS RUNNING...", 2, ST7735_YELLOW);
    Serial.println("{\"type\":\"STATUS\",\"state\":\"RUNNING\",\"msg\":\"Motors spinning...\"}");
    buzzer(1800, 80);

    drive4(m1, m2, m3, m4);
    if (duration > 0) {
      delay(duration);
      stop_4w();
    }

    buzzer(2200, 150);
    Serial.println("{\"type\":\"RES\",\"status\":\"DONE\",\"msg\":\"Motor test completed\"}");
    
    tft_.fillScreen(ST7735_BLACK);
    printText(10, 45, "TEST COMPLETE!", 2, ST7735_GREEN);
    printText(5, 80, "Waiting for serial...", 1, ST7735_WHITE);
    delay(800);
    drawIdleScreen();
    return;
  }

  // 6. TEST_PID ทดสอบการวิ่งตามเส้น PID หรือการเลี้ยวต่างๆ
  if (cmd.equalsIgnoreCase("TEST_PID")) {
    String mode = getJsonStr(cmdLine, "mode");
    float kp = getJsonFloat(cmdLine, "kp", line_KP);
    float kp_b = getJsonFloat(cmdLine, "kp_b", line_KP_B);
    float kd = getJsonFloat(cmdLine, "kd", line_KD);
    float kd_b = getJsonFloat(cmdLine, "kd_b", line_KD_B);
    int speed = getJsonInt(cmdLine, "speed", 50);
    float val = getJsonFloat(cmdLine, "val", 0.0f);
    int waitBtn = getJsonInt(cmdLine, "wait_btn", 1);

    // กำหนดค่า KP และ KD ที่ส่งมา
    if (kp > 0.001f) line_KP = kp;
    if (kp_b > 0.001f) line_KP_B = kp_b;
    line_KD = kd;
    line_KD_B = kd_b;

    // รอกดสวิตช์ก่อนเริ่มวิ่ง
    if (waitBtn == 1) {
      tft_.fillScreen(ST7735_BLACK);
      printText(5, 5, "== PID TEST READY ==", 1, ST7735_YELLOW);
      printText(5, 20, "Mode: " + mode, 1, ST7735_CYAN);
      String kdStr = (line_KD > 0.001f) ? String(line_KD, 1) : "Auto";
      printText(5, 34, "Spd:" + String(speed) + " P:" + String(line_KP, 2) + " D:" + kdStr, 1, ST7735_WHITE);
      printText(5, 48, "Param: " + String(val, 1), 1, ST7735_WHITE);
      printText(5, 80, "> PRESS BUTTON <", 2, ST7735_GREEN);

      buzzer(1500, 100);
      Serial.println("{\"type\":\"STATUS\",\"state\":\"WAIT_BTN\",\"msg\":\"Press switch on robot to run PID test\"}");
      waitForButton("Press to Run PID");
    }

    // เริ่มทำงาน
    tft_.fillScreen(ST7735_BLACK);
    printText(5, 30, "RUNNING PID...", 2, ST7735_CYAN);
    printText(5, 65, "Mode: " + mode, 1, ST7735_WHITE);
    Serial.println("{\"type\":\"STATUS\",\"state\":\"RUNNING\",\"msg\":\"PID test in progress...\"}");
    buzzer(2000, 150);

    // ดำเนินการตามโหมดที่เลือก
    if (mode.equalsIgnoreCase("phase")) {
      Fline_by_phase(speed, val);
    } else if (mode.equalsIgnoreCase("pass")) {
      Fline_by_pass(speed, (int)val);
    } else if (mode.equalsIgnoreCase("time")) {
      Fline_by_time(speed, (unsigned long)val);
    } else if (mode.equalsIgnoreCase("turnLeft")) {
      turnLeft(speed);
    } else if (mode.equalsIgnoreCase("turnRight")) {
      turnRight(speed);
    } else if (mode.equalsIgnoreCase("checkLine")) {
      checkLine();
    } else if (mode.equalsIgnoreCase("phase_b")) {
      Fline_by_phase_B(speed, val);
    } else if (mode.equalsIgnoreCase("pass_b")) {
      Fline_by_pass_B(speed, (int)val);
    } else if (mode.equalsIgnoreCase("turnLeft_b")) {
      turnLeft_B(speed);
    } else if (mode.equalsIgnoreCase("turnRight_b")) {
      turnRight_B(speed);
    } else if (mode.equalsIgnoreCase("checkLine_b")) {
      checkLine_B();
    } else if (mode.equalsIgnoreCase("follow_notline")) {
      Follow_Notline(speed, (unsigned long)val);
    } else if (mode.equalsIgnoreCase("follow_notline_enc")) {
      Follow_Notline_by_Encoder(speed, val);
    } else if (mode.equalsIgnoreCase("backward_notline")) {
      Backword_Notline(speed, (unsigned long)val);
    } else {
      // โหมดพื้นฐานเดินหน้าตามเส้น
      Fline_by_phase(speed, (val > 0.1f) ? val : 25.0f);
    }

    stop_4w();
    buzzer(2200, 200);

    Serial.println("{\"type\":\"RES\",\"status\":\"DONE\",\"msg\":\"PID test finished\"}");
    tft_.fillScreen(ST7735_BLACK);
    printText(10, 45, "PID TEST DONE!", 2, ST7735_GREEN);
    printText(5, 80, "Waiting for serial...", 1, ST7735_WHITE);
    delay(800);
    drawIdleScreen();
    return;
  }

  // คำสั่งที่ไม่รู้จัก
  Serial.println("{\"type\":\"ERR\",\"msg\":\"Unknown command: " + cmd + "\"}");
}

} // namespace SerialTester

// ============================================================================
// ฟังก์ชันหลักสำหรับรันโหมด Serial Test & Web Controller
// เรียกใช้ที่ไหนก็ได้ เช่น ใน setup() หรือใน loop()
// จะวนลูปไม่มีที่สิ้นสุด รับคำสั่งจาก Web Dashboard ผ่าน Serial USB
// ============================================================================
inline void runSerialTestMode() {
  Serial.println("\n==========================================");
  Serial.println("  PUPPYBOT6 WEB CONTROLLER ACTIVE");
  Serial.println("  Baud: 115200");
  Serial.println("  Waiting for Web Dashboard commands...");
  Serial.println("==========================================");

  buzzer(2000, 100);
  delay(100);
  buzzer(2400, 150);

  SerialTester::drawIdleScreen();

  String inputBuffer = "";
  inputBuffer.reserve(256);

  unsigned long lastTelemTime = 0;
  unsigned long lastScreenUpdate = 0;

  // วนลูปการทำงานไม่มีที่สิ้นสุด (Infinite Loop)
  while (true) {
    unsigned long now = millis();

    // 1. อ่านข้อมูลคำสั่งจาก Serial USB
    while (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == '\n' || c == '\r') {
        if (inputBuffer.length() > 0) {
          inputBuffer.trim();
          SerialTester::processCommand(inputBuffer);
          inputBuffer = "";
        }
      } else {
        if (inputBuffer.length() < 250) {
          inputBuffer += c;
        }
      }
    }

    // 2. ส่งข้อมูล Telemetry ให้หน้าเว็บทุกๆ 120ms
    if (now - lastTelemTime >= 120) {
      lastTelemTime = now;
      SerialTester::sendTelemetry();
    }

    // 3. อัปเดตข้อมูลบนหน้าจอ TFT หุ่นยนต์ทุกๆ 200ms
    if (now - lastScreenUpdate >= 200) {
      lastScreenUpdate = now;
      SerialTester::updateIdleDisplay();
    }

    delay(5);
  }
}

#endif // PUPPYBOT6_SERIAL_TESTER_H
