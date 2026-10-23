import React, { useState, useEffect } from 'react';
import { serial } from '../services/serialService';
import { 
  Zap, 
  ArrowUp, 
  ArrowDown, 
  RotateCcw, 
  RotateCw, 
  Square, 
  Play, 
  Sliders, 
  Save, 
  ArrowLeft, 
  ArrowRight,
  Gauge
} from 'lucide-react';

interface MotorControlProps {
  isConnected: boolean;
}

export const MotorControl: React.FC<MotorControlProps> = ({ isConnected }) => {
  const [m1, setM1] = useState<number>(0);
  const [m2, setM2] = useState<number>(0);
  const [m3, setM3] = useState<number>(0);
  const [m4, setM4] = useState<number>(0);
  const [duration, setDuration] = useState<number>(1200);
  const [waitBtn, setWaitBtn] = useState<boolean>(true);

  // Motor Trims & Battery Compensation
  const [trimM1, setTrimM1] = useState<number>(0);
  const [trimM2, setTrimM2] = useState<number>(0);
  const [trimM3, setTrimM3] = useState<number>(0);
  const [trimM4, setTrimM4] = useState<number>(0);
  const [trimOffset, setTrimOffset] = useState<number>(0);
  const [trimSaved, setTrimSaved] = useState<boolean>(false);

  // Load saved settings
  useEffect(() => {
    const saved = localStorage.getItem('puppybot_motor_settings');
    if (saved) {
      try {
        const s = JSON.parse(saved);
        if (s.m1 !== undefined) setM1(s.m1);
        if (s.m2 !== undefined) setM2(s.m2);
        if (s.m3 !== undefined) setM3(s.m3);
        if (s.m4 !== undefined) setM4(s.m4);
        if (s.duration !== undefined) setDuration(s.duration);
        if (s.waitBtn !== undefined) setWaitBtn(s.waitBtn);
        if (s.trimM1 !== undefined) setTrimM1(s.trimM1);
        if (s.trimM2 !== undefined) setTrimM2(s.trimM2);
        if (s.trimM3 !== undefined) setTrimM3(s.trimM3);
        if (s.trimM4 !== undefined) setTrimM4(s.trimM4);
        if (s.trimOffset !== undefined) setTrimOffset(s.trimOffset);
      } catch {}
    }
  }, []);

  // Save settings
  useEffect(() => {
    localStorage.setItem('puppybot_motor_settings', JSON.stringify({
      m1, m2, m3, m4, duration, waitBtn, trimM1, trimM2, trimM3, trimM4, trimOffset
    }));
  }, [m1, m2, m3, m4, duration, waitBtn, trimM1, trimM2, trimM3, trimM4, trimOffset]);

  const handleSendMotors = async (v1 = m1, v2 = m2, v3 = m3, v4 = m4, time = duration) => {
    if (!isConnected) {
      alert('กรุณาเชื่อมต่อ COM Port ก่อนส่งคำสั่ง!');
      return;
    }
    await serial.sendCommand({
      cmd: 'TEST_MOTOR',
      m1: v1,
      m2: v2,
      m3: v3,
      m4: v4,
      time,
      wait_btn: waitBtn ? 1 : 0
    });
  };

  const set4WheelSpeeds = (s1: number, s2: number, s3: number, s4: number) => {
    setM1(s1);
    setM2(s2);
    setM3(s3);
    setM4(s4);
  };

  const handleSaveTrims = async () => {
    if (!isConnected) {
      alert('กรุณาเชื่อมต่อ COM Port ก่อน!');
      return;
    }
    await serial.sendCommand({
      cmd: 'SET_CONFIG',
      m1: trimM1,
      m2: trimM2,
      m3: trimM3,
      m4: trimM4,
      offset: trimOffset
    });
    setTrimSaved(true);
    setTimeout(() => setTrimSaved(false), 2000);
  };

  const handleStop = async () => {
    setM1(0);
    setM2(0);
    setM3(0);
    setM4(0);
    await serial.emergencyStop();
  };

  return (
    <div className="glass-panel rounded-xl p-5 border border-slate-800 space-y-6">
      {/* Header */}
      <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-2 border-b border-slate-800 pb-3">
        <div>
          <h3 className="text-base font-bold text-white flex items-center gap-2">
            <Zap className="w-4 h-4 text-cyan-400" />
            <span>ทดสอบการหมุนของมอเตอร์ 4 ล้ออิสระ (4WD Motor Diagnostics)</span>
          </h3>
          <p className="text-xs text-slate-400 mt-0.5">
            ปรับความเร็วและทิศทางแยก 4 ล้ออิสระ พร้อมระบบตั้งค่า Motor Trim ชดเชยล้อเอียง
          </p>
        </div>
        <span className="text-xs px-2.5 py-1 rounded-full font-mono bg-cyan-950/40 text-cyan-300 border border-cyan-800/40">
          PWM -100% ถึง +100%
        </span>
      </div>

      {/* 4 Motors Sliders & Numeric Inputs */}
      <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
        {/* M1 */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 space-y-2">
          <div className="flex justify-between items-center text-xs">
            <span className="font-bold text-slate-200 flex items-center gap-1.5">
              <span className="px-1.5 py-0.5 rounded bg-cyan-950 text-cyan-400 border border-cyan-800/50 font-mono">M1</span>
              <span>ล้อหน้าซ้าย (Front-Left)</span>
            </span>
            <div className="flex items-center gap-1">
              <input
                type="number"
                min="-100"
                max="100"
                value={m1}
                onChange={(e) => setM1(parseInt(e.target.value) || 0)}
                className="w-16 bg-slate-950 border border-slate-700 rounded px-2 py-0.5 text-cyan-300 font-mono font-bold text-right outline-none focus:border-cyan-500"
              />
              <span className="text-slate-400 font-mono text-xs">%</span>
            </div>
          </div>
          <input
            type="range"
            min="-100"
            max="100"
            value={m1}
            onChange={(e) => setM1(Number(e.target.value))}
            className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-cyan-400"
          />
          <div className="flex justify-between text-[10px] text-slate-500 font-mono">
            <span>-100% (ถอย)</span>
            <span>0% (หยุด)</span>
            <span>+100% (เดินหน้า)</span>
          </div>
        </div>

        {/* M2 */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 space-y-2">
          <div className="flex justify-between items-center text-xs">
            <span className="font-bold text-slate-200 flex items-center gap-1.5">
              <span className="px-1.5 py-0.5 rounded bg-cyan-950 text-cyan-400 border border-cyan-800/50 font-mono">M2</span>
              <span>ล้อหน้าขวา (Front-Right)</span>
            </span>
            <div className="flex items-center gap-1">
              <input
                type="number"
                min="-100"
                max="100"
                value={m2}
                onChange={(e) => setM2(parseInt(e.target.value) || 0)}
                className="w-16 bg-slate-950 border border-slate-700 rounded px-2 py-0.5 text-cyan-300 font-mono font-bold text-right outline-none focus:border-cyan-500"
              />
              <span className="text-slate-400 font-mono text-xs">%</span>
            </div>
          </div>
          <input
            type="range"
            min="-100"
            max="100"
            value={m2}
            onChange={(e) => setM2(Number(e.target.value))}
            className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-cyan-400"
          />
          <div className="flex justify-between text-[10px] text-slate-500 font-mono">
            <span>-100% (ถอย)</span>
            <span>0% (หยุด)</span>
            <span>+100% (เดินหน้า)</span>
          </div>
        </div>

        {/* M3 */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 space-y-2">
          <div className="flex justify-between items-center text-xs">
            <span className="font-bold text-slate-200 flex items-center gap-1.5">
              <span className="px-1.5 py-0.5 rounded bg-indigo-950 text-indigo-400 border border-indigo-800/50 font-mono">M3</span>
              <span>ล้อหลังซ้าย (Back-Left)</span>
            </span>
            <div className="flex items-center gap-1">
              <input
                type="number"
                min="-100"
                max="100"
                value={m3}
                onChange={(e) => setM3(parseInt(e.target.value) || 0)}
                className="w-16 bg-slate-950 border border-slate-700 rounded px-2 py-0.5 text-indigo-300 font-mono font-bold text-right outline-none focus:border-indigo-500"
              />
              <span className="text-slate-400 font-mono text-xs">%</span>
            </div>
          </div>
          <input
            type="range"
            min="-100"
            max="100"
            value={m3}
            onChange={(e) => setM3(Number(e.target.value))}
            className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-400"
          />
          <div className="flex justify-between text-[10px] text-slate-500 font-mono">
            <span>-100% (ถอย)</span>
            <span>0% (หยุด)</span>
            <span>+100% (เดินหน้า)</span>
          </div>
        </div>

        {/* M4 */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 space-y-2">
          <div className="flex justify-between items-center text-xs">
            <span className="font-bold text-slate-200 flex items-center gap-1.5">
              <span className="px-1.5 py-0.5 rounded bg-indigo-950 text-indigo-400 border border-indigo-800/50 font-mono">M4</span>
              <span>ล้อหลังขวา (Back-Right)</span>
            </span>
            <div className="flex items-center gap-1">
              <input
                type="number"
                min="-100"
                max="100"
                value={m4}
                onChange={(e) => setM4(parseInt(e.target.value) || 0)}
                className="w-16 bg-slate-950 border border-slate-700 rounded px-2 py-0.5 text-indigo-300 font-mono font-bold text-right outline-none focus:border-indigo-500"
              />
              <span className="text-slate-400 font-mono text-xs">%</span>
            </div>
          </div>
          <input
            type="range"
            min="-100"
            max="100"
            value={m4}
            onChange={(e) => setM4(Number(e.target.value))}
            className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-400"
          />
          <div className="flex justify-between text-[10px] text-slate-500 font-mono">
            <span>-100% (ถอย)</span>
            <span>0% (หยุด)</span>
            <span>+100% (เดินหน้า)</span>
          </div>
        </div>
      </div>

      {/* Quick Motion Presets (Exact match with old web) */}
      <div className="bg-slate-900/50 p-4 rounded-xl border border-slate-800 space-y-2.5">
        <label className="text-xs font-semibold text-slate-300">
          ทิศทางลัด 4 ล้อพร้อมกัน (Quick Motion Presets @ 50% Speed):
        </label>
        <div className="grid grid-cols-2 sm:grid-cols-4 md:grid-cols-7 gap-2">
          <button
            type="button"
            onClick={() => set4WheelSpeeds(50, 50, 50, 50)}
            className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-cyan-600/30 text-slate-200 hover:text-cyan-300 text-xs font-semibold border border-slate-700 transition-colors"
          >
            ⬆️ เดินหน้า
          </button>
          <button
            type="button"
            onClick={() => set4WheelSpeeds(-50, -50, -50, -50)}
            className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-cyan-600/30 text-slate-200 hover:text-cyan-300 text-xs font-semibold border border-slate-700 transition-colors"
          >
            ⬇️ ถอยหลัง
          </button>
          <button
            type="button"
            onClick={() => set4WheelSpeeds(-50, 50, 50, -50)}
            className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-indigo-600/30 text-slate-200 hover:text-indigo-300 text-xs font-semibold border border-slate-700 transition-colors"
          >
            ⬅️ สไลด์ซ้าย
          </button>
          <button
            type="button"
            onClick={() => set4WheelSpeeds(50, -50, -50, 50)}
            className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-indigo-600/30 text-slate-200 hover:text-indigo-300 text-xs font-semibold border border-slate-700 transition-colors"
          >
            ➡️ สไลด์ขวา
          </button>
          <button
            type="button"
            onClick={() => set4WheelSpeeds(-50, 50, -50, 50)}
            className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-indigo-600/30 text-slate-200 hover:text-indigo-300 text-xs font-semibold border border-slate-700 transition-colors"
          >
            🔄 เลี้ยวซ้าย
          </button>
          <button
            type="button"
            onClick={() => set4WheelSpeeds(50, -50, 50, -50)}
            className="px-3 py-2 rounded-xl bg-slate-800 hover:bg-indigo-600/30 text-slate-200 hover:text-indigo-300 text-xs font-semibold border border-slate-700 transition-colors"
          >
            🔁 เลี้ยวขวา
          </button>
          <button
            type="button"
            onClick={handleStop}
            className="px-3 py-2 rounded-xl bg-rose-600/30 hover:bg-rose-600 text-rose-300 hover:text-white text-xs font-bold border border-rose-500/50 transition-colors"
          >
            ⏹️ หยุด
          </button>
        </div>
      </div>

      {/* Duration, Wait Btn & Action */}
      <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 items-center bg-slate-950/60 p-4 rounded-xl border border-slate-800">
        <div className="flex items-center gap-2 text-xs">
          <label className="text-slate-300 font-semibold whitespace-nowrap">ระยะเวลาการหมุน (ms):</label>
          <input
            type="number"
            step="100"
            min="100"
            max="15000"
            value={duration}
            onChange={(e) => setDuration(parseInt(e.target.value) || 1000)}
            className="w-24 bg-slate-900 border border-slate-700 rounded-lg px-2.5 py-1.5 text-xs text-amber-300 font-mono font-bold text-right outline-none"
          />
          <span className="text-slate-400 font-mono">ms</span>
        </div>

        <label className="flex items-center gap-2 cursor-pointer text-xs">
          <input
            type="checkbox"
            checked={waitBtn}
            onChange={(e) => setWaitBtn(e.target.checked)}
            className="w-4 h-4 rounded border-slate-700 bg-slate-900 text-cyan-500 focus:ring-0"
          />
          <span className="text-slate-300">รอกดสวิตช์ที่ตัวหุ่นก่อนหมุนล้อ</span>
        </label>
      </div>

      {/* Action Send Button */}
      <div className="flex justify-end gap-2">
        <button
          onClick={() => set4WheelSpeeds(0, 0, 0, 0)}
          className="px-4 py-2 rounded-xl text-xs font-semibold bg-slate-800 hover:bg-slate-700 text-slate-300 transition-colors"
        >
          รีเซ็ตเป็น 0
        </button>
        <button
          onClick={() => handleSendMotors()}
          disabled={!isConnected}
          className="inline-flex items-center gap-2 px-6 py-2.5 rounded-xl text-xs font-bold bg-cyan-500 hover:bg-cyan-400 text-slate-950 transition-all shadow-glow-cyan active:scale-95 disabled:opacity-50"
        >
          <Play className="w-4 h-4 fill-current" />
          <span>เริ่มทดสอบการหมุน 4 ล้อ (Run 4WD Test)</span>
        </button>
      </div>

      {/* Motor Trims Accordion (100% of Old Web) */}
      <details className="bg-slate-900/60 rounded-xl p-4 border border-slate-800 text-xs">
        <summary className="font-bold text-slate-200 cursor-pointer flex items-center gap-2 hover:text-cyan-300">
          <Sliders className="w-4 h-4 text-cyan-400" />
          <span>⚙️ ตั้งค่าชดเชยล้อ (Motor Trim) และ Battery Speed Compensation</span>
        </summary>
        <div className="pt-4 grid grid-cols-2 sm:grid-cols-5 gap-3">
          <div className="space-y-1">
            <span className="text-slate-400 font-mono">M1 Trim:</span>
            <input
              type="number"
              value={trimM1}
              onChange={(e) => setTrimM1(parseInt(e.target.value) || 0)}
              className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2 py-1 text-cyan-300 font-mono text-center outline-none"
            />
          </div>
          <div className="space-y-1">
            <span className="text-slate-400 font-mono">M2 Trim:</span>
            <input
              type="number"
              value={trimM2}
              onChange={(e) => setTrimM2(parseInt(e.target.value) || 0)}
              className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2 py-1 text-cyan-300 font-mono text-center outline-none"
            />
          </div>
          <div className="space-y-1">
            <span className="text-slate-400 font-mono">M3 Trim:</span>
            <input
              type="number"
              value={trimM3}
              onChange={(e) => setTrimM3(parseInt(e.target.value) || 0)}
              className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2 py-1 text-indigo-300 font-mono text-center outline-none"
            />
          </div>
          <div className="space-y-1">
            <span className="text-slate-400 font-mono">M4 Trim:</span>
            <input
              type="number"
              value={trimM4}
              onChange={(e) => setTrimM4(parseInt(e.target.value) || 0)}
              className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2 py-1 text-indigo-300 font-mono text-center outline-none"
            />
          </div>
          <div className="space-y-1">
            <span className="text-slate-400 font-mono">Speed Offset %:</span>
            <input
              type="number"
              value={trimOffset}
              onChange={(e) => setTrimOffset(parseInt(e.target.value) || 0)}
              className="w-full bg-slate-950 border border-slate-700 rounded-lg px-2 py-1 text-amber-300 font-mono text-center outline-none"
            />
          </div>
        </div>
        <div className="flex justify-end pt-3">
          <button
            onClick={handleSaveTrims}
            disabled={!isConnected}
            className="inline-flex items-center gap-1.5 px-4 py-1.5 rounded-lg text-xs font-bold bg-cyan-600 hover:bg-cyan-500 text-white transition-colors disabled:opacity-40"
          >
            <Save className="w-3.5 h-3.5" />
            <span>{trimSaved ? 'บันทึกสำเร็จ!' : 'บันทึก Trim ลงหุ่น'}</span>
          </button>
        </div>
      </details>
    </div>
  );
};
