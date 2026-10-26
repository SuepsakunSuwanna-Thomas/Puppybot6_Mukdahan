#ifndef PUPPYBOT6_COLOR_SCANNER_H
#define PUPPYBOT6_COLOR_SCANNER_H

#include <Arduino.h>
#include <algorithm>
#include "config.h"
#include "motor_drive.h"
#include "sensor_system.h"
#include "eeprom_calib.h"
#include "line_follow.h"

// ============================================================================
// 1. การตั้งค่าตำแหน่งพัลส์สำหรับ 6 ช่องสี (Slot Targets Configuration)
// ============================================================================
// ตำแหน่งพัลส์สะสมสัมบูรณ์ (Absolute Target Pulses) นับจากเส้นดำจุดสตาร์ท
// สามารถปรับแก้ค่าตำแหน่งของแต่ละช่องแยกอิสระได้เลย ไม่กระทบช่องอื่น
inline long TARGET_COLOR_PULSES[6] = {1150, 1720, 2080, 3755, 4000, 5240};

// ระยะหน้าต่างการอ่านสีก่อนและหลังเป้าหมาย (+- 20 พัลส์)
const long COLOR_SCAN_WINDOW = 30;

// ============================================================================
// 2. โครงสร้างบันทึกข้อมูลแถบสีและสรุปผลภารกิจ
// ============================================================================
struct ColorRecord {
  char name[12];
  RobotColor color;
  long pulse;
  float cm;
  int r, g, b, lux;
  const char* sensor;
};

struct MissionSummary {
  bool completed;
  unsigned long duration_ms;
  float distance_cm;
  long pulses;
  int colorCount;
  ColorRecord records[12];
  char finishReason[24];
};

inline MissionSummary mission_summary = {false, 0, 0.0f, 0, 0, {}, "INIT"};

enum ScreenState {
  SCREEN_COLOR_SUMMARY,
  SCREEN_TIME_SUMMARY
};
inline ScreenState current_screen_state = SCREEN_COLOR_SUMMARY;
inline bool screen_drawn = false;

// Forward Declarations
void drawColorSummaryScreen();
void drawMissionTimeOnlyScreen();

// ============================================================================
// 3. ฟังก์ชันสแกนสีแบบต่อเนื่อง (Continuous Non-Stop Scan)
// ============================================================================
// ============================================================================
// 3. ฟังก์ชันสแกนสีแบบต่อเนื่อง (Continuous Non-Stop Scan)
// ============================================================================
// speed: ความเร็วในการวิ่งตามเส้น
// max_slots: จำนวนช่องที่ต้องการทดสอบ (1 ถึง 6 ช่อง, ค่าเริ่มต้น 6)
//   - หากระบุ 1-5 ช่อง: หุ่นยนต์จะวิ่งไปหยุดตรงตำแหน่งพัลส์ของช่องนั้นทันที เพื่อตรวจดูตำแหน่งจริงว่าถึงไหน
//   - หากระบุ 6 ช่อง: หุ่นยนต์จะสแกนครบทุกช่องจนพ้นแถบคำสั่งซื้อ
inline void test_start_and_scan_colors(int speed = 30, int max_slots = 6) {
  max_slots = constrain(max_slots, 1, 6);

  // หากทดสอบช่อง 1-5 ให้หยุดตรงพิกัดพัลส์ของช่องนั้นทันทีเพื่อเช็คตำแหน่งว่าวิ่งถึงไหน
  // หากทดสอบครบ 6 ช่อง ให้วิ่งจนพ้นหน้าต่างสแกนของช่องที่ 6
  const long finish_pulse = (max_slots < 6) ? TARGET_COLOR_PULSES[max_slots - 1]
                                            : (TARGET_COLOR_PULSES[5] + COLOR_SCAN_WINDOW + 5);

  mission_summary.colorCount = 0;
  buzzer(1500, 60);

  // 1. วิ่งตรงจนกว่าจะเจอเส้นดำขอบกล่องสตาร์ท
  while (!isFrontSensorOnLine(3)) {
    if (isButtonPressed()) { stop_4w(); return; }
    fd_4w(speed);
    delay(1);
  }

  reset_PID_state();
  resetDistance();
  long baseline_pulse = get_pulse_Encoder(selection_Encoder);

  // สถิติการอ่านเซนเซอร์สีในแต่ละช่อง [slot 0..5][color 1..6]
  int color_counts[6][7] = {0};
  int total_samples[6] = {0};
  long min_diff[6][7];
  for (int s = 0; s < 6; s++) {
    for (int c = 1; c <= 6; c++) {
      min_diff[s][c] = 99999999L;
    }
  }

  unsigned long last_sample_time = 0;

  // ตรวจสอบความถูกต้องของลำดับพัลส์
  for (int i = 0; i < max_slots - 1; i++) {
    if (TARGET_COLOR_PULSES[i] >= TARGET_COLOR_PULSES[i+1]) {
      Serial.printf("[เตือนภัย บัคพัลส์] ช่อง %d (%ld) ดันมากกว่าหรือเท่ากับช่อง %d (%ld)! พัลส์ต้องเรียงจากน้อยไปมากเสมอ\n",
                    i + 1, TARGET_COLOR_PULSES[i], i + 2, TARGET_COLOR_PULSES[i+1]);
    }
  }

  // ตัวแปรเช็คว่าช่องใดหยุดอ่าน 10ms ไปแล้วบ้าง
  bool slot_stopped[6] = {false};

  // 2. วิ่งตามเส้นแบบ PID ไปข้างหน้า พร้อมหยุดอ่าน 10ms ในแต่ละช่อง
  while (true) {
    if (isButtonPressed()) { stop_4w(); return; }

    long current_p = abs(get_pulse_Encoder(selection_Encoder) - baseline_pulse);

    // ตรวจสอบว่าถึงพิกัดกึ่งกลางช่องใด เพื่อหยุด 10ms อ่านค่าสีแบบนิ่งสนิท
    for (int step = 0; step < max_slots; step++) {
      if (current_p >= TARGET_COLOR_PULSES[step] && !slot_stopped[step]) {
        slot_stopped[step] = true;
        stop_4w(); // หยุดมอเตอร์ทั้ง 4 ล้อสนิททันที

        // หยุด 10ms อ่านค่าสีแบบนิ่งสนิท ไร้การสั่นไหวและ Motion Blur
        unsigned long stop_start = millis();
        while (millis() - stop_start < 10) {
          int r = Read_Color_TCS(0);
          int g = Read_Color_TCS(1);
          int b = Read_Color_TCS(2);
          int clear_val = Read_Color_TCS(4);
          RobotColor col = detectColorFloor();

          if (col >= 1 && col <= 6) {
            color_counts[step][col] += 5; // ให้น้ำหนักค่าสีตอนหยุดนิ่งสูงเป็นพิเศษ
            total_samples[step] += 5;
          }

          for (int c = 0; c < 5; c++) {
            long dr = r - calib.tcs_front_rgb[c][0];
            long dg = g - calib.tcs_front_rgb[c][1];
            long db = b - calib.tcs_front_rgb[c][2];
            long diff = dr * dr + dg * dg + db * db;
            if (c == 1 && (g < r + 15 || g < b + 15)) diff += 15000;
            int color_id = c + 1;
            if (diff < min_diff[step][color_id]) {
              min_diff[step][color_id] = diff;
            }
          }
          if (clear_val < min_diff[step][COLOR_BLACK]) {
            min_diff[step][COLOR_BLACK] = clear_val;
          }
          delay(1);
        }
        _line_prev_err = 0; // รีเซ็ต derivative ป้องกันหุ่นยนต์กระตุกตอนออกตัวใหม่
        break;
      }
    }

    if (current_p >= finish_pulse) break;

    // อ่านค่าสีช่วงวิ่งเข้าหา/ออกจากหน้าต่างของช่อง [target - 30, target + 30]
    for (int step = 0; step < max_slots; step++) {
      long p_start = TARGET_COLOR_PULSES[step] - COLOR_SCAN_WINDOW;
      long p_end   = TARGET_COLOR_PULSES[step] + COLOR_SCAN_WINDOW;

      if (current_p >= p_start && current_p <= p_end) {
        if (millis() - last_sample_time >= 3) {
          last_sample_time = millis();

          int r = Read_Color_TCS(0);
          int g = Read_Color_TCS(1);
          int b = Read_Color_TCS(2);
          int clear_val = Read_Color_TCS(4);
          RobotColor col = detectColorFloor();

          if (col >= 1 && col <= 6) {
            color_counts[step][col]++;
            total_samples[step]++;
          }

          for (int c = 0; c < 5; c++) {
            long dr = r - calib.tcs_front_rgb[c][0];
            long dg = g - calib.tcs_front_rgb[c][1];
            long db = b - calib.tcs_front_rgb[c][2];
            long diff = dr * dr + dg * dg + db * db;
            if (c == 1 && (g < r + 15 || g < b + 15)) diff += 15000;
            int color_id = c + 1;
            if (diff < min_diff[step][color_id]) {
              min_diff[step][color_id] = diff;
            }
          }
          if (clear_val < min_diff[step][COLOR_BLACK]) {
            min_diff[step][COLOR_BLACK] = clear_val;
          }
        }
        break;
      }
    }

    lineFollow_PID_auto(speed);
  }

  // วิ่งถึงเป้าหมายแล้ว หยุดสนิททันที
  stop_4w();
  buzzer(1800, 150);

  // 3. วิเคราะห์ข้อมูลสีและจับคู่แบบ 1-to-1 (รับประกันสีไม่ซ้ำกันอย่างแน่นอน 100% แม้จะสแกนแค่ 2 ช่อง)
  const RobotColor candidate_colors[6] = {
    COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_WHITE, COLOR_BLACK
  };

  RobotColor final_colors[6] = {COLOR_UNKNOWN};
  long score[6][7] = {0};

  for (int s = 0; s < max_slots; s++) {
    int c_white = color_counts[s][COLOR_WHITE];
    int c_red   = color_counts[s][COLOR_RED];
    int c_green = color_counts[s][COLOR_GREEN];
    int c_blue  = color_counts[s][COLOR_BLUE];
    int c_yellow= color_counts[s][COLOR_YELLOW];
    int c_black = color_counts[s][COLOR_BLACK];
    int non_white_sum = c_red + c_green + c_blue + c_yellow + c_black;

    for (int ci = 0; ci < 6; ci++) {
      RobotColor c = candidate_colors[ci];
      if (c == COLOR_WHITE) {
        if (non_white_sum == 0) {
          score[s][c] = 50000L + (c_white * 100L);
        } else if (c_white > non_white_sum * 4) {
          score[s][c] = c_white * 300L;
        } else {
          score[s][c] = 0;
        }
      } else {
        if (color_counts[s][c] > 0) {
          score[s][c] = color_counts[s][c] * 1000L;
          if (min_diff[s][c] < 20000) {
            score[s][c] += (20000 - min_diff[s][c]) / 20;
          }
        } else {
          score[s][c] = 0;
        }
      }
    }
  }

  // คำนวณจับคู่สีแบบ 1-to-1 ด้วย Permutation สำหรับทุกจำนวนช่องที่สแกน (1 ถึง 6 ช่อง)
  // รับประกันว่าช่องที่ 1 และช่องที่ 2 (รวมถึงช่องอื่นๆ) จะไม่มีทางได้สีซ้ำกันเด็ดขาด
  long max_total_score = -1;
  int perm[6] = {0, 1, 2, 3, 4, 5};

  do {
    long current_score = 0;
    for (int i = 0; i < max_slots; i++) {
      RobotColor col = candidate_colors[perm[i]];
      current_score += score[i][col];
    }
    if (current_score > max_total_score) {
      max_total_score = current_score;
      for (int i = 0; i < max_slots; i++) {
        final_colors[i] = candidate_colors[perm[i]];
      }
    }
  } while (std::next_permutation(perm, perm + 6));

  // 4. บันทึกผลลัพธ์ลงใน mission_summary
  mission_summary.colorCount = 0;
  char titleBuf[64];
  snprintf(titleBuf, sizeof(titleBuf), "\n=== SCAN RESULTS (Scanned %d of 6 Slots) ===", max_slots);
  Serial.println(titleBuf);

  for (int step = 0; step < max_slots; step++) {
    RobotColor final_col = final_colors[step];
    strncpy(mission_summary.records[step].name, getColorName(final_col), sizeof(mission_summary.records[step].name));
    mission_summary.records[step].color = final_col;
    mission_summary.records[step].pulse = TARGET_COLOR_PULSES[step];
    mission_summary.records[step].cm = encoderToCm(TARGET_COLOR_PULSES[step]);
    mission_summary.colorCount++;

    char logBuf[96];
    snprintf(logBuf, sizeof(logBuf), "Slot #%d [Target Pulse %4ld]: %-6s (Counts: R%d G%d B%d Y%d W%d K%d)",
             step + 1, TARGET_COLOR_PULSES[step], getColorName(final_col),
             color_counts[step][COLOR_RED], color_counts[step][COLOR_GREEN],
             color_counts[step][COLOR_BLUE], color_counts[step][COLOR_YELLOW],
             color_counts[step][COLOR_WHITE], color_counts[step][COLOR_BLACK]);
    Serial.println(logBuf);
  }

  mission_summary.completed = true;
  drawColorSummaryScreen();
}

// ฟังก์ชันทางเลือกชื่อเดียวกัน
inline void scanColorsContinuous(int speed = 30, int max_slots = 6) {
  test_start_and_scan_colors(speed, max_slots);
}

// ============================================================================
// 4. ฟังก์ชันช่วยเหลือสำหรับอ่านข้อมูลสีในแต่ละช่อง (Query Helpers)
// ============================================================================
// อ่านสีของช่องที่ระบุ (ช่อง 0 ถึง 5)
inline RobotColor getColorAtSlot(int slotIndex) {
  if (slotIndex >= 0 && slotIndex < mission_summary.colorCount) {
    return mission_summary.records[slotIndex].color;
  }
  return COLOR_UNKNOWN;
}

// ค้นหาว่าสีที่ต้องการ อยู่ที่ช่องลำดับใด (คืนค่า 0-5, หรือ -1 หากไม่พบ)
inline int findSlotOfColor(RobotColor col) {
  for (int i = 0; i < mission_summary.colorCount; i++) {
    if (mission_summary.records[i].color == col) {
      return i;
    }
  }
  return -1;
}

// ตรวจสอบว่าช่องที่ระบุตรงกับสีที่ต้องการหรือไม่
inline bool isSlotColor(int slotIndex, RobotColor col) {
  return (getColorAtSlot(slotIndex) == col);
}

// ============================================================================
// 5. ฟังก์ชันวาดหน้าจอสรุปผล (UI Display Screens)
// ============================================================================
inline void drawColorSummaryScreen() {
  tft_.fillScreen(ST7735_BLACK);

  // 1. Header Bar ด้านบนสุด
  tft_.fillRoundRect(2, 2, 156, 16, 3, 0x0A22); // Emerald Dark Green
  tft_.drawRoundRect(2, 2, 156, 16, 3, ST7735_GREEN);
  printText(24, 6, "COLOR SCAN SUMMARY", 1, ST7735_GREEN, 0x0A22);

  // 2. แสดงรายการสีทั้งหมดที่บันทึกได้
  int count = mission_summary.colorCount;
  if (count == 0) {
    tft_.fillRoundRect(4, 25, 152, 74, 4, 0x0841);
    tft_.drawRoundRect(4, 25, 152, 74, 4, ST7735_CYAN);
    printText(20, 42, "No color detected!", 1, ST7735_RED, 0x0841);
    printText(14, 60, "Check TCS Sensor & Calib", 1, ST7735_WHITE, 0x0841);
  } else {
    int spacing = (count >= 7) ? 12 : 14;
    int y = (count >= 7) ? 20 : 22;

    for (int i = 0; i < count && i < 7; i++) {
      ColorRecord &rec = mission_summary.records[i];

      uint16_t badgeColor = ST7735_WHITE;
      if (rec.color == COLOR_RED) badgeColor = ST7735_RED;
      else if (rec.color == COLOR_GREEN) badgeColor = ST7735_GREEN;
      else if (rec.color == COLOR_BLUE) badgeColor = ST7735_CYAN;
      else if (rec.color == COLOR_YELLOW) badgeColor = ST7735_YELLOW;
      else if (rec.color == COLOR_BLACK) badgeColor = 0x632C; // Dark Gray

      // วาดสัญลักษณ์จุดสี (Color Badge Box)
      tft_.fillRoundRect(4, y + 1, 8, (spacing > 12 ? 9 : 8), 2, badgeColor);
      tft_.drawRoundRect(4, y + 1, 8, (spacing > 12 ? 9 : 8), 2, ST7735_WHITE);

      // ข้อความ: ลำดับ, ชื่อสี, พัลส์, ระยะทาง cm
      char itemBuf[32];
      snprintf(itemBuf, sizeof(itemBuf), "#%d %-6s %4ldp %4.1fcm",
               i + 1, rec.name, rec.pulse, rec.cm);
      printText(16, y + 1, itemBuf, 1, badgeColor, ST7735_BLACK);

      y += spacing;
    }
  }

  // 3. แถบคำสั่ง Footer ด้านล่างสุด
  tft_.fillRoundRect(2, 107, 156, 18, 3, 0x0841);
  tft_.drawRoundRect(2, 107, 156, 18, 3, ST7735_CYAN);
  if (button_mode == 3) {
    printText(17, 112, "[OK]=TIME  [B1]=START", 1, ST7735_YELLOW, 0x0841);
  } else {
    printText(14, 112, "PRESS [BTN] FOR TIME >", 1, ST7735_YELLOW, 0x0841);
  }
}

inline void drawMissionTimeOnlyScreen() {
  tft_.fillScreen(ST7735_BLACK);

  // 1. Header Bar ด้านบนสุด
  tft_.fillRoundRect(4, 4, 152, 16, 3, 0x0A22); // Emerald Dark Green
  tft_.drawRoundRect(4, 4, 152, 16, 3, ST7735_GREEN);
  printText(28, 8, "MISSION TIME", 1, ST7735_GREEN, 0x0A22);

  unsigned long total_ms = mission_summary.duration_ms;
  unsigned long m = total_ms / 60000;
  unsigned long s = (total_ms % 60000) / 1000;
  unsigned long ms = total_ms % 1000;

  // 2. Main Hero Time Card ขนาดใหญ่ตรงกลางจอ
  tft_.fillRoundRect(4, 25, 152, 74, 4, 0x0841); // Dark Tech Blue
  tft_.drawRoundRect(4, 25, 152, 74, 4, ST7735_CYAN);
  tft_.drawRoundRect(5, 26, 150, 72, 3, 0x0115);

  printText(10, 31, "TOTAL ELAPSED", 1, ST7735_CYAN, 0x0841);

  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu.%03lu", m, s, ms);
  printText(26, 47, timeBuf, 2, ST7735_YELLOW, 0x0841);

  char subTimeBuf[36];
  if (m > 0) {
    snprintf(subTimeBuf, sizeof(subTimeBuf), "%lu min  %lu sec  %03lu ms", m, s, ms);
  } else {
    snprintf(subTimeBuf, sizeof(subTimeBuf), "%lu sec  %03lu ms", s, ms);
  }
  int subLen = strlen(subTimeBuf);
  int subX = (160 - (subLen * 6)) / 2;
  if (subX < 6) subX = 6;
  printText(subX, 71, subTimeBuf, 1, ST7735_WHITE, 0x0841);

  char distBuf[36];
  snprintf(distBuf, sizeof(distBuf), "Dist: %.1f cm (%ld p)", mission_summary.distance_cm, mission_summary.pulses);
  int distLen = strlen(distBuf);
  int distX = (160 - (distLen * 6)) / 2;
  if (distX < 6) distX = 6;
  printText(distX, 85, distBuf, 1, ST7735_CYAN, 0x0841);

  // 3. แถบคำสั่งด้านล่างสุดสำหรับกดเริ่มใหม่
  tft_.fillRoundRect(4, 105, 152, 18, 3, 0x2104);
  tft_.drawRoundRect(4, 105, 152, 18, 3, 0x4208);
  if (button_mode == 3) {
    printText(14, 110, "[OK]=COLOR  [B1]=START", 1, ST7735_YELLOW, 0x2104);
  } else {
    printText(14, 110, "PRESS BUTTON TO RESTART", 1, ST7735_YELLOW, 0x2104);
  }
}

// จัดการ UI และการกดปุ่มสลับหน้าจอ / เริ่มใหม่
inline void updateMissionUI() {
  if (!mission_summary.completed) {
    displayLightSensorsFrontAndBack();
    if (checkLongPress(1200)) {
      runCalibrationMenu();
    }
    return;
  }

  if (!screen_drawn) {
    if (current_screen_state == SCREEN_COLOR_SUMMARY) {
      drawColorSummaryScreen();
    } else {
      drawMissionTimeOnlyScreen();
    }
    screen_drawn = true;
  }

  if (button_mode == 3) {
    if (isBtnOkPressed()) {
      buzzer(2000, 60);
      while (isBtnOkPressed()) delay(10);
      delay(150);
      current_screen_state = (current_screen_state == SCREEN_COLOR_SUMMARY) ? SCREEN_TIME_SUMMARY : SCREEN_COLOR_SUMMARY;
      screen_drawn = false;
    } else if (isBtnStartPressed()) {
      buzzer(2400, 100);
      while (isBtnStartPressed()) delay(10);
      delay(200);
      current_screen_state = SCREEN_COLOR_SUMMARY;
      screen_drawn = false;
      test_start_and_scan_colors(30);
    }
  } else {
    if (isButtonPressed()) {
      buzzer(2200, 80);
      while (isButtonPressed()) delay(10);
      delay(150);

      if (current_screen_state == SCREEN_COLOR_SUMMARY) {
        current_screen_state = SCREEN_TIME_SUMMARY;
        screen_drawn = false;
      } else {
        current_screen_state = SCREEN_COLOR_SUMMARY;
        screen_drawn = false;
        test_start_and_scan_colors(30);
      }
    }
  }
}

#endif // PUPPYBOT6_COLOR_SCANNER_H
