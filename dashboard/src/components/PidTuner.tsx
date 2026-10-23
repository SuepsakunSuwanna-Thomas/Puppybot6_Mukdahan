import React, { useState, useEffect } from 'react';
import { serial } from '../services/serialService';
import { RobotState } from '../types';
import { 
  Sliders, 
  Play, 
  Sparkles, 
  Zap, 
  RotateCcw, 
  RotateCw, 
  Plus, 
  Minus, 
  Check, 
  ArrowRight,
  ShieldCheck,
  Flame,
  CornerDownRight
} from 'lucide-react';

interface PidTunerProps {
  robotState: RobotState;
  isConnected: boolean;
}

export const PidTuner: React.FC<PidTunerProps> = ({ robotState, isConnected }) => {
  const [mode, setMode] = useState<string>('phase');
  const [speed, setSpeed] = useState<number>(50);
  const [kp, setKp] = useState<number>(0.40);
  const [kd, setKd] = useState<number>(-1.00); // -1 = Auto
  const [val, setVal] = useState<number>(30.0);
  const [waitBtn, setWaitBtn] = useState<boolean>(true);

  // Load saved settings
  useEffect(() => {
    const saved = localStorage.getItem('puppybot_pid_settings_v2');
    if (saved) {
      try {
        const s = JSON.parse(saved);
        if (s.mode) setMode(s.mode);
        if (s.speed !== undefined) setSpeed(s.speed);
        if (s.kp !== undefined) setKp(s.kp);
        if (s.kd !== undefined) setKd(s.kd);
        if (s.val !== undefined) setVal(s.val);
        if (s.waitBtn !== undefined) setWaitBtn(s.waitBtn);
      } catch {}
    }
  }, []);

  // Save settings on change
  useEffect(() => {
    localStorage.setItem('puppybot_pid_settings_v2', JSON.stringify({
      mode, speed, kp, kd, val, waitBtn
    }));
  }, [mode, speed, kp, kd, val, waitBtn]);

  // Adjusters
  const adjustKp = (delta: number) => {
    setKp(prev => Math.max(0.00, Math.min(3.00, parseFloat((prev + delta).toFixed(2)))));
  };

  const adjustKd = (delta: number) => {
    setKd(prev => {
      if (prev < 0) return 1.00; // if auto, start from 1.0
      return Math.max(0.00, Math.min(30.00, parseFloat((prev + delta).toFixed(2))));
    });
  };

  const adjustSpeed = (delta: number) => {
    setSpeed(prev => Math.max(10, Math.min(100, prev + delta)));
  };

  const adjustVal = (delta: number) => {
    setVal(prev => Math.max(0, parseFloat((prev + delta).toFixed(1))));
  };

  // Quick Profiles
  const applyPreset = (presetSpeed: number, presetKp: number, presetKd: number, defaultVal: number = 30.0) => {
    setSpeed(presetSpeed);
    setKp(presetKp);
    setKd(presetKd);
    setVal(defaultVal);
  };

  // Dynamic Parameter Label & Presets based on mode
  const getParamInfo = () => {
    if (mode === 'phase' || mode === 'phase_b' || mode === 'follow_notline_enc') {
      return {
        label: 'ระยะทางตามเส้น (เซนติเมตร cm):',
        unit: 'cm',
        step: 0.5,
        presets: [15, 30, 50, 80],
        presetsLabels: ['15 cm', '30 cm', '50 cm', '80 cm'],
        disabled: false
      };
    } else if (mode === 'pass' || mode === 'pass_b') {
      return {
        label: 'จำนวนเส้นตัดที่ต้องการวิ่งผ่าน (เส้น):',
        unit: 'เส้น',
        step: 1,
        presets: [1, 2, 3, 5],
        presetsLabels: ['1 เส้น', '2 เส้น', '3 เส้น', '5 เส้น'],
        disabled: false
      };
    } else if (mode === 'time' || mode === 'follow_notline' || mode === 'backward_notline') {
      return {
        label: 'ระยะเวลาการวิ่ง (มิลลิวินาที ms):',
        unit: 'ms',
        step: 100,
        presets: [500, 1000, 1500, 2500],
        presetsLabels: ['500 ms', '1000 ms', '1500 ms', '2500 ms'],
        disabled: false
      };
    } else {
      return {
        label: 'พารามิเตอร์เพิ่มเติม (ไม่ต้องระบุค่า):',
        unit: '-',
        step: 1,
        presets: [0],
        presetsLabels: ['Auto (หมุนจนเจอเส้น)'],
        disabled: true
      };
    }
  };

  const paramInfo = getParamInfo();

  const handleSendTest = async () => {
    if (!isConnected) {
      alert('กรุณาเชื่อมต่อ COM Port ก่อนส่งคำสั่ง!');
      return;
    }

    await serial.sendCommand({
      cmd: 'TEST_PID',
      mode,
      speed,
      kp,
      kd,
      val,
      wait_btn: waitBtn ? 1 : 0
    });
  };

  return (
    <div className="glass-panel rounded-xl p-5 border border-slate-800 space-y-5">
      {/* Header */}
      <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-2 border-b border-slate-800 pb-3">
        <div>
          <h3 className="text-base font-bold text-white flex items-center gap-2">
            <Sliders className="w-4 h-4 text-cyan-400" />
            <span>ทดสอบการเคลื่อนที่และตามเส้นแบบ PID (Line Following Test)</span>
          </h3>
          <p className="text-xs text-slate-400 mt-0.5">
            ปรับตั้งค่าความเร็ว, Line KP, Line KD ได้ทั้งแบบสไลเดอร์และพิมพ์ตัวเลขกรอกเองได้ละเอียดทุกทศนิยม
          </p>
        </div>
        <span className="text-xs px-2.5 py-1 rounded-full font-medium bg-cyan-950/40 text-cyan-300 border border-cyan-800/40 font-mono">
          {waitBtn ? '⏳ รอกดสวิตช์ที่ตัวหุ่น' : '⚡ วิ่งทันทีเมื่อส่งคำสั่ง'}
        </span>
      </div>

      {/* 4 Quick Profiles */}
      <div>
        <div className="flex items-center gap-1.5 text-xs text-slate-400 mb-2">
          <Sparkles className="w-3.5 h-3.5 text-cyan-400" />
          <span>พรีเซ็ตแนะนำตามสภาพสนาม (Quick Profiles):</span>
        </div>
        <div className="grid grid-cols-2 sm:grid-cols-4 gap-2.5">
          <button
            type="button"
            onClick={() => applyPreset(40, 0.55, -1.00, 30.0)}
            className="p-3 rounded-xl border border-slate-800 hover:border-emerald-500/50 bg-slate-900/60 hover:bg-slate-900 text-left transition-all group"
          >
            <div className="text-xs font-bold text-slate-200 group-hover:text-emerald-300 flex items-center justify-between">
              <span className="flex items-center gap-1"><ShieldCheck className="w-3.5 h-3.5 text-emerald-400" /> ปลอดภัย 100%</span>
              <span className="text-[10px] text-slate-400 font-mono">40%</span>
            </div>
            <p className="text-[10px] text-slate-400 mt-1 font-mono">
              Spd 40 &bull; Kp 0.55 &bull; Kd Auto
            </p>
          </button>

          <button
            type="button"
            onClick={() => applyPreset(50, 0.40, -1.00, 30.0)}
            className="p-3 rounded-xl border border-slate-800 hover:border-cyan-500/50 bg-slate-900/60 hover:bg-slate-900 text-left transition-all group"
          >
            <div className="text-xs font-bold text-slate-200 group-hover:text-cyan-300 flex items-center justify-between">
              <span className="flex items-center gap-1"><Zap className="w-3.5 h-3.5 text-cyan-400" /> มาตรฐานแข่งขัน</span>
              <span className="text-[10px] text-cyan-400 font-mono font-bold">50%</span>
            </div>
            <p className="text-[10px] text-slate-400 mt-1 font-mono">
              Spd 50 &bull; Kp 0.40 &bull; Kd Auto
            </p>
          </button>

          <button
            type="button"
            onClick={() => applyPreset(80, 0.32, -1.00, 50.0)}
            className="p-3 rounded-xl border border-slate-800 hover:border-amber-500/50 bg-slate-900/60 hover:bg-slate-900 text-left transition-all group"
          >
            <div className="text-xs font-bold text-slate-200 group-hover:text-amber-300 flex items-center justify-between">
              <span className="flex items-center gap-1"><Flame className="w-3.5 h-3.5 text-amber-400" /> ทางตรง Speed Sprint</span>
              <span className="text-[10px] text-amber-400 font-mono font-bold">80%</span>
            </div>
            <p className="text-[10px] text-slate-400 mt-1 font-mono">
              Spd 80 &bull; Kp 0.32 &bull; Kd Auto
            </p>
          </button>

          <button
            type="button"
            onClick={() => applyPreset(45, 0.50, 10.50, 20.0)}
            className="p-3 rounded-xl border border-slate-800 hover:border-indigo-500/50 bg-slate-900/60 hover:bg-slate-900 text-left transition-all group"
          >
            <div className="text-xs font-bold text-slate-200 group-hover:text-indigo-300 flex items-center justify-between">
              <span className="flex items-center gap-1"><CornerDownRight className="w-3.5 h-3.5 text-indigo-400" /> ทางโค้งหักศอก</span>
              <span className="text-[10px] text-indigo-400 font-mono font-bold">45%</span>
            </div>
            <p className="text-[10px] text-slate-400 mt-1 font-mono">
              Spd 45 &bull; Kp 0.50 &bull; Kd 10.50
            </p>
          </button>
        </div>
      </div>

      {/* Mode Selection */}
      <div className="space-y-1.5">
        <label className="text-xs font-semibold text-slate-300">
          เลือกฟังก์ชันการเคลื่อนที่ (Motion Function):
        </label>
        <select
          value={mode}
          onChange={(e) => {
            const newMode = e.target.value;
            setMode(newMode);
            if (newMode === 'phase' || newMode === 'phase_b') setVal(30.0);
            else if (newMode === 'pass' || newMode === 'pass_b') setVal(2);
            else if (newMode === 'time' || newMode === 'follow_notline' || newMode === 'backward_notline') setVal(1500);
            else setVal(0);
          }}
          className="w-full bg-slate-900 border border-slate-800 rounded-xl px-3.5 py-2.5 text-xs text-slate-200 outline-none font-medium focus:border-cyan-500"
        >
          <optgroup label="เดินหน้าตามเส้น (Forward Line Follow)">
            <option value="phase">Fline_by_phase - วิ่งตามเส้นตามระยะทาง (ซม. cm)</option>
            <option value="pass">Fline_by_pass - วิ่งตามเส้นผ่านเส้นตัด (จำนวนเส้น)</option>
            <option value="time">Fline_by_time - วิ่งตามเส้นตามเวลา (มิลลิวินาที ms)</option>
            <option value="turnLeft">turnLeft - เลี้ยวซ้ายที่แยก (หมุนซ้ายทันที)</option>
            <option value="turnRight">turnRight - เลี้ยวขวาที่แยก (หมุนขวาทันที)</option>
            <option value="turnLeft_by_stop">turnLeft_by_stop - เลี้ยวซ้ายแบบหยุดนิ่งก่อนเลี้ยว</option>
            <option value="turnRight_by_stop">turnRight_by_stop - เลี้ยวขวาแบบหยุดนิ่งก่อนเลี้ยว</option>
            <option value="checkLine">checkLine - จัดเซนเซอร์หน้า (A3) ให้ตรงกึ่งกลางเส้น</option>
          </optgroup>
          <optgroup label="ถอยหลังตามเส้น (Backward Line Follow)">
            <option value="phase_b">Fline_by_phase_B - ถอยหลังตามเส้นตามระยะ (ซม. cm)</option>
            <option value="pass_b">Fline_by_pass_B - ถอยหลังตามเส้นผ่านเส้นตัด (จำนวนเส้น)</option>
            <option value="turnLeft_b">turnLeft_B - ถอยหลังเลี้ยวซ้าย</option>
            <option value="turnRight_b">turnRight_B - ถอยหลังเลี้ยวขวา</option>
            <option value="checkLine_b">checkLine_B - จัดเซนเซอร์หลัง (A10) ให้ตรงกึ่งกลางเส้น</option>
          </optgroup>
          <optgroup label="วิ่งตรงแบบไม่ใช้ PID (Not-Line)">
            <option value="follow_notline">Follow_Notline - วิ่งตรงไม่ดูเส้นตามเวลา (ms)</option>
            <option value="follow_notline_enc">Follow_Notline_by_Encoder - วิ่งตรงไม่ดูเส้นตามระยะ Encoder (cm)</option>
            <option value="backward_notline">Backword_Notline - ถอยหลังตรงไม่ดูเส้นตามเวลา (ms)</option>
          </optgroup>
        </select>
      </div>

      {/* Main Parameters Grid with Direct Typing & Fine-tuning Buttons */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {/* Speed */}
        <div className="bg-slate-900/60 p-3.5 rounded-xl border border-slate-800 space-y-2">
          <div className="flex items-center justify-between text-xs">
            <span className="font-semibold text-slate-300">ความเร็ว (Speed):</span>
            <div className="flex items-center gap-1.5">
              <input
                type="number"
                min="10"
                max="100"
                value={speed}
                onChange={(e) => setSpeed(Math.max(10, Math.min(100, parseInt(e.target.value) || 10)))}
                className="w-16 bg-slate-950 border border-slate-700 rounded px-2 py-0.5 text-cyan-400 font-mono font-bold text-right outline-none focus:border-cyan-500"
              />
              <span className="text-slate-400 font-mono">%</span>
            </div>
          </div>
          <input
            type="range"
            min="10"
            max="100"
            value={speed}
            onChange={(e) => setSpeed(Number(e.target.value))}
            className="w-full h-1.5 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-cyan-400"
          />
          <div className="flex items-center justify-between text-[11px]">
            <div className="flex items-center gap-1">
              <button onClick={() => adjustSpeed(-5)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono">-5</button>
              <button onClick={() => adjustSpeed(-1)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono">-1</button>
              <button onClick={() => adjustSpeed(+1)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono">+1</button>
              <button onClick={() => adjustSpeed(+5)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono">+5</button>
            </div>
            <span className="text-slate-500 font-mono text-[10px]">10% - 100%</span>
          </div>
        </div>

        {/* Line KP */}
        <div className="bg-slate-900/60 p-3.5 rounded-xl border border-slate-800 space-y-2">
          <div className="flex items-center justify-between text-xs">
            <span className="font-semibold text-slate-300">Line KP (การตอบสนองเลี้ยว &mdash; มาตรฐาน 0.40):</span>
            <div className="flex items-center gap-1.5">
              <input
                type="number"
                min="0.00"
                max="3.00"
                step="0.01"
                value={kp}
                onChange={(e) => setKp(parseFloat(e.target.value) || 0)}
                className="w-20 bg-slate-950 border border-slate-700 rounded px-2 py-0.5 text-indigo-300 font-mono font-bold text-right outline-none focus:border-indigo-500"
              />
            </div>
          </div>
          <input
            type="range"
            min="0.00"
            max="3.00"
            step="0.01"
            value={kp}
            onChange={(e) => setKp(parseFloat(e.target.value))}
            className="w-full h-1.5 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-400"
          />
          <div className="flex items-center justify-between text-[11px]">
            <div className="flex items-center gap-1">
              <button onClick={() => adjustKp(-0.05)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono">-0.05</button>
              <button onClick={() => adjustKp(-0.01)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono">-0.01</button>
              <button onClick={() => adjustKp(+0.01)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono">+0.01</button>
              <button onClick={() => adjustKp(+0.05)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono">+0.05</button>
            </div>
            <span className="text-slate-500 font-mono text-[10px]">0.00 - 3.00</span>
          </div>
        </div>
      </div>

      {/* Line KD Slider, Direct Input, and Preset Chips */}
      <div className="bg-slate-900/60 p-3.5 rounded-xl border border-slate-800 space-y-2">
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-2 text-xs">
          <div className="flex flex-wrap items-center gap-2">
            <span className="font-semibold text-slate-300">
              Line KD (ลดการสะบัด/เพิ่มความนิ่ง &mdash; Auto หรือ 5.50 - 14.50):
            </span>
            {kd < 0 ? (
              <span className="px-2 py-0.5 rounded text-[11px] font-mono font-bold bg-cyan-950/40 text-cyan-400 border border-cyan-800/40">
                โหมด Auto (คำนวณตาม Kp)
              </span>
            ) : kd === 0 ? (
              <span className="px-2 py-0.5 rounded text-[11px] font-mono font-bold bg-slate-800 text-slate-400 border border-slate-700">
                0.00 (ปิดระบบ D)
              </span>
            ) : (
              <span className="px-2 py-0.5 rounded text-[11px] font-mono font-bold bg-emerald-950/50 text-emerald-300 border border-emerald-500/50 flex items-center gap-1">
                <span>⚡ ใช้งานค่าที่กรอกเอง:</span>
                <strong className="text-white">{kd.toFixed(2)}</strong>
                <span>(หุ่นวิ่งด้วยค่านี้)</span>
              </span>
            )}
          </div>
          <div className="flex items-center gap-1.5">
            <span className="text-slate-400 text-[11px]">พิมพ์ค่าเอง:</span>
            <input
              type="number"
              min="-1.00"
              max="30.00"
              step="0.01"
              value={kd}
              onChange={(e) => setKd(parseFloat(e.target.value) || 0)}
              className="w-20 bg-slate-950 border border-slate-700 rounded px-2 py-0.5 text-cyan-300 font-mono font-bold text-right outline-none focus:border-cyan-500"
            />
          </div>
        </div>

        <input
          type="range"
          min="0.00"
          max="25.00"
          step="0.01"
          value={kd < 0 ? 12.30 : kd}
          onChange={(e) => setKd(parseFloat(e.target.value))}
          className="w-full h-1.5 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-cyan-400"
        />

        {/* Quick KD Preset Chips */}
        <div className="flex flex-wrap items-center justify-between gap-2 pt-1">
          <div className="flex flex-wrap items-center gap-1.5">
            <span className="text-[11px] text-slate-500">พรีเซ็ต KD:</span>
            <button
              type="button"
              onClick={() => setKd(0.00)}
              className={`px-2 py-1 rounded text-xs font-mono transition-colors ${kd === 0.00 ? 'bg-cyan-500 text-slate-950 font-bold' : 'bg-slate-800 hover:bg-slate-700 text-slate-300'}`}
            >
              0.00 (ปิด D)
            </button>
            <button
              type="button"
              onClick={() => setKd(5.50)}
              className={`px-2 py-1 rounded text-xs font-mono transition-colors ${kd === 5.50 ? 'bg-cyan-500 text-slate-950 font-bold' : 'bg-slate-800 hover:bg-slate-700 text-slate-300'}`}
            >
              5.50
            </button>
            <button
              type="button"
              onClick={() => setKd(7.50)}
              className={`px-2 py-1 rounded text-xs font-mono transition-colors ${kd === 7.50 ? 'bg-cyan-500 text-slate-950 font-bold' : 'bg-slate-800 hover:bg-slate-700 text-slate-300'}`}
            >
              7.50
            </button>
            <button
              type="button"
              onClick={() => setKd(10.50)}
              className={`px-2 py-1 rounded text-xs font-mono transition-colors ${kd === 10.50 ? 'bg-cyan-500 text-slate-950 font-bold' : 'bg-slate-800 hover:bg-slate-700 text-slate-300'}`}
            >
              10.50
            </button>
            <button
              type="button"
              onClick={() => setKd(12.30)}
              className={`px-2 py-1 rounded text-xs font-mono transition-colors ${kd === 12.30 ? 'bg-cyan-500 text-slate-950 font-bold' : 'bg-slate-800 hover:bg-slate-700 text-slate-300'}`}
            >
              12.30
            </button>
            <button
              type="button"
              onClick={() => setKd(-1.00)}
              className={`px-2.5 py-1 rounded text-xs font-mono transition-colors ${
                kd < 0
                  ? 'bg-cyan-500 text-slate-950 font-bold shadow-glow-cyan'
                  : 'bg-slate-800 hover:bg-slate-700 text-slate-300'
              }`}
            >
              Auto (คำนวณตาม Kp)
            </button>
          </div>

          <div className="flex items-center gap-1">
            <button onClick={() => adjustKd(-0.5)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono text-[11px]">-0.5</button>
            <button onClick={() => adjustKd(-0.1)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono text-[11px]">-0.1</button>
            <button onClick={() => adjustKd(+0.1)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono text-[11px]">+0.1</button>
            <button onClick={() => adjustKd(+0.5)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono text-[11px]">+0.5</button>
          </div>
        </div>
      </div>

      {/* Dynamic Parameter Input (Distance / Pass / Time) */}
      <div className="bg-slate-900/60 p-3.5 rounded-xl border border-slate-800 space-y-2">
        <div className="flex items-center justify-between text-xs">
          <label className="font-semibold text-slate-300">{paramInfo.label}</label>
          <div className="flex items-center gap-1.5">
            <input
              type="number"
              step={paramInfo.step}
              value={val}
              disabled={paramInfo.disabled}
              onChange={(e) => setVal(parseFloat(e.target.value) || 0)}
              className="w-24 bg-slate-950 border border-slate-700 rounded px-2 py-0.5 text-emerald-300 font-mono font-bold text-right outline-none focus:border-emerald-500 disabled:opacity-50"
            />
            <span className="text-slate-400 font-mono text-xs">{paramInfo.unit}</span>
          </div>
        </div>

        {/* Quick Value Chips */}
        <div className="flex flex-wrap items-center justify-between gap-2 pt-1">
          <div className="flex flex-wrap items-center gap-1.5">
            <span className="text-[11px] text-slate-500">ค่าลัด:</span>
            {paramInfo.presets.map((presetVal, idx) => (
              <button
                key={idx}
                type="button"
                onClick={() => setVal(presetVal)}
                className={`px-2.5 py-1 rounded text-xs font-mono transition-colors ${val === presetVal ? 'bg-emerald-500 text-slate-950 font-bold' : 'bg-slate-800 hover:bg-slate-700 text-slate-300'}`}
              >
                {paramInfo.presetsLabels[idx]}
              </button>
            ))}
          </div>

          {!paramInfo.disabled && (
            <div className="flex items-center gap-1">
              <button onClick={() => adjustVal(-5)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono text-[11px]">-5</button>
              <button onClick={() => adjustVal(-1)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono text-[11px]">-1</button>
              <button onClick={() => adjustVal(+1)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono text-[11px]">+1</button>
              <button onClick={() => adjustVal(+5)} className="px-1.5 py-0.5 bg-slate-800 hover:bg-slate-700 rounded text-slate-300 font-mono text-[11px]">+5</button>
            </div>
          )}
        </div>
      </div>

      {/* Checkbox: Wait for Button on Robot */}
      <div className="bg-slate-950/60 p-3 rounded-xl border border-slate-800/80 flex items-center justify-between gap-3 text-xs">
        <label className="flex items-center gap-2.5 cursor-pointer">
          <input
            type="checkbox"
            checked={waitBtn}
            onChange={(e) => setWaitBtn(e.target.checked)}
            className="w-4 h-4 rounded border-slate-700 bg-slate-900 text-cyan-500 focus:ring-0 cursor-pointer"
          />
          <span className="text-slate-300">
            <strong>รอกดสวิตช์ที่ตัวหุ่นยนต์ก่อนเริ่มวิ่ง (Wait for Switch)</strong> &mdash; หุ่นยนต์จะส่งเสียงปี๊บเตือนและรอกดสวิตช์ก่อนเริ่มวิ่ง เพื่อให้มีเวลาจัดตำแหน่งหุ่นบนเส้น
          </span>
        </label>
      </div>

      {/* Action Footer Button */}
      <div className="flex flex-col sm:flex-row items-center justify-between gap-3 pt-2">
        <div className="text-xs text-slate-400">
          คำสั่งที่จะส่ง: <span className="font-mono text-cyan-300 font-bold">TEST_PID ({mode})</span> | Spd: <span className="font-mono text-cyan-300 font-bold">{speed}%</span> | Kp: <span className="font-mono text-indigo-300 font-bold">{kp.toFixed(2)}</span> | Kd: <span className={`font-mono font-bold px-1.5 py-0.5 rounded ${kd < 0 ? 'bg-cyan-950/60 text-cyan-300 border border-cyan-800/40' : 'bg-emerald-950/60 text-emerald-300 border border-emerald-500/50'}`}>{kd < 0 ? 'Auto' : `${kd.toFixed(2)} (ค่าที่กรอกเอง)`}</span> | Val: <span className="font-mono text-emerald-300">{val}</span>
        </div>

        <button
          type="button"
          onClick={handleSendTest}
          disabled={!isConnected}
          className="inline-flex items-center gap-2 px-6 py-3 rounded-xl text-xs font-bold bg-cyan-500 hover:bg-cyan-400 text-slate-950 transition-all shadow-glow-cyan active:scale-95 disabled:opacity-50"
        >
          <Play className="w-4 h-4 fill-current" />
          <span>ส่งคำสั่งทดสอบ PID ไปยังหุ่นยนต์ (Send to Robot)</span>
        </button>
      </div>
    </div>
  );
};
