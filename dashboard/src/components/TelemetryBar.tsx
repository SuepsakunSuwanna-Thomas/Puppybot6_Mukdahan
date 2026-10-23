import React from 'react';
import { TelemetryData } from '../types';
import { serial } from '../services/serialService';
import { 
  Eye, 
  Compass, 
  Ruler, 
  Gauge, 
  RotateCcw, 
  Sparkles, 
  Radio, 
  Layers 
} from 'lucide-react';

interface TelemetryBarProps {
  telem: TelemetryData | null;
}

export const TelemetryBar: React.FC<TelemetryBarProps> = ({ telem }) => {
  // Helpers
  const frontR = telem?.frgb?.[0] ?? 0;
  const frontG = telem?.frgb?.[1] ?? 0;
  const frontB = telem?.frgb?.[2] ?? 0;
  const frontColorName = telem?.fn ?? 'UNKNOWN';

  const sideR = telem?.srgb?.[0] ?? 0;
  const sideG = telem?.srgb?.[1] ?? 0;
  const sideB = telem?.srgb?.[2] ?? 0;
  const sideColorName = telem?.sn ?? 'UNKNOWN';

  const flSensors = telem?.fl ?? [0, 0, 0, 0, 0, 0, 0];
  const blSensors = telem?.bl ?? [0, 0, 0, 0, 0, 0, 0];

  const irDist = telem?.ir ?? 0;
  const encDist = telem?.enc ?? 0;
  const encRaw = telem?.enc_raw ?? 0;
  const encScale = telem?.enc_scale ?? 50.0;
  const yaw = telem?.yaw ?? 0;
  const isBtnPressed = telem?.btn === 1;

  // Calculate Line Center Position (-3 to +3)
  // Weighted average: sensors 0..6 correspond to -3, -2, -1, 0, +1, +2, +3
  let sumWeights = 0;
  let activeCount = 0;
  flSensors.forEach((val, idx) => {
    if (val === 1) {
      sumWeights += (idx - 3);
      activeCount++;
    }
  });
  const lineOffset = activeCount > 0 ? (sumWeights / activeCount) : 0;
  const hasLine = activeCount > 0;

  const handleResetEncoder = async () => {
    await serial.sendCommand({ cmd: 'RESET_ENC' });
  };

  return (
    <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3.5 mb-6">
      {/* 1. Dual Color Sensors (Floor & Side Can) */}
      <div className="glass-panel rounded-xl p-3.5 border border-slate-800/80 hover:border-slate-700/80 transition-all">
        <div className="flex items-center justify-between mb-2.5">
          <div className="flex items-center gap-1.5 text-xs font-semibold text-slate-300">
            <Eye className="w-3.5 h-3.5 text-cyan-400" />
            <span>TCS34725 Dual Color (พื้น & กระป๋อง)</span>
          </div>
          <span className="text-[10px] font-mono font-medium px-1.5 py-0.5 rounded bg-cyan-950/40 text-cyan-400 border border-cyan-800/40">
            I2C DUAL
          </span>
        </div>

        <div className="grid grid-cols-2 gap-2.5">
          {/* Floor Sensor */}
          <div className="bg-slate-900/60 rounded-lg p-2.5 border border-slate-800/60 flex flex-col justify-between">
            <div className="flex items-center justify-between text-[11px] mb-1.5">
              <span className="text-slate-400">1. สีพื้นสนาม</span>
              <span className="font-bold text-xs px-1.5 py-0.5 rounded bg-slate-800 text-cyan-300">
                {frontColorName}
              </span>
            </div>
            <div className="flex items-center gap-2">
              <div 
                className="w-8 h-8 rounded-md border border-slate-700 shadow-sm flex-shrink-0 transition-colors"
                style={{ backgroundColor: `rgb(${frontR}, ${frontG}, ${frontB})` }}
                title={`RGB: ${frontR}, ${frontG}, ${frontB}`}
              />
              <div className="flex-1 space-y-1">
                <div className="flex items-center gap-1 text-[10px] font-mono text-slate-400">
                  <span className="text-rose-400 font-bold w-2.5">R</span>
                  <div className="flex-1 bg-slate-800 h-1.5 rounded-full overflow-hidden">
                    <div className="bg-rose-500 h-full transition-all" style={{ width: `${Math.min(100, (frontR / 255) * 100)}%` }} />
                  </div>
                  <span className="w-6 text-right">{frontR}</span>
                </div>
                <div className="flex items-center gap-1 text-[10px] font-mono text-slate-400">
                  <span className="text-emerald-400 font-bold w-2.5">G</span>
                  <div className="flex-1 bg-slate-800 h-1.5 rounded-full overflow-hidden">
                    <div className="bg-emerald-500 h-full transition-all" style={{ width: `${Math.min(100, (frontG / 255) * 100)}%` }} />
                  </div>
                  <span className="w-6 text-right">{frontG}</span>
                </div>
                <div className="flex items-center gap-1 text-[10px] font-mono text-slate-400">
                  <span className="text-blue-400 font-bold w-2.5">B</span>
                  <div className="flex-1 bg-slate-800 h-1.5 rounded-full overflow-hidden">
                    <div className="bg-blue-500 h-full transition-all" style={{ width: `${Math.min(100, (frontB / 255) * 100)}%` }} />
                  </div>
                  <span className="w-6 text-right">{frontB}</span>
                </div>
              </div>
            </div>
          </div>

          {/* Side Can Sensor */}
          <div className="bg-slate-900/60 rounded-lg p-2.5 border border-slate-800/60 flex flex-col justify-between">
            <div className="flex items-center justify-between text-[11px] mb-1.5">
              <span className="text-slate-400">2. สีกระป๋องข้าง</span>
              <span className="font-bold text-xs px-1.5 py-0.5 rounded bg-slate-800 text-indigo-300">
                {sideColorName}
              </span>
            </div>
            <div className="flex items-center gap-2">
              <div 
                className="w-8 h-8 rounded-md border border-slate-700 shadow-sm flex-shrink-0 transition-colors"
                style={{ backgroundColor: `rgb(${sideR}, ${sideG}, ${sideB})` }}
                title={`RGB: ${sideR}, ${sideG}, ${sideB}`}
              />
              <div className="flex-1 space-y-1">
                <div className="flex items-center gap-1 text-[10px] font-mono text-slate-400">
                  <span className="text-rose-400 font-bold w-2.5">R</span>
                  <div className="flex-1 bg-slate-800 h-1.5 rounded-full overflow-hidden">
                    <div className="bg-rose-500 h-full transition-all" style={{ width: `${Math.min(100, (sideR / 255) * 100)}%` }} />
                  </div>
                  <span className="w-6 text-right">{sideR}</span>
                </div>
                <div className="flex items-center gap-1 text-[10px] font-mono text-slate-400">
                  <span className="text-emerald-400 font-bold w-2.5">G</span>
                  <div className="flex-1 bg-slate-800 h-1.5 rounded-full overflow-hidden">
                    <div className="bg-emerald-500 h-full transition-all" style={{ width: `${Math.min(100, (sideG / 255) * 100)}%` }} />
                  </div>
                  <span className="w-6 text-right">{sideG}</span>
                </div>
                <div className="flex items-center gap-1 text-[10px] font-mono text-slate-400">
                  <span className="text-blue-400 font-bold w-2.5">B</span>
                  <div className="flex-1 bg-slate-800 h-1.5 rounded-full overflow-hidden">
                    <div className="bg-blue-500 h-full transition-all" style={{ width: `${Math.min(100, (sideB / 255) * 100)}%` }} />
                  </div>
                  <span className="w-6 text-right">{sideB}</span>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* 2. 14-Channel Line Sensors & Center Offset Bar */}
      <div className="glass-panel rounded-xl p-3.5 border border-slate-800/80 hover:border-slate-700/80 transition-all flex flex-col justify-between">
        <div className="flex items-center justify-between mb-2">
          <div className="flex items-center gap-1.5 text-xs font-semibold text-slate-300">
            <Layers className="w-3.5 h-3.5 text-cyan-400" />
            <span>14-Channel Line Sensor (หน้า & หลัง)</span>
          </div>
          <span className={`text-[10px] font-mono font-semibold px-2 py-0.5 rounded border ${
            hasLine 
              ? 'bg-cyan-950/40 text-cyan-300 border-cyan-500/40' 
              : 'bg-rose-950/40 text-rose-300 border-rose-500/40'
          }`}>
            {hasLine ? `OFFSET: ${lineOffset.toFixed(2)}` : 'OUT OF LINE'}
          </span>
        </div>

        {/* Dynamic Center Deviation Bar */}
        <div className="bg-slate-900/60 rounded-lg p-2 border border-slate-800/60 mb-2">
          <div className="flex justify-between text-[10px] text-slate-400 mb-1 font-mono">
            <span>&larr; ซ้าย (L)</span>
            <span className="text-cyan-400 font-semibold">กึ่งกลาง (Center)</span>
            <span>ขวา (R) &rarr;</span>
          </div>
          <div className="relative w-full h-2.5 bg-slate-800 rounded-full overflow-hidden flex items-center">
            {/* Center mark */}
            <div className="absolute left-1/2 top-0 bottom-0 w-0.5 bg-slate-500 z-10 -translate-x-1/2" />
            {hasLine && (
              <div 
                className="absolute h-full w-4 rounded-full bg-cyan-400 shadow-glow-cyan transition-all duration-75 -translate-x-1/2"
                style={{ left: `${((lineOffset + 3) / 6) * 100}%` }}
              />
            )}
          </div>
        </div>

        {/* Front 7 & Back 7 LED Grid */}
        <div className="space-y-1.5">
          {/* Front Row (A0 - A6) */}
          <div className="flex items-center justify-between text-[11px] bg-slate-900/40 rounded px-2 py-1">
            <span className="text-slate-400 font-mono text-[10px] w-12">หน้า (F)</span>
            <div className="flex items-center gap-1.5">
              {flSensors.map((val, idx) => (
                <div key={`fl-${idx}`} className="flex flex-col items-center">
                  <div
                    className={`w-3.5 h-3.5 rounded-full border transition-all ${
                      val === 1
                        ? 'bg-cyan-400 border-cyan-300 shadow-glow-cyan scale-110'
                        : 'bg-slate-800 border-slate-700'
                    }`}
                  />
                  <span className={`text-[9px] font-mono ${idx === 3 ? 'text-cyan-300 font-bold' : 'text-slate-500'}`}>
                    A{idx}
                  </span>
                </div>
              ))}
            </div>
          </div>

          {/* Back Row (A7 - A13) */}
          <div className="flex items-center justify-between text-[11px] bg-slate-900/40 rounded px-2 py-1">
            <span className="text-slate-400 font-mono text-[10px] w-12">หลัง (B)</span>
            <div className="flex items-center gap-1.5">
              {blSensors.map((val, idx) => (
                <div key={`bl-${idx}`} className="flex flex-col items-center">
                  <div
                    className={`w-3.5 h-3.5 rounded-full border transition-all ${
                      val === 1
                        ? 'bg-indigo-400 border-indigo-300 shadow-sm scale-110'
                        : 'bg-slate-800 border-slate-700'
                    }`}
                  />
                  <span className={`text-[9px] font-mono ${idx === 3 ? 'text-indigo-300 font-bold' : 'text-slate-500'}`}>
                    A{idx + 7}
                  </span>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>

      {/* 3. Navigation: IR, Encoder, IMU Yaw & Switch */}
      <div className="glass-panel rounded-xl p-3.5 border border-slate-800/80 hover:border-slate-700/80 transition-all flex flex-col justify-between">
        <div className="flex items-center justify-between mb-2">
          <div className="flex items-center gap-1.5 text-xs font-semibold text-slate-300">
            <Compass className="w-3.5 h-3.5 text-cyan-400" />
            <span>Navigation (IR / ไจโร / Encoder)</span>
          </div>
          <div className="flex items-center gap-1">
            <span className={`text-[10px] px-1.5 py-0.5 rounded font-mono ${
              isBtnPressed ? 'bg-amber-500/20 text-amber-300 border border-amber-500/40 font-bold' : 'bg-slate-800 text-slate-400'
            }`}>
              SW: {isBtnPressed ? 'PRESSED' : 'RELEASE'}
            </span>
          </div>
        </div>

        <div className="grid grid-cols-3 gap-2">
          {/* IR Distance */}
          <div className="bg-slate-900/60 rounded-lg p-2 border border-slate-800/60 flex flex-col justify-between">
            <span className="text-[10px] text-slate-400 font-mono">IR Distance</span>
            <div className="text-base font-bold font-mono text-cyan-300 my-1">
              {irDist.toFixed(1)} <small className="text-[10px] text-slate-400">cm</small>
            </div>
            <div className="bg-slate-800 h-1 rounded-full overflow-hidden">
              <div 
                className="bg-cyan-400 h-full transition-all"
                style={{ width: `${Math.min(100, (irDist / 40) * 100)}%` }}
              />
            </div>
          </div>

          {/* Encoder Distance */}
          <div className="bg-slate-900/60 rounded-lg p-2 border border-slate-800/60 flex flex-col justify-between">
            <div className="flex items-center justify-between">
              <span className="text-[10px] text-slate-400 font-mono">Encoder</span>
              <button
                onClick={handleResetEncoder}
                className="text-[9px] px-1 py-0.2 rounded bg-slate-800 hover:bg-slate-700 text-slate-300 transition-colors"
                title="รีเซ็ต Encoder เป็น 0"
              >
                Zero
              </button>
            </div>
            <div className="text-base font-bold font-mono text-emerald-300 my-1">
              {encDist.toFixed(1)} <small className="text-[10px] text-slate-400">cm</small>
            </div>
            <span className="text-[9px] text-slate-500 font-mono truncate">
              {encRaw}p ({encScale}/cm)
            </span>
          </div>

          {/* Gyro Yaw */}
          <div className="bg-slate-900/60 rounded-lg p-2 border border-slate-800/60 flex flex-col justify-between items-center text-center">
            <span className="text-[10px] text-slate-400 font-mono">Gyro Yaw</span>
            <div className="relative w-8 h-8 rounded-full border border-slate-700 flex items-center justify-center my-1 bg-slate-950">
              <div 
                className="w-0.5 h-6 bg-rose-500 rounded-full origin-center transition-transform duration-100"
                style={{ transform: `rotate(${yaw}deg)` }}
              />
              <div className="absolute w-1.5 h-1.5 rounded-full bg-white" />
            </div>
            <span className="text-[11px] font-bold font-mono text-amber-300">
              {yaw.toFixed(1)}&deg;
            </span>
          </div>
        </div>
      </div>
    </div>
  );
};
