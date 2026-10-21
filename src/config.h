#ifndef PUPPYBOT6_CONFIG_H
#define PUPPYBOT6_CONFIG_H

#include <Arduino.h>

// ============================================================================
// 1. Motor Trims & Battery Speed Compensation (ปรับแต่งมอเตอร์)
// ============================================================================
// มอเตอร์ 4 ตัว:
// M1 = ซ้ายหน้า (Left Front)
// M2 = ขวาหน้า (Right Front)
// M3 = ซ้ายหลัง (Left Back)
// M4 = ขวาหลัง (Right Back)
//
// สามารถใส่ค่าลบหรือบวกเพื่อชดเชยให้วิ่งตรงเมื่อวิ่งแบบไม่มีเส้น (Follow_Notline)
extern int motor_trim_M1; // ค่า trim มอเตอร์ 1 (ซ้ายหน้า)
extern int motor_trim_M2; // ค่า trim มอเตอร์ 2 (ขวาหน้า)
extern int motor_trim_M3; // ค่า trim มอเตอร์ 3 (ซ้ายหลัง)
extern int motor_trim_M4; // ค่า trim มอเตอร์ 4 (ขวาหลัง)

// speed_offset_pct: เพิ่มหรือลดความเร็วรวมเป็นเปอร์เซ็นต์ (เช่น +5 หรือ -10)
// เพื่อชดเชยเวลาเปลี่ยนแบตเตอรี่แล้วแรงดันเปลี่ยน
// กรณีวิ่ง 100 แล้วบวกอีก 5 มอเตอร์จะถูกจำกัดที่ 100% สูงสุด
extern int speed_offset_pct;

// ค่า KP และ KD สำหรับการวิ่งตามเส้น (สามารถปรับแต่งได้ที่นี่ ไม่ต้องไล่แก้ในโค้ด)
extern float line_KP;   // ค่า KP วิ่งตามเส้นเดินหน้า (ค่าตั้งต้น 0.4)
extern float line_KP_B; // ค่า KP วิ่งตามเส้นถอยหลัง (ค่าตั้งต้น 0.4)
extern float line_KD;   // ค่า KD วิ่งตามเส้นเดินหน้า (0.0f = auto ตามความเร็ว หรือตั้งค่าเอง เช่น 7.5)
extern float line_KD_B; // ค่า KD วิ่งตามเส้นถอยหลัง (0.0f = auto ตามความเร็ว หรือตั้งค่าเอง เช่น 7.5)

// ============================================================================
// 2. Button Selection (สวิตช์ / ปุ่มกด)
// ============================================================================
// button_mode:
//   0 = ใช้ปุ่มบนบอร์ด (Pin 3)
//   1 = ใช้ปุ่มภายนอก ADC(15) < 500
extern int button_mode;

#define EXTERNAL_BUTTON_PIN             15
#define EXTERNAL_BUTTON_PRESS_THRESHOLD 500  // ต่ำกว่า 500 คือกดปุ่ม
#define ONBOARD_BUTTON_PIN              3

// ============================================================================
// 3. Sensor Layout (เซนเซอร์หน้า และ เซนเซอร์หลัง)
// ============================================================================
// ข้างหน้ามี 7 ตัว: ADC(0) ถึง ADC(6) -> center = ADC(3)
#define NUM_FRONT_SENSORS 7
#define FRONT_CENTER_INDEX 3
const int FRONT_PINS[NUM_FRONT_SENSORS] = {0, 1, 2, 3, 4, 5, 6};

// ข้างหลังมี 7 ตัว: ADC(7) ถึง ADC(13) -> center = ADC(10) (index 3 ใน array)
#define NUM_BACK_SENSORS 7
#define BACK_CENTER_INDEX 3
const int BACK_PINS[NUM_BACK_SENSORS] = {7, 8, 9, 10, 11, 12, 13};

// เซนเซอร์วัดระยะห่าง Infrared GP2Y0A41SK0F (4-30cm) ต่อที่ ADC(14)
#define IR_DISTANCE_PIN 14

// ============================================================================
// 4. Encoder & Distance Conversion (การแปลงระยะ Encoder เป็น เซนติเมตร)
// ============================================================================
// จำนวน pulse ต่อ 1 ซม. (สามารถปรับจูนตามขนาดล้อและอัตราทดเกียร์)
extern float ENCODER_PULSES_PER_CM;

// ============================================================================
// 5. EEPROM Data Structures (โครงสร้างข้อมูลบันทึกใน EEPROM)
// ============================================================================
#define EEPROM_ADDR_BASE 0
#define EEPROM_MAGIC     0x50555036 // "PUP6"

// รายชื่อสีสำหรับการคาลิเบรต TCS34725
enum RobotColor {
  COLOR_UNKNOWN = 0,
  COLOR_RED     = 1,
  COLOR_GREEN   = 2,
  COLOR_BLUE    = 3,
  COLOR_YELLOW  = 4,
  COLOR_WHITE   = 5,
  COLOR_BLACK   = 6
};

#define NUM_CALIB_COLORS 6 // RED, GREEN, BLUE, YELLOW, WHITE, BLACK

struct RobotCalibration {
  uint32_t magic;
  
  // เซนเซอร์หน้า 7 ตัว (ADC 0-6)
  int front_min[NUM_FRONT_SENSORS];
  int front_max[NUM_FRONT_SENSORS];
  
  // เซนเซอร์หลัง 7 ตัว (ADC 7-13)
  int back_min[NUM_BACK_SENSORS];
  int back_max[NUM_BACK_SENSORS];
  
  // IR Distance GP2Y0A41SK0F (ADC 14)
  int ir_raw_5cm;   // ค่า ADC ที่ระยะ 5 ซม.
  int ir_raw_10cm;  // ค่า ADC ที่ระยะ 10 ซม.
  
  // TCS Color Reference (R, G, B ratios 0-100)
  // [0]=RED, [1]=GREEN, [2]=BLUE, [3]=YELLOW, [4]=WHITE, [5]=BLACK
  int tcs_front_rgb[NUM_CALIB_COLORS][3];
  int tcs_back_rgb[NUM_CALIB_COLORS][3];
};

extern RobotCalibration calib;

#endif // PUPPYBOT6_CONFIG_H
