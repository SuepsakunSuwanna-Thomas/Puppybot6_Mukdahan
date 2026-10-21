
#ifndef _PUPPYBOT_IMU_H
#define _PUPPYBOT_IMU_H
#include <ICM20948wire1.h>
#include <Wire.h>
#include "pio_encoder.h"
#ifndef I2C_ADDR
#define I2C_ADDR 0x69
#endif


namespace PuppyBotIMU {
    static PioEncoder encoder_PIO_M1(17);
    static PioEncoder encoder_PIO_M2(19);
    
    static ICM20948 icm;
    static float continuousYaw = 0;
    static float offset_yaw = 0;
    static float read_Yaw = 0;
    static float newYaw = 0;
    static float pitch = 0;
    static float roll = 0;
    static int state_begin_Encoder_M1 = 0;
    static int state_begin_Encoder_M2 = 0;
    static int Set_Mode_Gyro =0;

    void setModeGyro(int modeGyro){
        Set_Mode_Gyro = modeGyro;
    }
    void resetBoard() {
        Serial.println("Rebooting...");
    }
    void init_encoder(int position_motor){
        if(position_motor == 1){
            encoder_PIO_M1.begin();
        }
        else{
            encoder_PIO_M2.begin();
        }
        
    }
    void initIMU() {
        Wire1.setSDA(6);
        Wire1.setSCL(7);
        Wire1.begin();
        int code = icm.begin();
        icm.setFusionMode(FUSION_6_AXIS);
        Wire1.setClock(1000000);
        switch (code) {
            case ICM_SUCCESS:
                Serial.println("Init ok");
                break;
            case ICM_BAD_WHOAMI:
            case ICM_DMP_ERROR:
            case ICM_MAG_ERROR:
            case ICM_SERIAL_ERROR:
            case ICM_SETUP_ERROR:
                Serial.println("ICM-20948 Initialization Failed");
                resetBoard();
                break;
        }
        icm.startSensor(INV_SENSOR_TYPE_MAGNETOMETER, 16000);
        icm.startSensor(INV_SENSOR_TYPE_GAME_ROTATION_VECTOR, 16000);
        icm.startSensor(INV_SENSOR_TYPE_ROTATION_VECTOR, 16000);
        icm.setHighPowerMode(1);
        //
    }
    void updateIMU_fine_offset_Yaw(int round) {
      for (int Rread = 0; Rread < round; Rread++) {
        icm.update();
        if (icm.available()) {
          float qx = icm.x();
          float qy = icm.y();
          float qz = icm.z();
          float qw = icm.w();
          read_Yaw = atan2(2.0 * (qw * qx + qy * qz), 1.0 - 2.0 * (qx * qx + qy * qy)) * 180.0 / PI;
          if (read_Yaw < 0) { read_Yaw += 360; }
          icm.clearAvailable();
        }
        delay(10);
      }
      offset_yaw = read_Yaw;
    }

    void updateIMU() {
        icm.update();
        float qx = icm.x();
        float qy = icm.y();
        float qz = icm.z();
        float qw = icm.w();
        newYaw = atan2(2.0 * (qw * qx + qy * qz), 1.0 - 2.0 * (qx * qx + qy * qy)) * 180.0 / PI;
        float sinp = 2 * (qw * qy - qz * qx);
        pitch = (fabs(sinp) >= 1) ? copysign(M_PI / 2, sinp) : asin(sinp);
        pitch *= 180.0 / M_PI;

        float siny_cosp = 2 * (qw * qz + qx * qy);
        float cosy_cosp = 1 - 2 * (qy * qy + qz * qz);
        roll = atan2(siny_cosp, cosy_cosp) * 180.0 / M_PI;
    }

    float getYaw() {
        updateIMU();
        if (isnan(newYaw)) return read_Yaw;
        if (newYaw < 0) newYaw += 360;
        if (newYaw > 360) newYaw -= 360;
        read_Yaw = newYaw;
        return read_Yaw;
    }
    void updateContinuousYaw() {
    static float lastYaw = 0;
    static bool firstRun = true;

    updateIMU(); // อ่านค่าจาก IMU

    float currentYaw = getYaw();

    if (firstRun) {
      continuousYaw = currentYaw;
      lastYaw = currentYaw;
      firstRun = false;
      return;
    }
    float deltaYaw = currentYaw - lastYaw;
    if (deltaYaw > 180.0) deltaYaw -= 360.0;
    if (deltaYaw < -180.0) deltaYaw += 360.0;
    continuousYaw += deltaYaw;
    lastYaw = currentYaw;
  }
    int get_pulse_Encoder(int position_motor){
        int count_encoder = 0;
        if(position_motor == 1){
            if(state_begin_Encoder_M1 == 0){
            state_begin_Encoder_M1 = 1;
            encoder_PIO_M1.begin();
            }
            count_encoder =  encoder_PIO_M1.getCount();
        }
        else{
            if(state_begin_Encoder_M2 == 0){
            state_begin_Encoder_M2 = 1;
            encoder_PIO_M2.begin();
            }
            count_encoder =  encoder_PIO_M2.getCount();
        }
        return count_encoder;
    }
    void resetEncoder(int position_motor){
        if(position_motor == 1){
            encoder_PIO_M1.reset();
        }
        else{
            encoder_PIO_M2.reset();
        }
    }
    float getPitch() { updateIMU(); return pitch; }
    float getRoll() { updateIMU(); return roll; }
    float getOffsetYaw() { return offset_yaw; }
    float getContinuousYaw() { updateContinuousYaw(); return continuousYaw; }
    void resetContinuousYaw() { continuousYaw = getYaw(); }
}

using namespace PuppyBotIMU; 

#endif  


