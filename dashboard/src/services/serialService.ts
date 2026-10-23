import { TelemetryData, CalibData, RobotState } from '../types';

type LogCallback = (msg: string, type: 'rx' | 'tx' | 'system' | 'warn' | 'err') => void;
type TelemetryCallback = (data: TelemetryData) => void;
type StateCallback = (state: RobotState, message?: string, round?: number, timeMs?: number) => void;
type CalibCallback = (data: CalibData) => void;
type ConnectionCallback = (connected: boolean, portInfo?: string) => void;

class SerialService {
  private port: any = null;
  private reader: any = null;
  private writer: any = null;
  private keepReading: boolean = false;
  private inputBuffer: string = '';
  private isConnecting: boolean = false;
  private isAutoReconnect: boolean = true;
  private baudRate: number = 115200;

  private logListeners: Set<LogCallback> = new Set();
  private telemListeners: Set<TelemetryCallback> = new Set();
  private stateListeners: Set<StateCallback> = new Set();
  private calibListeners: Set<CalibCallback> = new Set();
  private connectionListeners: Set<ConnectionCallback> = new Set();

  constructor() {
    this.initAutoReconnect();
  }

  public isSupported(): boolean {
    return 'serial' in navigator;
  }

  public isConnected(): boolean {
    return this.port !== null && this.port.readable !== null;
  }

  public subscribeLog(cb: LogCallback) {
    this.logListeners.add(cb);
    return () => { this.logListeners.delete(cb); };
  }

  public subscribeTelemetry(cb: TelemetryCallback) {
    this.telemListeners.add(cb);
    return () => { this.telemListeners.delete(cb); };
  }

  public subscribeState(cb: StateCallback) {
    this.stateListeners.add(cb);
    return () => { this.stateListeners.delete(cb); };
  }

  public subscribeCalib(cb: CalibCallback) {
    this.calibListeners.add(cb);
    return () => { this.calibListeners.delete(cb); };
  }

  public subscribeConnection(cb: ConnectionCallback) {
    this.connectionListeners.add(cb);
    return () => { this.connectionListeners.delete(cb); };
  }

  private notifyLog(msg: string, type: 'rx' | 'tx' | 'system' | 'warn' | 'err') {
    this.logListeners.forEach(cb => cb(msg, type));
  }

  private notifyConnection(connected: boolean) {
    this.connectionListeners.forEach(cb => cb(connected));
  }

  private notifyState(state: RobotState, msg = '', round?: number, timeMs?: number) {
    this.stateListeners.forEach(cb => cb(state, msg, round, timeMs));
  }

  public async connect(baudRate: number = 115200): Promise<boolean> {
    if (!this.isSupported()) {
      alert('บราวเซอร์ไม่รองรับ Web Serial API! กรุณาใช้ Chrome หรือ Edge บนคอมพิวเตอร์');
      return false;
    }

    this.baudRate = baudRate;
    try {
      this.notifyLog(`[SYSTEM] กำลังขออนุญาตเลือก COM Port (${baudRate} baud)...`, 'system');
      // @ts-ignore
      const port = await navigator.serial.requestPort();
      await this.openPort(port, baudRate);
      this.isAutoReconnect = true;
      localStorage.setItem('puppybot_auto_connect', 'true');
      return true;
    } catch (err: any) {
      if (err.name !== 'NotFoundError') {
        this.notifyLog(`[ERROR] การเชื่อมต่อผิดพลาด: ${err.message}`, 'err');
      }
      this.notifyConnection(false);
      return false;
    }
  }

  public async disconnect(): Promise<void> {
    this.isAutoReconnect = false;
    localStorage.setItem('puppybot_auto_connect', 'false');
    await this.cleanupPort();
    this.notifyConnection(false);
    this.notifyState('DISCONNECTED');
    this.notifyLog('[SYSTEM] ตัดการเชื่อมต่อเรียบร้อยแล้ว', 'system');
  }

  private async openPort(port: any, baudRate: number) {
    if (this.isConnecting) return;
    this.isConnecting = true;

    try {
      if (this.port) {
        await this.cleanupPort();
      }

      await port.open({ baudRate });
      this.port = port;
      this.keepReading = true;
      this.notifyConnection(true);
      this.notifyState('IDLE', 'PuppyBot6 Online & Ready');
      this.notifyLog(`[SYSTEM] เชื่อมต่อสำเร็จ (${baudRate} baud)!`, 'rx');

      // Start Read Loop
      this.readLoop();

      // Handshake
      setTimeout(() => {
        this.sendCommand({ cmd: 'PING' });
        setTimeout(() => {
          this.sendCommand({ cmd: 'GET_CALIB' });
        }, 350);
      }, 400);
    } finally {
      this.isConnecting = false;
    }
  }

  private async cleanupPort() {
    this.keepReading = false;
    if (this.reader) {
      try { await this.reader.cancel(); } catch {}
      try { this.reader.releaseLock(); } catch {}
      this.reader = null;
    }
    if (this.writer) {
      try { this.writer.releaseLock(); } catch {}
      this.writer = null;
    }
    if (this.port) {
      try { await this.port.close(); } catch {}
      this.port = null;
    }
  }

  private async handleDeviceLost() {
    await this.cleanupPort();
    if (this.isAutoReconnect) {
      this.notifyConnection(false);
      this.notifyState('DISCONNECTED');
      this.notifyLog('[SYSTEM] ⚡ ถอดสาย USB แล้ว — เสียบกลับเมื่อไหร่ระบบจะเชื่อมต่อให้อัตโนมัติทันที', 'system');
    } else {
      this.notifyConnection(false);
      this.notifyState('DISCONNECTED');
    }
  }

  private async readLoop() {
    const textDecoder = new TextDecoder();
    while (this.port && this.port.readable && this.keepReading) {
      try {
        this.reader = this.port.readable.getReader();
        while (true) {
          const { value, done } = await this.reader.read();
          if (done) break;
          if (value) {
            this.inputBuffer += textDecoder.decode(value);
            this.processBuffer();
          }
        }
      } catch (err) {
        if (this.keepReading) {
          await this.handleDeviceLost();
          break;
        }
      } finally {
        if (this.reader) {
          try { this.reader.releaseLock(); } catch {}
          this.reader = null;
        }
      }
    }
  }

  private processBuffer() {
    let lineEnd = this.inputBuffer.indexOf('\n');
    while (lineEnd !== -1) {
      const line = this.inputBuffer.substring(0, lineEnd).trim();
      this.inputBuffer = this.inputBuffer.substring(lineEnd + 1);

      if (line.length > 0) {
        this.handleLine(line);
      }
      lineEnd = this.inputBuffer.indexOf('\n');
    }
  }

  private handleLine(line: string) {
    if (!line.includes('"type":"TELEM"')) {
      this.notifyLog(`[RX] ${line}`, 'rx');
    }

    try {
      if (line.startsWith('{') && line.endsWith('}')) {
        const data = JSON.parse(line);
        this.processPacket(data);
      }
    } catch {}
  }

  private processPacket(data: any) {
    if (data.type === 'TELEM') {
      this.telemListeners.forEach(cb => cb(data as TelemetryData));
      return;
    }

    if (data.type === 'CALIB_DATA') {
      this.calibListeners.forEach(cb => cb(data as CalibData));
      return;
    }

    if (data.type === 'STATUS') {
      if (data.state === 'WAIT_BTN') {
        this.notifyState('WAIT_BTN', data.msg, data.round);
      } else if (data.state === 'RUNNING') {
        this.notifyState('RUNNING', data.msg, data.round);
      } else if (data.state === 'CALIBRATING') {
        this.notifyState('CALIBRATING', data.msg);
      }
      return;
    }

    if (data.type === 'RES') {
      if (data.status === 'DONE') {
        this.notifyState('IDLE', data.msg, data.round, data.time_ms);
      } else if (data.status === 'CALIB_DONE') {
        this.notifyState('CALIB_DONE', data.msg);
      } else if (data.status === 'STOPPED') {
        this.notifyState('STOPPED', data.msg);
      } else if (data.status === 'CONFIG_SAVED') {
        this.notifyLog(`[CONFIG] ${data.msg}`, 'rx');
      }
      return;
    }

    if (data.type === 'PONG') {
      this.notifyState('IDLE', 'PuppyBot6 Online & Ready');
    }
  }

  public async sendCommand(obj: any): Promise<boolean> {
    if (!this.port || !this.port.writable) {
      this.notifyLog('[WARN] ยังไม่ได้เชื่อมต่อ COM Port!', 'warn');
      return false;
    }

    try {
      const jsonStr = JSON.stringify(obj) + '\n';
      this.notifyLog(`[TX] ${jsonStr.trim()}`, 'tx');

      this.writer = this.port.writable.getWriter();
      const encoder = new TextEncoder();
      await this.writer.write(encoder.encode(jsonStr));
      this.writer.releaseLock();
      this.writer = null;
      return true;
    } catch (err: any) {
      this.notifyLog(`[ERROR] Send error: ${err.message}`, 'err');
      if (this.writer) {
        try { this.writer.releaseLock(); } catch {}
        this.writer = null;
      }
      return false;
    }
  }

  public async emergencyStop(): Promise<void> {
    await this.sendCommand({ cmd: 'STOP' });
    this.notifyState('STOPPED', 'EMERGENCY STOP TRIGGERED');
  }

  private initAutoReconnect() {
    if (!this.isSupported()) return;

    // @ts-ignore
    navigator.serial.addEventListener('connect', async (event: any) => {
      this.notifyLog('[SYSTEM] ตรวจพบการเสียบสาย USB กลับเข้ามา...', 'system');
      if (this.isAutoReconnect && !this.port && !this.isConnecting) {
        setTimeout(async () => {
          try {
            await this.openPort(event.target, this.baudRate);
            this.notifyLog('[SYSTEM] ⚡ เชื่อมต่อ PuppyBot6 อัตโนมัติสำเร็จ!', 'rx');
          } catch (e) {
            console.warn(e);
          }
        }, 500);
      }
    });

    // @ts-ignore
    navigator.serial.addEventListener('disconnect', async () => {
      this.notifyLog('[SYSTEM] ตรวจพบการถอดสาย USB', 'warn');
      await this.handleDeviceLost();
    });

    // Watchdog
    setInterval(async () => {
      if (this.isAutoReconnect && !this.port && !this.isConnecting) {
        try {
          // @ts-ignore
          const ports = await navigator.serial.getPorts();
          if (ports && ports.length > 0) {
            await this.openPort(ports[0], this.baudRate);
            this.notifyLog('[SYSTEM] ⚡ ตรวจพบบอร์ดและเชื่อมต่อให้อัตโนมัติสำเร็จ!', 'rx');
          }
        } catch {}
      }
    }, 1500);
  }
}

export const serial = new SerialService();
