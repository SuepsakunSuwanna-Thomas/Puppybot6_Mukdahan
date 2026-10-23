export interface TaskState {
  has: number;
  round: number;
  time_ms: number;
  running: number;
  cmd: string;
  mode: string;
}

export interface TelemetryData {
  type: 'TELEM';
  fc: number;        // Floor color enum
  fn: string;        // Floor color name
  frgb: [number, number, number];
  sc: number;        // Side color enum
  sn: string;        // Side color name
  srgb: [number, number, number];
  ir: number;        // IR distance in cm
  enc: number;       // Encoder distance in cm
  enc_raw: number;   // Raw pulses
  enc_scale: number; // Pulses per cm
  yaw: number;       // Gyro yaw in degrees
  btn: number;       // 1 if pressed, 0 if released
  task: TaskState;
  fl: number[];      // Front line sensors (7 bits: 0 or 1)
  bl: number[];      // Back line sensors (7 bits: 0 or 1)
}

export interface CalibData {
  type: 'CALIB_DATA';
  front_min: number[];
  front_max: number[];
  back_min: number[];
  back_max: number[];
  ir_5cm: number;
  ir_10cm: number;
  enc_scale: number;
}

export type RobotState = 'DISCONNECTED' | 'IDLE' | 'WAIT_BTN' | 'RUNNING' | 'CALIBRATING' | 'CALIB_DONE' | 'STOPPED';

export type StepType = 
  | 'phase' 
  | 'pass' 
  | 'time' 
  | 'turnLeft' 
  | 'turnRight' 
  | 'turnLeft_by_stop' 
  | 'turnRight_by_stop' 
  | 'checkLine' 
  | 'phase_b' 
  | 'pass_b' 
  | 'turnLeft_b' 
  | 'turnRight_b' 
  | 'checkLine_b' 
  | 'follow_notline' 
  | 'follow_notline_enc' 
  | 'backward_notline'
  | 'servo'
  | 'motor'
  | 'delay';

export interface SequenceStep {
  id: string;
  type: StepType;
  title: string;
  speed: number;
  val: number;          // distance (cm), pass count, or duration (ms)
  kp?: number;
  kd?: number;
  s1?: number;           // Servo 1 angle
  s2?: number;           // Servo 2 angle
  m1?: number;
  m2?: number;
  m3?: number;
  m4?: number;
  motor_time?: number;
  delayMs?: number;
  note?: string;
  status?: 'idle' | 'active' | 'done' | 'error';
  elapsedMs?: number;
}

export interface LapLog {
  id: string;
  timestamp: string;
  round: number;
  timeMs: number;
  cmd: string;
  mode: string;
  speed?: number;
}
