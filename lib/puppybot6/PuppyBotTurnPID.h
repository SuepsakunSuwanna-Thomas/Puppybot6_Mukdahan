#include "PuppyBotIMU.h"
enum Direction_turnPID { NORTH = 0,
                         EAST = 1,
                         SOUTH = 2,
                         WEST = 3 };

int Direction_turnPIDIndex = NORTH;
float previousYaw_turnPID = 0;

float preverror_turnPID = 0;
float integral_turn = 0;
float turnDirection_PID_KP = 2, turnDirection_PID_KD = 0;
int speedMin_turnDirection = 15, speedMax_turnDirection = 60;
int error_for_turnPID = 1;
int state_begin_Encoder = 0;



void set_data_for_turnDirection(int error_PID, int speedMin, int speedMax, float KP, float KD) {
  turnDirection_PID_KP = KP;
  turnDirection_PID_KD = KD;
  speedMin_turnDirection = speedMin;
  speedMax_turnDirection = speedMax;
  error_for_turnPID = error_PID;
}

float shortestAngle(float currentAngle, float targetAngle) {
  float error = fmod((targetAngle - currentAngle + 540), 360) - 180;
  return error;
}

void turnPID(int condition, float targetYaw_turnPID, int speedTurn_min, int speedTurn_max, float kp_turnPID, float kd_turnPID) {
  unsigned long startTime = millis();
  integral_turn = 0;
  int stableCount = 0;
  int overshootCount = 0;
  bool hasOvershoot = false;

  float preverror_turnPID = shortestAngle(Con_yaw_loop1, targetYaw_turnPID);
  unsigned long now2 = millis();
  while (true) {
    float current_Yaw = Con_yaw_loop1;
    float error_turnPID = shortestAngle(current_Yaw, targetYaw_turnPID);
    unsigned long now = millis();
    float deltaTime = max((now - startTime) / 1000.0, 0.001);
    startTime = now;
    integral_turn += error_turnPID * deltaTime;
    integral_turn = constrain(integral_turn, -50, 50);
    float derivative = (error_turnPID - preverror_turnPID) / deltaTime;
    float output = kp_turnPID * error_turnPID + kd_turnPID * derivative;
    int speed_L = constrain(abs(output), speedTurn_min, speedTurn_max);
    int speed_R = constrain(abs(output), speedTurn_min, speedTurn_max);
    if (output < 0) {
      if (condition == 1) {
        motor(1, 0);
        motor(2, speed_R);
        if (Set_Mode_Gyro == 0) {
          motor(3, 0);
          motor(4, speed_R);
        }
      } else if (condition == 0) {
        motor(1, -speed_L);
        motor(2, speed_R);
        if (Set_Mode_Gyro == 0) {
          motor(3, -speed_L);
          motor(4, speed_R);
        }
      }
    } else {
      if (condition == 1) {
        motor(1, speed_L);
        motor(2, 0);
        if (Set_Mode_Gyro == 0) {
          motor(3, speed_L);motor(4, 0);
        }
      } else if (condition == 0) {
        motor(1, speed_L);motor(2, -speed_R);
        if (Set_Mode_Gyro == 0) {
          motor(3, speed_L);motor(4, -speed_R);
        }
      }
    }
    if (abs(error_turnPID) < error_for_turnPID) {
      stableCount++;
      if (stableCount > 5) {
        ao();
        break;
      }
    } else {
      stableCount = 0;
    }

    if (millis() - now2 > 2500) {
      Serial.println("Timeout! Exiting loop...");
      ao();
      break;
    }
    delay(1);
    preverror_turnPID = error_turnPID;
  }
}


void turnByAngle(int turnAngle,int  condition) {
  float currentYaw = Con_yaw_loop1;
  float targetYaw = currentYaw + turnAngle;
  targetYaw = fmod(targetYaw, 360);
  turnPID(condition,targetYaw, speedMin_turnDirection, speedMax_turnDirection, turnDirection_PID_KP, turnDirection_PID_KD);
  status_resetYaw = 1;
}


float normalizeAngle(float angle) {
  while (angle > 180) angle -= 360;
  while (angle <= -180) angle += 360;
  return angle;
}

void moveStraightPID(int Movedirection, float targetYaw_straight, int speedBase, float duration, float kp_straight, float kd_straight,int Slow) {
    unsigned long startTime = millis();
    unsigned long startTime_reduc = millis();
    unsigned long endTime = startTime + duration; 
    float integral_straight = 0, preverror_straight = 0;

    while (millis() < endTime) {   
        float current_Yaw = Con_yaw_loop1;
        float error_straight = normalizeAngle(targetYaw_straight - current_Yaw);
        unsigned long now = millis();
        float deltaTime = max((now - startTime) / 1000.0, 0.001);
        startTime = now;
        integral_straight += error_straight * deltaTime;
        integral_straight = constrain(integral_straight, -50, 50);  
        float derivative = (error_straight - preverror_straight) / deltaTime;
        float output = kp_straight * error_straight + kd_straight * derivative;
        preverror_straight = error_straight;
        int rightSpeed;
        int leftSpeed;

        if(Slow == 1){
          unsigned long elapsedTime = now - startTime_reduc;
          long timeError = endTime - now;
          float speedReduction;
          if (timeError <= 500 && timeError > 0) {
            speedReduction = timeError * 0.01;
            speedReduction = constrain(speedReduction, 0, speedBase);
          } else if (timeError <= 0) {
            speedReduction = 0;
          } else {
            speedReduction = speedBase;
          }
          rightSpeed = constrain(speedReduction - output, 10, 100);
          leftSpeed = constrain(speedReduction + output, 10, 100);
        }
        else{
          rightSpeed = constrain(speedBase - output, 10, 100);
          leftSpeed = constrain(speedBase + output, 10, 100);
        }
        if (Movedirection == 0) {  
            motor(1, leftSpeed);
            motor(2, rightSpeed);
            if(Set_Mode_Gyro == 0){
              motor(3, leftSpeed);
              motor(4, rightSpeed);
            }
            
        } else {
            motor(1, -rightSpeed);
            motor(2, -leftSpeed);
            if(Set_Mode_Gyro == 0){
              motor(3, -rightSpeed);
              motor(4, -leftSpeed);
            } 
        }
    }
    ao(); 
}


void MoveStraightDirection(int Movedirection, int targetYaw_straight, int speedBase, float duration, float kp_straight, float kd_straight, int Slow) {
  float targetYaw_MovePID = 0;
  float currentYaw = offset_yaw_loop1;  

  if (Con_yaw_loop1 >= 360 || Con_yaw_loop1 <= 0) {
    status_resetYaw = 1;
  }
  switch (targetYaw_straight) {
    case 0:  // N (North)
      targetYaw_MovePID = currentYaw;
      break;
    case 1:  // E (East)
      targetYaw_MovePID = currentYaw + 90;
      break;
    case 2:  // S (South)
      targetYaw_MovePID = currentYaw + 180;
      break;
    case 3:  // W (West)
      targetYaw_MovePID = currentYaw - 90;
      break;
    case 4:  // W (West)
      targetYaw_MovePID = currentYaw - 135;
      break;
    case 5:  // W (West)
      targetYaw_MovePID = currentYaw - 45;
      break;
    case 6:  // W (West)
      targetYaw_MovePID = currentYaw + 45;
      break;
    case 7:  // W (West)
      targetYaw_MovePID = currentYaw + 135;
      break;
  }

  targetYaw_MovePID = normalizeAngle(targetYaw_MovePID);
  moveStraightPID(Movedirection, targetYaw_MovePID, speedBase, duration, kp_straight, kd_straight, Slow);
}


void moveStraightPID_Encoder(int Movedirection, float targetYaw_straight, int speedBase, int target_encoder, float kp_straight, float kd_straight, int Slow) {

  long initialEncoderCount = get_pulse_Encoder(selection_Encoder);
  unsigned long startTime = millis();
  float integral_straight = 0, preverror_straight = 0;
  while (abs(get_pulse_Encoder(selection_Encoder) - initialEncoderCount) < target_encoder) {
    float current_Yaw = Con_yaw_loop1;
    float error_straight = normalizeAngle(targetYaw_straight - current_Yaw);
    unsigned long now = millis();
    float deltaTime = max((now - startTime) / 1000.0, 0.001);
    startTime = now;
    integral_straight += error_straight * deltaTime;
    integral_straight = constrain(integral_straight, -50, 50);
    float derivative = (error_straight - preverror_straight) / deltaTime;
    float output = kp_straight * error_straight  + kd_straight * derivative;
    preverror_straight = error_straight;
    int rightSpeed;
    int leftSpeed;
    if (Slow == 1) {
      float kp_encoder = 0.05;
      long encoderMoved = abs(get_pulse_Encoder(selection_Encoder) - initialEncoderCount);
      long encoderError = target_encoder - encoderMoved;
      float speedReduction = encoderError * kp_encoder;
      speedReduction = constrain(speedReduction, 10, speedBase);
      rightSpeed = constrain(speedReduction - output, 0, 100);
      leftSpeed = constrain(speedReduction + output, 0, 100);
    } else {
      rightSpeed = constrain(speedBase - output, 0, 100);
      leftSpeed = constrain(speedBase + output, 0, 100);
    }
    if (Movedirection == 0) {  
      motor(1, leftSpeed);
      motor(2, rightSpeed);
      motor(3, leftSpeed);
      motor(4, rightSpeed);
    } else {  // ถอยหลัง
      motor(1, -rightSpeed);
      motor(2, -leftSpeed);
      motor(3, -rightSpeed);
      motor(4, -leftSpeed);
    }
  }
  ao();
}
void MoveStraightDirection_Encoder(int Movedirection, int targetYaw_straight, int speedBase, float duration, float kp_straight, float kd_straight, int Slow) {
  float targetYaw_MovePID = 0;
  float currentYaw = offset_yaw_loop1; 

  if (Con_yaw_loop1 >= 360 || Con_yaw_loop1 <= 0) {
    status_resetYaw = 1;
  }
  switch (targetYaw_straight) {
    case 0:  // N (North)
      targetYaw_MovePID = currentYaw;
      break;
    case 1:  // E (East)
      targetYaw_MovePID = currentYaw + 90;
      break;
    case 2:  // S (South)
      targetYaw_MovePID = currentYaw + 180;
      break;
    case 3:  // W (West)
      targetYaw_MovePID = currentYaw - 90;
      break;
    case 4:  // W (West)
      targetYaw_MovePID = currentYaw - 135;
      break;
    case 5:  // W (West)
      targetYaw_MovePID = currentYaw - 45;
      break;
    case 6:  // W (West)
      targetYaw_MovePID = currentYaw + 45;
      break;
    case 7:  // W (West)
      targetYaw_MovePID = currentYaw + 135;
      break;
  }
  targetYaw_MovePID = normalizeAngle(targetYaw_MovePID);
  moveStraightPID_Encoder(Movedirection, targetYaw_MovePID, speedBase, duration, kp_straight, kd_straight, Slow);
}

int getClosestAngle(float currentAngle) {
  // ปรับให้อยู่ในช่วง 0-360
  while (currentAngle < 0) currentAngle += 360;
  while (currentAngle >= 360) currentAngle -= 360;
  int angles[] = { 0, 90, 180, 270, 360 };
  int closest = angles[0];
  float minDiff = fabs(currentAngle - angles[0]);
  for (int i = 1; i < 5; i++) {
    float diff = fabs(currentAngle - angles[i]);
    if (diff < minDiff) {
      minDiff = diff;
      closest = angles[i];
    }
  }
  if (closest == 360) closest = 0;
  return closest + (offset_yaw_loop1 - 180);
}
int Fix_direction(){
  int currentAngle = Con_yaw_loop1;
  while (currentAngle < 0) currentAngle += 360;
  while (currentAngle >= 360) currentAngle -= 360;
  int angles[] = { 0, 90, 180, 270, 360 };
  int closest = angles[0];
  float minDiff = fabs(currentAngle - angles[0]);
  for (int i = 1; i < 5; i++) {
    float diff = fabs(currentAngle - angles[i]);
    if (diff < minDiff) {
      minDiff = diff;
      closest = angles[i];
    }
  }
  if (closest == 360) closest = 0;
  return closest + (offset_yaw_loop1 - 180);

}
void turnByAngle_fix(int turnAngle, int condition) {
  float currentYaw = Con_yaw_loop1;                        
  float roughTargetYaw = currentYaw + turnAngle;           
  int snappedTargetYaw = getClosestAngle(roughTargetYaw); 
  turnPID(condition, snappedTargetYaw, speedMin_turnDirection, speedMax_turnDirection, turnDirection_PID_KP, turnDirection_PID_KD);
  status_resetYaw = 1;
}
void moveStraightSnapToNearest(int Movedirection, int speedBase, int target_encoder, float kp, float kd, int Slow) {
  float currentYaw = Con_yaw_loop1;             
  int snappedYaw = getClosestAngle(currentYaw);  
  moveStraightPID_Encoder(Movedirection, snappedYaw, speedBase, target_encoder, kp, kd, Slow);
}