/**
 * PUPPYBOT6 PRO MISSION CONTROL & DIAGNOSTICS LAB
 * Web Serial API Controller & Real-Time Telemetry Streamer
 */

// Global State
let serialPort = null;
let reader = null;
let writer = null;
let keepReading = false;
let inputBuffer = '';

// DOM Elements
const baudSelect = document.getElementById('baud-select');
const btnConnect = document.getElementById('btn-connect');
const btnDisconnect = document.getElementById('btn-disconnect');
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

// ============================================================================
// 1. SERIAL CONNECTION & STREAM HANDLING
// ============================================================================

// Check Web Serial API Support
if (!('serial' in navigator)) {
  alert('เบราว์เซอร์ของคุณไม่รองรับ Web Serial API!\nกรุณาใช้งานบน Google Chrome, Microsoft Edge, หรือ Opera บนคอมพิวเตอร์');
  logTerminal('[ERROR] Web Serial API not supported in this browser. Please use Chrome or Edge.', 'term-err');
}

btnConnect.addEventListener('click', async () => {
  try {
    const baudRate = parseInt(baudSelect.value, 10);
    logTerminal(`[SYSTEM] Requesting Serial Port access at ${baudRate} baud...`, 'term-system');

    serialPort = await navigator.serial.requestPort();
    await serialPort.open({ baudRate: baudRate });

    keepReading = true;
    updateConnectionUI(true);
    logTerminal(`[SYSTEM] Connected to Serial Port successfully!`, 'term-rx');

    // เริ่มต้นอ่านข้อมูลเข้า
    readSerialLoop();

    // ส่ง PING ตรวจสอบสถานะหุ่น
    sendCommand({ cmd: 'PING' });
  } catch (err) {
    console.error('Serial connection error:', err);
    logTerminal(`[ERROR] Connection failed: ${err.message}`, 'term-err');
    updateConnectionUI(false);
  }
});

btnDisconnect.addEventListener('click', async () => {
  await disconnectSerial();
});

async function disconnectSerial() {
  keepReading = false;
  if (reader) {
    try {
      await reader.cancel();
      reader.releaseLock();
    } catch (e) {}
    reader = null;
  }
  if (writer) {
    try {
      writer.releaseLock();
    } catch (e) {}
    writer = null;
  }
  if (serialPort) {
    try {
      await serialPort.close();
    } catch (e) {}
    serialPort = null;
  }
  updateConnectionUI(false);
  logTerminal('[SYSTEM] Disconnected from Serial Port.', 'term-system');
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

// วนลูปอ่านข้อมูลทีละชิ้นจาก Serial Port
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
        console.error('Read error:', err);
        logTerminal(`[ERROR] Read stream error: ${err.message}`, 'term-err');
      }
    } finally {
      if (reader) {
        reader.releaseLock();
        reader = null;
      }
    }
  }
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
  // บันทึกเฉพาะข้อมูลที่ไม่ใช่ TELEMETRY ถี่ๆ ลงหน้าต่าง terminal เพื่อไม่ให้รก
  if (!line.includes('"type":"TELEM"')) {
    logTerminal(`[RX] ${line}`, 'term-rx');
  }

  // พยายามแยกวิเคราะห์ JSON
  try {
    if (line.startsWith('{') && line.endsWith('}')) {
      const data = JSON.parse(line);
      processJsonPacket(data);
    }
  } catch (err) {
    // ไม่ใช่ JSON หรือ parsing ล้มเหลว แสดงเป็นข้อความธรรมดา
  }
}

// ส่งข้อความ JSON ไปยังหุ่นยนต์
async function sendCommand(obj) {
  if (!serialPort || !serialPort.writable) {
    logTerminal('[WARN] Serial port not connected! Cannot send command.', 'term-warn');
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
      writer.releaseLock();
      writer = null;
    }
    return false;
  }
}

// ============================================================================
// 2. JSON PACKET DISPATCHER & TELEMETRY UPDATES
// ============================================================================

function processJsonPacket(data) {
  // 1. แพ็กเก็ตข้อมูล Telemetry จากเซนเซอร์
  if (data.type === 'TELEM') {
    updateTelemetrySensors(data);
    return;
  }

  // 2. สถานะการทำงานของหุ่น (STATUS)
  if (data.type === 'STATUS') {
    if (data.state === 'WAIT_BTN') {
      setRobotState('WAIT_BTN', data.msg);
    } else if (data.state === 'RUNNING') {
      setRobotState('RUNNING', data.msg);
    }
    return;
  }

  // 3. ผลการตอบรับคำสั่ง (RES)
  if (data.type === 'RES') {
    if (data.status === 'DONE') {
      setRobotState('DONE', data.msg);
    } else if (data.status === 'STOPPED') {
      setRobotState('STOPPED', data.msg);
    } else if (data.status === 'CONFIG_SAVED') {
      logTerminal(`[CONFIG] ${data.msg}`, 'term-rx');
    }
    return;
  }

  // 4. ข้อความ PONG
  if (data.type === 'PONG') {
    setRobotState('IDLE', 'PuppyBot6 Online & Ready');
  }
}

function setRobotState(state, message = '') {
  const pidExec = document.getElementById('test-exec-status');
  const motorExec = document.getElementById('motor-exec-status');
  const servoExec = document.getElementById('servo-exec-status');

  if (state === 'WAIT_BTN') {
    robotStatePill.className = 'status-pill status-waiting-switch';
    robotStateText.textContent = 'ROBOT: WAITING SWITCH ⏳';
    instructionStepText.innerHTML = '<strong>⚠️ หุ่นยนต์กำลังรอกดสวิตช์:</strong> กรุณากดสวิตช์ (ปุ่มบนบอร์ด หรือ ADC15) เพื่อเริ่มวิ่งตามที่ตั้งค่าไว้!';
    if (pidExec) pidExec.innerHTML = `<span style="color:#fbbf24;">⏳ หุ่นยนต์กำลังรอกดสวิตช์ที่ตัวหุ่น... (${message})</span>`;
    if (motorExec) motorExec.innerHTML = `<span style="color:#fbbf24;">⏳ รอกดสวิตช์เพื่อหมุนมอเตอร์...</span>`;
    if (servoExec) servoExec.innerHTML = `<span style="color:#fbbf24;">⏳ รอกดสวิตช์เพื่อหมุนเซอร์โว...</span>`;
  } else if (state === 'RUNNING') {
    robotStatePill.className = 'status-pill status-running';
    robotStateText.textContent = 'ROBOT: RUNNING ⚡';
    instructionStepText.innerHTML = '<strong>▶️ หุ่นยนต์กำลังทำงานตามคำสั่ง...</strong> รอจนกว่าจะเสร็จสิ้นภารกิจ';
    if (pidExec) pidExec.innerHTML = `<span style="color:#00f0ff;">⚡ กำลังวิ่งทดสอบ PID... (${message})</span>`;
    if (motorExec) motorExec.innerHTML = `<span style="color:#00f0ff;">⚡ มอเตอร์กำลังหมุน...</span>`;
    if (servoExec) servoExec.innerHTML = `<span style="color:#00f0ff;">⚡ เซอร์โวกำลังหมุน...</span>`;
  } else if (state === 'DONE') {
    robotStatePill.className = 'status-pill status-connected';
    robotStateText.textContent = 'ROBOT: COMPLETED ✅';
    instructionStepText.innerHTML = '<strong>✅ ภารกิจเสร็จสิ้น!</strong> หุ่นยนต์หยุดนิ่งและรอรับคำสั่งถัดไปจากหน้าเว็บต่อทันที';
    if (pidExec) pidExec.innerHTML = `<span style="color:#34d399;">✅ ทดสอบ PID สำเร็จเรียบร้อย!</span>`;
    if (motorExec) motorExec.innerHTML = `<span style="color:#34d399;">✅ ทดสอบมอเตอร์ 4 ล้อเรียบร้อย!</span>`;
    if (servoExec) servoExec.innerHTML = `<span style="color:#34d399;">✅ ตั้งมุมเซอร์โวเรียบร้อย!</span>`;
    setTimeout(() => {
      setRobotState('IDLE');
    }, 3000);
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
    instructionStepText.innerHTML = '1. ตั้งค่าการทดสอบด้านล่าง &rarr; 2. กดปุ่มส่งคำสั่ง &rarr; 3. หุ่นยนต์จะส่งเสียงปี๊บและรอกดสวิตช์ &rarr; 4. กดสวิตช์ที่ตัวหุ่นยนต์เพื่อเริ่มวิ่ง!';
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


  // 3. เซนเซอร์เส้นหน้า 7 ตัว (ADC 0..6)
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

  // 4. เซนเซอร์เส้นหลัง 7 ตัว (ADC 7..13)
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

  // 5. ระยะห่าง IR
  if (data.ir !== undefined) {
    valIr.textContent = data.ir.toFixed(1);
    const irPct = Math.min(100, (data.ir / 30) * 100);
    meterIr.style.width = `${irPct}%`;
  }

  // 6. ระยะทาง Encoder
  if (data.enc !== undefined) {
    valEnc.textContent = data.enc.toFixed(0);
  }

  // 7. องศาไจโร IMU Yaw
  if (data.yaw !== undefined) {
    valYaw.textContent = data.yaw.toFixed(1);
    compassNeedle.style.transform = `rotate(${data.yaw}deg)`;
  }

  // 8. สวิตช์
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
  });
});

// ============================================================================
// 4. PID & LINE FOLLOW LAB CONTROLS
// ============================================================================
const pidModeSelect = document.getElementById('pid-mode-select');
const pidSpeedSlider = document.getElementById('pid-speed-slider');
const pidSpeedVal = document.getElementById('pid-speed-val');
const pidKpSlider = document.getElementById('pid-kp-slider');
const pidKpVal = document.getElementById('pid-kp-val');
const pidParamInput = document.getElementById('pid-param-input');
const paramLabel = document.getElementById('param-label');
const presetButtons = document.getElementById('preset-buttons');
const pidWaitBtn = document.getElementById('pid-wait-btn');
const btnSendPid = document.getElementById('btn-send-pid');

// Speed Slider
pidSpeedSlider.addEventListener('input', (e) => {
  pidSpeedVal.textContent = `${e.target.value}%`;
});

// KP Slider
pidKpSlider.addEventListener('input', (e) => {
  pidKpVal.textContent = parseFloat(e.target.value).toFixed(2);
});

// KD Slider
const pidKdSlider = document.getElementById('pid-kd-slider');
const pidKdVal = document.getElementById('pid-kd-val');

if (pidKdSlider) {
  pidKdSlider.addEventListener('input', (e) => {
    const val = parseFloat(e.target.value);
    pidKdVal.textContent = (val === 0) ? '0.0 (Auto)' : val.toFixed(1);
  });

  document.querySelectorAll('#kd-preset-buttons .btn-chip').forEach(btn => {
    btn.addEventListener('click', () => {
      const kd = parseFloat(btn.getAttribute('data-kd'));
      pidKdSlider.value = kd;
      pidKdVal.textContent = (kd === 0) ? '0.0 (Auto)' : kd.toFixed(1);
      document.querySelectorAll('#kd-preset-buttons .btn-chip').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
    });
  });
}

// Dynamic Parameter Label & Presets based on Mode
pidModeSelect.addEventListener('change', updatePidParamPresets);

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
  });
  presetButtons.appendChild(chip);
}

// Initial Call
updatePidParamPresets();

// Send PID Command
btnSendPid.addEventListener('click', async () => {
  const mode = pidModeSelect.value;
  const speed = parseInt(pidSpeedSlider.value, 10);
  const kp = parseFloat(pidKpSlider.value);
  const kd = pidKdSlider ? parseFloat(pidKdSlider.value) : 0.0;
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
// 5. 4WD INDEPENDENT MOTOR & TRIM LAB
// ============================================================================
const m1Slider = document.getElementById('m1-slider');
const m2Slider = document.getElementById('m2-slider');
const m3Slider = document.getElementById('m3-slider');
const m4Slider = document.getElementById('m4-slider');
const m1Val = document.getElementById('m1-val');
const m2Val = document.getElementById('m2-val');
const m3Val = document.getElementById('m3-val');
const m4Val = document.getElementById('m4-val');

const motorDurationInput = document.getElementById('motor-duration-input');
const motorWaitBtn = document.getElementById('motor-wait-btn');
const btnSendMotor = document.getElementById('btn-send-motor');

// Sliders Live Update
[m1Slider, m2Slider, m3Slider, m4Slider].forEach((slider, idx) => {
  slider.addEventListener('input', () => {
    const badges = [m1Val, m2Val, m3Val, m4Val];
    badges[idx].textContent = slider.value;
  });
});

// Presets Motion Buttons
document.getElementById('preset-fd').addEventListener('click', () => set4WheelSpeeds(50, 50, 50, 50));
document.getElementById('preset-bk').addEventListener('click', () => set4WheelSpeeds(-50, -50, -50, -50));
document.getElementById('preset-sl').addEventListener('click', () => set4WheelSpeeds(-50, 50, 50, -50));
document.getElementById('preset-sr').addEventListener('click', () => set4WheelSpeeds(50, -50, -50, 50));
document.getElementById('preset-tl').addEventListener('click', () => set4WheelSpeeds(-50, 50, -50, 50));
document.getElementById('preset-tr').addEventListener('click', () => set4WheelSpeeds(50, -50, 50, -50));
document.getElementById('preset-stop').addEventListener('click', () => set4WheelSpeeds(0, 0, 0, 0));

function set4WheelSpeeds(s1, s2, s3, s4) {
  m1Slider.value = s1; m1Val.textContent = s1;
  m2Slider.value = s2; m2Val.textContent = s2;
  m3Slider.value = s3; m3Val.textContent = s3;
  m4Slider.value = s4; m4Val.textContent = s4;
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
});

// ============================================================================
// 6. DUAL SERVO LAB CONTROLS
// ============================================================================
const s1Slider = document.getElementById('s1-slider');
const s2Slider = document.getElementById('s2-slider');
const s1Badge = document.getElementById('s1-badge');
const s2Badge = document.getElementById('s2-badge');
const servoWaitBtn = document.getElementById('servo-wait-btn');
const btnSendServo = document.getElementById('btn-send-servo');

s1Slider.addEventListener('input', (e) => {
  s1Badge.textContent = `${e.target.value}°`;
});

s2Slider.addEventListener('input', (e) => {
  s2Badge.textContent = `${e.target.value}°`;
});

// Servo 1 Presets
document.querySelectorAll('.s1-preset').forEach(btn => {
  btn.addEventListener('click', () => {
    const ang = btn.getAttribute('data-ang');
    s1Slider.value = ang;
    s1Badge.textContent = `${ang}°`;
  });
});

// Servo 2 Presets
document.querySelectorAll('.s2-preset').forEach(btn => {
  btn.addEventListener('click', () => {
    const ang = btn.getAttribute('data-ang');
    s2Slider.value = ang;
    s2Badge.textContent = `${ang}°`;
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
      // ไม่ใช่ json ส่งเป็นสตริง
      if (writer || (serialPort && serialPort.writable)) {
        logTerminal(`[TX-RAW] ${text}`, 'term-tx');
        const writer = serialPort.writable.getWriter();
        await writer.write(new TextEncoder().encode(text + '\n'));
        writer.releaseLock();
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

  // ควบคุม Auto-scroll
  if (terminalAutoscroll.checked) {
    terminalOutput.scrollTop = terminalOutput.scrollHeight;
  }

  // ป้องกัน memory leak ตัดข้อความเก่าออกหากเกิน 300 บรรทัด
  if (terminalOutput.children.length > 300) {
    terminalOutput.removeChild(terminalOutput.firstChild);
  }
}
