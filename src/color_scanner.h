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
inline long TARGET_COLOR_PULSES[6] = {1040, 1275, 1510, 1755, 2000, 2240};

// ระยะหน้าต่างการอ่านสีก่อนและหลังเป้าหมาย (+- 30 พัลส์)
const long COLOR_SCAN_WINDOW = 30;

// พัลส์วิ่งออกจากจุดสตาร์ทก่อนเริ่มสแกนสีรัวๆ (ปรับเปลี่ยนค่าได้ที่นี่ หรือใส่ในฟังก์ชันได้เลย)
inline long INITIAL_SCAN_START_PULSES = 150;

// ตำแหน่งกึ่งกลางของแถบสีแรกนับจากเส้นดำจุดสตาร์ท (ประมาณ 1040 พัลส์)
inline long FIRST_STRIP_PULSE = 1040;

// ระยะห่างระหว่างกึ่งกลางของแต่ละช่องสี (เท่ากันทุกช่องตามแผ่นเพลท ประมาณ 235-240 พัลส์)
inline long STRIP_STEP_PULSES = 240;

// ระยะพัลส์เผื่อ (Tolerance) สำหรับตัดสินว่าเป็นสีขาว เมื่อเลยจุดกึ่งกลางช่องไปแล้วไม่พบสีใดๆ
inline long WHITE_TIMEOUT_TOLERANCE = 60;

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
// ฟังก์ชันตรวจสอบและป้องกันสีซ้ำ 100% (Zero-Duplicate Color Integrity Guard)
// ตรวจสอบทั้ง 6 ช่อง หากมีสีซ้ำกันจะทำการแก้เป็นสีที่ขาดหายไปทันที รับประกัน 0 สีซ้ำ
// ============================================================================
inline void enforceUnique6Colors(MissionSummary &summary) {
  if (summary.colorCount != 6) return;

  int counts[7] = {0};
  for (int i = 0; i < 6; i++) {
    RobotColor c = summary.records[i].color;
    if (c >= 1 && c <= 6) counts[c]++;
  }

  bool has_duplicate = false;
  for (int c = 1; c <= 6; c++) {
    if (counts[c] != 1) {
      has_duplicate = true;
      break;
    }
  }

  if (!has_duplicate) {
    Serial.println(">> [COLOR INTEGRITY CHECK] Passed 100%! All 6 colors (R, G, B, Y, W, K) are completely unique with 0 duplicates!");
    return;
  }

  Serial.println(">> [COLOR INTEGRITY CHECK] Duplicate color detected! Repairing to strict 1-to-1 unique colors...");

  // รวบรวมแม่สีที่ยังขาดหายไปใน 6 ช่อง
  RobotColor missing[6];
  int missing_count = 0;
  for (int c = 1; c <= 6; c++) {
    if (counts[c] == 0) {
      missing[missing_count++] = (RobotColor)c;
    }
  }

  // แก้ไขช่องที่มีสีซ้ำ โดยนำสีที่ขาดหายไปมาแทนที่
  bool seen[7] = {false};
  int missing_idx = 0;
  for (int i = 0; i < 6; i++) {
    RobotColor c = summary.records[i].color;
    if (c < 1 || c > 6 || seen[c]) {
      if (missing_idx < missing_count) {
        RobotColor replacement = missing[missing_idx++];
        summary.records[i].color = replacement;
        strncpy(summary.records[i].name, getColorName(replacement), sizeof(summary.records[i].name));
        seen[replacement] = true;
        Serial.printf(">> [REPAIR] Fixed duplicate in Slot #%d -> Assigned missing color: %s\n",
                      i + 1, getColorName(replacement));
      }
    } else {
      seen[c] = true;
    }
  }
}

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

  // 1. วิ่งตรงออกจากจุดสตาร์ทจนกว่าเซนเซอร์หน้าจะแตะเจอเส้นดำเส้นแรกของสนาม
  while (countFrontSensorsOnLine() == 0) {
    if (isButtonPressed()) { stop_4w(); return; }
    fd_4w(speed);
    delay(1);
  }

  buzzer(1800, 80); // เสียงเตือน: แตะเจอเส้นดำเส้นแรกเรียบร้อย เริ่มต้นระบบทำงาน
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

  // 2. วิ่งตามเส้นแบบ PID ไปข้างหน้า สแกนสีแบบต่อเนื่องไหลลื่น ไร้การหยุดดีเลย์
  while (true) {
    if (isButtonPressed()) { stop_4w(); return; }

    long current_p = abs(get_pulse_Encoder(selection_Encoder) - baseline_pulse);
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

  enforceUnique6Colors(mission_summary);
  mission_summary.completed = true;
  drawColorSummaryScreen();
}

// ฟังก์ชันทางเลือกชื่อเดียวกัน
inline void scanColorsContinuous(int speed = 30, int max_slots = 6) {
  test_start_and_scan_colors(speed, max_slots);
}

// ============================================================================
// 3.1 ฟังก์ชันทดลองอ่านสีแบบตรวจจับแถบแทรกผ่านพื้นขาว (Equidistant Transition Scan)
// ============================================================================
// หุ่นวิ่งออกจากจุดสตาร์ท -> เจอเส้นดำแรก -> วิ่งตามเส้น 500-600 พัลส์ -> หยุด 1000ms
// -> ออกตัววิ่งตามเส้นพร้อมอ่านค่าสีรัวๆ:
//   - หากมีสีอื่น (แดง, เขียว, น้ำเงิน, เหลือง, ดำ) เข้ามาแทรกแทนที่พื้นขาว ให้นับเป็นสีนั้น
//   - เนื่องจากระยะห่างระหว่างแต่ละช่องสีเท่าๆ กัน (STRIP_STEP_PULSES)
//     หากตรงไหนวิ่งผ่านช่องสีแล้วสีหายไปนาน (ไม่พบสีใดๆ) แปลว่าช่องนั้นคือ "สีขาว"
//   - มีสีขาวได้เพียง 1 ช่องเท่านั้นในชุดคำสั่งซื้อ 6 สี
//   - ตรวจจับครบ 6 สีแล้วหยุดสนิททันที และคำนวณจับคู่ 1-to-1 ป้องกันสีซ้ำกัน 100%
inline void test_auto_scan_6colors(int speed = 30, long initial_skip_pulses = -1, long strip_step = -1, long first_strip = -1) {
  if (initial_skip_pulses < 0) initial_skip_pulses = INITIAL_SCAN_START_PULSES;
  if (strip_step < 0) strip_step = STRIP_STEP_PULSES;
  if (first_strip < 0) first_strip = FIRST_STRIP_PULSE;
  mission_summary.colorCount = 0;
  buzzer(1500, 60);

  // 1. วิ่งตรงออกจากจุดสตาร์ทจนกว่าเซนเซอร์หน้าจะแตะเจอเส้นดำเส้นแรกของสนาม
  while (countFrontSensorsOnLine() == 0) {
    if (isButtonPressed()) { stop_4w(); return; }
    fd_4w(speed);
    delay(1);
  }

  buzzer(1800, 80); // เสียงเตือน: แตะเจอเส้นดำเส้นแรกเรียบร้อย เริ่มต้นระบบทำงาน
  reset_PID_state();
  resetDistance();
  long baseline_pulse = get_pulse_Encoder(selection_Encoder);

  // 2. วิ่งตามเส้นจนกว่าจะพ้นพัลส์ตอนแรก (ออกจากโซนสตาร์ท เข้าใกล้โซนแถบสี)
  while (abs(get_pulse_Encoder(selection_Encoder) - baseline_pulse) < initial_skip_pulses) {
    if (isButtonPressed()) { stop_4w(); return; }
    lineFollow_PID_auto(speed);
  }

  // วิ่งต่อเนื่องเข้าสู่โซนแถบสีทันที (สแกนสีแบบต่อเนื่อง ไหลลื่น ไร้ดีเลย์)
  long scan_start_pulse = abs(get_pulse_Encoder(selection_Encoder) - baseline_pulse);
  long max_scan_pulse = scan_start_pulse + 2600; // ระยะสิ้นสุดของแผ่นเพลทคำสั่งซื้อ วิ่งพ้นแถบสีที่ 6 แน่นอน
  long white_gap_threshold = 400; // เกณฑ์ตัดสินช่องสีขาว (ปกติก้าวละ ~240p ถ้ามีขาวแทรกจะกว้าง ~480-500p)

  Serial.printf("\n=== AUTO SCAN STARTED (Start: %ld p, Step: %ld p, White Gap Threshold: %ld p) ===\n",
                scan_start_pulse, strip_step, white_gap_threshold);
  Serial.println("--- Active EEPROM Calibrated TCS Floor Values ---");
  const char* cnames[6] = {"RED", "GREEN", "BLUE", "YELLOW", "WHITE", "BLACK"};
  for (int c = 0; c < 6; c++) {
    Serial.printf("  [%-6s] R:%3d G:%3d B:%3d\n",
                  cnames[c],
                  calib.tcs_front_rgb[c][0],
                  calib.tcs_front_rgb[c][1],
                  calib.tcs_front_rgb[c][2]);
  }

  enum ScanState { STATE_ON_WHITE, STATE_IN_STRIP };
  ScanState state = STATE_ON_WHITE;

  // โครงสร้างบันทึกแถบสีที่ตรวจพบจริงบนแผ่นเพลท
  struct FoundStrip {
    RobotColor color;
    long pulse;
    int samples;
    int color_counts[7];
    long sum_r, sum_g, sum_b, sum_clear;
  };
  FoundStrip found_strips[8];
  int found_count = 0;

  int strip_counts[7] = {0};
  long strip_start_pulse = 0;
  long strip_sum_r = 0, strip_sum_g = 0, strip_sum_b = 0, strip_sum_clear = 0;
  long last_strip_pulse = scan_start_pulse;
  int white_consecutive = 0;
  unsigned long last_sample_time = 0;
  unsigned long last_log_time = 0;

  // ตรวจจับช่องที่ 1 แบบเรียลไทม์ขณะวิ่งผ่าน (หากเลยขอบช่อง 1 แล้วยังไม่เจอแม่สีใดๆ = ช่อง 1 คือสีขาว)
  bool slot1_checked = false;
  bool slot1_passed_as_white = false;
  long slot1_boundary = first_strip + (long)(strip_step * 0.45f);

  // 3. วิ่งตามเส้นและเก็บค่าสีรัวๆ จนตรวจพบแถบสีครบ หรือวิ่งจนพ้นแผ่นเพลท
  while (abs(get_pulse_Encoder(selection_Encoder) - baseline_pulse) < max_scan_pulse) {
    if (isButtonPressed()) { stop_4w(); return; }

    long current_p = abs(get_pulse_Encoder(selection_Encoder) - baseline_pulse);

    // ตรวจสอบช่องที่ 1 แบบเรียลไทม์ขณะวิ่งผ่านขอบเขตช่อง 1
    if (!slot1_checked && current_p >= slot1_boundary) {
      slot1_checked = true;
      if (found_count == 0 && state == STATE_ON_WHITE) {
        slot1_passed_as_white = true;
        Serial.printf(">> [REALTIME EVENT] Passed Slot 1 boundary (%ld p) with NO colored strip! Marked Slot 1 as WHITE!\n", current_p);
      }
    }

    // สุ่มอ่านค่าสีรัวๆ ทุก 2ms (ความเร็วสูงสุดเพื่อให้ได้ตัวอย่างสีหลายสิบรอบในแถบ 2cm)
    if (millis() - last_sample_time >= 2) {
      last_sample_time = millis();

      int r = Read_Color_TCS(0);
      int g = Read_Color_TCS(1);
      int b = Read_Color_TCS(2);
      int clear_val = Read_Color_TCS(4);
      RobotColor col = detectColorFloor();

      // แสดงผลสถานะเซนเซอร์สดๆ ทุก 150ms เพื่อให้ผู้ใช้งานเช็คค่าได้ทันที
      if (millis() - last_log_time >= 150) {
        last_log_time = millis();
        Serial.printf("[Pulse %4ld] RGB:(%3d,%3d,%3d) Clear:%4d -> %-6s (Strips: %d)\n",
                      current_p, r, g, b, clear_val, getColorName(col), found_count);
      }

      // ป้องกันการตรวจจับหลอกช่วงก่อนถึงพิกัดแถบสี
      if (current_p < scan_start_pulse + 30) {
        state = STATE_ON_WHITE;
      } else if (state == STATE_ON_WHITE) {
        // เมื่อเซนเซอร์สัมผัสโดนแม่สี (แดง เขียว น้ำเงิน เหลือง ดำ) ที่แทรกเข้ามาแทนที่พื้นขาว
        if (col != COLOR_WHITE && col != COLOR_UNKNOWN) {
          state = STATE_IN_STRIP;
          strip_start_pulse = current_p;
          for (int c = 1; c <= 6; c++) strip_counts[c] = 0;
          strip_counts[col]++;
          strip_sum_r = r;
          strip_sum_g = g;
          strip_sum_b = b;
          strip_sum_clear = clear_val;
          white_consecutive = 0;
          Serial.printf(">> [STRIP ENTER] at pulse %ld -> %s\n", current_p, getColorName(col));
        }
      } else if (state == STATE_IN_STRIP) {
        // กำลังอยู่ในแถบสี: สะสมตัวอย่างสี
        if (col >= 1 && col <= 6) {
          strip_counts[col]++;
        }
        strip_sum_r += r;
        strip_sum_g += g;
        strip_sum_b += b;
        strip_sum_clear += clear_val;

        if (col == COLOR_WHITE) {
          white_consecutive++;
          // พ้นแถบสีกลับสู่พื้นขาว (เจอขาวติดต่อกัน 4-5 รอบ = ~8-10ms ป้องกันแสงสะท้อนชั่วคราว)
          int req_white = (speed >= 45) ? 4 : 5;
          if (white_consecutive >= req_white) {
            long center_pulse = (strip_start_pulse + current_p) / 2;

            // ค้นหาแม่สีที่เด่นที่สุดในแถบนี้ (ไม่นับสีขาว)
            RobotColor dominant_col = COLOR_UNKNOWN;
            int max_sample = 0;
            for (int c = 1; c <= 6; c++) {
              if (c != COLOR_WHITE && strip_counts[c] > max_sample) {
                max_sample = strip_counts[c];
                dominant_col = (RobotColor)c;
              }
            }

            // กรอง Noise: แถบสีจริง (ความเร็วสูง >= 45 ใช้เกณฑ์ 2 ตัวอย่าง, ปกติ 3 ตัวอย่าง)
            int min_samples = (speed >= 45) ? 2 : 3;
            bool is_duplicate_nearby = (found_count > 0 && (center_pulse - found_strips[found_count - 1].pulse) < 100);

            if (dominant_col != COLOR_UNKNOWN && max_sample >= min_samples && !is_duplicate_nearby && found_count < 6) {
              found_strips[found_count].color = dominant_col;
              found_strips[found_count].pulse = center_pulse;
              found_strips[found_count].samples = max_sample;
              found_strips[found_count].sum_r = strip_sum_r;
              found_strips[found_count].sum_g = strip_sum_g;
              found_strips[found_count].sum_b = strip_sum_b;
              found_strips[found_count].sum_clear = strip_sum_clear;
              for (int c = 1; c <= 6; c++) {
                found_strips[found_count].color_counts[c] = strip_counts[c];
              }
              found_count++;

              Serial.printf(">> [STRIP CONFIRMED #%d] %s at pulse %ld (Samples: %d)\n",
                            found_count, getColorName(dominant_col), center_pulse, max_sample);

              last_strip_pulse = center_pulse;

              // หากพบครบ 5 แม่สีแล้ว (แดง เขียว น้ำเงิน เหลือง ดำ) แสดงว่าสีที่เหลือคือ สีขาว แน่นอน
              if (found_count == 5) {
                Serial.println(">> Found all 5 non-white colors! Completing mission...");
                break;
              }
            }

            state = STATE_ON_WHITE;
            white_consecutive = 0;
          }
        } else {
          white_consecutive = 0;
        }
      }
    }

    lineFollow_PID_auto(speed);
  }

  // หากถึงระยะสิ้นสุดขณะที่ยังคาอยู่ในแถบสี ให้บันทึกแถบสีนั้นด้วย
  if (state == STATE_IN_STRIP && found_count < 6) {
    long current_p = abs(get_pulse_Encoder(selection_Encoder) - baseline_pulse);
    long center_pulse = (strip_start_pulse + current_p) / 2;
    RobotColor dominant_col = COLOR_UNKNOWN;
    int max_sample = 0;
    for (int c = 1; c <= 6; c++) {
      if (c != COLOR_WHITE && strip_counts[c] > max_sample) {
        max_sample = strip_counts[c];
        dominant_col = (RobotColor)c;
      }
    }
    int min_samples = (speed >= 45) ? 2 : 3;
    bool is_duplicate_nearby = (found_count > 0 && (center_pulse - found_strips[found_count - 1].pulse) < 100);
    if (dominant_col != COLOR_UNKNOWN && max_sample >= min_samples && !is_duplicate_nearby && found_count < 6) {
      found_strips[found_count].color = dominant_col;
      found_strips[found_count].pulse = center_pulse;
      found_strips[found_count].samples = max_sample;
      found_strips[found_count].sum_r = strip_sum_r;
      found_strips[found_count].sum_g = strip_sum_g;
      found_strips[found_count].sum_b = strip_sum_b;
      found_strips[found_count].sum_clear = strip_sum_clear;
      for (int c = 1; c <= 6; c++) {
        found_strips[found_count].color_counts[c] = strip_counts[c];
      }
      found_count++;
      Serial.printf(">> [STRIP CONFIRMED AT SCAN END #%d] %s at pulse %ld (Samples: %d)\n",
                    found_count, getColorName(dominant_col), center_pulse, max_sample);
    }
  }

  // วิ่งสแกนเรียบร้อยแล้ว สั่งหยุดสนิททันที
  stop_4w();
  buzzer(1800, 200);
  Serial.printf("\n=== SCAN MOTOR STOPPED at pulse %ld (Total Colored Strips Found: %d) ===\n",
                abs(get_pulse_Encoder(selection_Encoder) - baseline_pulse), found_count);

  // 4. สรุปลำดับแถบสีทั้ง 6 ช่อง โดยจัดตำแหน่ง "สีขาว" ลงในช่องว่างกว้างที่สีหายไปนาน
  // ค้นหาตำแหน่งของช่องสีขาว:
  int white_slot_index = -1; // 0 ถึง 5
  long measured_step = strip_step;

  if (found_count >= 5) {
    measured_step = ((found_strips[found_count - 1].pulse - found_strips[0].pulse) / (found_count - 1));
    if (measured_step < 120 || measured_step > 400) measured_step = strip_step;

    long p0 = found_strips[0].pulse;
    long p_last = found_strips[found_count - 1].pulse;

    // ตรวจสอบระยะพัลส์ของแถบแรก p0:
    // หากช่อง 1 มีสี: p0 จะเริ่มที่ประมาณ first_strip (~1040p)
    // หากช่อง 1 เป็นสีขาว: แถบสีแรกที่เจอคือช่อง 2 (p0 จะกระโดดไปที่ ~1280p)
    long dist_to_slot1 = abs(p0 - first_strip);
    long dist_to_slot2 = abs(p0 - (first_strip + measured_step));

    // เช็คว่าแถบแรกเข้าข่ายเป็นช่อง 1 ชัดเจนหรือไม่
    bool slot1_is_colored = (dist_to_slot1 < dist_to_slot2) && (p0 < first_strip + (long)(measured_step * 0.60f));

    // ตรวจหาช่องว่างระหว่างแถบสี: เกณฑ์ต้องกว้าง >= 1.8 เท่าของก้าว และแถบแรกต้องไม่ใช่ช่อง 1 ที่ตรวจพบชัดเจน
    long gap_threshold = (long)(measured_step * 1.80f);
    long max_gap = 0;
    int max_gap_after_idx = -1;

    for (int i = 0; i < found_count - 1; i++) {
      long gap = found_strips[i + 1].pulse - found_strips[i].pulse;
      if (gap > max_gap) {
        max_gap = gap;
        max_gap_after_idx = i;
      }
    }

    if (max_gap >= gap_threshold && max_gap_after_idx >= 0 && !slot1_is_colored) {
      // มีช่องว่างกว้างมากผิดปกติระหว่างแถบ และไม่ใช่กรณีสีขาวอยู่ช่อง 1 หรือ 6
      white_slot_index = max_gap_after_idx + 1;
      Serial.printf(">> WHITE identified in Middle Slot #%d (Gap was %ld pulses, threshold: %ld)\n",
                    white_slot_index + 1, max_gap, gap_threshold);
    } else {
      // สีขาวอยู่ที่หัวแถว (ช่อง 1) หรือท้ายแถว (ช่อง 6)
      if (slot1_passed_as_white || !slot1_is_colored) {
        white_slot_index = 0; // สีขาวอยู่ที่ช่อง 1 (แถบสีแรกที่เจอคือช่อง 2)
        Serial.printf(">> WHITE identified in First Slot #1 (p0: %ld, first_strip: %ld, dist1: %ld, dist2: %ld, step: %ld)\n",
                      p0, first_strip, dist_to_slot1, dist_to_slot2, measured_step);
      } else {
        white_slot_index = 5; // สีขาวอยู่ที่ช่อง 6 (ท้ายสุด)
        Serial.printf(">> WHITE identified in Last Slot #6 (p0: %ld, first_strip: %ld, dist1: %ld, dist2: %ld, step: %ld)\n",
                      p0, first_strip, dist_to_slot1, dist_to_slot2, measured_step);
      }
    }
  } else {
    // กรณีเก็บแม่สีได้ 1-4 สี:
    float step_f = (found_count >= 2) ? ((float)(found_strips[found_count - 1].pulse - found_strips[0].pulse) / (float)(found_count - 1)) : (float)strip_step;
    if (step_f < 120.0f || step_f > 400.0f) step_f = (float)strip_step;
    long p0 = (found_count > 0) ? found_strips[0].pulse : 0;

    long dist_to_slot1 = abs(p0 - first_strip);
    long dist_to_slot2 = abs(p0 - (first_strip + (long)step_f));

    if (slot1_passed_as_white || (dist_to_slot2 < dist_to_slot1 && p0 >= first_strip + (long)(step_f * 0.55f))) {
      white_slot_index = 0; // สีขาวอยู่ที่ช่อง 1
    } else {
      white_slot_index = 5; // สีขาวอยู่ที่ช่อง 6
    }
  }

  // ==========================================================================
  // จัดสรรสีลงทั้ง 6 ช่อง โดยรับประกันว่า "ไม่มีสีซ้ำกัน 100%" (Strict 1-to-1 Bijection)
  // แม่สี 5 สีที่ไม่ใช่สีขาว: RED, GREEN, BLUE, YELLOW, BLACK
  // ==========================================================================
  const RobotColor non_white_colors[5] = {
    COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_BLACK
  };

  // กำหนด 5 ช่องที่ไม่ใช่สีขาว (Non-White Slots)
  int non_white_slots[5];
  int nw_idx = 0;
  for (int s = 0; s < 6; s++) {
    if (s != white_slot_index) {
      non_white_slots[nw_idx++] = s;
    }
  }

  // จับคู่แถบสี found_strips เข้ากับ 5 ช่องที่ไม่ใช่สีขาว (Index -1 แปลว่าช่องนั้นหลุดแถบสีไป)
  int strip_to_nw_slot[5];
  for (int i = 0; i < 5; i++) strip_to_nw_slot[i] = -1;

  if (found_count == 5) {
    // พบครบ 5 แถบสี: จับคู่แถบสี 0..4 เข้ากับ non_white_slots 0..4 แบบ 1-to-1 ตามลำดับ
    for (int i = 0; i < 5; i++) strip_to_nw_slot[i] = i;
  } else if (found_count < 5) {
    // พบ 1-4 แถบสี (วิ่งเร็วทำให้เซนเซอร์จับบางแถบไม่ทัน):
    // จับคู่แถบสีแต่ละแถบเข้ากับ non_white_slot ที่มีพิกัดพัลส์ใกล้เคียงที่สุด
    bool slot_taken[5] = {false};
    for (int fi = 0; fi < found_count; fi++) {
      long f_pulse = found_strips[fi].pulse;
      long best_diff = 999999L;
      int best_slot_idx = -1;
      for (int si = 0; si < 5; si++) {
        if (!slot_taken[si]) {
          int real_slot = non_white_slots[si];
          long est_pulse = (white_slot_index == 0)
                           ? (found_strips[0].pulse + (real_slot - 1) * measured_step)
                           : (found_strips[0].pulse + real_slot * measured_step);
          long diff = abs(f_pulse - est_pulse);
          if (diff < best_diff) {
            best_diff = diff;
            best_slot_idx = si;
          }
        }
      }
      if (best_slot_idx >= 0) {
        slot_taken[best_slot_idx] = true;
        strip_to_nw_slot[best_slot_idx] = fi;
      }
    }
  } else {
    // พบ >= 6 แถบสี: ใช้ 5 แถบแรก
    for (int i = 0; i < 5; i++) strip_to_nw_slot[i] = i;
  }

  // คำนวณ Permutation 5! = 120 แบบ เพื่อหาการจับคู่ที่ดีที่สุดในระดับโกลบอล
  // รับประกันว่าสี RED, GREEN, BLUE, YELLOW, BLACK จะถูกกระจายให้ 5 ช่องนี้ โดย "ไม่มีทางซ้ำกันแน่นอน"
  int perm[5] = {0, 1, 2, 3, 4};
  long best_total_score = -999999999L;
  int best_perm[5] = {0, 1, 2, 3, 4};

  do {
    long current_score = 0;
    for (int i = 0; i < 5; i++) {
      RobotColor candidate = non_white_colors[perm[i]];
      int fi = strip_to_nw_slot[i];

      if (fi >= 0 && fi < found_count) {
        // 1. คะแนนจากผลโหวตของเซนเซอร์ในแถบนี้
        current_score += (long)found_strips[fi].color_counts[candidate] * 150L;
        // 2. คะแนนโบนัสหากตรงกับสีเด่น
        if (found_strips[fi].color == candidate) {
          current_score += 6000L;
        }
        // 3. คะแนนระยะห่าง Euclidean RGB เทียบกับค่า EEPROM
        if (found_strips[fi].samples > 0) {
          long avg_r = found_strips[fi].sum_r / found_strips[fi].samples;
          long avg_g = found_strips[fi].sum_g / found_strips[fi].samples;
          long avg_b = found_strips[fi].sum_b / found_strips[fi].samples;
          long avg_c = found_strips[fi].sum_clear / found_strips[fi].samples;

          int c_idx = -1;
          if (candidate == COLOR_RED) c_idx = 0;
          else if (candidate == COLOR_GREEN) c_idx = 1;
          else if (candidate == COLOR_BLUE) c_idx = 2;
          else if (candidate == COLOR_YELLOW) c_idx = 3;
          else if (candidate == COLOR_BLACK) c_idx = 5;

          if (c_idx >= 0) {
            long dr = avg_r - calib.tcs_front_rgb[c_idx][0];
            long dg = avg_g - calib.tcs_front_rgb[c_idx][1];
            long db = avg_b - calib.tcs_front_rgb[c_idx][2];
            long dist = dr * dr + dg * dg + db * db;
            if (candidate == COLOR_BLACK && avg_c < 130) dist /= 2;
            current_score += max(0L, 20000L - dist / 5);
          }
        }
      }
    }
    if (current_score > best_total_score) {
      best_total_score = current_score;
      for (int i = 0; i < 5; i++) best_perm[i] = perm[i];
    }
  } while (std::next_permutation(perm, perm + 5));

  // 5. บันทึกผลลัพธ์ลงใน mission_summary ตามลำดับช่อง 1 ถึง 6 อย่างถูกต้องแม่นยำ
  mission_summary.colorCount = 0;

  for (int slot = 0; slot < 6; slot++) {
    if (slot == white_slot_index) {
      strncpy(mission_summary.records[slot].name, "WHITE", sizeof(mission_summary.records[slot].name));
      mission_summary.records[slot].color = COLOR_WHITE;
      if (slot > 0) {
        mission_summary.records[slot].pulse = mission_summary.records[slot - 1].pulse + measured_step;
      } else if (found_count > 0) {
        mission_summary.records[slot].pulse = found_strips[0].pulse - measured_step;
      } else {
        mission_summary.records[slot].pulse = first_strip;
      }
    } else {
      // ค้นหาตำแหน่งของ slot นี้ใน non_white_slots
      int nw_i = -1;
      for (int i = 0; i < 5; i++) {
        if (non_white_slots[i] == slot) { nw_i = i; break; }
      }
      RobotColor c = non_white_colors[best_perm[nw_i]];
      strncpy(mission_summary.records[slot].name, getColorName(c), sizeof(mission_summary.records[slot].name));
      mission_summary.records[slot].color = c;

      int fi = strip_to_nw_slot[nw_i];
      if (fi >= 0 && fi < found_count) {
        mission_summary.records[slot].pulse = found_strips[fi].pulse;
      } else {
        mission_summary.records[slot].pulse = (slot > 0) ? (mission_summary.records[slot - 1].pulse + measured_step) : 1000;
      }
    }

    mission_summary.records[slot].cm = encoderToCm(mission_summary.records[slot].pulse);
    mission_summary.colorCount++;

    Serial.printf("Final Slot #%d [Pulse %4ld]: %-6s\n",
                  slot + 1, mission_summary.records[slot].pulse, mission_summary.records[slot].name);
  }

  // ตรวจสอบและบังคับความถูกต้องรอบสุดท้าย (Double-check Zero Duplicates)
  enforceUnique6Colors(mission_summary);

  mission_summary.completed = true;
  drawColorSummaryScreen();
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
