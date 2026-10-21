# PuppyBot6 - โครงสร้างโปรเจกต์ PlatformIO

โปรเจกต์ควบคุมหุ่นยนต์ PuppyBot6 พัฒนาด้วย **PlatformIO** รองรับบอร์ดไมโครคอนโทรลเลอร์ **RP2040 / RP2350** (เช่น Adafruit Metro RP2350, Raspberry Pi Pico) พร้อมระบบ Dual-Core และไลบรารีทั้งหมดแบบ Standalone ภายในโปรเจกต์

---

## 📁 โครงสร้างโปรเจกต์

```text
puppybot6_Mukdahan/
├── platformio.ini         # การกำหนดค่าบอร์ด, คอมไพเลอร์ และพารามิเตอร์การอัปโหลด
├── src/                   # โค้ดต้นฉบับหลักของโปรเจกต์
│   └── main.cpp           # โปรแกรมหลัก (Core 0: setup/loop, Core 1: setup1/loop1)
├── lib/                   # รวมไลบรารีทุกตัวไว้ในนี้ (รวมถึง Servo, IMU, และ Turn PID)
│   ├── puppybot6/         # puppybot6.h, PuppyBotIMU.h, PuppyBotTurnPID.h, PicoEncoder
│   ├── Servo/             # ไลบรารีควบคุม Servo Motor (PIO-based)
│   ├── rp2040-encoder-library/ # ไดรเวอร์อ่าน Encoder มอเตอร์ความเร็วสูงด้วย PIO
│   ├── ICM-20948/         # ไดรเวอร์เซนเซอร์ IMU 9-DOF พร้อมระบบ DMP
│   ├── Adafruit_ST7735/   # ไดรเวอร์จอแสดงผลสี ST7735 SPI TFT
│   ├── Adafruit_GFX/      # แกนการวาดกราฟิกและฟอนต์
│   ├── Adafruit_BusIO/    # ตัวช่วยจัดการ I2C และ SPI บัส
│   ├── Adafruit_MCP3008/  # ไดรเวอร์ ADC 8 ช่อง สำหรับ Sensor Bar
│   └── Adafruit_TCS34725/ # ไดรเวอร์เซนเซอร์ตรวจจับสีแบบ I2C
├── docs/                  # เอกสารคู่มือและการใช้งาน
│   └── วิธีการใช้งานบอร์ด.docx
├── backup/                # สำรองไฟล์ต้นฉบับและไฟล์ ZIP
└── .gitignore             # รายการไฟล์ที่ไม่ต้องนำเข้า Version Control
```

---

## ⚙️ การคอมไพล์และอัปโหลดโค้ด

### 1. ใช้ปุ่มบนแถบสถานะ PlatformIO (VS Code / Antigravity IDE)
- **คอมไพล์ (Build)**: คลิกไอคอนเครื่องหมายถูก `✓` ด้านล่าง
- **อัปโหลด (Upload)**: เสียบสาย USB เข้าบอร์ดแล้วคลิกไอคอนลูกศร `→` ด้านล่าง
- **ดูข้อมูล (Serial Monitor)**: คลิกไอคอนปลั๊กสายไฟ `Serial Monitor` (Baudrate: `115200`)

### 2. ใช้คำสั่งผ่าน Terminal (Command Line)
- **คอมไพล์บอร์ดค่าเริ่มต้น (Adafruit Metro RP2350)**:
  ```powershell
  pio run
  ```
- **อัปโหลดลงบอร์ด**:
  ```powershell
  pio run -t upload
  ```
- **หากใช้งานบอร์ด Raspberry Pi Pico (Pico 1 / RP2040)**:
  ```powershell
  pio run -e pico
  pio run -e pico -t upload
  ```

---

## 🤖 ระบบการเคลื่อนที่และการนำทาง (Navigation & Movement System)

ไฟล์ระบบทั้งหมดอยู่ในโฟลเดอร์ `src/`:
- [config.h](file:///D:/puppybot6_Mukdahan/src/config.h) & [config.cpp](file:///D:/puppybot6_Mukdahan/src/config.cpp): ค่าตั้งต้น, Motor Trims, Battery Scale, Pin Mapping
- [motor_drive.h](file:///D:/puppybot6_Mukdahan/src/motor_drive.h): ระบบขับเคลื่อน 4 ล้อพร้อมกัน (M1=ซ้ายหน้า, M2=ขวาหน้า, M3=ซ้ายหลัง, M4=ขวาหลัง)
- [sensor_system.h](file:///D:/puppybot6_Mukdahan/src/sensor_system.h): เซนเซอร์หน้า 7 ตัว, หลัง 7 ตัว, IR Distance (ซม.), ปุ่มกด, Encoder (ซม.)
- [eeprom_calib.h](file:///D:/puppybot6_Mukdahan/src/eeprom_calib.h): ระบบบันทึก EEPROM และเมนู Interactive บนหน้าจอ LCD
- [line_follow.h](file:///D:/puppybot6_Mukdahan/src/line_follow.h): ฟังก์ชันการวิ่งตามเส้นและการหมุนเลี้ยวทั้งหมด

### 1. ฟังก์ชันการวิ่งตามเส้นและเคลื่อนที่ทั้งหมด
| ฟังก์ชัน | พารามิเตอร์ | คำอธิบาย |
|---|---|---|
| `Fline_by_pass(speed, pass_lines)` | ความเร็ว, จำนวนเส้นตัด | วิ่งตามเส้น PID ผ่านเส้นตัดครบตามจำนวนแล้วหยุด |
| `Fline_by_pass_B(speed, pass_lines)` | ความเร็ว, จำนวนเส้นตัด | วิ่งตามเส้นถอยหลัง PID ผ่านเส้นตัดครบตามจำนวนแล้วหยุด |
| `Fline(speed, sensor)` | ความเร็ว, ลำดับเซนเซอร์ (0-6 หรือ -1) | วิ่งตามเส้นจนกว่าเซนเซอร์ที่ระบุเจอเส้นแล้วหยุด |
| `Fline_B(speed, sensor)` | ความเร็ว, ลำดับเซนเซอร์ (0-6 หรือ -1) | วิ่งตามเส้นถอยหลังจนกว่าเซนเซอร์หลังที่ระบุเจอเส้นแล้วหยุด |
| `Fline_by_phase(speed, distance_cm)` | ความเร็ว, ระยะทาง (ซม.) | วิ่งตามเส้นจนกว่าระยะจะถึงตามที่ตั้ง (คำนวณจาก Encoder) |
| `Fline_B_by_phase(speed, distance_cm)` | ความเร็ว, ระยะทาง (ซม.) | วิ่งตามเส้นถอยหลังจนกว่าระยะจะถึงตามที่ตั้ง |
| `Fline_by_time(speed, time_ms)` | ความเร็ว, เวลา (ms) | วิ่งตามเส้น PID ตามเวลาที่กำหนด |
| `Fline_B_by_time(speed, time_ms)` | ความเร็ว, เวลา (ms) | วิ่งตามเส้นถอยหลัง PID ตามเวลาที่กำหนด |
| `Fline_by_Encoder(speed, pulses)` | ความเร็ว, จำนวน pulse | วิ่งตามเส้นตามระยะ pulse ของ Encoder |
| `Fline_B_by_Encoder(speed, pulses)` | ความเร็ว, จำนวน pulse | วิ่งตามเส้นถอยหลังตาม pulse ของ Encoder |
| `Follow_Notline(speed, time_ms)` | ความเร็ว, เวลา (ms) | วิ่งไปข้างหน้า 4 ล้อแบบไม่ใช้ PID ตามเวลา (ใช้ Trim ชดเชย) |
| `Backword_Notline(speed, time_ms)` | ความเร็ว, เวลา (ms) | วิ่งถอยหลัง 4 ล้อแบบไม่ใช้ PID ตามเวลา |
| `Follow_Notline_by_Encoder(speed, pulses)` | ความเร็ว, จำนวน pulse | วิ่งไปข้างหน้าแบบไม่ใช้ PID ตามระยะทาง Encoder |
| `Backword_Notline_by_Encoder(speed, pulses)` | ความเร็ว, จำนวน pulse | วิ่งถอยหลังแบบไม่ใช้ PID ตามระยะทาง Encoder |
| `turnLeft(speed)` | ความเร็ว | วิ่ง PID ตามเส้น พอเจอเส้นตัดขยับกึ่งกลางแล้วหมุนซ้าย (`sl`) จนเซนเซอร์หน้าเจอเส้นใหม่ |
| `turnRight(speed)` | ความเร็ว | วิ่ง PID ตามเส้น พอเจอเส้นตัดขยับกึ่งกลางแล้วหมุนขวา (`sr`) จนเซนเซอร์หน้าเจอเส้นใหม่ |
| `turnLeft_B(speed)` | ความเร็ว | วิ่งถอยหลัง PID พอเจอเส้นแล้วหมุนขวา (`sr`) สลับกันจนเซนเซอร์หลังเจอเส้นใหม่ |
| `turnRight_B(speed)` | ความเร็ว | วิ่งถอยหลัง PID พอเจอเส้นแล้วหมุนซ้าย (`sl`) สลับกันจนเซนเซอร์หลังเจอเส้นใหม่ |
| `turnLeft_by_stop(speed)` | ความเร็ว | หมุนซ้าย (`sl`) อยู่กับที่ทันทีจนกว่าเซนเซอร์หน้าจะเจอเส้น |
| `turnRight_by_stop(speed)` | ความเร็ว | หมุนขวา (`sr`) อยู่กับที่ทันทีจนกว่าเซนเซอร์หน้าจะเจอเส้น |
| `checkLine(speed)` | ความเร็ว (ค่าตั้งต้น 30) | อ่านค่าเซนเซอร์หน้าแล้วขยับซ้าย/ขวาเพื่อให้เซนเซอร์ตรงกลาง (ADC 3) ตรงเส้น |
| `checkLine_B(speed)` | ความเร็ว (ค่าตั้งต้น 30) | อ่านค่าเซนเซอร์หลังแล้วขยับเพื่อให้เซนเซอร์ตรงกลาง (ADC 10) ตรงเส้น |
| `lineFollow_PID_auto(speed)` | ความเร็ว | วิ่ง PID หน้า 4 ล้อ แยก KD อัตโนมัติตามช่วงความเร็ว (KP=0.4) |
| `lineFollow_PID_B_auto(speed)` | ความเร็ว | วิ่งถอยหลัง PID หลัง 4 ล้อ แยก KD อัตโนมัติตามช่วงความเร็ว |

---

### 2. ตัวแปรปรับแต่งมอเตอร์และแบตเตอรี่ (ใน `config.h` / `config.cpp`)
- **Motor Trims**:
  ```cpp
  motor_trim_M1 = 0;  // ซ้ายหน้า
  motor_trim_M2 = 0;  // ขวาหน้า
  motor_trim_M3 = 0;  // ซ้ายหลัง
  motor_trim_M4 = 0;  // ขวาหลัง
  ```
  ใส่ค่าบวกหรือลบเพื่อชดเชยมอเตอร์ให้หุ่นวิ่งตรงเป๊ะเมื่อใช้ `Follow_Notline`
- **Battery Compensation**:
  ```cpp
  speed_offset_pct = 0; // % ปรับความเร็วรวม เช่น +5 หรือ -10 (จำกัด 100% สูงสุด)
  ```
- **Button Mode Selection**:
  ```cpp
  button_mode = 1; // 1 = ใช้ปุ่มภายนอก ADC(15) < 500, 0 = ใช้ปุ่มบนบอร์ด Pin 3
  ```

---

### 3. ฟังก์ชันระยะทางเป็น "เซนติเมตร" (cm)
- `getDistanceCm()` / `get_distance_cm()`: Return ระยะทางที่หุ่นยนต์เคลื่อนที่ไปจาก Encoder เป็นเซนติเมตร
- `readDistanceIR()` / `get_ir_distance_cm()`: Return ระยะห่างจากเซนเซอร์อินฟราเรด GP2Y0A41SK0F (ADC 14) เป็นเซนติเมตร (แปลงความสัมพันธ์ทางแสง $1/d$ แม่นยำ)

---

### 4. เมนูคาลิเบรต EEPROM (EEPROM Calibration Menu)
- **วิธีเข้าเมนู**: กดปุ่ม (ภายนอก ADC 15 หรือบนบอร์ด) ค้างไว้ 1.5 วินาที ตอนเปิดเครื่องหรือขณะอยู่ในหน้าจอ loop()
- **การควบคุมในเมนู**:
  - **กดสั้น (< 0.7 วิ)**: เลื่อนไปยังเมนูถัดไป (1 -> 2 -> 3 -> 4 -> 5)
  - **กดยาว (>= 0.7 วิ)**: เลือกเมนูนั้น (Select)
- **รายการ 4 เมนูคาลิเบรต**:
  1. `1. FRONT SENSOR`: คาลิเบรตเซนเซอร์หน้า 7 ตัว (ADC 0 - 6) บนพื้นขาวและพื้นดำ
  2. `2. BACK SENSOR`: คาลิเบรตเซนเซอร์หลัง 7 ตัว (ADC 7 - 13)
  3. `3. TCS COLORS`: คาลิเบรตเซนเซอร์สีทั้ง 6 สี (RED, GREEN, BLUE, YELLOW, WHITE, BLACK) ทั้งหน้าและหลัง
  4. `4. IR DISTANCE`: คาลิเบรตระยะ 5 ซม. และ 10 ซม. พร้อมหน้าจอทดสอบระยะแบบ Real-time
  5. `5. SAVE & EXIT`: บันทึกข้อมูลทั้งหมดลง EEPROM ถาวรและออกจากเมนู

