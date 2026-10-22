#pragma once
#include <Servo.h>
// #include "TFT_eSPI.h" 
#include <Wire.h>

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
// #include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
// #include <SPI.h>

Adafruit_ST7735 tft_ = Adafruit_ST7735(13, 8, 11, 10, 9);
#include <Adafruit_MCP3008.h>

Adafruit_MCP3008 adc;


// #define ST7735_BLACK ST7735_BLACK
// #define ST7735_WHITE ST7735_WHITE
// #define ST7735_RED ST7735_RED
// #define ST7735_GREEN ST7735_GREEN
// #define ST7735_BLUE ST7735_BLUE
// #define ST7735_CYAN ST7735_CYAN
// #define ST7735_MAGENTA ST7735_MAGENTA
// #define ST7735_YELLOW ST7735_YELLOW
// #define ST7735_ORANGE ST7735_ORANGE
#include <Adafruit_TCS34725.h>
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_2_4MS, TCS34725_GAIN_4X);
Adafruit_TCS34725 tcsB = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_2_4MS, TCS34725_GAIN_4X);


Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;
#define _servo1 34
#define _servo2 35
#define _servo3 36
#define _servo4 37
#define _servo5 38
#define _servo6 39



#define motor1A  21
#define motor1B  22
#define motor2A  23
#define motor2B  24
#define motor3A  25
#define motor3B  26
#define motor4A  28
#define motor4B  29
#define motor5A  30
#define motor5B  31
#define motor6A  32
#define motor6B  33

int _sensorPins[20];
int _NumofSensor = 0;
int _min_sensor_values[20];
int _max_sensor_values[20];
int _lastPosition = 0;
int _Sensitive  = 50;
int stateOfRunPID = 0;
float  errors = 0, output = 0, integral = 0, derivative = 0, _previous_error = 0;
uint8_t FrontLineColor = 0;
uint8_t BackLineColor = 0;

float yaw_loop1 = 0;
float Con_yaw_loop1 = 0;
float offset_yaw_loop1 = 0;
uint8_t status_resetYaw = 0;

int TCS_R,TCS_G,TCS_B,TCS_T,TCS_L = 0;

// TCS3472 / TCS34725 I2C Addresses (Auto-detected)
uint8_t tcs_front_addr = 0x29;
uint8_t tcs_back_addr = 0x29;
bool tcs_front_found = false;
bool tcs_back_found = false;

// ฟังก์ชันสแกนหา I2C Address อัตโนมัติสำหรับ TCS3472 / TCS34725
// ฟังก์ชันสแกนหาและเชื่อมต่อ TCS3472 / TCS34725 แบบปลอดภัย (เฉพาะแอดเดรสที่ถูกต้อง)
inline uint8_t init_TCS_sensor(Adafruit_TCS34725 &sensor, TwoWire &wireBus, bool &foundFlag) {
  // แอดเดรสมาตรฐานทั้งหมดของตระกูล TCS3472 / TCS34725
  const uint8_t validAddrs[] = {0x29, 0x39, 0x49, 0x10};
  for (uint8_t addr : validAddrs) {
    if (sensor.begin(addr, &wireBus)) {
      foundFlag = true;
      return addr;
    }
  }
  foundFlag = false;
  return 0x29; // default fallback
}

void puppybot6_setup() {
  adc.begin(10, 11, 12, 14);
  analogWriteResolution(10);
  analogWriteRange(1023);
  tft_.initR(INITR_BLACKTAB);
  // tft_.init();
  tft_.setRotation(3); // กลับหน้าจอ 180 องศาเป็นฝั่งตรงข้าม
  tft_.fillScreen(ST7735_BLACK);
  pinMode(motor1A,OUTPUT);
  pinMode(motor2A,OUTPUT);
  pinMode(motor3A,OUTPUT);
  pinMode(motor4A,OUTPUT);
  pinMode(motor5A,OUTPUT);
  pinMode(motor6A,OUTPUT);

  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();
  delay(15); // รอเซนเซอร์ TCS บูตระบบ

  // ตรวจจับและเชื่อมต่อเซนเซอร์สี TCS3472 ด้านหน้า (SDA=4, SCL=5)
  tcs_front_addr = init_TCS_sensor(tcs, Wire, tcs_front_found);
  if (tcs_front_found) {
    Serial.printf("[TCS Front] Found & initialized TCS3472 at I2C: 0x%02X (SDA=4, SCL=5)\n", tcs_front_addr);
  } else {
    Serial.printf("[TCS Front] Warning: TCS3472 not detected on Wire (SDA=4, SCL=5)\n");
  }
}

void set_TCS_back(){
  Wire1.setSDA(6);
  Wire1.setSCL(7);
  Wire1.begin();
  delay(15);

  // ตรวจสอบเซนเซอร์สีด้านหลัง (SDA=6, SCL=7)
  tcs_back_addr = init_TCS_sensor(tcsB, Wire1, tcs_back_found);
  if (tcs_back_found) {
    Serial.printf("[TCS Back] Found & initialized TCS3472 at I2C: 0x%02X (SDA=6, SCL=7)\n", tcs_back_addr);
  } else {
    Serial.println("[TCS Back] No TCS detected on Wire1 - Back color reader safely disabled");
  }
}

bool isConnected(uint8_t addr = 0) {
  if (!tcs_front_found) return false;
  if (addr == 0) addr = tcs_front_addr;
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

bool isConnected_B(uint8_t addr = 0) {
  if (!tcs_back_found) return false;
  if (addr == 0) addr = tcs_back_addr;
  Wire1.beginTransmission(addr);
  return (Wire1.endTransmission() == 0);
}

// แคชผลการอ่านค่าสี เพื่อให้ R, G, B และ Lux เป็นข้อมูลเฟรมเดียวกันเป๊ะ และไม่สแปมบัส I2C
static unsigned long _tcs_f_last_read = 0;
static long _tcs_f_cached_r = 0;
static long _tcs_f_cached_g = 0;
static long _tcs_f_cached_b = 0;
static long _tcs_f_cached_temp = 0;
static long _tcs_f_cached_lux = 0;

long Read_Color_TCS(int color_of_sensor)
{
  if (!tcs_front_found) return 0;

  unsigned long now = millis();
  // อัปเดตเฟรมใหม่เมื่อผ่านไปอย่างน้อย 15ms หรือการอ่านครั้งแรก
  if (now - _tcs_f_last_read >= 15 || _tcs_f_last_read == 0) {
    if (isConnected(tcs_front_addr)) {
      uint16_t clearcol_lib = 0, red_lib = 0, green_lib = 0, blue_lib = 0, lux = 0, colorTemp = 0;
      tcs.getRawData(&red_lib, &green_lib, &blue_lib, &clearcol_lib);
      colorTemp = tcs.calculateColorTemperature(red_lib, green_lib, blue_lib);
      lux = tcs.calculateLux(red_lib, green_lib, blue_lib);

      float average_lib = ((float)red_lib + (float)green_lib + (float)blue_lib) / 3.0f;
      if (average_lib < 1.0f) average_lib = 1.0f; // ป้องกันการหารด้วยศูนย์

      _tcs_f_cached_r = (long)(((float)red_lib / average_lib) * 100.0f);
      _tcs_f_cached_g = (long)(((float)green_lib / average_lib) * 100.0f);
      _tcs_f_cached_b = (long)(((float)blue_lib / average_lib) * 100.0f);
      _tcs_f_cached_temp = colorTemp;
      _tcs_f_cached_lux = (long)lux;
      _tcs_f_last_read = now;
    }
  }

  if (color_of_sensor == 0) return _tcs_f_cached_r;
  if (color_of_sensor == 1) return _tcs_f_cached_g;
  if (color_of_sensor == 2) return _tcs_f_cached_b;
  if (color_of_sensor == 3) return _tcs_f_cached_temp;
  if (color_of_sensor == 4) return _tcs_f_cached_lux;

  return 0;
}

int Read_Color_TCS_B(int color_of_sensor)
{
  long data_color = 0.00;
	 if(color_of_sensor == 0){
	  data_color =  TCS_R;
	 }
	 else if(color_of_sensor == 1){
	  data_color =  TCS_G;
	 }
	  else if(color_of_sensor == 2){
	  data_color =  TCS_B;
	 }
	 else if(color_of_sensor == 3){
	  data_color =  TCS_T;
	 }
	 else if(color_of_sensor == 4){
	  data_color =  TCS_L;
	 }

  return data_color;
}

int Read_Color_TCS_B_Raw(int color_of_sensor)
{
  // หากไม่มีเซนเซอร์หลัง ให้คืนค่า 0 ทันที เพื่อไม่ให้ Core 1 แย่งบัส I2C กับ Core 0
  if (!tcs_back_found) return 0;

  uint16_t clearcol_lib, red_lib, green_lib, blue_lib,lux,colorTemp;
  float average_lib, r_lib, g_lib, b_lib;
  long data_color = 0.00;
  if(isConnected_B(tcs_back_addr)){
	 tcsB.getRawData(&red_lib, &green_lib, &blue_lib, &clearcol_lib);
	 colorTemp = tcsB.calculateColorTemperature(red_lib, green_lib, blue_lib);
	 lux = tcsB.calculateLux(red_lib, green_lib, blue_lib);

	 average_lib = ((float)red_lib + (float)green_lib + (float)blue_lib) / 3.0f;
   if (average_lib < 1.0f) average_lib = 1.0f;
	 r_lib = red_lib/average_lib;
	 g_lib = green_lib/average_lib;
	 b_lib = blue_lib/average_lib;
	 if(color_of_sensor == 0){
	  data_color =  r_lib*100;
	 }
	 else if(color_of_sensor == 1){
	  data_color =  g_lib*100;
	 }
	  else if(color_of_sensor == 2){
	  data_color =  b_lib*100;
	 }
	 else if(color_of_sensor == 3){
	  data_color =  colorTemp;
	 }
	 else if(color_of_sensor == 4){
	  data_color =  lux;
	 }
  }

  return data_color;
}


int ADC(int analog_CH) {
   int val = 0;
  if (analog_CH < 8 ) {
    val = adc.readADC(analog_CH);
  }
  else if (analog_CH >= 8 ) {
    val = analogRead(32 +analog_CH);
  }
  
  return val;
}

int IN(int _pins) {
  if (_pins == 1) {_pins = 25;}
  else if(_pins >=2 && _pins <=4){_pins = 25 +_pins;}
  else{return 0;}
  pinMode(_pins, INPUT); 
  return digitalRead(_pins);
}
void OUT(int _pins,uint8_t _Status){
  if (_pins == 1) {_pins = 25;}
  else if(_pins >=2 && _pins <=4){_pins = 25 +_pins;}
  pinMode(_pins, OUTPUT); 
  digitalWrite(_pins,_Status);
}
void buzzer(int freq, int timr_delay) {
  pinMode(2, OUTPUT);
  tone(2, freq);
  delay(timr_delay);
  tone(2, 0);
}
void drawString(String text,uint8_t x,uint8_t y){
	tft_.setCursor(x, y);
  tft_.setTextWrap(true);
  tft_.println(text);
}
void printText(uint8_t x,uint8_t y,String text,uint8_t size,uint16_t  color){
	tft_.setCursor(x, y);
	tft_.setTextSize(size);
  tft_.setTextColor(color);
  tft_.setTextWrap(false);
  tft_.print(text);
}
void printText(uint8_t x,uint8_t y,String text,uint8_t size,uint16_t  color1,uint16_t  color2){
  tft_.setCursor(x, y);
  tft_.setTextSize(size);
  tft_.setTextColor(color1,color2);
  tft_.setTextWrap(false);
  tft_.print(text);
}
void printNumber(uint8_t x,uint8_t y,long text,uint8_t size,uint16_t  color1,uint16_t  color2){
  tft_.setCursor(x, y);
  tft_.setTextSize(size);
  tft_.setTextColor(color1,color2);
  tft_.setTextWrap(false);
  String text_ ;
  if(text <9){
    text_ = String(text) + " "+ " "+ " "; 
  }
  else if(text <99){
    text_ =  String(text) + " "+ " "; 
  }
  else if(text <999){
    text_ =  String(text) + " "; 
  }
  else if(text <9999){
    text_ =  String(text) + " "; 
  }
  else{
   text_ =  String(text) ; 
  }

  //text_ =  String(text) ;
  tft_.println(text_);
}
void wait_SW1() {
  int state_waitSW1 = 0;
  pinMode(3, INPUT_PULLUP);
  tft_.setTextSize(2);
  tft_.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft_.fillScreen(ST7735_BLACK);
  do {
    
    tft_.setTextColor(ST7735_WHITE, ST7735_BLACK);
    drawString("0="+String(ADC(0))+"  ",0,0);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("1="+String(ADC(1))+"  ",80,0);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("2="+String(ADC(2))+"  ",0,17);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("3="+String(ADC(3))+"  ",80,17);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("4="+String(ADC(4))+"  ",0,34);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("5="+String(ADC(5))+"  ",80,34);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("6="+String(ADC(6))+"  ",0,51);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("7="+String(ADC(7))+"  ",80,51);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("8="+String(ADC(8))+"  ",0,68);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("9="+String(ADC(9))+"  ",80,68);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("15="+String(ADC(15))+"  ",0,85);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    drawString("G="+String(yaw_loop1)+"  ",80,85);if((digitalRead(3) == 0)){pinMode(3, OUTPUT);break;}
    if(state_waitSW1 == 0){
      state_waitSW1 = 1;
      tft_.setTextColor(ST7735_RED, ST7735_BLUE);
      drawString("  SW1 Press  ",0,105);
    }
    else
    {
      state_waitSW1 = 0;
      tft_.setTextColor(ST7735_GREEN, ST7735_YELLOW);
      drawString("  SW1 Press  ",0,105);
    }
  	
  	delay(50);
  } while (digitalRead(3) == 1);
  pinMode(3, OUTPUT);
  buzzer(500,100);
  buzzer(2000,100);
  tft_.fillScreen(ST7735_BLACK);
  delay(500);
  
  
}

void motor(int pin, int speed_Motor) {
  if (speed_Motor > 100)speed_Motor = 100;
  if (speed_Motor < -100)speed_Motor = -100;
  if (pin == 1) {
    if (speed_Motor < 0) {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor1A,0);
      analogWrite(motor1B,abs(speed_Motor));

    }
    else {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor1A,1);
      analogWrite(motor1B,abs(speed_Motor));
    }
  }
  else if (pin == 2) {
    if (speed_Motor < 0) {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor2A,0);
      analogWrite(motor2B, abs(speed_Motor));
    }
    else {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor2A,1);
      analogWrite(motor2B, abs(speed_Motor));
    }
  }
  else if (pin == 3) {
    if (speed_Motor < 0) {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor3A,0);
      analogWrite(motor3B,abs(speed_Motor));
    }
    else {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor3A,1);
      analogWrite(motor3B, abs(speed_Motor));
    }
  }
  else if (pin == 4) {
    if (speed_Motor < 0) {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor4A,0);
      analogWrite(motor4B,abs(speed_Motor));

    }
    else {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor4A,1);
      analogWrite(motor4B,abs(speed_Motor));
    }
  }
  else if (pin == 5) {
    if (speed_Motor < 0) {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor5A,0);
      analogWrite(motor5B,abs(speed_Motor));

    }
    else {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor5A,1);
      analogWrite(motor5B,abs(speed_Motor));
    }
  }
  else if (pin == 6) {
    if (speed_Motor < 0) {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor6A,0);
      analogWrite(motor6B,abs(speed_Motor));

    }
    else {
      speed_Motor = abs(speed_Motor) * 10.23;
      digitalWrite(motor6A,1);
      analogWrite(motor6B,abs(speed_Motor));
    }
  }
}
void motor_control(uint8_t state , int _speed) {
  switch (state) {
    case 0: {
        motor(1, _speed);
        motor(2, _speed);
      } break;
    case 1: {
        motor(1, -_speed);
        motor(2, -_speed);
      } break;
    case 2: {
        motor(1, _speed);
        motor(2, 0);
      } break;
    case 3: {
        motor(1, 0);
        motor(2, _speed);
      } break;
    case 4: {
        motor(1, -_speed);
        motor(2, _speed);
      } break;
    case 5: {
        motor(1, _speed);
        motor(2, -_speed);
      } break;
    case 6: {
        motor(1, _speed);
      } break;
    case 7: {
        motor(2, _speed);
      } break;
    case 8: {
        motor(1, -_speed);
      } break;
    case 9: {
        motor(2, -_speed);
      } break;
  }
}
void ao(){
	analogWrite(motor1B, 0);
    analogWrite(motor2B, 0);
    analogWrite(motor3B, 0);
    analogWrite(motor4B, 0);
    analogWrite(motor5B, 0);
    analogWrite(motor6B, 0);
}
void aoS(int speed_break){
  speed_break = constrain(speed_break, 0, 100);
  speed_break = speed_break * 10.23;
  analogWrite(motor1B, speed_break);
  analogWrite(motor1A, speed_break);
  analogWrite(motor2B, speed_break);
  analogWrite(motor2A, speed_break);
}
void motorStop(int motor_ch){
  	if(motor_ch == 0){
	  analogWrite(motor1B, 0);
	  analogWrite(motor1A, 0);
	  analogWrite(motor2B, 0);
	  analogWrite(motor2A, 0);
	  analogWrite(motor3B, 0);
	  analogWrite(motor3A, 0);
	  analogWrite(motor4B, 0);
	  analogWrite(motor4A, 0);
	}
	else if(motor_ch == 1 ){
	  analogWrite(motor1B, 0);
	  analogWrite(motor1A, 0);
	}
	else if(motor_ch == 2 ){
	  analogWrite(motor2B, 0);
	  analogWrite(motor2A, 0);
	}
	else if(motor_ch == 3 ){
	  analogWrite(motor3B, 0);
	  analogWrite(motor3A, 0);
	}
	else if(motor_ch == 4 ){
	  analogWrite(motor4B, 0);
	  analogWrite(motor4A, 0);
	}
	else{
	  analogWrite(motor1B, 0);
    analogWrite(motor2B, 0);
    analogWrite(motor3B, 0);
    analogWrite(motor4B, 0);
    analogWrite(motor5B, 0);
    analogWrite(motor6B, 0);
	}
}
void motorBreak(){
    analogWrite(motor1B, 0);
    analogWrite(motor2B, 0);
    analogWrite(motor3B, 0);
    analogWrite(motor4B, 0);
    analogWrite(motor5B, 0);
    analogWrite(motor6B, 0);
}
void motorBreak(int motor_ch){
  
	if(motor_ch == 0){
	  analogWrite(motor1B, 0);
	  analogWrite(motor2B, 0);
	  analogWrite(motor3B, 0);
	  analogWrite(motor4B, 0);
    analogWrite(motor5B, 0);
    analogWrite(motor6B, 0);

	}
	else if(motor_ch == 1 ){
	  analogWrite(motor1B, 1023);
	  analogWrite(motor1A, 1023);
	}
	else if(motor_ch == 2 ){
	  analogWrite(motor2B, 1023);
	  analogWrite(motor2A, 1023);
	}
	else if(motor_ch == 3 ){
	  analogWrite(motor3B, 1023);
	  analogWrite(motor3A, 1023);
	}
	else if(motor_ch == 4 ){
	  analogWrite(motor4B, 1023);
	  analogWrite(motor4A, 1023);
	}
	else{
	  analogWrite(motor1B, 1023);
	  analogWrite(motor1A, 1023);
	  analogWrite(motor2B, 1023);
	  analogWrite(motor2A, 1023);
	  analogWrite(motor3B, 1023);
	  analogWrite(motor3A, 1023);
	  analogWrite(motor4B, 1023);
	  analogWrite(motor4A, 1023);
	}
}
void fd(int speed_Motor){
	motor(1,speed_Motor);
	motor(2,speed_Motor);
	motor(3,speed_Motor);
	motor(4,speed_Motor);
}
void fd2(int speed_MotorA,int speed_MotorB){
  motor(1,speed_MotorA);
  motor(2,speed_MotorB);
}
void bk(int speed_Motor){
	motor(1,-speed_Motor);
	motor(2,-speed_Motor);
	motor(3,-speed_Motor);
	motor(4,-speed_Motor);
}
void bk2(int speed_MotorA,int speed_MotorB){
  motor(1,-speed_MotorA);
  motor(2,-speed_MotorB);
}
void tl(int speed_Motor){
	motor(1,0);
	motor(2,speed_Motor);
}
void tr(int speed_Motor){
	motor(1,speed_Motor);
	motor(2,0);
}
void sl(int speed_Motor){
	motor(1,-speed_Motor);
	motor(2,speed_Motor);
}
void sr(int speed_Motor){
	motor(1,speed_Motor);
	motor(2,-speed_Motor);
}

void servoRun(uint8_t servo_ch, int16_t angle) {

  if (servo_ch == 1)
  {
    if(angle == -1){servo1.detach();}
    servo1.attach(_servo1,300,2500,angle);
  }
  if (servo_ch == 2)
  {
    if(angle == -1){servo2.detach();}
    servo2.attach(_servo2,300,2500,angle);
  }
  if (servo_ch == 3)
  {
    if(angle == -1){servo3.detach();}
    servo3.attach(_servo3,300,2500,angle);
  }
  if (servo_ch == 4)
  {
    extern int button_mode;
    if (button_mode == 3) {
      servo4.detach();
      return; // ป้องกันขา 37 ชนกับปุ่มกดภายนอกใน button_mode = 3
    }
    if(angle == -1){servo4.detach();}
    servo4.attach(_servo4,300,2500,angle);
  }
}
int ultrasonic(uint8_t Echo_pin , uint8_t Trig_pin) {
  int ECHO = Echo_pin;
  int TRIG = Trig_pin;
  pinMode(ECHO, INPUT);
  pinMode(TRIG, OUTPUT);
  long duration = 0;
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  duration += pulseIn(ECHO, HIGH);

  // Calculating the distance
  return (duration) * 0.034 / 2;
}



//*********************************************************  TC01Sensor **********************************************************************************************
//*********************************************************  TC01Sensor **********************************************************************************************
//*********************************************************  TC01Sensor **********************************************************************************************
//*********************************************************  TC01Sensor **********************************************************************************************

void setSensorPins(const int * _pins, int _NumofSensor_)
{
  _NumofSensor = _NumofSensor_;
  // _sensorPins = (int *)realloc(_sensorPins, sizeof(int) * _NumofSensor_);
  // _min_sensor_values = (int *)realloc(_min_sensor_values, sizeof(int) * _NumofSensor_);
  // _max_sensor_values = (int *)realloc(_max_sensor_values, sizeof(int) * _NumofSensor_);
  for (uint8_t i = 0; i < _NumofSensor_; i++)
  {
    _sensorPins[i] = _pins[i];
    _min_sensor_values[i] = 1023;
    _max_sensor_values[i] = 0;
  }

}
void setSensorMin(const int * _MinSensor)
{
  for (uint8_t i = 0; i < _NumofSensor; i++)
  {
    _min_sensor_values[i] = _MinSensor[i];
  }
}
void setSensorMax(const int * _MaxSensor)
{
  for (uint8_t i = 0; i < _NumofSensor; i++)
  {
    _max_sensor_values[i] = _MaxSensor[i];
  }
}
void setSensitive(const uint16_t  _SensorSensitive)
{
  _Sensitive = _SensorSensitive;
}
void setFrontLineColor(const uint16_t  _setFrontLineColor)     // if Value = 1 is BlackLine ,value = 0 is WhiteLine
{
  FrontLineColor = _setFrontLineColor;
}
int refSensor(int ch){
  return ( _max_sensor_values[ch] + _min_sensor_values[ch] ) / 2 ;
}

int readSensorMinValue(uint8_t _Pin) {
  return _min_sensor_values[_Pin];
}
int readSensorMaxValue(uint8_t _Pin) {
  return _max_sensor_values[_Pin];
}
int ReadLightSensor(int analog_CH) {
  int value = 0;

  if(FrontLineColor == 0)value= map(ADC(_sensorPins[analog_CH]), _min_sensor_values[analog_CH], _max_sensor_values[analog_CH], 100, 0);
  else if (FrontLineColor == 1) value= map(ADC(_sensorPins[analog_CH]), _min_sensor_values[analog_CH], _max_sensor_values[analog_CH], 0, 100);
  if(value < 0)value = 0;
  else if(value >100)value = 100;
  return value;
}
// void showGraph(){
//   int state_waitSW1 = 0;
//   pinMode(6, INPUT_PULLUP);
//   tft_.fillScreen(ST7735_BLACK);

//   do {
//     //tft_.fillRect(20,0,140,128,ST7735_BLACK);
//     for(int i = 0;i<_NumofSensor;i++){
//       tft_.setTextColor(ST7735_WHITE, ST7735_BLACK);
//       tft_.setTextSize(1);
//       drawString("A"+String(i)+"=",0,10*i);
//       tft_.fillRect(20,10*i,100,5,ST7735_BLACK);
//       tft_.fillRect(20,10*i,ReadLightSensor(i),5,ST7735_ORANGE);
//       drawString(String(ReadLightSensor(i))+"  ",130,10*i);
//     }
//     tft_.setTextSize(2);

//     static unsigned long lastTimeUpdateBackground = 0;
//     static bool flagBackground = false;

//     if(millis()-lastTimeUpdateBackground >= 100){
//       lastTimeUpdateBackground = millis();
//       flagBackground =! flagBackground;
//       tft_.setTextColor(flagBackground?ST7735_RED:ST7735_GREEN, flagBackground?ST7735_BLUE:ST7735_YELLOW);
//       drawString("  SW1 Press  ",0,115);
//     }
//     delay(50);
//   } while (digitalRead(6) == 1);
//   tft_.fillScreen(ST7735_BLACK);
//   buzzer(500,100);
//  }
// void setCalibrate(int cal_round) {
//   tft_.fillScreen(ST7735_BLACK);
//   if(_NumofSensor <= 0){
//     printText(0,20,"  No Sensors ",2,ST7735_WHITE);
//     printText(0,50,"   Defined   ",2,ST7735_WHITE);
//     while(1){

//     }
//     //printText(0,70,"No Sensors Defined",2,ST7735_WHITE);
//   }
//   printText(0,10,"  Calibrate  ",2,ST7735_WHITE);
//   printText(0,50,"   Sensor  ",2,ST7735_WHITE);

//   tft_.setTextColor(ST7735_WHITE, ST7735_BLACK);
//   tft_.setTextSize(2);
      
//   for (int round_count = 0; round_count < cal_round; round_count ++ ) {
//     drawString("Count="+String(cal_round-round_count)+"   ",0,90);

//     for (uint8_t i = 0; i < _NumofSensor; i++)
//     {
//       if (ADC(_sensorPins[i]) > _max_sensor_values[i] || _max_sensor_values[i] > 1023 ) {
//         _max_sensor_values[i]  = ADC(_sensorPins[i]);
//         if (_max_sensor_values[i] > 1023 )_max_sensor_values[i] = 1023;
//       }
//     }
//     for (uint8_t i = 0; i < _NumofSensor; i++)
//     {
//       if (ADC(_sensorPins[i]) < _min_sensor_values[i] || _min_sensor_values[i] == 0) {
//         _min_sensor_values[i] = ADC(_sensorPins[i]);
//         if (_min_sensor_values[i] < 0) _min_sensor_values[i] = 0;
//       }
//     }
//   }
//   for (uint8_t i = 0; i < _NumofSensor; i++)
//   {
//     _max_sensor_values[i] =  _max_sensor_values[i];
//     _min_sensor_values[i] = _min_sensor_values[i];
//   }
//   tft_.fillScreen(ST7735_BLACK);
//   tft_.setTextSize(1);
//   tft_.setTextColor(ST7735_WHITE);
//   if(_NumofSensor >0){tft_.setCursor(0, 0);tft_.print("A0 >> Min="+String(readSensorMinValue(0))+"  Max="+String(readSensorMaxValue(0)));}
//   if(_NumofSensor >1){tft_.setCursor(0, 10);tft_.print("A1 >> Min="+String(readSensorMinValue(1))+"  Max="+String(readSensorMaxValue(1)));}
//   if(_NumofSensor >2){tft_.setCursor(0, 20);tft_.print("A2 >> Min="+String(readSensorMinValue(2))+"  Max="+String(readSensorMaxValue(2)));}
//   if(_NumofSensor >3){tft_.setCursor(0, 30);tft_.print("A3 >> Min="+String(readSensorMinValue(3))+"  Max="+String(readSensorMaxValue(3)));}
//   if(_NumofSensor >4){tft_.setCursor(0, 40);tft_.print("A4 >> Min="+String(readSensorMinValue(4))+"  Max="+String(readSensorMaxValue(4)));}
//   if(_NumofSensor >5){tft_.setCursor(0, 50);tft_.print("A5 >> Min="+String(readSensorMinValue(5))+"  Max="+String(readSensorMaxValue(5)));}
//   if(_NumofSensor >6){tft_.setCursor(0, 60);tft_.print("A6 >> Min="+String(readSensorMinValue(6))+"  Max="+String(readSensorMaxValue(6)));}
//   if(_NumofSensor >7){tft_.setCursor(0, 70);tft_.print("A7 >> Min="+String(readSensorMinValue(7))+"  Max="+String(readSensorMaxValue(7)));}
//   if(_NumofSensor >8){tft_.setCursor(0, 80);tft_.print("A8 >> Min="+String(readSensorMinValue(8))+"  Max="+String(readSensorMaxValue(8)));}
//   if(_NumofSensor >9){tft_.setCursor(0, 90);tft_.print("A9 >> Min="+String(readSensorMinValue(9))+"  Max="+String(readSensorMaxValue(9)));}
//   if(_NumofSensor >10){tft_.setCursor(0, 100);tft_.print("A10 >> Min="+String(readSensorMinValue(10))+"  Max="+String(readSensorMaxValue(10)));}

// }


int readline()
{
  bool onLine = false;
  long avg = 1;
  long sum = 1;
  for (uint8_t i = 0; i < _NumofSensor; i++)
  {
    long value = ReadLightSensor(i);
    // long value =  0 ;
    // if( FrontLineColor == 0)value = map(ADC(_sensorPins[i]), _min_sensor_values[i], _max_sensor_values[i], 1000, 0);
    // else value = map(ADC(_sensorPins[i]), _min_sensor_values[i], _max_sensor_values[i], 0, 1000);
    // if(value < 0)value = 0;
    if (value > _Sensitive) {
      onLine = true;
    }
    if (value > 5)
    {
      avg += (long)value * (i * 100);
      sum += value;
    }
  }
  if (!onLine)
  {
    if (_lastPosition < (_NumofSensor - 1) * 100 / 2)
    {
      return 10;
    }
    else
    {
      return ((_NumofSensor - 1) * 100);
    }
  }
  _lastPosition = avg / sum;
  return _lastPosition;
}

void lineFollow_PID(int RUN_PID_speed , float RUN_PID_KP, float RUN_PID_KI, float RUN_PID_KD) {

  int speed_PID = RUN_PID_speed;
  int present_position = readline();
  int setpoint = ((_NumofSensor - 1) * 100) / 2;
  static int _previous_error = 0; 
  errors = present_position - setpoint;
  derivative = (errors - _previous_error) ;
  output = RUN_PID_KP * errors  + RUN_PID_KD * derivative;
  //int max_output = RUN_PID_speed;
  // if (output > max_output)output = max_output;
  // else if (output < -max_output)output = -max_output;

  int motorL = constrain(RUN_PID_speed + output, -RUN_PID_speed, RUN_PID_speed);
  int motorR = constrain(RUN_PID_speed - output, -RUN_PID_speed, RUN_PID_speed);
  // if(m1Speed < 0 )m1Speed = 0;
  // if(m2Speed < 0 )m2Speed = 0;

  motor(1,motorL);
  motor(2,motorR);
  _previous_error = errors;
  delay(1);

}
void run_PID(int RUN_PID_speed , int RUN_PID_Mspeed, float RUN_PID_KP, float RUN_PID_KD) {
  int speed_PID = RUN_PID_speed;
  int present_position = readline();
  int setpoint = ((_NumofSensor - 1) * 100) / 2;
  errors = present_position - setpoint;
  static int _previous_error = 0;
  derivative = (errors - _previous_error) ;
  output = RUN_PID_KP * errors  + RUN_PID_KD * derivative;
    _previous_error = errors;
  int max_output = RUN_PID_Mspeed;
  if (output > max_output)output = max_output;
  else if (output < -max_output)output = -max_output;
  int m1Speed = speed_PID + output ;
  int m2Speed = speed_PID - output;
  m1Speed = constrain(m1Speed, 0, RUN_PID_speed);
    m2Speed = constrain(m2Speed, 0, RUN_PID_speed);

  motor(1,m1Speed);
  motor(2,m2Speed);
  delay(1);
    _previous_error = errors;

}
//*********************************************************  TC01Sensor **********************************************************************************************
//*********************************************************  TC01Sensor **********************************************************************************************
//*********************************************************  TC01Sensor **********************************************************************************************

int _sensorPins_B[20];
int _NumofSensor_B = 0;
int _min_sensor_values_B[20];
int _max_sensor_values_B[20];
int _lastPosition_B = 0;
int _Sensitive_B  = 20;
int stateOfRunPID_B = 0;
float  errors_B = 0, output_B = 0, integral_B = 0, derivative_B = 0, _previous_error_B = 0;
void setSensorPins_B(const int * _pins, int _NumofSensor_)
{
  _NumofSensor_B = _NumofSensor_;
  // _sensorPins = (int *)realloc(_sensorPins, sizeof(int) * _NumofSensor_);
  // _min_sensor_values = (int *)realloc(_min_sensor_values, sizeof(int) * _NumofSensor_);
  // _max_sensor_values = (int *)realloc(_max_sensor_values, sizeof(int) * _NumofSensor_);
  for (uint8_t i = 0; i < _NumofSensor_B; i++)
  {
    _sensorPins_B[i] = _pins[i];
    _min_sensor_values_B[i] = 1023;
    _max_sensor_values_B[i] = 0;
  }

}
void setSensorMin_B(const int * _MinSensor)
{
  for (uint8_t i = 0; i < _NumofSensor_B; i++)
  {
    _min_sensor_values_B[i] = _MinSensor[i];
  }
}
void setSensorMax_B(const int * _MaxSensor)
{
  for (uint8_t i = 0; i < _NumofSensor_B; i++)
  {
    _max_sensor_values_B[i] = _MaxSensor[i];
  }
}
void setBackLineColor(const uint16_t  setBackLineColor)     // if Value = 1 is BlackLine ,value = 0 is WhiteLine
{
  BackLineColor = setBackLineColor;
}
int refSensor_B(int ch){
  return ( _max_sensor_values_B[ch] + _min_sensor_values_B[ch] ) / 2 ;
}
int ReadLightSensor_B(int analog_CH) {
  int value = 0;

  if(BackLineColor == 0)value= map(ADC(_sensorPins_B[analog_CH]), _min_sensor_values_B[analog_CH], _max_sensor_values_B[analog_CH], 100, 0);
  else if (BackLineColor == 1) value= map(ADC(_sensorPins_B[analog_CH]), _min_sensor_values_B[analog_CH], _max_sensor_values_B[analog_CH], 0, 100);
  if(value < 0)value = 0;
  else if(value >100)value = 100;
  return value;
}
int readline_B()
{
  bool onLine = false;
  long avg = 0;
  long sum = 0;
  for (uint8_t i = 0; i < _NumofSensor_B; i++)
  {
    long value =  ReadLightSensor_B(i);
    // if( BlackLineColor == 0)value = map(ADC(_sensorPins_B[i]), _min_sensor_values_B[i], _max_sensor_values_B[i], 100, 0);
    // else value = map(ADC(_sensorPins_B[i]), _min_sensor_values_B[i], _max_sensor_values_B[i], 0, 100);
    if (value > _Sensitive_B) {
      onLine = true;
    }
    if (value > 5)
    {
      avg += (long)value * (i * 100);
      sum += value;
    }
  }
  if (!onLine)
  {
    if (_lastPosition_B < (_NumofSensor_B - 1) * 100 / 2)
    {
      return 0;
    }
    else
    {
      return (_NumofSensor_B - 1) * 100;
    }
  }
  _lastPosition_B = avg / sum;
  return _lastPosition_B;
}
void run_PID_B(int RUN_PID_speed , int RUN_PID_Mspeed, float RUN_PID_KP, float RUN_PID_KD) {
  int speed_PID = RUN_PID_speed;
  int present_position = readline_B();
  int setpoint = ((_NumofSensor_B - 1) * 100) / 2;
  errors = present_position - setpoint;
  static int _previous_error_B = 0;
  derivative = (errors - _previous_error_B) ;
  output = RUN_PID_KP * errors  + RUN_PID_KD * derivative;
  int max_output = RUN_PID_Mspeed;
  if (output > max_output)output = max_output;
  else if (output < -max_output)output = -max_output;
  int m1Speed = speed_PID - output ;
  int m2Speed = speed_PID + output;

  m1Speed = constrain(m1Speed, 0, RUN_PID_speed);
    m2Speed = constrain(m2Speed, 0, RUN_PID_speed);

  motor(1,-m1Speed);
  motor(2,-m2Speed);
  delay(1);
    _previous_error_B = errors;

}
void setCalibrate_B(int cal_round) {
  tft_.fillScreen(ST7735_BLACK);
  if(_NumofSensor_B <= 0){
    printText(0,20,"  No Sensors ",2,ST7735_WHITE);
    printText(0,50,"   Defined   ",2,ST7735_WHITE);
    while(1){

    }
    //printText(0,70,"No Sensors Defined",2,ST7735_WHITE);
  }
  printText(0,10," Calibrate_B ",2,ST7735_WHITE);
  printText(0,50,"   Sensor  ",2,ST7735_WHITE);

  tft_.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft_.setTextSize(2);
      
  for (int round_count = 0; round_count < cal_round; round_count ++ ) {
    drawString("Count="+String(cal_round-round_count)+"   ",0,90);

    for (uint8_t i = 0; i < _NumofSensor_B; i++)
    {
      if (ADC(_sensorPins_B[i]) > _max_sensor_values_B[i] || _max_sensor_values_B[i] > 1023 ) {
        _max_sensor_values_B[i]  = ADC(_sensorPins_B[i]);
        if (_max_sensor_values_B[i] > 1023 )_max_sensor_values_B[i] = 1023;
      }
    }
    for (uint8_t i = 0; i < _NumofSensor_B; i++)
    {
      if (ADC(_sensorPins_B[i]) < _min_sensor_values_B[i] || _min_sensor_values_B[i] == 0) {
        _min_sensor_values_B[i] = ADC(_sensorPins_B[i]);
        if (_min_sensor_values_B[i] < 0) _min_sensor_values_B[i] = 0;
      }
    }
  }
  for (uint8_t i = 0; i < _NumofSensor_B; i++)
  {
    _max_sensor_values_B[i] =  _max_sensor_values_B[i];
    _min_sensor_values_B[i] = _min_sensor_values_B[i];
  }
  tft_.fillScreen(ST7735_BLACK);
  tft_.setTextSize(1);
  tft_.setTextColor(ST7735_WHITE);
  if(_NumofSensor_B >0){tft_.setCursor(0, 0);tft_.print("A0 >> Min="+String(readSensorMinValue(0))+"  Max="+String(readSensorMaxValue(0)));}
  if(_NumofSensor_B >1){tft_.setCursor(0, 10);tft_.print("A1 >> Min="+String(readSensorMinValue(1))+"  Max="+String(readSensorMaxValue(1)));}
  if(_NumofSensor_B >2){tft_.setCursor(0, 20);tft_.print("A2 >> Min="+String(readSensorMinValue(2))+"  Max="+String(readSensorMaxValue(2)));}
  if(_NumofSensor_B >3){tft_.setCursor(0, 30);tft_.print("A3 >> Min="+String(readSensorMinValue(3))+"  Max="+String(readSensorMaxValue(3)));}
  if(_NumofSensor_B >4){tft_.setCursor(0, 40);tft_.print("A4 >> Min="+String(readSensorMinValue(4))+"  Max="+String(readSensorMaxValue(4)));}
  if(_NumofSensor_B >5){tft_.setCursor(0, 50);tft_.print("A5 >> Min="+String(readSensorMinValue(5))+"  Max="+String(readSensorMaxValue(5)));}
  if(_NumofSensor_B >6){tft_.setCursor(0, 60);tft_.print("A6 >> Min="+String(readSensorMinValue(6))+"  Max="+String(readSensorMaxValue(6)));}
  if(_NumofSensor_B >7){tft_.setCursor(0, 70);tft_.print("A7 >> Min="+String(readSensorMinValue(7))+"  Max="+String(readSensorMaxValue(7)));}
  if(_NumofSensor_B >8){tft_.setCursor(0, 80);tft_.print("A8 >> Min="+String(readSensorMinValue(8))+"  Max="+String(readSensorMaxValue(8)));}
  if(_NumofSensor_B >9){tft_.setCursor(0, 90);tft_.print("A9 >> Min="+String(readSensorMinValue(9))+"  Max="+String(readSensorMaxValue(9)));}
  if(_NumofSensor_B >10){tft_.setCursor(0, 100);tft_.print("A10 >> Min="+String(readSensorMinValue(10))+"  Max="+String(readSensorMaxValue(10)));}
}







bool Read_status_sensor(int pin_sensor){
	return ADC(_sensorPins[pin_sensor]) < ((_max_sensor_values[pin_sensor] + _min_sensor_values[pin_sensor]) / 2) ? true : false;
}
bool Read_status_sensor_B(int pin_sensor){
	return ADC(_sensorPins_B[pin_sensor]) < ((_max_sensor_values_B[pin_sensor] + _min_sensor_values_B[pin_sensor]) / 2) ? true : false;
}

int Read_sumValue_sensor(){
	int value = 0;
	for(int i = 0;i<_NumofSensor;i++){
		value +=ReadLightSensor(i);
	}
    return value;
}

int Read_sumValue_sensor_B(){
  int value = 0;
  for(int i = 0;i<_NumofSensor_B;i++){
    value += ReadLightSensor_B(i);
  }
    return value;
}


int ReadSensorMinValue_B(uint8_t _Pin){
    return _min_sensor_values_B[_Pin];
}
int ReadSensorMaxValue_B(uint8_t _Pin){
    return _max_sensor_values_B[_Pin];
}

void Run_PID_B_until_backSensor(int RUN_PID_speed,float RUN_PID_KP,float RUN_PID_KD,int sumValue_traget){
  do{
    int present_position_B = readline_B();
    int setpoint_B = ((_NumofSensor_B - 1) * 100) / 2;
    errors_B = present_position_B - setpoint_B;
    static int _previous_error_B = 0;
    derivative_B = (errors_B - _previous_error_B) ;
    output_B = RUN_PID_KP * errors_B  + RUN_PID_KD * derivative_B;
    
    int m1Speed = RUN_PID_speed - output_B ;
    int m2Speed = RUN_PID_speed + output_B;

    m1Speed = constrain(m1Speed, -RUN_PID_speed, RUN_PID_speed);
    m2Speed = constrain(m2Speed, -RUN_PID_speed, RUN_PID_speed);

    motor(1,-m1Speed);
    motor(2,-m2Speed);
    delay(1);
    _previous_error_B = errors_B;
  }while(Read_sumValue_sensor_B() < sumValue_traget);
}
void Run_PID_B4DW_until_backSensor(int RUN_PID_speed,float RUN_PID_KP,float RUN_PID_KD ,int sumValue_traget){
  do{
    int present_position_B = readline_B();
    int setpoint_B = ((_NumofSensor_B - 1) * 100) / 2;
    errors_B = present_position_B - setpoint_B;
    static int _previous_error_B = 0;
    derivative_B = (errors_B - _previous_error_B) ;
    output_B = RUN_PID_KP * errors_B  + RUN_PID_KD * derivative_B;
    
    int m1Speed = RUN_PID_speed - output_B ;
    int m2Speed = RUN_PID_speed + output_B;

    m1Speed = constrain(m1Speed, -RUN_PID_speed, RUN_PID_speed);
    m2Speed = constrain(m2Speed, -RUN_PID_speed, RUN_PID_speed);

    motor(1,-m1Speed);
    motor(2,-m2Speed);
    motor(3,-m1Speed);
    motor(4,-m2Speed);
    delay(1);
    _previous_error_B = errors_B;
  }while(Read_sumValue_sensor_B() < sumValue_traget);
}

void Run_PID_until_frontSensor(int RUN_PID_speed,float RUN_PID_KP,float RUN_PID_KD,int sumValue_traget){
  do{
    int present_position = readline();
    int setpoint = ((_NumofSensor - 1) * 100) / 2;
    errors = present_position - setpoint;
    static int _previous_error = 0;
    derivative = (errors - _previous_error) ;
    output = RUN_PID_KP * errors  + RUN_PID_KD * derivative;
    
    int m1Speed = RUN_PID_speed + output ;
    int m2Speed = RUN_PID_speed - output;

    m1Speed = constrain(m1Speed, -RUN_PID_speed, RUN_PID_speed);
    m2Speed = constrain(m2Speed, -RUN_PID_speed, RUN_PID_speed);

    motor(1,m1Speed);
    motor(2,m2Speed);
    delay(1);
    _previous_error = errors;
  }while(Read_sumValue_sensor() < sumValue_traget);
}
void Run_PID4DW_until_frontSensor(int RUN_PID_speed,float RUN_PID_KP,float RUN_PID_KD ,int sumValue_traget){
  do{
    int present_position = readline();
    int setpoint = ((_NumofSensor - 1) * 100) / 2;
    errors = present_position - setpoint;
    static int _previous_error = 0;
    derivative = (errors - _previous_error) ;
    output = RUN_PID_KP * errors  + RUN_PID_KD * derivative;
    

    int m1Speed = RUN_PID_speed + output ;
    int m2Speed = RUN_PID_speed - output;

    m1Speed = constrain(m1Speed, -RUN_PID_speed, RUN_PID_speed);
    m2Speed = constrain(m2Speed, -RUN_PID_speed, RUN_PID_speed);

    motor(1,m1Speed);
    motor(2,m2Speed);
    motor(3,m1Speed);
    motor(4,m2Speed);
    delay(1);
    _previous_error = errors;
  }while(Read_sumValue_sensor() < sumValue_traget);
}
