/**
 * PUPPYBOT6 PRO MISSION CONTROL & DIAGNOSTICS LAB
 * Web Serial API Controller, Dynamic Inputs, Auto-Reconnect & LocalStorage Persistence
 */

// Global State
let serialPort = null;
let reader = null;
let writer = null;
let keepReading = false;
let inputBuffer = '';
let isConnecting = false;

const STORAGE_KEY = 'puppybot_dashboard_config_v1';
let isAutoReconnectActive = localStorage.getItem('puppybot_auto_connect') === 'true';

// DOM Elements: Connection & Header
const baudSelect = document.getElementById('baud-select');
const btnConnect = document.getElementById('btn-connect');
const btnDisconnect = document.getElementById('btn-disconnect');
const btnResetSettings = document.getElementById('btn-reset-settings');
const btnEmergencyStop = document.getElementById('btn-emergency-stop');
const connectionStatusPill = document.getElementById('connection-status-pill');
const connectionStatusText = document.getElementById('connection-status-text');
const robotStatePill = document.getElementById('robot-state-pill');
const robotStateText = document.getElementById('robot-state-text');
const instructionBanner = document.getElementById('instruction-banner');
const instructionStepText = document.getElementById('instruction-step-text');

// Telemetry Elements
const frontColorName = document.getElementById('front-color-name');
const frontColorSwatch = document.getElementById('front-color-swatch');
const fMeterR = document.getElementById('f-meter-r');
const fMeterG = document.getElementById('f-meter-g');
const fMeterB = document.getElementById('f-meter-b');
const fValR = document.getElementById('f-val-r');
const fValG = document.getElementById('f-val-g');
const fValB = document.getElementById('f-val-b');

const frontLineStatus = document.getElementById('front-line-status');
const backLineStatus = document.getElementById('back-line-status');
const crossBadge = document.getElementById('cross-badge');

const valIr = document.getElementById('val-ir');
const meterIr = document.getElementById('meter-ir');
const valEnc = document.getElementById('val-enc');
const valYaw = document.getElementById('val-yaw');
const compassNeedle = document.getElementById('compass-needle');
const switchIndicator = document.getElementById('switch-indicator');
const valBtn = document.getElementById('val-btn');

// Terminal Elements
const terminalOutput = document.getElementById('terminal-output');
const terminalInput = document.getElementById('terminal-input');
const btnSendRaw = document.getElementById('btn-send-raw');
const btnClearTerminal = document.getElementById('btn-clear-terminal');
const terminalAutoscroll = document.getElementById('terminal-autoscroll');

// PID Lab Elements
const pidModeSelect = document.getElementById('pid-mode-select');
const pidSpeedSlider = document.getElementById('pid-speed-slider');
const pidSpeedNum = document.getElementById('pid-speed-num');
const pidKpSlider = document.getElementById('pid-kp-slider');
const pidKpNum = document.getElementById('pid-kp-num');
const pidKdSlider = document.getElementById('pid-kd-slider');
const pidKdNum = document.getElementById('pid-kd-num');
const pidKdAutoBadge = document.getElementById('pid-kd-auto-badge');
const pidParamInput = document.getElementById('pid-param-input');
const paramLabel = document.getElementById('param-label');
const presetButtons = document.getElementById('preset-buttons');
const pidWaitBtn = document.getElementById('pid-wait-btn');
const btnSendPid = document.getElementById('btn-send-pid');
const testExecStatus = document.getElementById('test-exec-status');

// 4WD Motor Lab Elements
const m1Slider = document.getElementById('m1-slider');
const m1Num = document.getElementById('m1-num');
const m2Slider = document.getElementById('m2-slider');
const m2Num = document.getElementById('m2-num');
const m3Slider = document.getElementById('m3-slider');
const m3Num = document.getElementById('m3-num');
const m4Slider = document.getElementById('m4-slider');
const m4Num = document.getElementById('m4-num');
const motorDurationInput = document.getElementById('motor-duration-input');
const motorWaitBtn = document.getElementById('motor-wait-btn');
const btnSendMotor = document.getElementById('btn-send-motor');
const motorExecStatus = document.getElementById('motor-exec-status');

// Servo Lab Elements
const s1Slider = document.getElementById('s1-slider');
const s1Num = document.getElementById('s1-num');
const s2Slider = document.getElementById('s2-slider');
const s2Num = document.getElementById('s2-num');
const servoWaitBtn = document.getElementById('servo-wait-btn');
const btnSendServo = document.getElementById('btn-send-servo');
const servoExecStatus = document.getElementById('servo-exec-status');

// ============================================================================
// 1. SERIAL CONNECTION, RECONNECT & STREAM HANDLING
// ============================================================================

// Check Web Serial API Support
if (!('serial' in navigator)) {
  alert('เบราว์เซอร์ของคุณไม่รองรับ Web Serial API!\nกรุณาใช้งานบน Google Chrome, Microsoft Edge, หรือ Opera บนคอมพิวเตอร์');
  logTerminal('[ERROR] Web Serial API not supported in this browser. Please use Chrome or Edge.', 'term-err');
}

// User clicks Connect COM Port
btnConnect.addEventListener('click', async () => {
  try {
    const baudRate = parseInt(baudSelect.value, 10);
    logTerminal(`[SYSTEM] กำลังขอสิทธิ์เลือก Serial Port ที่ ${baudRate} baud...`, 'term-system');

    const port = await navigator.serial.requestPort();
    await openAndStartPort(port, baudRate);
    isAutoReconnectActive = true;
    localStorage.setItem('puppybot_auto_connect', 'true');
  } catch (err) {
    if (err.name !== 'NotFoundError') {
      console.error('Serial connection error:', err);
      logTerminal(`[ERROR] การเชื่อมต่อล้มเหลว: ${err.message}`, 'term-err');
    }
    updateConnectionUI(false);
  }
});

// User explicitly clicks Disconnect
btnDisconnect.addEventListener('click', async () => {
  isAutoReconnectActive = false;
  localStorage.setItem('puppybot_auto_connect', 'false');
  await cleanupPort();
  updateConnectionUI(false);
  logTerminal('[SYSTEM] ตัดการเชื่อมต่อด้วยตนเองแล้ว (ปิดระบบเชื่อมต่ออัตโนมัติ)', 'term-system');
});

// Helper: Open and start port
async function openAndStartPort(port, baudRate) {
  if (isConnecting) return;
  isConnecting = true;

  try {
    if (serialPort) {
      await cleanupPort();
    }

    await port.open({ baudRate: baudRate });
    serialPort = port;
    keepReading = true;
    updateConnectionUI(true);
    logTerminal(`[SYSTEM] เชื่อมต่อกับบอร์ดสำเร็จ (${baudRate} baud)!`, 'term-rx');

    // เริ่มต้นอ่านข้อมูลเข้า
    readSerialLoop();

    // ส่ง PING ตรวจสอบสถานะหุ่น และดึงค่า EEPROM Calibration ล่าสุด
    setTimeout(() => {
      sendCommand({ cmd: 'PING' });
      setTimeout(() => {
        sendCommand({ cmd: 'GET_CALIB' });
      }, 350);
    }, 400);
  } finally {
    isConnecting = false;
  }
}

// Helper: Safely release and close port
async function cleanupPort() {
  keepReading = false;
  if (reader) {
    try { await reader.cancel(); } catch (e) {}
    try { reader.releaseLock(); } catch (e) {}
    reader = null;
  }
  if (writer) {
    try { writer.releaseLock(); } catch (e) {}
    writer = null;
  }
  if (serialPort) {
    try { await serialPort.close(); } catch (e) {}
    serialPort = null;
  }
}

// Device lost (e.g. cable unplugged to run the robot)
async function handleDeviceLost() {
  await cleanupPort();
  if (isAutoReconnectActive) {
    updateAutoReconnectUI();
    logTerminal('[SYSTEM] ⚡ ถอดสายหุ่นยนต์ไปวิ่งได้เลย &mdash; เมื่อนำกลับมาเสียบ USB ระบบจะเชื่อมต่อให้อัตโนมัติทันที', 'term-system');
  } else {
    updateConnectionUI(false);
  }
}

function updateConnectionUI(connected) {
  if (connected) {
    btnConnect.style.display = 'none';
    btnDisconnect.style.display = 'inline-flex';
    connectionStatusPill.className = 'status-pill status-connected';
    connectionStatusText.textContent = 'เชื่อมต่อแล้ว (Connected)';
    baudSelect.disabled = true;
  } else {
    btnConnect.style.display = 'inline-flex';
    btnDisconnect.style.display = 'none';
    connectionStatusPill.className = 'status-pill status-disconnected';
    connectionStatusText.textContent = 'ไม่ได้เชื่อมต่อ';
    baudSelect.disabled = false;
    setRobotState('IDLE');
  }
}

function updateAutoReconnectUI() {
  btnConnect.style.display = 'none';
  btnDisconnect.style.display = 'inline-flex';
  connectionStatusPill.className = 'status-pill status-reconnecting';
  connectionStatusText.textContent = '⚡ รอเสียบสาย USB (Auto-Reconnecting...)';
  baudSelect.disabled = true;
  instructionStepText.innerHTML = '<strong>⚡ หุ่นยนต์ถูกถอดสาย:</strong> นำไปวิ่งบนสนามได้เลย! เมื่อนำกลับมาเสียบ USB บอร์ดจะเชื่อมต่อให้อัตโนมัติทันที';
}

// Read Loop with Disconnect Catching
async function readSerialLoop() {
  const textDecoder = new TextDecoder();
  while (serialPort && serialPort.readable && keepReading) {
    try {
      reader = serialPort.readable.getReader();
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) {
          const chunk = textDecoder.decode(value);
          inputBuffer += chunk;
          processIncomingBuffer();
        }
      }
    } catch (err) {
      if (keepReading) {
        console.warn('Read stream terminated (cable unplugged):', err);
        await handleDeviceLost();
        break;
      }
    } finally {
      if (reader) {
        try { reader.releaseLock(); } catch (e) {}
        reader = null;
      }
    }
  }
}

// Auto-Reconnect Listeners via Web Serial API
if ('serial' in navigator) {
  // When an authorized USB device is plugged in
  navigator.serial.addEventListener('connect', async (event) => {
    logTerminal('[SYSTEM] ตรวจพบการเสียบสาย USB กลับเข้ามา...', 'term-system');
    if (isAutoReconnectActive && !serialPort && !isConnecting) {
      setTimeout(async () => {
        try {
          const baudRate = parseInt(baudSelect.value, 10);
          await openAndStartPort(event.target, baudRate);
          logTerminal('[SYSTEM] ⚡ เชื่อมต่อ PuppyBot6 อัตโนมัติสำเร็จ!', 'term-rx');
        } catch (err) {
          console.warn('Auto-reconnect on connect event failed:', err);
        }
      }, 500);
    }
  });

  // When device is unplugged
  navigator.serial.addEventListener('disconnect', async () => {
    logTerminal('[SYSTEM] ตรวจพบการถอดสาย USB', 'term-warn');
    await handleDeviceLost();
  });

  // Watchdog Polling (every 1.5s) to guarantee reconnection even if connect event didn't fire
  setInterval(async () => {
    if (isAutoReconnectActive && !serialPort && !isConnecting) {
      try {
        const ports = await navigator.serial.getPorts();
        if (ports && ports.length > 0) {
          const baudRate = parseInt(baudSelect.value, 10);
          await openAndStartPort(ports[0], baudRate);
          logTerminal('[SYSTEM] ⚡ ตรวจพบบอร์ดและเชื่อมต่อให้อัตโนมัติสำเร็จ!', 'term-rx');
        }
      } catch (e) {
        // Port not yet ready
      }
    }
  }, 1500);
}

// แยกบรรทัดและประมวลผล JSON
function processIncomingBuffer() {
  let lineEnd = inputBuffer.indexOf('\n');
  while (lineEnd !== -1) {
    const rawLine = inputBuffer.substring(0, lineEnd).trim();
    inputBuffer = inputBuffer.substring(lineEnd + 1);

    if (rawLine.length > 0) {
      handleIncomingLine(rawLine);
    }
    lineEnd = inputBuffer.indexOf('\n');
  }
}

function handleIncomingLine(line) {
  if (!line.includes('"type":"TELEM"')) {
    logTerminal(`[RX] ${line}`, 'term-rx');
  }

  try {
    if (line.startsWith('{') && line.endsWith('}')) {
      const data = JSON.parse(line);
      processJsonPacket(data);
    }
  } catch (err) {
    // Non-JSON line
  }
}

// ส่งข้อความ JSON ไปยังหุ่นยนต์
async function sendCommand(obj) {
  if (!serialPort || !serialPort.writable) {
    logTerminal('[WARN] ยังไม่ได้เชื่อมต่อ COM Port! ไม่สามารถส่งคำสั่งได้', 'term-warn');
    alert('กรุณาเชื่อมต่อ COM Port ก่อนส่งคำสั่ง!');
    return false;
  }

  try {
    const jsonStr = JSON.stringify(obj) + '\n';
    logTerminal(`[TX] ${jsonStr.trim()}`, 'term-tx');

    writer = serialPort.writable.getWriter();
    const encoder = new TextEncoder();
    await writer.write(encoder.encode(jsonStr));
    writer.releaseLock();
    writer = null;
    return true;
  } catch (err) {
    console.error('Send error:', err);
    logTerminal(`[ERROR] Send error: ${err.message}`, 'term-err');
    if (writer) {
      try { writer.releaseLock(); } catch (e) {}
      writer = null;
    }
    return false;
  }
}

// ============================================================================
// 2. JSON PACKET DISPATCHER & TELEMETRY UPDATES
// ============================================================================

function processJsonPacket(data) {
  if (data.type === 'TELEM') {
    updateTelemetrySensors(data);
    return;
  }

  if (data.type === 'CALIB_DATA') {
    updateCalibTable(data);
    return;
  }

  if (data.type === 'STATUS') {
    if (data.state === 'WAIT_BTN') {
      setRobotState('WAIT_BTN', data.msg, data.round);
    } else if (data.state === 'RUNNING') {
      setRobotState('RUNNING', data.msg, data.round);
    } else if (data.state === 'CALIBRATING') {
      setRobotState('CALIBRATING', data.msg);
    }
    return;
  }

  if (data.type === 'RES') {
    if (data.status === 'DONE') {
      setRobotState('DONE', data.msg, data.round, data.time_ms);
    } else if (data.status === 'CALIB_DONE') {
      setRobotState('CALIB_DONE', data.msg);
    } else if (data.status === 'STOPPED') {
      setRobotState('STOPPED', data.msg);
    } else if (data.status === 'CONFIG_SAVED') {
      logTerminal(`[CONFIG] ${data.msg}`, 'term-rx');
    }
    return;
  }

  if (data.type === 'PONG') {
    setRobotState('IDLE', 'PuppyBot6 Online & Ready');
  }
}

function setRobotState(state, message = '', round = null, time_ms = null) {
  const pidExec = document.getElementById('test-exec-status');
  const motorExec = document.getElementById('motor-exec-status');
  const servoExec = document.getElementById('servo-exec-status');
  const roundTag = (round !== null && round !== undefined) ? ` [รอบ: #${round}]` : '';

  let timeText = '';
  let timePillHtml = '';
  if (time_ms !== null && time_ms !== undefined && time_ms > 0) {
    const s = Math.floor(time_ms / 1000);
    const ms = time_ms % 1000;
    const secStr = (time_ms / 1000).toFixed(3);
    timeText = `⏱️ ใช้เวลา: ${s} วินาที ${ms} มิลลิวินาที (${secStr}s / ${time_ms.toLocaleString()} ms)`;
    timePillHtml = `<span class="lap-time-badge" style="display:inline-flex; align-items:center; gap:6px; background:rgba(0,240,255,0.15); border:1px solid #00f0ff; color:#00f0ff; padding:3px 10px; border-radius:12px; font-weight:bold; font-size:13px; margin-left:8px;">⏱️ ${s} วิ ${ms} ms (${secStr}s)</span>`;
  }

  if (state === 'WAIT_BTN') {
    robotStatePill.className = 'status-pill status-waiting-switch';
    robotStateText.textContent = `ROBOT: WAITING SWITCH ⏳${roundTag}`;
    instructionStepText.innerHTML = `<strong>⚠️ หุ่นยนต์รอกดสวิตช์${roundTag}:</strong> กดสวิตช์ที่ตัวหุ่นยนต์เพื่อเริ่มวิ่ง (กดซ้ำกี่รอบก็ได้ หรือส่งคำสั่งใหม่จากเว็บ)`;
    if (pidExec) pidExec.innerHTML = `<span style="color:#fbbf24;">⏳ รอกดสวิตช์ที่ตัวหุ่นยนต์...${roundTag} (${message})</span>`;
    if (motorExec) motorExec.innerHTML = `<span style="color:#fbbf24;">⏳ รอกดสวิตช์เพื่อหมุนมอเตอร์...${roundTag}</span>`;
    if (servoExec) servoExec.innerHTML = `<span style="color:#fbbf24;">⏳ รอกดสวิตช์เพื่อหมุนเซอร์โว...${roundTag}</span>`;
  } else if (state === 'RUNNING') {
    robotStatePill.className = 'status-pill status-running';
    robotStateText.textContent = `ROBOT: RUNNING ⚡${roundTag}`;
    instructionStepText.innerHTML = `<strong>▶️ หุ่นยนต์กำลังทำงาน${roundTag}...</strong> รอจนกว่าจะเสร็จสิ้นภารกิจ (กำลังจับเวลา)`;
    if (pidExec) pidExec.innerHTML = `<span style="color:#00f0ff;">⚡ กำลังวิ่งทดสอบ PID...${roundTag} (กำลังจับเวลา)</span>`;
    if (motorExec) motorExec.innerHTML = `<span style="color:#00f0ff;">⚡ มอเตอร์กำลังหมุน...${roundTag}</span>`;
    if (servoExec) servoExec.innerHTML = `<span style="color:#00f0ff;">⚡ เซอร์โวกำลังหมุน...${roundTag}</span>`;
  } else if (state === 'CALIBRATING') {
    robotStatePill.className = 'status-pill status-running';
    robotStateText.textContent = `ROBOT: CALIBRATING 🛠️`;
    instructionStepText.innerHTML = `<strong>🛠️ กำลังคาลิเบรต:</strong> ทำตามขั้นตอนบนหน้าจอ LCD/TFT ของหุ่นยนต์ (${message})`;
  } else if (state === 'CALIB_DONE') {
    robotStatePill.className = 'status-pill status-connected';
    robotStateText.textContent = `ROBOT: CALIBRATED ✅`;
    instructionStepText.innerHTML = `<strong>✅ คาลิเบรตเสร็จสมบูรณ์และบันทึก EEPROM แล้ว!</strong> (${message})`;
    setTimeout(() => {
      setRobotState('IDLE');
    }, 4000);
  } else if (state === 'DONE') {
    robotStatePill.className = 'status-pill status-connected';
    const timeShort = (time_ms !== null && time_ms !== undefined && time_ms > 0) ? ` ⏱️ ${(time_ms / 1000).toFixed(3)}s` : '';
    robotStateText.textContent = `ROBOT: READY FOR RE-RUN ✅${roundTag}${timeShort}`;
    instructionStepText.innerHTML = `<strong>✅ ภารกิจสำเร็จ${roundTag}!</strong> ${timeText ? `<br><span style="color:#00f0ff; font-weight:bold; font-size:15px;">${timeText}</span>` : ''}<br>สามารถกดสวิตช์ที่ตัวหุ่นเพื่อวิ่งซ้ำรอบต่อไปได้ทันที (หรือส่งคำสั่งใหม่จากเว็บ)`;
    if (pidExec) {
      pidExec.innerHTML = `
        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px;">
          <span style="color:#34d399; font-weight:bold;">✅ สำเร็จ${roundTag}! (กดสวิตช์ที่ตัวหุ่นเพื่อวิ่งซ้ำได้เรื่อยๆ หรือส่งคำสั่งใหม่)</span>
          ${timePillHtml}
        </div>`;
    }
    if (motorExec) motorExec.innerHTML = `<span style="color:#34d399;">✅ สำเร็จ${roundTag}!${timePillHtml}</span>`;
    if (servoExec) servoExec.innerHTML = `<span style="color:#34d399;">✅ สำเร็จ${roundTag}!${timePillHtml}</span>`;
  } else if (state === 'STOPPED') {
    robotStatePill.className = 'status-pill status-disconnected';
    robotStateText.textContent = 'ROBOT: STOPPED 🛑';
    instructionStepText.innerHTML = '<strong>🛑 EMERGENCY STOP!</strong> มอเตอร์ทั้งหมดถูกสั่งหยุดการทำงานทันที';
    if (pidExec) pidExec.innerHTML = `<span style="color:#ef4444;">🛑 สั่งหยุดฉุกเฉินแล้ว</span>`;
    if (motorExec) motorExec.innerHTML = `<span style="color:#ef4444;">🛑 สั่งหยุดฉุกเฉินแล้ว</span>`;
    if (servoExec) servoExec.innerHTML = `<span style="color:#ef4444;">🛑 สั่งหยุดฉุกเฉินแล้ว</span>`;
  } else {
    // IDLE
    robotStatePill.className = 'status-pill status-idle';
    robotStateText.textContent = 'ROBOT: IDLE (พร้อมรับคำสั่ง)';
    instructionStepText.innerHTML = '1. ตั้งค่าการทดสอบหรือคาลิเบรต &rarr; 2. กดปุ่มส่งคำสั่งจากเว็บ &rarr; 3. หุ่นยนต์จะแสดงผลบนจอและรอกดสวิตช์ &rarr; 4. กดสวิตช์ที่ตัวหุ่นยนต์เพื่อเริ่มทำงาน!';
  }
}

// อัปเดตการแสดงผลเซนเซอร์ทั้งหมดในหน้าเว็บ
function updateTelemetrySensors(data) {
  // 1. สีหน้า (Front Color)
  if (data.fn) {
    frontColorName.textContent = data.fn;
    frontColorName.className = `color-badge badge-${data.fn.toLowerCase()}`;
  }
  if (data.frgb && data.frgb.length === 3) {
    const [r, g, b] = data.frgb;
    fValR.textContent = r;
    fValG.textContent = g;
    fValB.textContent = b;
    fMeterR.style.width = `${Math.min(100, (r / 200) * 100)}%`;
    fMeterG.style.width = `${Math.min(100, (g / 200) * 100)}%`;
    fMeterB.style.width = `${Math.min(100, (b / 200) * 100)}%`;
    frontColorSwatch.style.backgroundColor = getCssColorFromName(data.fn, r, g, b);
  }

  // 2. เซนเซอร์เส้นหน้า 7 ตัว (ADC 0..6)
  let fOnCount = 0;
  if (data.fl && data.fl.length === 7) {
    for (let i = 0; i < 7; i++) {
      const led = document.getElementById(`fl-led-${i}`);
      if (led) {
        if (data.fl[i] === 1) {
          led.classList.add('active');
          fOnCount++;
        } else {
          led.classList.remove('active');
        }
      }
    }
    frontLineStatus.textContent = `${fOnCount}/7 เซนเซอร์เจอดำ`;
  }

  // 3. เซนเซอร์เส้นหลัง 7 ตัว (ADC 7..13)
  let bOnCount = 0;
  if (data.bl && data.bl.length === 7) {
    for (let i = 0; i < 7; i++) {
      const led = document.getElementById(`bl-led-${i}`);
      if (led) {
        if (data.bl[i] === 1) {
          led.classList.add('active');
          bOnCount++;
        } else {
          led.classList.remove('active');
        }
      }
    }
    backLineStatus.textContent = `${bOnCount}/7 เซนเซอร์เจอดำ`;
  }

  // ป้ายแจ้งเตือนเส้นตัด (Crossing)
  if (fOnCount >= 4 || bOnCount >= 4) {
    crossBadge.textContent = '⚠️ เจอเส้นตัด (CROSSING)';
    crossBadge.className = 'badge-live';
    crossBadge.style.borderColor = '#f59e0b';
    crossBadge.style.color = '#fbbf24';
  } else {
    crossBadge.textContent = 'LINE TRACK NORMAL';
    crossBadge.className = 'badge-neutral';
    crossBadge.style.borderColor = '';
    crossBadge.style.color = '';
  }

  // 4. ระยะห่าง IR
  if (data.ir !== undefined) {
    valIr.textContent = data.ir.toFixed(1);
    const irPct = Math.min(100, (data.ir / 30) * 100);
    meterIr.style.width = `${irPct}%`;
  }

  // 5. ระยะทาง Encoder
  if (data.enc !== undefined) {
    valEnc.textContent = data.enc.toFixed(0);
  }

  // 6. องศาไจโร IMU Yaw
  if (data.yaw !== undefined) {
    valYaw.textContent = data.yaw.toFixed(1);
    compassNeedle.style.transform = `rotate(${data.yaw}deg)`;
  }

  // 7. สวิตช์
  if (data.btn !== undefined) {
    if (data.btn === 1) {
      switchIndicator.classList.add('active');
      valBtn.textContent = 'กดอยู่ (PRESSED!)';
    } else {
      switchIndicator.classList.remove('active');
      valBtn.textContent = 'ปล่อย (Released)';
    }
  }
}

function getCssColorFromName(name, r, g, b) {
  if (!name) return '#334155';
  switch (name.toUpperCase()) {
    case 'RED': return '#ef4444';
    case 'GREEN': return '#10b981';
    case 'BLUE': return '#3b82f6';
    case 'YELLOW': return '#eab308';
    case 'WHITE': return '#ffffff';
    case 'BLACK': return '#1e293b';
    default:
      return `rgb(${Math.min(255, r * 2)}, ${Math.min(255, g * 2)}, ${Math.min(255, b * 2)})`;
  }
}

// ============================================================================
// 3. TAB NAVIGATION
// ============================================================================
const tabButtons = document.querySelectorAll('.tab-btn');
const tabPanes = document.querySelectorAll('.tab-pane');

tabButtons.forEach(btn => {
  btn.addEventListener('click', () => {
    tabButtons.forEach(b => b.classList.remove('active'));
    tabPanes.forEach(p => p.classList.remove('active'));

    btn.classList.add('active');
    const targetPaneId = btn.getAttribute('data-tab');
    const targetPane = document.getElementById(targetPaneId);
    if (targetPane) targetPane.classList.add('active');
    saveSettingsDebounced();
  });
});

// ============================================================================
// 4. TWO-WAY BINDING: SLIDERS & NUMERIC INPUTS HELPER
// ============================================================================

function bindSliderAndNumber(sliderId, numId, onChange) {
  const slider = document.getElementById(sliderId);
  const num = document.getElementById(numId);
  if (!slider || !num) return;

  const isFloat = num.step && num.step.includes('.');
  const decimals = isFloat ? num.step.split('.')[1].length : 0;

  slider.addEventListener('input', () => {
    num.value = isFloat ? parseFloat(slider.value).toFixed(decimals) : slider.value;
    if (onChange) onChange(num.value);
    saveSettingsDebounced();
  });

  num.addEventListener('input', () => {
    if (num.value !== '') {
      slider.value = num.value;
      if (onChange) onChange(num.value);
      saveSettingsDebounced();
    }
  });

  num.addEventListener('blur', () => {
    let val = parseFloat(num.value);
    const min = parseFloat(num.min);
    const max = parseFloat(num.max);
    if (!isNaN(val)) {
      if (!isNaN(min) && val < min) val = min;
      if (!isNaN(max) && val > max) val = max;
      num.value = isFloat ? val.toFixed(decimals) : val;
      slider.value = val;
      if (onChange) onChange(val);
      saveSettingsDebounced();
    }
  });
}

// Bind PID Sliders
bindSliderAndNumber('pid-speed-slider', 'pid-speed-num');
bindSliderAndNumber('pid-kp-slider', 'pid-kp-num');
bindSliderAndNumber('pid-kd-slider', 'pid-kd-num', (val) => {
  updateKdAutoBadge(parseFloat(val));
});

function updateKdAutoBadge(val) {
  if (pidKdAutoBadge) {
    pidKdAutoBadge.textContent = (val === 0) ? '(Auto)' : '';
  }
}

// KD Preset Buttons
document.querySelectorAll('#kd-preset-buttons .btn-chip').forEach(btn => {
  btn.addEventListener('click', () => {
    const kd = parseFloat(btn.getAttribute('data-kd'));
    if (pidKdSlider) pidKdSlider.value = kd;
    if (pidKdNum) pidKdNum.value = kd;
    updateKdAutoBadge(kd);
    document.querySelectorAll('#kd-preset-buttons .btn-chip').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    saveSettingsDebounced();
  });
});

// Dynamic PID Parameter Presets
pidModeSelect.addEventListener('change', () => {
  updatePidParamPresets();
  saveSettingsDebounced();
});

function updatePidParamPresets() {
  const mode = pidModeSelect.value;
  presetButtons.innerHTML = '';

  if (mode === 'phase' || mode === 'phase_b' || mode === 'follow_notline_enc') {
    paramLabel.textContent = 'ระยะทางตามเส้น (เซนติเมตร cm):';
    pidParamInput.value = '30.0';
    pidParamInput.disabled = false;
    [15, 30, 50, 80].forEach(val => createPresetChip(`${val} cm`, val));
  } else if (mode === 'pass' || mode === 'pass_b') {
    paramLabel.textContent = 'จำนวนเส้นตัดที่ต้องการวิ่งผ่าน (เส้น):';
    pidParamInput.value = '2';
    pidParamInput.disabled = false;
    [1, 2, 3, 5].forEach(val => createPresetChip(`${val} เส้น`, val));
  } else if (mode === 'time' || mode === 'follow_notline' || mode === 'backward_notline') {
    paramLabel.textContent = 'ระยะเวลาการวิ่ง (มิลลิวินาที ms):';
    pidParamInput.value = '1500';
    pidParamInput.disabled = false;
    [500, 1000, 1500, 2500].forEach(val => createPresetChip(`${val} ms`, val));
  } else {
    // turnLeft, turnRight, checkLine
    paramLabel.textContent = 'พารามิเตอร์เพิ่มเติม:';
    pidParamInput.value = '0';
    pidParamInput.disabled = true;
    createPresetChip('ไม่ต้องระบุค่า (Auto)', 0);
  }
}

function createPresetChip(text, val) {
  const chip = document.createElement('button');
  chip.type = 'button';
  chip.className = 'btn-chip';
  chip.textContent = text;
  chip.addEventListener('click', () => {
    pidParamInput.value = val;
    document.querySelectorAll('#preset-buttons .btn-chip').forEach(c => c.classList.remove('active'));
    chip.classList.add('active');
    saveSettingsDebounced();
  });
  presetButtons.appendChild(chip);
}

// Send PID Command
btnSendPid.addEventListener('click', async () => {
  const mode = pidModeSelect.value;
  const speed = parseInt(pidSpeedSlider.value, 10);
  const kp = parseFloat(pidKpNum.value) || 0.40;
  const kd = pidKdNum ? (parseFloat(pidKdNum.value) || 0.0) : 0.0;
  const val = parseFloat(pidParamInput.value) || 0;
  const waitBtn = pidWaitBtn.checked ? 1 : 0;

  const payload = {
    cmd: 'TEST_PID',
    mode: mode,
    speed: speed,
    kp: kp,
    kd: kd,
    val: val,
    wait_btn: waitBtn
  };

  await sendCommand(payload);
});

// ============================================================================
// 5. 4WD MOTORS & TRIM LAB
// ============================================================================

// Bind 4 Motor Sliders with Numeric Inputs
bindSliderAndNumber('m1-slider', 'm1-num');
bindSliderAndNumber('m2-slider', 'm2-num');
bindSliderAndNumber('m3-slider', 'm3-num');
bindSliderAndNumber('m4-slider', 'm4-num');

// Presets Motion Buttons
document.getElementById('preset-fd').addEventListener('click', () => set4WheelSpeeds(50, 50, 50, 50));
document.getElementById('preset-bk').addEventListener('click', () => set4WheelSpeeds(-50, -50, -50, -50));
document.getElementById('preset-sl').addEventListener('click', () => set4WheelSpeeds(-50, 50, 50, -50));
document.getElementById('preset-sr').addEventListener('click', () => set4WheelSpeeds(50, -50, -50, 50));
document.getElementById('preset-tl').addEventListener('click', () => set4WheelSpeeds(-50, 50, -50, 50));
document.getElementById('preset-tr').addEventListener('click', () => set4WheelSpeeds(50, -50, 50, -50));
document.getElementById('preset-stop').addEventListener('click', () => set4WheelSpeeds(0, 0, 0, 0));

function set4WheelSpeeds(s1, s2, s3, s4) {
  if (m1Slider) { m1Slider.value = s1; if (m1Num) m1Num.value = s1; }
  if (m2Slider) { m2Slider.value = s2; if (m2Num) m2Num.value = s2; }
  if (m3Slider) { m3Slider.value = s3; if (m3Num) m3Num.value = s3; }
  if (m4Slider) { m4Slider.value = s4; if (m4Num) m4Num.value = s4; }
  saveSettingsDebounced();
}

// Send Motor Command
btnSendMotor.addEventListener('click', async () => {
  const m1 = parseInt(m1Slider.value, 10);
  const m2 = parseInt(m2Slider.value, 10);
  const m3 = parseInt(m3Slider.value, 10);
  const m4 = parseInt(m4Slider.value, 10);
  const time = parseInt(motorDurationInput.value, 10) || 1000;
  const waitBtn = motorWaitBtn.checked ? 1 : 0;

  const payload = {
    cmd: 'TEST_MOTOR',
    m1: m1,
    m2: m2,
    m3: m3,
    m4: m4,
    time: time,
    wait_btn: waitBtn
  };

  await sendCommand(payload);
});

// Save Motor Trims
document.getElementById('btn-save-trim').addEventListener('click', async () => {
  const m1Trim = parseInt(document.getElementById('trim-m1').value, 10) || 0;
  const m2Trim = parseInt(document.getElementById('trim-m2').value, 10) || 0;
  const m3Trim = parseInt(document.getElementById('trim-m3').value, 10) || 0;
  const m4Trim = parseInt(document.getElementById('trim-m4').value, 10) || 0;
  const offset = parseInt(document.getElementById('trim-offset').value, 10) || 0;

  const payload = {
    cmd: 'SET_CONFIG',
    m1: m1Trim,
    m2: m2Trim,
    m3: m3Trim,
    m4: m4Trim,
    offset: offset
  };

  await sendCommand(payload);
  saveSettingsDebounced();
});

// ============================================================================
// 6. DUAL SERVO LAB
// ============================================================================

bindSliderAndNumber('s1-slider', 's1-num');
bindSliderAndNumber('s2-slider', 's2-num');

// Servo 1 Presets
document.querySelectorAll('.s1-preset').forEach(btn => {
  btn.addEventListener('click', () => {
    const ang = btn.getAttribute('data-ang');
    if (s1Slider) s1Slider.value = ang;
    if (s1Num) s1Num.value = ang;
    saveSettingsDebounced();
  });
});

// Servo 2 Presets
document.querySelectorAll('.s2-preset').forEach(btn => {
  btn.addEventListener('click', () => {
    const ang = btn.getAttribute('data-ang');
    if (s2Slider) s2Slider.value = ang;
    if (s2Num) s2Num.value = ang;
    saveSettingsDebounced();
  });
});

// Send Servo Command
btnSendServo.addEventListener('click', async () => {
  const s1 = parseInt(s1Slider.value, 10);
  const s2 = parseInt(s2Slider.value, 10);
  const waitBtn = servoWaitBtn.checked ? 1 : 0;

  const payload = {
    cmd: 'SERVO',
    s1: s1,
    s2: s2,
    wait_btn: waitBtn
  };

  await sendCommand(payload);
});

// ============================================================================
// 7. EMERGENCY STOP & TERMINAL
// ============================================================================

btnEmergencyStop.addEventListener('click', async () => {
  await sendCommand({ cmd: 'STOP' });
});

btnSendRaw.addEventListener('click', async () => {
  const text = terminalInput.value.trim();
  if (text) {
    try {
      const obj = JSON.parse(text);
      await sendCommand(obj);
      terminalInput.value = '';
    } catch (e) {
      if (serialPort && serialPort.writable) {
        logTerminal(`[TX-RAW] ${text}`, 'term-tx');
        const w = serialPort.writable.getWriter();
        await w.write(new TextEncoder().encode(text + '\n'));
        w.releaseLock();
        terminalInput.value = '';
      }
    }
  }
});

terminalInput.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') {
    btnSendRaw.click();
  }
});

btnClearTerminal.addEventListener('click', () => {
  terminalOutput.innerHTML = '';
});

function logTerminal(message, className = '') {
  const now = new Date();
  const timeStr = now.toTimeString().split(' ')[0] + '.' + String(now.getMilliseconds()).padStart(3, '0');

  const div = document.createElement('div');
  div.className = `term-line ${className}`;
  div.textContent = `[${timeStr}] ${message}`;

  terminalOutput.appendChild(div);

  if (terminalAutoscroll.checked) {
    terminalOutput.scrollTop = terminalOutput.scrollHeight;
  }

  if (terminalOutput.children.length > 300) {
    terminalOutput.removeChild(terminalOutput.firstChild);
  }
}

// ============================================================================
// 8. PERSISTENCE (LOCALSTORAGE MEMORY ACROSS RELOADS & RESETS)
// ============================================================================

let saveDebounceTimer = null;
function saveSettingsDebounced() {
  clearTimeout(saveDebounceTimer);
  saveDebounceTimer = setTimeout(saveAllSettings, 200);
}

function saveAllSettings() {
  try {
    const settings = {
      baud: baudSelect ? baudSelect.value : '115200',
      pidMode: pidModeSelect ? pidModeSelect.value : 'phase',
      pidSpeed: pidSpeedSlider ? pidSpeedSlider.value : '50',
      pidKp: pidKpSlider ? pidKpSlider.value : '1.8',
      pidKd: pidKdSlider ? pidKdSlider.value : '0.0',
      pidParam: pidParamInput ? pidParamInput.value : '30.0',
      pidWaitBtn: pidWaitBtn ? pidWaitBtn.checked : true,
      m1: m1Slider ? m1Slider.value : '0',
      m2: m2Slider ? m2Slider.value : '0',
      m3: m3Slider ? m3Slider.value : '0',
      m4: m4Slider ? m4Slider.value : '0',
      motorDuration: motorDurationInput ? motorDurationInput.value : '1200',
      motorWaitBtn: motorWaitBtn ? motorWaitBtn.checked : true,
      trimM1: document.getElementById('trim-m1')?.value || '0',
      trimM2: document.getElementById('trim-m2')?.value || '0',
      trimM3: document.getElementById('trim-m3')?.value || '0',
      trimM4: document.getElementById('trim-m4')?.value || '0',
      trimOffset: document.getElementById('trim-offset')?.value || '0',
      s1: s1Slider ? s1Slider.value : '50',
      s2: s2Slider ? s2Slider.value : '146',
      servoWaitBtn: servoWaitBtn ? servoWaitBtn.checked : false,
      terminalAutoscroll: terminalAutoscroll ? terminalAutoscroll.checked : true,
      activeTab: document.querySelector('.tab-btn.active')?.getAttribute('data-tab') || 'tab-pid'
    };
    localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
  } catch (e) {
    console.warn('saveAllSettings error:', e);
  }
}

function loadAllSettings() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) {
      updatePidParamPresets();
      return;
    }
    const s = JSON.parse(raw);

    if (s.baud && baudSelect) baudSelect.value = s.baud;
    if (s.pidMode && pidModeSelect) {
      pidModeSelect.value = s.pidMode;
      updatePidParamPresets();
    } else {
      updatePidParamPresets();
    }

    if (s.pidSpeed !== undefined) {
      if (pidSpeedSlider) pidSpeedSlider.value = s.pidSpeed;
      if (pidSpeedNum) pidSpeedNum.value = s.pidSpeed;
    }
    if (s.pidKp !== undefined) {
      if (pidKpSlider) pidKpSlider.value = s.pidKp;
      if (pidKpNum) pidKpNum.value = s.pidKp;
    }
    if (s.pidKd !== undefined && pidKdSlider) {
      pidKdSlider.value = s.pidKd;
      if (pidKdNum) pidKdNum.value = s.pidKd;
      updateKdAutoBadge(parseFloat(s.pidKd));
    }
    if (s.pidParam !== undefined && pidParamInput) {
      pidParamInput.value = s.pidParam;
    }
    if (s.pidWaitBtn !== undefined && pidWaitBtn) {
      pidWaitBtn.checked = s.pidWaitBtn;
    }

    if (s.m1 !== undefined) { if (m1Slider) m1Slider.value = s.m1; if (m1Num) m1Num.value = s.m1; }
    if (s.m2 !== undefined) { if (m2Slider) m2Slider.value = s.m2; if (m2Num) m2Num.value = s.m2; }
    if (s.m3 !== undefined) { if (m3Slider) m3Slider.value = s.m3; if (m3Num) m3Num.value = s.m3; }
    if (s.m4 !== undefined) { if (m4Slider) m4Slider.value = s.m4; if (m4Num) m4Num.value = s.m4; }
    if (s.motorDuration !== undefined && motorDurationInput) motorDurationInput.value = s.motorDuration;
    if (s.motorWaitBtn !== undefined && motorWaitBtn) motorWaitBtn.checked = s.motorWaitBtn;

    const tm1 = document.getElementById('trim-m1'); if (tm1 && s.trimM1 !== undefined) tm1.value = s.trimM1;
    const tm2 = document.getElementById('trim-m2'); if (tm2 && s.trimM2 !== undefined) tm2.value = s.trimM2;
    const tm3 = document.getElementById('trim-m3'); if (tm3 && s.trimM3 !== undefined) tm3.value = s.trimM3;
    const tm4 = document.getElementById('trim-m4'); if (tm4 && s.trimM4 !== undefined) tm4.value = s.trimM4;
    const toff = document.getElementById('trim-offset'); if (toff && s.trimOffset !== undefined) toff.value = s.trimOffset;

    if (s.s1 !== undefined) { if (s1Slider) s1Slider.value = s.s1; if (s1Num) s1Num.value = s.s1; }
    if (s.s2 !== undefined) { if (s2Slider) s2Slider.value = s.s2; if (s2Num) s2Num.value = s.s2; }
    if (s.servoWaitBtn !== undefined && servoWaitBtn) servoWaitBtn.checked = s.servoWaitBtn;
    if (s.terminalAutoscroll !== undefined && terminalAutoscroll) terminalAutoscroll.checked = s.terminalAutoscroll;

    if (s.activeTab) {
      const tabBtn = document.querySelector(`.tab-btn[data-tab="${s.activeTab}"]`);
      if (tabBtn) tabBtn.click();
    }
  } catch (e) {
    console.warn('loadAllSettings error:', e);
    updatePidParamPresets();
  }
}

// Reset Settings Button
if (btnResetSettings) {
  btnResetSettings.addEventListener('click', () => {
    if (confirm('คุณต้องการล้างค่าการตั้งค่าทั้งหมดและคืนเป็นค่าเริ่มต้นใช่หรือไม่?')) {
      localStorage.removeItem(STORAGE_KEY);
      location.reload();
    }
  });
}

// ============================================================================
// 8. SENSOR CALIBRATION CENTER
// ============================================================================

function updateCalibTable(data) {
  if (data.front_min && data.front_min.length === 7) {
    data.front_min.forEach((v, i) => {
      const el = document.getElementById(`f-min-${i}`);
      if (el) el.textContent = v;
    });
  }
  if (data.front_max && data.front_max.length === 7) {
    data.front_max.forEach((v, i) => {
      const el = document.getElementById(`f-max-${i}`);
      if (el) el.textContent = v;
    });
  }
  if (data.back_min && data.back_min.length === 7) {
    data.back_min.forEach((v, i) => {
      const el = document.getElementById(`b-min-${i}`);
      if (el) el.textContent = v;
    });
  }
  if (data.back_max && data.back_max.length === 7) {
    data.back_max.forEach((v, i) => {
      const el = document.getElementById(`b-max-${i}`);
      if (el) el.textContent = v;
    });
  }
  if (data.ir_5cm !== undefined) {
    const el = document.getElementById('eeprom-ir-5cm');
    if (el) el.textContent = data.ir_5cm;
  }
  if (data.ir_10cm !== undefined) {
    const el = document.getElementById('eeprom-ir-10cm');
    if (el) el.textContent = data.ir_10cm;
  }
  const msgEl = document.getElementById('calib-status-msg');
  if (msgEl) {
    const now = new Date();
    const timeStr = now.toTimeString().split(' ')[0];
    msgEl.innerHTML = `<span style="color:#10b981;">✓ อัปเดตข้อมูลจาก EEPROM ล่าสุดเมื่อ ${timeStr}</span>`;
  }
  logTerminal('[EEPROM] อัปเดตตารางค่าคาลิเบรตล่าสุดสำเร็จ', 'term-rx');
}

const btnGetCalib = document.getElementById('btn-get-calib');
const btnCalibFront = document.getElementById('btn-calib-front');
const btnCalibBack = document.getElementById('btn-calib-back');
const btnCalibColor = document.getElementById('btn-calib-color');
const btnCalibIr = document.getElementById('btn-calib-ir');
const btnCalibMenu = document.getElementById('btn-calib-menu');

if (btnGetCalib) {
  btnGetCalib.addEventListener('click', async () => {
    await sendCommand({ cmd: 'GET_CALIB' });
  });
}

if (btnCalibFront) {
  btnCalibFront.addEventListener('click', async () => {
    if (confirm('ต้องการเริ่มคาลิเบรต "เซนเซอร์แสงหน้า 7 ตัว" ใช่หรือไม่?\n\nกรุณาเตรียมวางหุ่นยนต์บนพื้นขาวและเส้นดำตามขั้นตอนบนหน้าจอหุ่นยนต์')) {
      await sendCommand({ cmd: 'CALIB', target: 'front' });
    }
  });
}

if (btnCalibBack) {
  btnCalibBack.addEventListener('click', async () => {
    if (confirm('ต้องการเริ่มคาลิเบรต "เซนเซอร์แสงหลัง 7 ตัว" ใช่หรือไม่?\n\nกรุณาเตรียมวางหุ่นยนต์บนพื้นขาวและเส้นดำตามขั้นตอนบนหน้าจอหุ่นยนต์')) {
      await sendCommand({ cmd: 'CALIB', target: 'back' });
    }
  });
}

if (btnCalibColor) {
  btnCalibColor.addEventListener('click', async () => {
    if (confirm('ต้องการเริ่มคาลิเบรต "เซนเซอร์สี TCS34725" ใช่หรือไม่?\n\nกรุณาเตรียมแผ่นสี: แดง (RED), เขียว (GREEN), น้ำเงิน (BLUE), เหลือง (YELLOW)')) {
      await sendCommand({ cmd: 'CALIB', target: 'color' });
    }
  });
}

if (btnCalibIr) {
  btnCalibIr.addEventListener('click', async () => {
    if (confirm('ต้องการเริ่มคาลิเบรต "เซนเซอร์วัดระยะ Sharp IR" ใช่หรือไม่?\n\nกรุณาเตรียมวัตถุขวางที่ระยะ 5 ซม. และ 10 ซม.')) {
      await sendCommand({ cmd: 'CALIB', target: 'ir' });
    }
  });
}

if (btnCalibMenu) {
  btnCalibMenu.addEventListener('click', async () => {
    await sendCommand({ cmd: 'CALIB', target: 'menu' });
  });
}

// Inputs change event listeners for saving
[pidParamInput, pidWaitBtn, motorDurationInput, motorWaitBtn, servoWaitBtn, terminalAutoscroll, baudSelect].forEach(el => {
  if (el) {
    el.addEventListener('change', saveSettingsDebounced);
  }
});

['trim-m1', 'trim-m2', 'trim-m3', 'trim-m4', 'trim-offset'].forEach(id => {
  const el = document.getElementById(id);
  if (el) el.addEventListener('input', saveSettingsDebounced);
});

// ============================================================================
// 9. INITIALIZATION ON PAGE LOAD
// ============================================================================

window.addEventListener('DOMContentLoaded', async () => {
  // 1. Restore all saved input parameters
  loadAllSettings();

  // 2. Check Auto-Reconnect status
  if (isAutoReconnectActive && 'serial' in navigator) {
    updateAutoReconnectUI();
    try {
      const ports = await navigator.serial.getPorts();
      if (ports.length > 0) {
        const baudRate = parseInt(baudSelect.value, 10);
        logTerminal('[SYSTEM] ตรวจพบการจับคู่บอร์ดเดิม กำลังเชื่อมต่ออัตโนมัติ...', 'term-system');
        await openAndStartPort(ports[0], baudRate);
      } else {
        logTerminal('[SYSTEM] ⚡ โหมดเชื่อมต่ออัตโนมัติเปิดอยู่ &mdash; รอเสียบสาย USB...', 'term-system');
      }
    } catch (err) {
      console.warn('Initial auto-connect attempt failed:', err);
    }
  }
});
