import React from 'react';
import { LapLog } from '../types';
import { Timer, Trophy, Trash2, TrendingDown } from 'lucide-react';

interface LapHistoryProps {
  logs: LapLog[];
  onClear: () => void;
}

export const LapHistory: React.FC<LapHistoryProps> = ({ logs, onClear }) => {
  // Find best (fastest) lap
  const bestTime = logs.length > 0 ? Math.min(...logs.map(l => l.timeMs)) : null;

  return (
    <div className="glass-panel rounded-xl p-5 border border-slate-800 space-y-4">
      <div className="flex items-center justify-between border-b border-slate-800 pb-3">
        <div className="flex items-center gap-2">
          <Timer className="w-4 h-4 text-cyan-400" />
          <h3 className="text-base font-bold text-white">
            บันทึกเวลาต่อรอบ &amp; Benchmark (Lap Timer History)
          </h3>
          <span className="text-xs px-2 py-0.5 rounded-full font-mono bg-slate-800 text-slate-300">
            {logs.length} ROLLS
          </span>
        </div>

        {logs.length > 0 && (
          <button
            onClick={onClear}
            className="inline-flex items-center gap-1 text-xs text-slate-400 hover:text-rose-400 transition-colors"
          >
            <Trash2 className="w-3.5 h-3.5" />
            <span>ล้างประวัติ</span>
          </button>
        )}
      </div>

      {logs.length === 0 ? (
        <div className="text-center py-8 text-slate-500 text-xs">
          ยังไม่มีข้อมูลบันทึกเวลา &bull; เมื่อหุ่นยนต์วิ่งเสร็จแต่ละรอบ ระบบจะจับเวลาและบันทึกอัตโนมัติที่นี่
        </div>
      ) : (
        <div className="overflow-x-auto rounded-lg border border-slate-800">
          <table className="w-full text-left text-xs">
            <thead className="bg-slate-950 text-slate-400 text-[11px] border-b border-slate-800 font-mono">
              <tr>
                <th className="py-2.5 px-3">รอบที่</th>
                <th className="py-2.5 px-3">เวลาที่ใช้</th>
                <th className="py-2.5 px-3">คำสั่ง / โหมด</th>
                <th className="py-2.5 px-3">ความเร็ว</th>
                <th className="py-2.5 px-3 text-right">เวลาที่บันทึก</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-slate-800/60 bg-slate-900/40">
              {logs.map((log) => {
                const isBest = bestTime !== null && log.timeMs === bestTime;
                const sec = (log.timeMs / 1000).toFixed(3);

                return (
                  <tr key={log.id} className="hover:bg-slate-800/40 transition-colors">
                    <td className="py-2.5 px-3 font-mono font-bold text-cyan-300 flex items-center gap-1.5">
                      #{log.round}
                      {isBest && (
                        <span className="inline-flex items-center gap-0.5 px-1.5 py-0.2 rounded text-[9px] font-sans font-bold bg-amber-500/20 text-amber-300 border border-amber-500/40">
                          <Trophy className="w-2.5 h-2.5" /> BEST
                        </span>
                      )}
                    </td>
                    <td className="py-2.5 px-3 font-mono font-bold text-slate-200">
                      {sec}s <small className="text-slate-500">({log.timeMs}ms)</small>
                    </td>
                    <td className="py-2.5 px-3 text-slate-300">
                      <span className="font-mono text-cyan-400">{log.cmd}</span> &bull; {log.mode}
                    </td>
                    <td className="py-2.5 px-3 font-mono text-slate-400">
                      {log.speed ? `${log.speed}%` : '-'}
                    </td>
                    <td className="py-2.5 px-3 text-right text-slate-500 font-mono text-[11px]">
                      {log.timestamp}
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
};
