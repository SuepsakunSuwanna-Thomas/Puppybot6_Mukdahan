import React, { useState, useEffect } from 'react';
import { serial } from '../services/serialService';
import { RobotState } from '../types';
import { 
  Cpu, 
  Power, 
  RefreshCw, 
  AlertOctagon, 
  Clock, 
  CheckCircle2, 
  Radio, 
  Zap, 
  Settings2 
} from 'lucide-react';

interface HeaderProps {
  robotState: RobotState;
  stateMessage: string;
  roundCount: number;
  lastLapMs: number;
  isConnected: boolean;
}

export const Header: React.FC<HeaderProps> = ({
  robotState,
  stateMessage,
  roundCount,
  lastLapMs,
  isConnected,
}) => {
  const [baudRate, setBaudRate] = useState<number>(115200);

  const handleConnect = async () => {
    await serial.connect(baudRate);
  };

  const handleDisconnect = async () => {
    await serial.disconnect();
  };

  const handleEmergencyStop = async () => {
    await serial.emergencyStop();
  };

  const formatLapTime = (ms: number) => {
    if (!ms || ms <= 0) return null;
    const s = Math.floor(ms / 1000);
    const remainder = ms % 1000;
    const secStr = (ms / 1000).toFixed(3);
    return `${s}s ${remainder}ms (${secStr}s)`;
  };

  return (
    <header className="border-b border-slate-800/80 bg-slate-950/80 backdrop-blur-md sticky top-0 z-50 px-4 lg:px-8 py-3 transition-all">
      <div className="max-w-7xl mx-auto flex flex-col md:flex-row items-center justify-between gap-3">
        {/* Brand */}
        <div className="flex items-center gap-3">
          <div className="relative flex items-center justify-center w-10 h-10 rounded-xl bg-gradient-to-tr from-cyan-600 to-indigo-600 shadow-glow-cyan">
            <Cpu className="w-5 h-5 text-white" />
            {isConnected && (
              <span className="absolute -top-1 -right-1 flex h-3 w-3">
                <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>
                <span className="relative inline-flex rounded-full h-3 w-3 bg-emerald-500"></span>
              </span>
            )}
          </div>
          <div>
            <div className="flex items-center gap-2">
              <h1 className="text-lg font-bold tracking-tight text-white font-sans flex items-center gap-1.5">
                PUPPYBOT 6 <span className="text-xs px-2 py-0.5 rounded-md font-semibold bg-cyan-500/10 text-cyan-400 border border-cyan-500/30">PRO MISSION</span>
              </h1>
            </div>
            <p className="text-xs text-slate-400 font-mono">
              RP2350 High-Speed Web Serial &bull; Sequence Studio
            </p>
          </div>
        </div>

        {/* Global Connection & Status Controls */}
        <div className="flex flex-wrap items-center gap-2.5">
          {/* Baud Rate Select */}
          <div className="flex items-center gap-1.5 bg-slate-900 border border-slate-800 rounded-lg px-2.5 py-1 text-xs">
            <span className="text-slate-400">Baud:</span>
            <select
              value={baudRate}
              onChange={(e) => setBaudRate(Number(e.target.value))}
              disabled={isConnected}
              aria-label="Serial Baud Rate"
              className="bg-transparent text-slate-200 outline-none font-mono cursor-pointer disabled:opacity-50"
            >
              <option value={115200} className="bg-slate-900">115200 (Default)</option>
              <option value={57600} className="bg-slate-900">57600</option>
              <option value={9600} className="bg-slate-900">9600</option>
            </select>
          </div>

          {/* Reset Settings Button */}
          <button
            onClick={() => {
              if (confirm('คุณต้องการล้างค่าการตั้งค่าทั้งหมดและคืนเป็นค่าเริ่มต้นใช่หรือไม่?')) {
                localStorage.clear();
                window.location.reload();
              }
            }}
            className="inline-flex items-center gap-1 px-2.5 py-1.5 rounded-lg text-xs font-semibold bg-slate-900 hover:bg-slate-800 text-slate-300 border border-slate-800 transition-colors"
            title="ล้างค่าที่จำไว้ทั้งหมดและคืนเป็นค่าเริ่มต้น"
          >
            <RefreshCw className="w-3.5 h-3.5 text-slate-400" />
            <span className="hidden sm:inline">คืนค่าเริ่มต้น</span>
          </button>

          {/* Connect / Disconnect Buttons */}
          {!isConnected ? (
            <button
              onClick={handleConnect}
              className="inline-flex items-center gap-1.5 px-3.5 py-1.5 rounded-lg text-xs font-semibold bg-cyan-500 hover:bg-cyan-400 text-slate-950 transition-all shadow-glow-cyan active:scale-95"
            >
              <Power className="w-3.5 h-3.5" />
              <span>เชื่อมต่อ COM Port</span>
            </button>
          ) : (
            <button
              onClick={handleDisconnect}
              className="inline-flex items-center gap-1.5 px-3.5 py-1.5 rounded-lg text-xs font-semibold bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 transition-all active:scale-95"
            >
              <Power className="w-3.5 h-3.5 text-rose-400" />
              <span>ตัดการเชื่อมต่อ</span>
            </button>
          )}

          {/* Connection Status Pill */}
          <div
            className={`inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs font-medium border transition-colors ${
              isConnected
                ? 'bg-emerald-950/40 border-emerald-500/40 text-emerald-400'
                : 'bg-slate-900 border-slate-800 text-slate-400'
            }`}
          >
            <span
              className={`w-2 h-2 rounded-full ${
                isConnected ? 'bg-emerald-400 animate-pulse' : 'bg-slate-600'
              }`}
            />
            <span>{isConnected ? 'ONLINE' : 'OFFLINE'}</span>
          </div>

          {/* Robot State Badge */}
          <div
            className={`inline-flex items-center gap-1.5 px-3 py-1 rounded-lg text-xs font-semibold border ${
              robotState === 'WAIT_BTN'
                ? 'bg-amber-950/40 border-amber-500/50 text-amber-300 animate-pulse'
                : robotState === 'RUNNING'
                ? 'bg-cyan-950/40 border-cyan-500/50 text-cyan-300'
                : robotState === 'STOPPED'
                ? 'bg-rose-950/40 border-rose-500/50 text-rose-300'
                : 'bg-slate-900 border-slate-800 text-slate-300'
            }`}
          >
            {robotState === 'WAIT_BTN' && <Clock className="w-3.5 h-3.5" />}
            {robotState === 'RUNNING' && <Zap className="w-3.5 h-3.5 text-cyan-400" />}
            {robotState === 'IDLE' && <CheckCircle2 className="w-3.5 h-3.5 text-slate-400" />}
            {robotState === 'STOPPED' && <AlertOctagon className="w-3.5 h-3.5 text-rose-400" />}
            <span>
              {robotState === 'WAIT_BTN'
                ? `⏳ รอกดสวิตช์${roundCount > 0 ? ` (รอบ #${roundCount})` : ''}`
                : robotState === 'RUNNING'
                ? `⚡ กำลังวิ่ง...${roundCount > 0 ? ` (#${roundCount})` : ''}`
                : robotState === 'STOPPED'
                ? '🛑 STOPPED'
                : 'ROBOT: READY'}
            </span>
          </div>

          {/* Last Lap Time Pill */}
          {lastLapMs > 0 && (
            <div className="hidden sm:inline-flex items-center gap-1 px-2.5 py-1 rounded-lg text-xs font-mono bg-cyan-950/30 border border-cyan-500/30 text-cyan-300">
              <Clock className="w-3 h-3 text-cyan-400" />
              <span>{formatLapTime(lastLapMs)}</span>
            </div>
          )}

          {/* Emergency Stop Button */}
          <button
            onClick={handleEmergencyStop}
            className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-bold bg-rose-600 hover:bg-rose-500 text-white transition-all shadow-glow-red active:scale-95 border border-rose-400/40"
          >
            <AlertOctagon className="w-3.5 h-3.5" />
            <span>STOP</span>
          </button>
        </div>
      </div>
    </header>
  );
};
