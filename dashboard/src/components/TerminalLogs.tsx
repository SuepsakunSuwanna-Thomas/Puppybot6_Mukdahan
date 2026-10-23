import React, { useState, useEffect, useRef } from 'react';
import { serial } from '../services/serialService';
import { Terminal, Send, Trash2, ArrowDown } from 'lucide-react';

interface LogItem {
  id: string;
  msg: string;
  type: 'rx' | 'tx' | 'system' | 'warn' | 'err';
  time: string;
}

interface TerminalLogsProps {
  isConnected: boolean;
}

export const TerminalLogs: React.FC<TerminalLogsProps> = ({ isConnected }) => {
  const [logs, setLogs] = useState<LogItem[]>([]);
  const [inputCmd, setInputCmd] = useState<string>('');
  const [autoScroll, setAutoScroll] = useState<boolean>(true);
  const [filter, setFilter] = useState<'all' | 'rx' | 'tx'>('all');
  const scrollRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const unsubscribe = serial.subscribeLog((msg, type) => {
      const now = new Date();
      const timeStr = now.toTimeString().split(' ')[0] + '.' + String(now.getMilliseconds()).padStart(3, '0');
      setLogs((prev) => [...prev.slice(-300), {
        id: `log-${Date.now()}-${Math.random()}`,
        msg,
        type,
        time: timeStr,
      }]);
    });

    return () => unsubscribe();
  }, []);

  useEffect(() => {
    if (autoScroll && scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [logs, autoScroll]);

  const handleSendRaw = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!inputCmd.trim()) return;

    if (!isConnected) {
      alert('กรุณาเชื่อมต่อ COM Port ก่อนส่งคำสั่ง!');
      return;
    }

    try {
      if (inputCmd.startsWith('{') && inputCmd.endsWith('}')) {
        await serial.sendCommand(JSON.parse(inputCmd));
      } else {
        await serial.sendCommand({ cmd: inputCmd.trim() });
      }
      setInputCmd('');
    } catch (err: any) {
      alert(`คำสั่งผิดรูปแบบ: ${err.message}`);
    }
  };

  const filteredLogs = logs.filter((l) => {
    if (filter === 'all') return true;
    if (filter === 'rx') return l.type === 'rx';
    if (filter === 'tx') return l.type === 'tx';
    return true;
  });

  return (
    <div className="glass-panel rounded-xl p-5 border border-slate-800 space-y-3 flex flex-col h-[520px]">
      <div className="flex flex-wrap items-center justify-between gap-2 border-b border-slate-800 pb-3">
        <div className="flex items-center gap-2">
          <Terminal className="w-4 h-4 text-cyan-400" />
          <h3 className="text-base font-bold text-white">
            Serial Terminal &amp; Raw Command Debugger
          </h3>
        </div>

        <div className="flex items-center gap-2 text-xs">
          {/* Filter Pills */}
          <div className="flex items-center bg-slate-900 border border-slate-800 rounded-lg p-0.5">
            <button
              onClick={() => setFilter('all')}
              className={`px-2 py-0.5 rounded-md text-[11px] font-medium transition-colors ${
                filter === 'all' ? 'bg-cyan-500/20 text-cyan-300 font-bold' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              ทั้งหมด
            </button>
            <button
              onClick={() => setFilter('rx')}
              className={`px-2 py-0.5 rounded-md text-[11px] font-medium transition-colors ${
                filter === 'rx' ? 'bg-emerald-500/20 text-emerald-300 font-bold' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              RX (รับ)
            </button>
            <button
              onClick={() => setFilter('tx')}
              className={`px-2 py-0.5 rounded-md text-[11px] font-medium transition-colors ${
                filter === 'tx' ? 'bg-cyan-500/20 text-cyan-300 font-bold' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              TX (ส่ง)
            </button>
          </div>

          {/* Autoscroll */}
          <label className="flex items-center gap-1.5 cursor-pointer text-slate-400 hover:text-slate-200">
            <input
              type="checkbox"
              checked={autoScroll}
              onChange={(e) => setAutoScroll(e.target.checked)}
              className="rounded border-slate-700 bg-slate-900 text-cyan-500 focus:ring-0"
            />
            <span className="text-[11px]">Auto-scroll</span>
          </label>

          {/* Clear */}
          <button
            onClick={() => setLogs([])}
            className="p-1.5 rounded-lg hover:bg-slate-800 text-slate-400 hover:text-slate-200 transition-colors"
            title="ล้างข้อความ Terminal"
          >
            <Trash2 className="w-3.5 h-3.5" />
          </button>
        </div>
      </div>

      {/* Terminal Output Area */}
      <div
        ref={scrollRef}
        className="flex-1 bg-slate-950 rounded-xl p-3.5 overflow-y-auto border border-slate-800 font-mono text-[11px] space-y-1 select-text"
      >
        {filteredLogs.length === 0 ? (
          <div className="text-slate-600 text-center py-10 font-sans">
            ยังไม่มีข้อความ Serial Log &bull; ส่งคำสั่งหรือเปิดการเชื่อมต่อเพื่อรับข้อมูล
          </div>
        ) : (
          filteredLogs.map((log) => {
            const isRx = log.type === 'rx';
            const isTx = log.type === 'tx';
            const isErr = log.type === 'err';
            const isWarn = log.type === 'warn';

            return (
              <div key={log.id} className="flex items-start gap-2 leading-relaxed">
                <span className="text-slate-600 select-none text-[10px] w-20 flex-shrink-0">
                  {log.time}
                </span>
                <span
                  className={`select-none font-bold text-[10px] px-1 rounded flex-shrink-0 ${
                    isRx
                      ? 'bg-emerald-950/60 text-emerald-400 border border-emerald-500/40'
                      : isTx
                      ? 'bg-cyan-950/60 text-cyan-400 border border-cyan-500/40'
                      : isErr
                      ? 'bg-rose-950/60 text-rose-400 border border-rose-500/40'
                      : isWarn
                      ? 'bg-amber-950/60 text-amber-400 border border-amber-500/40'
                      : 'bg-slate-900 text-slate-400'
                  }`}
                >
                  {log.type.toUpperCase()}
                </span>
                <span
                  className={`break-all ${
                    isRx
                      ? 'text-emerald-300'
                      : isTx
                      ? 'text-cyan-300'
                      : isErr
                      ? 'text-rose-300'
                      : isWarn
                      ? 'text-amber-300'
                      : 'text-slate-400'
                  }`}
                >
                  {log.msg}
                </span>
              </div>
            );
          })
        )}
      </div>

      {/* Raw Command Input & Shortcuts */}
      <form onSubmit={handleSendRaw} className="space-y-2">
        <div className="flex items-center gap-2">
          <input
            type="text"
            value={inputCmd}
            onChange={(e) => setInputCmd(e.target.value)}
            placeholder='พิมพ์คำสั่ง raw string หรือ JSON เช่น {"cmd":"PING"} หรือ STOP...'
            className="flex-1 bg-slate-900 border border-slate-800 rounded-lg px-3 py-2 text-xs font-mono text-cyan-300 outline-none focus:border-cyan-500"
          />
          <button
            type="submit"
            disabled={!isConnected || !inputCmd.trim()}
            className="inline-flex items-center gap-1.5 px-4 py-2 rounded-lg text-xs font-bold bg-cyan-500 hover:bg-cyan-400 text-slate-950 transition-all shadow-glow-cyan active:scale-95 disabled:opacity-50"
          >
            <Send className="w-3.5 h-3.5" />
            <span>ส่ง</span>
          </button>
        </div>

        {/* Quick Command Chips */}
        <div className="flex flex-wrap items-center gap-1.5 text-[11px] text-slate-400">
          <span className="text-slate-500">ทางลัด:</span>
          {['PING', 'GET_CALIB', 'RESET_ENC', 'STOP'].map((cmdStr) => (
            <button
              key={cmdStr}
              type="button"
              onClick={() => serial.sendCommand({ cmd: cmdStr })}
              disabled={!isConnected}
              className="px-2 py-0.5 rounded bg-slate-900 hover:bg-slate-800 text-cyan-400 font-mono border border-slate-800 transition-colors disabled:opacity-40"
            >
              {cmdStr}
            </button>
          ))}
        </div>
      </form>
    </div>
  );
};
