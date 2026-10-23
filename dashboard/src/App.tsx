import React, { useState, useEffect, useRef } from 'react';
import { serial } from './services/serialService';
import { TelemetryData, CalibData, RobotState, LapLog } from './types';
import { Header } from './components/Header';
import { TelemetryBar } from './components/TelemetryBar';
import { SequenceBuilder } from './components/SequenceBuilder';
import { PidTuner } from './components/PidTuner';
import { MotorControl } from './components/MotorControl';
import { ServoControl } from './components/ServoControl';
import { CalibrationPanel } from './components/CalibrationPanel';
import { LapHistory } from './components/LapHistory';
import { TerminalLogs } from './components/TerminalLogs';
import { 
  Bot, 
  Sliders, 
  Zap, 
  Cpu, 
  Wrench, 
  Timer, 
  Terminal, 
  HelpCircle,
  Sparkles,
  Info
} from 'lucide-react';

export const App: React.FC = () => {
  const [isConnected, setIsConnected] = useState<boolean>(false);
  const [robotState, setRobotState] = useState<RobotState>('DISCONNECTED');
  const [stateMessage, setStateMessage] = useState<string>('');
  const [roundCount, setRoundCount] = useState<number>(0);
  const [lastLapMs, setLastLapMs] = useState<number>(0);

  const [telem, setTelem] = useState<TelemetryData | null>(null);
  const [calib, setCalib] = useState<CalibData | null>(null);
  const telemRef = useRef<TelemetryData | null>(null);
  telemRef.current = telem;

  const [lapHistory, setLapHistory] = useState<LapLog[]>(() => {
    const saved = localStorage.getItem('puppybot_lap_history');
    if (saved) {
      try { return JSON.parse(saved); } catch {}
    }
    return [];
  });

  const [activeTab, setActiveTab] = useState<'sequence' | 'pid' | 'motor' | 'servo' | 'calib' | 'history' | 'terminal'>('sequence');

  // Subscriptions to SerialService
  useEffect(() => {
    const unsubConn = serial.subscribeConnection((connected) => {
      setIsConnected(connected);
      if (!connected) {
        setRobotState('DISCONNECTED');
      }
    });

    const unsubState = serial.subscribeState((state, message, round, timeMs) => {
      setRobotState(state);
      if (message) setStateMessage(message);
      if (round !== undefined) setRoundCount(round);
      if (timeMs !== undefined && timeMs > 0) {
        setLastLapMs(timeMs);

        // Record Lap Log
        const now = new Date();
        const timeStr = now.toLocaleTimeString('th-TH');
        const newLog: LapLog = {
          id: `lap-${Date.now()}`,
          timestamp: timeStr,
          round: round ?? 1,
          timeMs,
          cmd: telemRef.current?.task?.cmd || 'TEST_PID',
          mode: telemRef.current?.task?.mode || 'phase',
          speed: 60,
        };

        setLapHistory(prev => {
          const updated = [newLog, ...prev.slice(0, 49)];
          localStorage.setItem('puppybot_lap_history', JSON.stringify(updated));
          return updated;
        });
      }
    });

    const unsubTelem = serial.subscribeTelemetry((data) => {
      setTelem(data);
    });

    const unsubCalib = serial.subscribeCalib((data) => {
      setCalib(data);
    });

    return () => {
      unsubConn();
      unsubState();
      unsubTelem();
      unsubCalib();
    };
  }, []);

  return (
    <div className="min-h-screen bg-[#070b14] text-slate-100 flex flex-col font-sans relative overflow-x-hidden">
      {/* Background Neon Glowing Ambient Blobs */}
      <div className="fixed top-[-10%] left-[-5%] w-[45vw] h-[45vw] rounded-full bg-cyan-600/10 blur-[130px] pointer-events-none" />
      <div className="fixed bottom-[-10%] right-[-5%] w-[45vw] h-[45vw] rounded-full bg-indigo-600/10 blur-[130px] pointer-events-none" />

      {/* Header */}
      <Header
        robotState={robotState}
        stateMessage={stateMessage}
        roundCount={roundCount}
        lastLapMs={lastLapMs}
        isConnected={isConnected}
      />

      {/* Main Container */}
      <main className="flex-1 max-w-7xl w-full mx-auto px-4 lg:px-8 py-5">
        {/* Interactive Robot Workflow Banner */}
        <div className="mb-5 glass-panel rounded-xl p-3 border border-cyan-500/20 bg-gradient-to-r from-cyan-950/20 via-slate-900/40 to-indigo-950/20 flex items-center justify-between gap-3 text-xs">
          <div className="flex items-center gap-2.5">
            <span className="font-bold font-mono px-2 py-0.5 rounded bg-cyan-500/20 text-cyan-300 border border-cyan-500/30">
              WORKFLOW
            </span>
            <span className="text-slate-300">
              {robotState === 'WAIT_BTN' ? (
                <strong className="text-amber-300">
                  ⚠️ หุ่นยนต์รอกดสวิตช์: กดสวิตช์ที่ตัวหุ่นยนต์เพื่อเริ่มวิ่งได้ทันที (กดซ้ำเพื่อรันรอบใหม่ได้กี่รอบก็ได้!)
                </strong>
              ) : robotState === 'RUNNING' ? (
                <strong className="text-cyan-300">
                  ▶️ หุ่นยนต์กำลังทำงาน... กำลังจับเวลาความเร็วต่อรอบ
                </strong>
              ) : isConnected ? (
                <span>
                  1. เลือกสเต็ปวิ่ง &rarr; 2. ส่งคำสั่งไปยังบอร์ด &rarr; 3. กดสวิตช์ที่ตัวหุ่นยนต์เพื่อปล่อยตัว!
                </span>
              ) : (
                <span className="text-slate-400">
                  กรุณาคลิกปุ่ม <strong className="text-cyan-400">"เชื่อมต่อ COM Port"</strong> ด้านบนเพื่อเริ่มต้นใช้งาน
                </span>
              )}
            </span>
          </div>

          <div className="hidden sm:flex items-center gap-2 text-slate-400 font-mono text-[11px]">
            <Sparkles className="w-3.5 h-3.5 text-cyan-400" />
            <span>RP2350 150MHz Dual-Core</span>
          </div>
        </div>

        {/* Telemetry Dashboard Row */}
        <TelemetryBar telem={telem} />

        {/* Navigation Tabs */}
        <div className="flex items-center gap-1.5 overflow-x-auto pb-2 border-b border-slate-800/80 mb-5 scrollbar-none">
          <button
            onClick={() => setActiveTab('sequence')}
            className={`inline-flex items-center gap-2 px-4 py-2.5 rounded-xl text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'sequence'
                ? 'bg-cyan-500 text-slate-950 shadow-glow-cyan font-bold'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900/60'
            }`}
          >
            <Bot className="w-4 h-4" />
            <span>Mission Sequence Builder</span>
            <span className={`text-[10px] px-1.5 py-0.2 rounded font-mono ${
              activeTab === 'sequence' ? 'bg-slate-950/30 text-slate-900 font-bold' : 'bg-cyan-500/20 text-cyan-400'
            }`}>
              AUTO
            </span>
          </button>

          <button
            onClick={() => setActiveTab('pid')}
            className={`inline-flex items-center gap-2 px-4 py-2.5 rounded-xl text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'pid'
                ? 'bg-cyan-500 text-slate-950 shadow-glow-cyan font-bold'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900/60'
            }`}
          >
            <Sliders className="w-4 h-4" />
            <span>ทดสอบ PID เดี่ยว & Smart Tuner</span>
          </button>

          <button
            onClick={() => setActiveTab('motor')}
            className={`inline-flex items-center gap-2 px-4 py-2.5 rounded-xl text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'motor'
                ? 'bg-cyan-500 text-slate-950 shadow-glow-cyan font-bold'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900/60'
            }`}
          >
            <Zap className="w-4 h-4" />
            <span>ทดสอบการวิ่ง 4 ล้อ (4WD)</span>
          </button>

          <button
            onClick={() => setActiveTab('servo')}
            className={`inline-flex items-center gap-2 px-4 py-2.5 rounded-xl text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'servo'
                ? 'bg-cyan-500 text-slate-950 shadow-glow-cyan font-bold'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900/60'
            }`}
          >
            <Cpu className="w-4 h-4" />
            <span>ตั้งองศาเซอร์โว (Dual Servo)</span>
          </button>

          <button
            onClick={() => setActiveTab('calib')}
            className={`inline-flex items-center gap-2 px-4 py-2.5 rounded-xl text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'calib'
                ? 'bg-cyan-500 text-slate-950 shadow-glow-cyan font-bold'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900/60'
            }`}
          >
            <Wrench className="w-4 h-4" />
            <span>คาลิเบรตเซนเซอร์ & EEPROM</span>
          </button>

          <button
            onClick={() => setActiveTab('history')}
            className={`inline-flex items-center gap-2 px-4 py-2.5 rounded-xl text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'history'
                ? 'bg-cyan-500 text-slate-950 shadow-glow-cyan font-bold'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900/60'
            }`}
          >
            <Timer className="w-4 h-4" />
            <span>สถิติเวลา (Lap History)</span>
            {lapHistory.length > 0 && (
              <span className={`text-[10px] px-1.5 py-0.2 rounded font-mono ${
                activeTab === 'history' ? 'bg-slate-950/30 text-slate-900 font-bold' : 'bg-slate-800 text-slate-300'
              }`}>
                {lapHistory.length}
              </span>
            )}
          </button>

          <button
            onClick={() => setActiveTab('terminal')}
            className={`inline-flex items-center gap-2 px-4 py-2.5 rounded-xl text-xs font-semibold transition-all whitespace-nowrap ${
              activeTab === 'terminal'
                ? 'bg-cyan-500 text-slate-950 shadow-glow-cyan font-bold'
                : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900/60'
            }`}
          >
            <Terminal className="w-4 h-4" />
            <span>Serial Logs & Debug</span>
          </button>
        </div>

        {/* Tab Panes */}
        <div className="transition-all">
          {activeTab === 'sequence' && (
            <SequenceBuilder robotState={robotState} isConnected={isConnected} />
          )}

          {activeTab === 'pid' && (
            <PidTuner robotState={robotState} isConnected={isConnected} />
          )}

          {activeTab === 'motor' && (
            <MotorControl isConnected={isConnected} />
          )}

          {activeTab === 'servo' && (
            <ServoControl isConnected={isConnected} />
          )}

          {activeTab === 'calib' && (
            <CalibrationPanel calib={calib} isConnected={isConnected} />
          )}

          {activeTab === 'history' && (
            <LapHistory
              logs={lapHistory}
              onClear={() => {
                setLapHistory([]);
                localStorage.removeItem('puppybot_lap_history');
              }}
            />
          )}

          {activeTab === 'terminal' && (
            <TerminalLogs isConnected={isConnected} />
          )}
        </div>
      </main>

      {/* Footer */}
      <footer className="border-t border-slate-900/80 bg-slate-950/40 py-3.5 px-4 text-center text-xs text-slate-500 font-mono mt-auto">
        PuppyBot6 Pro Mission Control &bull; Powered by Next/React + Tailwind + shadcn/ui &bull; Web Serial API RP2350
      </footer>
    </div>
  );
};
