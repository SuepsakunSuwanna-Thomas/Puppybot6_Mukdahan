import React, { useState, useEffect } from 'react';
import { serial } from '../services/serialService';
import { Bot, Play, Sparkles } from 'lucide-react';

interface ServoControlProps {
  isConnected: boolean;
}

export const ServoControl: React.FC<ServoControlProps> = ({ isConnected }) => {
  const [s1, setS1] = useState<number>(50);
  const [s2, setS2] = useState<number>(146);
  const [waitBtn, setWaitBtn] = useState<boolean>(false);

  // Load saved settings
  useEffect(() => {
    const saved = localStorage.getItem('puppybot_servo_settings');
    if (saved) {
      try {
        const s = JSON.parse(saved);
        if (s.s1 !== undefined) setS1(s.s1);
        if (s.s2 !== undefined) setS2(s.s2);
        if (s.waitBtn !== undefined) setWaitBtn(s.waitBtn);
      } catch {}
    }
  }, []);

  // Save settings
  useEffect(() => {
    localStorage.setItem('puppybot_servo_settings', JSON.stringify({ s1, s2, waitBtn }));
  }, [s1, s2, waitBtn]);

  const handleSendServo = async (val1 = s1, val2 = s2) => {
    if (!isConnected) {
      alert('กรุณาเชื่อมต่อ COM Port ก่อนส่งคำสั่ง!');
      return;
    }
    await serial.sendCommand({
      cmd: 'SERVO',
      s1: val1,
      s2: val2,
      wait_btn: waitBtn ? 1 : 0
    });
  };

  const applyPreset = (val1: number, val2: number) => {
    setS1(val1);
    setS2(val2);
    handleSendServo(val1, val2);
  };

  return (
    <div className="glass-panel rounded-xl p-5 border border-slate-800 space-y-6">
      {/* Header */}
      <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-2 border-b border-slate-800 pb-3">
        <div>
          <h3 className="text-base font-bold text-white flex items-center gap-2">
            <Bot className="w-4 h-4 text-cyan-400" />
            <span>ตั้งมุมเซอร์โวมอเตอร์ 1 และ 2 (Dual Servo Angle Control)</span>
          </h3>
          <p className="text-xs text-slate-400 mt-0.5">
            servoRun(1, angle) &amp; servoRun(2, angle) ปรับหมุนแขนกลและกริปเปอร์จับกระป๋อง
          </p>
        </div>
        <span className="text-xs px-2.5 py-1 rounded-full font-mono bg-cyan-950/40 text-cyan-300 border border-cyan-800/40">
          0&deg; ถึง 180&deg;
        </span>
      </div>

      {/* Quick Combination Presets */}
      <div>
        <div className="flex items-center gap-1.5 text-xs text-slate-400 mb-2">
          <Sparkles className="w-3.5 h-3.5 text-cyan-400" />
          <span>ท่าทางสำเร็จรูป (Quick Dual Servo Presets):</span>
        </div>
        <div className="grid grid-cols-2 sm:grid-cols-4 gap-2.5">
          <button
            onClick={() => applyPreset(120, 60)}
            className="p-3 rounded-xl border border-slate-800 hover:border-cyan-500/50 bg-slate-900/60 hover:bg-slate-900 text-left transition-all group"
          >
            <div className="text-xs font-bold text-slate-200 group-hover:text-cyan-300">
              1. หนีบกระป๋อง (Grip)
            </div>
            <p className="text-[10px] text-slate-400 mt-1 font-mono">
              S1: 120&deg; / S2: 60&deg;
            </p>
          </button>

          <button
            onClick={() => applyPreset(50, 146)}
            className="p-3 rounded-xl border border-slate-800 hover:border-cyan-500/50 bg-slate-900/60 hover:bg-slate-900 text-left transition-all group"
          >
            <div className="text-xs font-bold text-slate-200 group-hover:text-cyan-300">
              2. อ้ากริปเปอร์ (Release)
            </div>
            <p className="text-[10px] text-slate-400 mt-1 font-mono">
              S1: 50&deg; / S2: 146&deg;
            </p>
          </button>

          <button
            onClick={() => applyPreset(140, 140)}
            className="p-3 rounded-xl border border-slate-800 hover:border-cyan-500/50 bg-slate-900/60 hover:bg-slate-900 text-left transition-all group"
          >
            <div className="text-xs font-bold text-slate-200 group-hover:text-cyan-300">
              3. ยกแขนขึ้น (Lift Up)
            </div>
            <p className="text-[10px] text-slate-400 mt-1 font-mono">
              S1: 140&deg; / S2: 140&deg;
            </p>
          </button>

          <button
            onClick={() => applyPreset(20, 20)}
            className="p-3 rounded-xl border border-slate-800 hover:border-cyan-500/50 bg-slate-900/60 hover:bg-slate-900 text-left transition-all group"
          >
            <div className="text-xs font-bold text-slate-200 group-hover:text-cyan-300">
              4. วางแขนลง (Lower Down)
            </div>
            <p className="text-[10px] text-slate-400 mt-1 font-mono">
              S1: 20&deg; / S2: 20&deg;
            </p>
          </button>
        </div>
      </div>

      {/* Sliders & Direct Numeric Typing Grid */}
      <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
        {/* Servo 1 */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 space-y-2.5">
          <div className="flex justify-between items-center text-xs">
            <span className="font-bold text-slate-200">เซอร์โว 1 (Servo 1 - มือจับ / หน้า)</span>
            <div className="flex items-center gap-1">
              <input
                type="number"
                min="0"
                max="180"
                value={s1}
                onChange={(e) => setS1(Math.max(0, Math.min(180, parseInt(e.target.value) || 0)))}
                className="w-16 bg-slate-950 border border-slate-700 rounded px-2 py-0.5 text-cyan-300 font-mono font-bold text-right outline-none focus:border-cyan-500"
              />
              <span className="text-slate-400 font-mono">&deg;</span>
            </div>
          </div>
          <input
            type="range"
            min="0"
            max="180"
            value={s1}
            onChange={(e) => setS1(Number(e.target.value))}
            className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-cyan-400"
          />
          <div className="flex flex-wrap items-center gap-1.5 pt-1">
            <span className="text-[11px] text-slate-500">มุมลัด S1:</span>
            <button onClick={() => setS1(50)} className="px-2 py-0.5 rounded text-xs bg-slate-800 hover:bg-slate-700 text-slate-300">50&deg; (เริ่ม)</button>
            <button onClick={() => setS1(90)} className="px-2 py-0.5 rounded text-xs bg-slate-800 hover:bg-slate-700 text-slate-300">90&deg; (กลาง)</button>
            <button onClick={() => setS1(140)} className="px-2 py-0.5 rounded text-xs bg-slate-800 hover:bg-slate-700 text-slate-300">140&deg; (วาง)</button>
            <button onClick={() => setS1(180)} className="px-2 py-0.5 rounded text-xs bg-slate-800 hover:bg-slate-700 text-slate-300">180&deg;</button>
          </div>
        </div>

        {/* Servo 2 */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 space-y-2.5">
          <div className="flex justify-between items-center text-xs">
            <span className="font-bold text-slate-200">เซอร์โว 2 (Servo 2 - แขนช่วย / หลัง)</span>
            <div className="flex items-center gap-1">
              <input
                type="number"
                min="0"
                max="180"
                value={s2}
                onChange={(e) => setS2(Math.max(0, Math.min(180, parseInt(e.target.value) || 0)))}
                className="w-16 bg-slate-950 border border-slate-700 rounded px-2 py-0.5 text-indigo-300 font-mono font-bold text-right outline-none focus:border-indigo-500"
              />
              <span className="text-slate-400 font-mono">&deg;</span>
            </div>
          </div>
          <input
            type="range"
            min="0"
            max="180"
            value={s2}
            onChange={(e) => setS2(Number(e.target.value))}
            className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-indigo-400"
          />
          <div className="flex flex-wrap items-center gap-1.5 pt-1">
            <span className="text-[11px] text-slate-500">มุมลัด S2:</span>
            <button onClick={() => setS2(0)} className="px-2 py-0.5 rounded text-xs bg-slate-800 hover:bg-slate-700 text-slate-300">0&deg;</button>
            <button onClick={() => setS2(90)} className="px-2 py-0.5 rounded text-xs bg-slate-800 hover:bg-slate-700 text-slate-300">90&deg; (กลาง)</button>
            <button onClick={() => setS2(146)} className="px-2 py-0.5 rounded text-xs bg-slate-800 hover:bg-slate-700 text-slate-300">146&deg; (เริ่ม)</button>
            <button onClick={() => setS2(180)} className="px-2 py-0.5 rounded text-xs bg-slate-800 hover:bg-slate-700 text-slate-300">180&deg;</button>
          </div>
        </div>
      </div>

      {/* Checkbox: Wait for Button */}
      <div className="bg-slate-950/60 p-3 rounded-xl border border-slate-800 flex items-center justify-between text-xs">
        <label className="flex items-center gap-2 cursor-pointer">
          <input
            type="checkbox"
            checked={waitBtn}
            onChange={(e) => setWaitBtn(e.target.checked)}
            className="w-4 h-4 rounded border-slate-700 bg-slate-900 text-cyan-500 focus:ring-0"
          />
          <span className="text-slate-300">
            รอกดสวิตช์ที่ตัวหุ่นก่อนหมุนเซอร์โว (ถ้าไม่ติ๊ก เซอร์โวจะหมุนตามตำแหน่งทันทีเมื่อกดปุ่มส่ง)
          </span>
        </label>
      </div>

      {/* Action Button */}
      <div className="flex justify-end pt-1">
        <button
          onClick={() => handleSendServo()}
          disabled={!isConnected}
          className="inline-flex items-center gap-2 px-6 py-2.5 rounded-xl text-xs font-bold bg-cyan-500 hover:bg-cyan-400 text-slate-950 transition-all shadow-glow-cyan active:scale-95 disabled:opacity-50"
        >
          <Play className="w-4 h-4 fill-current" />
          <span>ส่งค่ามุมเซอร์โวไปยังหุ่นยนต์ (Set Servo Angles)</span>
        </button>
      </div>
    </div>
  );
};
