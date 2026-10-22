#ifndef PUPPYBOT6_SERIAL_TESTER_H
#define PUPPYBOT6_SERIAL_TESTER_H

#include <Arduino.h>
#include <puppybot6.h>
#include "config.h"
#include "motor_drive.h"
#include "sensor_system.h"
#include "eeprom_calib.h"
#include "line_follow.h"

// ============================================================================
// PUPPYBOT6 SERIAL TESTER & WEB CONTROLLER
// รองรับการรับคำสั่งจาก Web Dashboard ผ่าน Serial USB (115200 Baud)
// - เมื่อเว็บส่งคำสั่งลงมา บอร์ดจะจำคำสั่งไว้ และรีเซ็ตรอบเป็น 0
// - หุ่นยนต์จะรอกดปุ่มเพื่อเริ่มทำงาน (หรือรอคำสั่งใหม่จากเว็บ)
// - เมื่อกดปุ่ม หุ่นจะทำงานตามคำสั่งนั้น และนับจำนวนรอบ (Round Count)
// - เมื่อจบงาน จะกลับมารอกดปุ่มอีกครั้ง สามารถกดปุ่มเพื่อทำงานซ้ำกี่รอบก็ได้!
// - หน้าจอ TFT จะแสดงผลชื่อฟังก์ชัน สถานะ และจำนวนรอบที่ทำไปแล้วชัดเจน
// - หากส่งคำสั่งใหม่จากเว็บ จะรีเซ็ตจำนวนรอบใหม่ และพร้อมรันคำสั่งใหม่ทันที
// ============================================================================

namespace SerialTester {

// โครงสร้างข้อมูลสำหรับจัดเก็บคำสั่งล่าสุดที่ส่งมาจากเว็บ
struct ActiveTask {
  bool hasCommand = false;
  String cmdType = "";     // "TEST_PID", "TEST_MOTOR", "SERVO"
  String mode = "";        // "phase", "pass", "time", "turnLeft", etc.
  int speed = 50;
  float val = 0.0f;
  float kp = 0.40f;
  float kd = -1.0f;
  float kp_b = 0.40f;
  float kd_b = -1.0f;
  int m1 = 0, m2 = 0, m3 = 0, m4 = 0;
  int motor_time = 1000;
  int s1 = 90, s2 = 90;
  int roundCount = 0;      // นับรอบตั้งแต่ส่งข้อมูลหาบอร์ด
  unsigned long lastElapsedMs = 0; // เวลาที่ใช้ในรอบล่าสุด (ms)
  bool isRunning = false;
};

static ActiveTask activeTask;

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
  int rF = Read_Color_TCS(0);
  int gF = Read_Color_TCS(1);
  int bF = Read_Color_TCS(2);
  RobotColor colF = detectColorFront();

  float irDist = readDistanceIR();
  float encDist = getDistanceCm();
  int btnPressed = isButtonPressed() ? 1 : 0;

  Serial.print("{\"type\":\"TELEM\"");
  
  // สีหน้า (Floor Ground)
  Serial.print(",\"fc\":"); Serial.print((int)colF);
  Serial.print(",\"fn\":\""); Serial.print(getColorName(colF)); Serial.print("\"");
  Serial.print(",\"frgb\":["); Serial.print(rF); Serial.print(","); Serial.print(gF); Serial.print(","); Serial.print(bF); Serial.print("]");

  // สีข้าง (Side Object/Cans)
  int rS = Read_Color_TCS_B(0);
  int gS = Read_Color_TCS_B(1);
  int bS = Read_Color_TCS_B(2);
  RobotColor colS = detectColorBack();
  Serial.print(",\"sc\":"); Serial.print((int)colS);
  Serial.print(",\"sn\":\""); Serial.print(getColorName(colS)); Serial.print("\"");
  Serial.print(",\"srgb\":["); Serial.print(rS); Serial.print(","); Serial.print(gS); Serial.print(","); Serial.print(bS); Serial.print("]");

  // ระยะทางและไจโร
  Serial.print(",\"ir\":"); Serial.print(irDist, 1);
  Serial.print(",\"enc\":"); Serial.print(encDist, 1);
  Serial.print(",\"enc_raw\":"); Serial.print(get_pulse_Encoder(selection_Encoder));
  Serial.print(",\"enc_scale\":"); Serial.print(ENCODER_PULSES_PER_CM, 1);
  Serial.print(",\"yaw\":"); Serial.print(yaw_loop1, 1);
  Serial.print(",\"btn\":"); Serial.print(btnPressed);

  // สถานะ Task และตัวนับรอบ + เวลา
  Serial.print(",\"task\":{");
  Serial.print("\"has\":"); Serial.print(activeTask.hasCommand ? 1 : 0);
  Serial.print(",\"round\":"); Serial.print(activeTask.roundCount);
  Serial.print(",\"time_ms\":"); Serial.print(activeTask.lastElapsedMs);
  Serial.print(",\"running\":"); Serial.print(activeTask.isRunning ? 1 : 0);
  Serial.print(",\"cmd\":\""); Serial.print(activeTask.cmdType); Serial.print("\"");
  Serial.print(",\"mode\":\""); Serial.print(activeTask.mode); Serial.print("\"");
  Serial.print("}");

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

// หน้าจอเมื่อเปิดเครื่องและยังไม่มีคำสั่งจากเว็บ
inline void drawIdleScreen() {
  tft_.fillScreen(ST7735_BLACK);
  printText(5, 4, "== WEB TEST LAB ==", 1, ST7735_CYAN);
  tft_.drawFastHLine(0, 16, 160, ST7735_BLUE);

  printText(5, 22, "STATUS: READY (WAIT CMD)", 1, ST7735_GREEN, ST7735_BLACK);
  printText(5, 36, "Serial: 115200 Baud", 1, ST7735_YELLOW, ST7735_BLACK);

  tft_.drawFastHLine(0, 50, 160, ST7735_BLUE);
  printText(5, 56, "Live Sensor Preview:", 1, ST7735_WHITE, ST7735_BLACK);
}

// อัปเดตข้อมูลบนหน้าจอ TFT ระหว่างรอคำสั่งแรก
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

// หน้าจอแสดงคำสั่งปัจจุบัน ตัวนับรอบ เวลาที่ใช้ และสถานะรอกดปุ่ม (กดได้เรื่อยๆ กี่รอบก็ได้)
inline void drawTaskReadyScreen() {
  tft_.fillScreen(ST7735_BLACK);
  printText(4, 3, "== WEB MISSION LAB ==", 1, ST7735_CYAN);
  tft_.drawFastHLine(0, 14, 160, ST7735_BLUE);

  // 1. ข้อมูลคำสั่งและพารามิเตอร์
  if (activeTask.cmdType == "TEST_PID") {
    printText(4, 17, "MODE: " + activeTask.mode, 1, ST7735_YELLOW, ST7735_BLACK);
    char pBuf[32];
    snprintf(pBuf, sizeof(pBuf), "Spd:%d Val:%.1f P:%.2f", activeTask.speed, activeTask.val, activeTask.kp);
    printText(4, 28, pBuf, 1, ST7735_WHITE, ST7735_BLACK);
  } else if (activeTask.cmdType == "TEST_MOTOR") {
    printText(4, 17, "MODE: 4WD MOTOR TEST", 1, ST7735_YELLOW, ST7735_BLACK);
    char mBuf[32];
    snprintf(mBuf, sizeof(mBuf), "M:%d,%d,%d,%d T:%dms", activeTask.m1, activeTask.m2, activeTask.m3, activeTask.m4, activeTask.motor_time);
    printText(4, 28, mBuf, 1, ST7735_WHITE, ST7735_BLACK);
  } else if (activeTask.cmdType == "SERVO") {
    printText(4, 17, "MODE: DUAL SERVO TEST", 1, ST7735_YELLOW, ST7735_BLACK);
    char sBuf[32];
    snprintf(sBuf, sizeof(sBuf), "S1:%d deg  S2:%d deg", activeTask.s1, activeTask.s2);
    printText(4, 28, sBuf, 1, ST7735_WHITE, ST7735_BLACK);
  }

  tft_.drawFastHLine(0, 39, 160, ST7735_BLUE);

  // 2. กล่องแสดงตัวนับรอบ (ROUNDS) และเวลาต่อรอบ (TIME) โดดเด่น ชัดเจน
  tft_.drawRect(2, 42, 156, 56, ST7735_GREEN);
  
  if (activeTask.roundCount == 0) {
    printText(6, 46, "ROUNDS COMPLETED:", 1, ST7735_WHITE, ST7735_BLACK);
    printText(16, 58, " [ ROUND 0 ]", 2, ST7735_GREEN, ST7735_BLACK);
    printText(6, 82, "Ready for 1st round run", 1, ST7735_CYAN, ST7735_BLACK);
  } else {
    char rBuf[32];
    snprintf(rBuf, sizeof(rBuf), "ROUND #%d COMPLETED", activeTask.roundCount);
    printText(6, 46, rBuf, 1, ST7735_GREEN, ST7735_BLACK);

    unsigned long s = activeTask.lastElapsedMs / 1000;
    unsigned long ms = activeTask.lastElapsedMs % 1000;
    float totalSec = (float)activeTask.lastElapsedMs / 1000.0f;

    // แสดงเวลา: ทั้งวินาทีและมิลลิวินาที
    char tBuf1[32];
    snprintf(tBuf1, sizeof(tBuf1), "TIME: %lu s %lu ms", s, ms);
    printText(6, 59, tBuf1, 1, ST7735_YELLOW, ST7735_BLACK);

    char tBuf2[32];
    snprintf(tBuf2, sizeof(tBuf2), "= %.3f sec (%lums)", totalSec, activeTask.lastElapsedMs);
    printText(6, 73, tBuf2, 1, ST7735_CYAN, ST7735_BLACK);

    printText(6, 85, "Lap timer recorded!", 1, ST7735_WHITE, ST7735_BLACK);
  }

  // 3. สถานะคำแนะนำการกดปุ่ม
  if (activeTask.roundCount == 0) {
    printText(4, 102, "> PRESS BTN TO START <", 1, ST7735_YELLOW, ST7735_BLACK);
  } else {
    printText(4, 102, "> PRESS BTN FOR NEXT <", 1, ST7735_YELLOW, ST7735_BLACK);
  }
  printText(4, 115, "(or send new cmd from web)", 1, ST7735_CYAN, ST7735_BLACK);
}

// หน้าจอตอนกำลังทำงาน (Running)
inline void drawTaskRunningScreen() {
  tft_.fillScreen(ST7735_BLACK);
  printText(4, 3, "== MISSION RUNNING ==", 1, ST7735_GREEN);
  tft_.drawFastHLine(0, 14, 160, ST7735_BLUE);

  char rBuf[28];
  snprintf(rBuf, sizeof(rBuf), "RUNNING ROUND: #%d", activeTask.roundCount + 1);
  printText(4, 22, rBuf, 1, ST7735_YELLOW, ST7735_BLACK);

  if (activeTask.cmdType == "TEST_PID") {
    printText(4, 36, "Mode: " + activeTask.mode, 1, ST7735_WHITE, ST7735_BLACK);
  }

  tft_.drawRect(8, 55, 144, 40, ST7735_RED);
  printText(16, 64, ">>> IN ACTION <<<", 1, ST7735_RED, ST7735_BLACK);
  printText(16, 78, "Timing lap duration...", 1, ST7735_YELLOW, ST7735_BLACK);

  printText(4, 110, "Emergency: click STOP on web", 1, ST7735_WHITE, ST7735_BLACK);
}

// ประมวลผลคำสั่งที่บันทึกไว้ใน activeTask
inline void executeActiveTask() {
  if (!activeTask.hasCommand) return;
  activeTask.isRunning = true;

  drawTaskRunningScreen();
  buzzer(1800, 100);

  Serial.print("{\"type\":\"STATUS\",\"state\":\"RUNNING\",\"round\":");
  Serial.print(activeTask.roundCount + 1);
  Serial.print(",\"mode\":\""); Serial.print(activeTask.mode);
  Serial.println("\",\"msg\":\"Mission running...\"}");

  unsigned long runStart = millis();

  // ทำงานตามประเภทคำสั่ง
  if (activeTask.cmdType == "TEST_PID") {
    // ปรับตั้งค่า KP และ KD
    if (activeTask.kp >= 0.0f) line_KP = activeTask.kp;
    if (activeTask.kp_b >= 0.0f) line_KP_B = activeTask.kp_b;
    line_KD = activeTask.kd;
    line_KD_B = activeTask.kd_b;

    String mode = activeTask.mode;
    int speed = activeTask.speed;
    float val = activeTask.val;

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
    } else if (mode.equalsIgnoreCase("turnLeft_by_stop")) {
      turnLeft_by_stop(speed);
    } else if (mode.equalsIgnoreCase("turnRight_by_stop")) {
      turnRight_by_stop(speed);
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
      Follow_Notline_by_Encoder(speed, (long)val);
    } else if (mode.equalsIgnoreCase("backward_notline")) {
      Backword_Notline(speed, (unsigned long)val);
    } else {
      Fline_by_phase(speed, (val > 0.1f) ? val : 25.0f);
    }
  } else if (activeTask.cmdType == "TEST_MOTOR") {
    drive4(activeTask.m1, activeTask.m2, activeTask.m3, activeTask.m4);
    if (activeTask.motor_time > 0) {
      delay(activeTask.motor_time);
      stop_4w();
    }
  } else if (activeTask.cmdType == "SERVO") {
    servoRun(1, activeTask.s1);
    servoRun(2, activeTask.s2);
    delay(400);
  }

  // จบการทำงาน
  stop_4w();
  unsigned long elapsedMs = millis() - runStart;
  activeTask.lastElapsedMs = elapsedMs;
  activeTask.roundCount++; // เพิ่มตัวนับรอบการทำงาน
  activeTask.isRunning = false;

  // เสียงแจ้งเตือนว่าทำงานเสร็จสมบูรณ์
  buzzer(2200, 120);
  delay(80);
  buzzer(2500, 150);

  Serial.print("{\"type\":\"RES\",\"status\":\"DONE\",\"round\":");
  Serial.print(activeTask.roundCount);
  Serial.print(",\"time_ms\":");
  Serial.print(elapsedMs);
  Serial.println(",\"msg\":\"Command finished. Ready for next run.\"}");

  // วาดหน้าจอกลับมารอการกดปุ่มอีกครั้ง (กดซ้ำกี่รอบก็ได้)
  drawTaskReadyScreen();
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
    activeTask.hasCommand = false;
    activeTask.isRunning = false;
    activeTask.roundCount = 0;
    activeTask.lastElapsedMs = 0;

    buzzer(800, 100);
    tft_.fillScreen(ST7735_RED);
    printText(15, 50, "EMERGENCY STOP", 2, ST7735_WHITE);
    delay(600);
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

  // 3.1 SET_ENC_SCALE ตั้งค่าสเกล Encoder pulses ต่อ เซนติเมตร
  if (cmd.equalsIgnoreCase("SET_ENC_SCALE")) {
    float val = getJsonFloat(cmdLine, "val", ENCODER_PULSES_PER_CM);
    if (val > 1.0f) {
      calib.encoder_pulses_per_cm = val;
      ENCODER_PULSES_PER_CM = val;
      saveCalibrationToEEPROM();
      buzzer(2000, 100);
      Serial.print("{\"type\":\"RES\",\"status\":\"ENC_SAVED\",\"val\":");
      Serial.print(val, 2);
      Serial.println(",\"msg\":\"Encoder scale saved to EEPROM\"}");
    }
    return;
  }

  // 3.2 RESET_ENC รีเซ็ตตัวนับพัลส์ Encoder เป็น 0
  if (cmd.equalsIgnoreCase("RESET_ENC")) {
    resetEncoder(selection_Encoder);
    buzzer(1500, 60);
    Serial.println("{\"type\":\"RES\",\"status\":\"ENC_RESET\",\"msg\":\"Encoder reset to 0\"}");
    return;
  }

  // 4. TEST_PID ทดสอบการวิ่งตามเส้น PID หรือการเลี้ยวต่างๆ
  if (cmd.equalsIgnoreCase("TEST_PID")) {
    activeTask.hasCommand = true;
    activeTask.cmdType = "TEST_PID";
    activeTask.mode = getJsonStr(cmdLine, "mode");
    activeTask.speed = getJsonInt(cmdLine, "speed", 50);
    activeTask.kp = getJsonFloat(cmdLine, "kp", line_KP);
    activeTask.kp_b = getJsonFloat(cmdLine, "kp_b", line_KP_B);
    activeTask.kd = getJsonFloat(cmdLine, "kd", line_KD);
    activeTask.kd_b = getJsonFloat(cmdLine, "kd_b", line_KD_B);
    activeTask.val = getJsonFloat(cmdLine, "val", 0.0f);
    activeTask.roundCount = 0; // รีเซ็ตรอบใหม่ทุกครั้งที่มีการส่งข้อมูลใหม่จากเว็บ!
    activeTask.lastElapsedMs = 0;
    activeTask.isRunning = false;

    buzzer(1800, 120);
    Serial.print("{\"type\":\"STATUS\",\"state\":\"WAIT_BTN\",\"round\":0,\"mode\":\"");
    Serial.print(activeTask.mode);
    Serial.println("\",\"msg\":\"New PID task received. Press button on robot to run.\"}");

    drawTaskReadyScreen();
    return;
  }

  // 5. TEST_MOTOR ทดสอบมอเตอร์ 4 ล้ออิสระ (M1, M2, M3, M4)
  if (cmd.equalsIgnoreCase("TEST_MOTOR")) {
    activeTask.hasCommand = true;
    activeTask.cmdType = "TEST_MOTOR";
    activeTask.mode = "4WD";
    activeTask.m1 = getJsonInt(cmdLine, "m1", 0);
    activeTask.m2 = getJsonInt(cmdLine, "m2", 0);
    activeTask.m3 = getJsonInt(cmdLine, "m3", 0);
    activeTask.m4 = getJsonInt(cmdLine, "m4", 0);
    activeTask.motor_time = getJsonInt(cmdLine, "time", 1000);
    activeTask.roundCount = 0; // รีเซ็ตรอบใหม่
    activeTask.lastElapsedMs = 0;
    activeTask.isRunning = false;

    buzzer(1800, 120);
    Serial.println("{\"type\":\"STATUS\",\"state\":\"WAIT_BTN\",\"round\":0,\"msg\":\"New Motor task received. Press button on robot to run.\"}");

    drawTaskReadyScreen();
    return;
  }

  // 6. SERVO สั่งงานมุมเซอร์โว 1 และ 2
  if (cmd.equalsIgnoreCase("SERVO")) {
    activeTask.hasCommand = true;
    activeTask.cmdType = "SERVO";
    activeTask.mode = "SERVO";
    activeTask.s1 = getJsonInt(cmdLine, "s1", 50);
    activeTask.s2 = getJsonInt(cmdLine, "s2", 146);
    activeTask.roundCount = 0; // รีเซ็ตรอบใหม่
    activeTask.lastElapsedMs = 0;
    activeTask.isRunning = false;

    buzzer(1800, 120);
    Serial.println("{\"type\":\"STATUS\",\"state\":\"WAIT_BTN\",\"round\":0,\"msg\":\"New Servo task received. Press button on robot to run.\"}");

    drawTaskReadyScreen();
    return;
  }

  // 7. CALIB คำสั่งเรียกใช้งานการคาลิเบรตแต่ละระบบ
  if (cmd.equalsIgnoreCase("CALIB")) {
    String target = getJsonStr(cmdLine, "target");
    target.trim();

    buzzer(1800, 120);
    Serial.println("{\"type\":\"STATUS\",\"state\":\"CALIBRATING\",\"target\":\"" + target + "\",\"msg\":\"Starting calibration on robot...\"}");

    if (target.equalsIgnoreCase("front")) {
      calibrateFrontSensors();
    } else if (target.equalsIgnoreCase("back")) {
      calibrateBackSensors();
    } else if (target.equalsIgnoreCase("color") || target.equalsIgnoreCase("color_floor")) {
      calibrateTCSColorsFloor();
    } else if (target.equalsIgnoreCase("color_side")) {
      calibrateTCSColorsSide();
    } else if (target.equalsIgnoreCase("encoder") || target.equalsIgnoreCase("enc")) {
      calibrateEncoderDistance();
    } else if (target.equalsIgnoreCase("ir")) {
      calibrateIRDistance();
    } else if (target.equalsIgnoreCase("menu")) {
      runCalibrationMenu();
    } else {
      runCalibrationMenu();
    }

    buzzer(2200, 120);
    delay(80);
    buzzer(2500, 150);

    Serial.println("{\"type\":\"RES\",\"status\":\"CALIB_DONE\",\"target\":\"" + target + "\",\"msg\":\"Calibration finished & saved to EEPROM!\"}");

    // ส่งข้อมูล EEPROM ล่าสุดกลับไปอัปเดตหน้าเว็บทันที
    Serial.print("{\"type\":\"CALIB_DATA\"");
    Serial.print(",\"front_min\":[");
    for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
      Serial.print(calib.front_min[i]);
      if (i < NUM_FRONT_SENSORS - 1) Serial.print(",");
    }
    Serial.print("],\"front_max\":[");
    for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
      Serial.print(calib.front_max[i]);
      if (i < NUM_FRONT_SENSORS - 1) Serial.print(",");
    }
    Serial.print("],\"back_min\":[");
    for (int i = 0; i < NUM_BACK_SENSORS; i++) {
      Serial.print(calib.back_min[i]);
      if (i < NUM_BACK_SENSORS - 1) Serial.print(",");
    }
    Serial.print("],\"back_max\":[");
    for (int i = 0; i < NUM_BACK_SENSORS; i++) {
      Serial.print(calib.back_max[i]);
      if (i < NUM_BACK_SENSORS - 1) Serial.print(",");
    }
    Serial.print("],\"ir_5cm\":"); Serial.print(calib.ir_raw_5cm);
    Serial.print(",\"ir_10cm\":"); Serial.print(calib.ir_raw_10cm);
    Serial.print(",\"enc_scale\":"); Serial.print(calib.encoder_pulses_per_cm, 2);
    Serial.println("}");

    if (activeTask.hasCommand) {
      drawTaskReadyScreen();
    } else {
      drawIdleScreen();
    }
    return;
  }

  // 8. GET_CALIB ดึงค่า Calibrate ที่บันทึกอยู่ใน EEPROM
  if (cmd.equalsIgnoreCase("GET_CALIB")) {
    Serial.print("{\"type\":\"CALIB_DATA\"");
    Serial.print(",\"front_min\":[");
    for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
      Serial.print(calib.front_min[i]);
      if (i < NUM_FRONT_SENSORS - 1) Serial.print(",");
    }
    Serial.print("],\"front_max\":[");
    for (int i = 0; i < NUM_FRONT_SENSORS; i++) {
      Serial.print(calib.front_max[i]);
      if (i < NUM_FRONT_SENSORS - 1) Serial.print(",");
    }
    Serial.print("],\"back_min\":[");
    for (int i = 0; i < NUM_BACK_SENSORS; i++) {
      Serial.print(calib.back_min[i]);
      if (i < NUM_BACK_SENSORS - 1) Serial.print(",");
    }
    Serial.print("],\"back_max\":[");
    for (int i = 0; i < NUM_BACK_SENSORS; i++) {
      Serial.print(calib.back_max[i]);
      if (i < NUM_BACK_SENSORS - 1) Serial.print(",");
    }
    Serial.print("],\"ir_5cm\":"); Serial.print(calib.ir_raw_5cm);
    Serial.print(",\"ir_10cm\":"); Serial.print(calib.ir_raw_10cm);
    Serial.print(",\"enc_scale\":"); Serial.print(calib.encoder_pulses_per_cm, 2);
    Serial.println("}");
    return;
  }

  // คำสั่งที่ไม่รู้จัก
  Serial.println("{\"type\":\"ERR\",\"msg\":\"Unknown command: " + cmd + "\"}");
}

} // namespace SerialTester

// ============================================================================
// ฟังก์ชันหลักสำหรับรันโหมด Serial Test & Web Controller
// จะวนลูปไม่มีที่สิ้นสุด รับคำสั่งจาก Web Dashboard ผ่าน Serial USB
// รองรับการรอกดปุ่มเพื่อเริ่ม และกดปุ่มซ้ำเพื่อวิ่งคำสั่งเดิมกี่รอบก็ได้
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

  // วนลูปการทำงานไม่มีที่สิ้นสุด (Infinite Non-blocking Loop)
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

    // 3. ตรวจสอบการกดปุ่มบนตัวหุ่นเพื่อรันฟังก์ชัน (กดซ้ำกี่รอบก็ได้)
    if (SerialTester::activeTask.hasCommand && !SerialTester::activeTask.isRunning) {
      if (isButtonPressed()) {
        buzzer(1200, 80);
        // รอปล่อยปุ่ม ป้องกันการเบิ้ล
        while (isButtonPressed()) {
          delay(10);
        }
        delay(60);
        SerialTester::executeActiveTask();
      }
    }

    // 4. อัปเดตข้อมูลบนหน้าจอ TFT (เฉพาะตอนที่ยังไม่มี Task จากเว็บ)
    if (!SerialTester::activeTask.hasCommand) {
      if (now - lastScreenUpdate >= 200) {
        lastScreenUpdate = now;
        SerialTester::updateIdleDisplay();
      }
    }

    delay(5);
  }
}

#endif // PUPPYBOT6_SERIAL_TESTER_H
