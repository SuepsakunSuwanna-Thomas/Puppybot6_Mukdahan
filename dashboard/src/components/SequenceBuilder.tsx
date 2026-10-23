import React, { useState, useEffect, useRef } from 'react';
import { SequenceStep, StepType, RobotState } from '../types';
import { serial } from '../services/serialService';
import { 
  Play, 
  Square, 
  Plus, 
  Trash2, 
  Copy, 
  ChevronUp, 
  ChevronDown, 
  Code, 
  Save, 
  FolderOpen, 
  Sparkles, 
  Check, 
  Clock, 
  ArrowRight, 
  Bot, 
  Zap, 
  Hourglass,
  Layers,
  Cpu,
  RotateCcw
} from 'lucide-react';

interface SequenceBuilderProps {
  robotState: RobotState;
  isConnected: boolean;
}

const DEFAULT_STEPS: SequenceStep[] = [
  {
    id: 'step-1',
    type: 'phase',
    title: '1. ออกจากจุดสตาร์ต ตามเส้น 30 cm',
    speed: 60,
    val: 30.0,
    kp: 0.40,
    kd: -1.0,
    status: 'idle',
  },
  {
    id: 'step-2',
    type: 'turnLeft',
    title: '2. เลี้ยวซ้าย 90 องศาเข้าแยก',
    speed: 50,
    val: 0,
    status: 'idle',
  },
  {
    id: 'step-3',
    type: 'pass',
    title: '3. วิ่งตามเส้นข้าม 1 เส้นตัด',
    speed: 65,
    val: 1,
    kp: 0.45,
    kd: -1.0,
    status: 'idle',
  },
  {
    id: 'step-4',
    type: 'delay',
    title: '4. หยุดนิ่งรอจัดตำแหน่ง 500 ms',
    speed: 0,
    val: 0,
    delayMs: 500,
    status: 'idle',
  },
  {
    id: 'step-5',
    type: 'servo',
    title: '5. สั่งเซอร์โวหนีบกระป๋อง (S1=120, S2=60)',
    speed: 0,
    val: 0,
    s1: 120,
    s2: 60,
    status: 'idle',
  },
  {
    id: 'step-6',
    type: 'turnRight',
    title: '6. เลี้ยวขวามุ่งหน้าจุดส่ง',
    speed: 50,
    val: 0,
    status: 'idle',
  },
  {
    id: 'step-7',
    type: 'phase',
    title: '7. วิ่งตรงเข้าโซนวางกระป๋อง 25 cm',
    speed: 55,
    val: 25.0,
    kp: 0.40,
    kd: -1.0,
    status: 'idle',
  },
];

export const SequenceBuilder: React.FC<SequenceBuilderProps> = ({ robotState, isConnected }) => {
  const [steps, setSteps] = useState<SequenceStep[]>(() => {
    const saved = localStorage.getItem('puppybot_sequence_v3');
    if (saved) {
      try { return JSON.parse(saved); } catch {}
    }
    return DEFAULT_STEPS;
  });

  const [activeStepIndex, setActiveStepIndex] = useState<number | null>(null);
  const [isSequenceRunning, setIsSequenceRunning] = useState<boolean>(false);
  const [showCodeModal, setShowCodeModal] = useState<boolean>(false);
  const [copiedCode, setCopiedCode] = useState<boolean>(false);
  const [totalElapsedMs, setTotalElapsedMs] = useState<number>(0);
  const [delayCountdown, setDelayCountdown] = useState<number | null>(null);

  const delayTimerRef = useRef<any>(null);
  const activeStepRef = useRef<number | null>(null);
  activeStepRef.current = activeStepIndex;

  const isRunningRef = useRef<boolean>(false);
  isRunningRef.current = isSequenceRunning;

  const stepsRef = useRef<SequenceStep[]>(steps);
  stepsRef.current = steps;

  // Auto-save to LocalStorage
  useEffect(() => {
    localStorage.setItem('puppybot_sequence_v3', JSON.stringify(steps));
  }, [steps]);

  // Clean up delay timer on unmount
  useEffect(() => {
    return () => {
      if (delayTimerRef.current) clearTimeout(delayTimerRef.current);
    };
  }, []);

  // Listen for Robot State completion to advance to the next step
  useEffect(() => {
    const unsubscribe = serial.subscribeState((state, msg, round, timeMs) => {
      if (isRunningRef.current && activeStepRef.current !== null) {
        const curIdx = activeStepRef.current;
        const curStep = stepsRef.current[curIdx];

        // Robot reported completion for active motion task
        if (state === 'IDLE' && timeMs !== undefined && timeMs > 0 && curStep && curStep.type !== 'delay') {
          // Mark current step done
          setSteps(prev => prev.map((s, idx) => {
            if (idx === curIdx) {
              return { ...s, status: 'done', elapsedMs: timeMs };
            }
            return s;
          }));

          setTotalElapsedMs(prev => prev + timeMs);

          // Advance to next step
          advanceToNextStep(curIdx + 1);
        }
      }
    });

    return () => { unsubscribe(); };
  }, []);

  const advanceToNextStep = (nextIndex: number) => {
    if (nextIndex < stepsRef.current.length) {
      setActiveStepIndex(nextIndex);
      setTimeout(() => {
        executeStep(stepsRef.current[nextIndex], nextIndex);
      }, 300);
    } else {
      // Sequence completed
      setIsSequenceRunning(false);
      setActiveStepIndex(null);
      setDelayCountdown(null);
    }
  };

  // Convert Step to Serial Command or Delay Execution
  const executeStep = async (step: SequenceStep, index: number) => {
    setSteps(prev => prev.map((s, idx) => idx === index ? { ...s, status: 'active' } : s));

    // Case 1: Delay
    if (step.type === 'delay') {
      const waitTime = step.delayMs && step.delayMs > 0 ? step.delayMs : 500;
      setDelayCountdown(waitTime);

      const startTime = Date.now();
      const interval = setInterval(() => {
        const remaining = Math.max(0, waitTime - (Date.now() - startTime));
        setDelayCountdown(remaining);
        if (remaining <= 0) {
          clearInterval(interval);
        }
      }, 50);

      delayTimerRef.current = setTimeout(() => {
        clearInterval(interval);
        setDelayCountdown(null);

        // Mark delay step done
        setSteps(prev => prev.map((s, idx) => idx === index ? { ...s, status: 'done', elapsedMs: waitTime } : s));
        setTotalElapsedMs(prev => prev + waitTime);

        // If part of full sequence, continue
        if (isRunningRef.current) {
          advanceToNextStep(index + 1);
        } else {
          setActiveStepIndex(null);
        }
      }, waitTime);
      return;
    }

    // Case 2: Servo
    if (step.type === 'servo') {
      await serial.sendCommand({
        cmd: 'SERVO',
        s1: step.s1 ?? 90,
        s2: step.s2 ?? 90,
      });

      // Servo finishes after brief delay
      setTimeout(() => {
        setSteps(prev => prev.map((s, idx) => idx === index ? { ...s, status: 'done', elapsedMs: 400 } : s));
        setTotalElapsedMs(prev => prev + 400);
        if (isRunningRef.current) {
          advanceToNextStep(index + 1);
        } else {
          setActiveStepIndex(null);
        }
      }, 450);
      return;
    }

    // Case 3: 4WD Motor
    if (step.type === 'motor') {
      const motorTime = step.motor_time && step.motor_time > 0 ? step.motor_time : 1000;
      await serial.sendCommand({
        cmd: 'TEST_MOTOR',
        m1: step.m1 ?? 0,
        m2: step.m2 ?? 0,
        m3: step.m3 ?? 0,
        m4: step.m4 ?? 0,
        time: motorTime,
      });
      return;
    }

    // Case 4: PID / Turn / CheckLine motion commands
    await serial.sendCommand({
      cmd: 'TEST_PID',
      mode: step.type,
      speed: step.speed,
      val: step.val,
      kp: step.kp ?? 0.40,
      kd: step.kd ?? -1.00,
    });
  };

  // Run Entire Sequence
  const handleStartSequence = async () => {
    if (!isConnected) {
      alert('กรุณาเชื่อมต่อ COM Port ก่อนรัน Sequence!');
      return;
    }
    if (steps.length === 0) return;

    // Reset status
    setSteps(prev => prev.map(s => ({ ...s, status: 'idle', elapsedMs: undefined })));
    setTotalElapsedMs(0);
    setIsSequenceRunning(true);
    setActiveStepIndex(0);

    // Run first step
    await executeStep(steps[0], 0);
  };

  const handleStopSequence = async () => {
    if (delayTimerRef.current) clearTimeout(delayTimerRef.current);
    setIsSequenceRunning(false);
    setActiveStepIndex(null);
    setDelayCountdown(null);
    await serial.emergencyStop();
  };

  // Add Step
  const handleAddStep = (type: StepType = 'phase') => {
    const newStep: SequenceStep = {
      id: `step-${Date.now()}-${Math.random()}`,
      type,
      title: getStepDefaultTitle(type),
      speed: type.includes('turn') ? 50 : 60,
      val: type === 'phase' || type === 'phase_b' ? 25.0 : type === 'pass' || type === 'pass_b' ? 1 : type === 'time' || type === 'follow_notline' || type === 'backward_notline' ? 1500 : 0,
      delayMs: 500,
      kp: 0.40,
      kd: -1.0,
      s1: 90,
      s2: 90,
      m1: 50,
      m2: 50,
      m3: 50,
      m4: 50,
      motor_time: 1000,
      status: 'idle',
    };
    setSteps(prev => [...prev, newStep]);
  };

  const handleDuplicateStep = (index: number) => {
    const target = steps[index];
    const copy: SequenceStep = {
      ...target,
      id: `step-${Date.now()}-${Math.random()}`,
      title: `${target.title} (สำเนา)`,
      status: 'idle',
      elapsedMs: undefined,
    };
    const next = [...steps];
    next.splice(index + 1, 0, copy);
    setSteps(next);
  };

  const handleDeleteStep = (index: number) => {
    setSteps(prev => prev.filter((_, idx) => idx !== index));
  };

  const handleMoveStep = (index: number, direction: 'up' | 'down') => {
    const targetIndex = direction === 'up' ? index - 1 : index + 1;
    if (targetIndex < 0 || targetIndex >= steps.length) return;
    const next = [...steps];
    const temp = next[index];
    next[index] = next[targetIndex];
    next[targetIndex] = temp;
    setSteps(next);
  };

  const handleUpdateStep = (index: number, updates: Partial<SequenceStep>) => {
    setSteps(prev => prev.map((s, idx) => idx === index ? { ...s, ...updates } : s));
  };

  // Generate 100% C++ Code for main.ino
  const generateCppCode = (): string => {
    let code = `// ============================================================================\n`;
    code += `// PUPPYBOT6 MISSION SEQUENCE — AUTO-GENERATED CODE\n`;
    code += `// วางฟังก์ชันนี้ใน main.ino เพื่อให้หุ่นยนต์รันต่อเนื่องบนสนามแข่งขัน\n`;
    code += `// ============================================================================\n\n`;
    code += `void runAutoMission() {\n`;
    code += `  buzzer(2000, 100); delay(100); buzzer(2400, 150);\n\n`;

    steps.forEach((step, idx) => {
      code += `  // Step ${idx + 1}: ${step.title}\n`;
      if (step.type.includes('phase') || step.type.includes('pass') || step.type === 'time') {
        if (step.kp && step.kp > 0) {
          code += `  line_KP = ${step.kp.toFixed(2)}f;\n`;
        }
        if (step.kd !== undefined && step.kd >= 0) {
          code += `  line_KD = ${step.kd.toFixed(2)}f;\n`;
        }
      }

      switch (step.type) {
        case 'phase':
          code += `  Fline_by_phase(${step.speed}, ${step.val.toFixed(1)}f);\n`;
          break;
        case 'pass':
          code += `  Fline_by_pass(${step.speed}, ${Math.round(step.val)});\n`;
          break;
        case 'time':
          code += `  Fline_by_time(${step.speed}, ${Math.round(step.val)});\n`;
          break;
        case 'turnLeft':
          code += `  turnLeft(${step.speed});\n`;
          break;
        case 'turnRight':
          code += `  turnRight(${step.speed});\n`;
          break;
        case 'turnLeft_by_stop':
          code += `  turnLeft_by_stop(${step.speed});\n`;
          break;
        case 'turnRight_by_stop':
          code += `  turnRight_by_stop(${step.speed});\n`;
          break;
        case 'checkLine':
          code += `  checkLine();\n`;
          break;
        case 'phase_b':
          code += `  Fline_by_phase_B(${step.speed}, ${step.val.toFixed(1)}f);\n`;
          break;
        case 'pass_b':
          code += `  Fline_by_pass_B(${step.speed}, ${Math.round(step.val)});\n`;
          break;
        case 'turnLeft_b':
          code += `  turnLeft_B(${step.speed});\n`;
          break;
        case 'turnRight_b':
          code += `  turnRight_B(${step.speed});\n`;
          break;
        case 'checkLine_b':
          code += `  checkLine_B();\n`;
          break;
        case 'follow_notline':
          code += `  Follow_Notline(${step.speed}, ${Math.round(step.val)});\n`;
          break;
        case 'follow_notline_enc':
          code += `  Follow_Notline_by_Encoder(${step.speed}, ${Math.round(step.val)});\n`;
          break;
        case 'backward_notline':
          code += `  Backword_Notline(${step.speed}, ${Math.round(step.val)});\n`;
          break;
        case 'servo':
          code += `  servoRun(1, ${step.s1 ?? 90});\n`;
          code += `  servoRun(2, ${step.s2 ?? 90});\n`;
          code += `  delay(350);\n`;
          break;
        case 'motor':
          code += `  drive4(${step.m1 ?? 0}, ${step.m2 ?? 0}, ${step.m3 ?? 0}, ${step.m4 ?? 0});\n`;
          code += `  delay(${step.motor_time ?? 1000});\n`;
          code += `  stop_4w();\n`;
          break;
        case 'delay':
          code += `  delay(${step.delayMs ?? 500});\n`;
          break;
      }
      code += `\n`;
    });

    code += `  // สิ้นสุดภารกิจ หยุดมอเตอร์ทั้งหมด\n`;
    code += `  stop_4w();\n`;
    code += `  buzzer(2400, 200);\n`;
    code += `}\n`;
    return code;
  };

  const handleCopyCode = () => {
    navigator.clipboard.writeText(generateCppCode());
    setCopiedCode(true);
    setTimeout(() => setCopiedCode(false), 2000);
  };

  return (
    <div className="space-y-4">
      {/* Top Action Toolbar */}
      <div className="glass-panel rounded-xl p-4 border border-slate-800 flex flex-col md:flex-row items-center justify-between gap-3">
        <div>
          <div className="flex items-center gap-2">
            <h2 className="text-base font-bold text-white flex items-center gap-2">
              <Bot className="w-4 h-4 text-cyan-400" />
              <span>Mission Sequence Builder (ระบบตั้งขั้นตอนวิ่งต่อเนื่อง)</span>
            </h2>
            <span className="text-xs px-2 py-0.5 rounded-full font-mono bg-cyan-950/60 text-cyan-300 border border-cyan-800/40 font-bold">
              {steps.length} STEPS
            </span>
          </div>
          <p className="text-xs text-slate-400 mt-0.5">
            กำหนดลำดับคำสั่งวิ่งต่อกันอัตโนมัติ (วิ่ง &rarr; เลี้ยว &rarr; จัดเส้น &rarr; ยกกระป๋อง &rarr; หน่วงเวลา) กรอกตัวเลขได้โดยตรงทุกช่อง
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-2 w-full md:w-auto justify-end">
          {/* Run Sequence / Stop Button */}
          {!isSequenceRunning ? (
            <button
              onClick={handleStartSequence}
              disabled={!isConnected || steps.length === 0}
              className="inline-flex items-center gap-2 px-4 py-2 rounded-xl text-xs font-bold bg-cyan-500 hover:bg-cyan-400 text-slate-950 transition-all shadow-glow-cyan active:scale-95 disabled:opacity-50 disabled:pointer-events-none"
            >
              <Play className="w-3.5 h-3.5 fill-current" />
              <span>รัน Sequence ทั้งหมด</span>
            </button>
          ) : (
            <button
              onClick={handleStopSequence}
              className="inline-flex items-center gap-2 px-4 py-2 rounded-xl text-xs font-bold bg-rose-600 hover:bg-rose-500 text-white transition-all shadow-glow-red active:scale-95 animate-pulse"
            >
              <Square className="w-3.5 h-3.5 fill-current" />
              <span>หยุด Sequence ทันที</span>
            </button>
          )}

          {/* Export C++ Code Button */}
          <button
            onClick={() => setShowCodeModal(true)}
            className="inline-flex items-center gap-1.5 px-3 py-2 rounded-xl text-xs font-semibold bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 transition-all active:scale-95"
            title="แปลงสเต็ปเป็นโค้ด C++ สำหรับ main.ino"
          >
            <Code className="w-3.5 h-3.5 text-cyan-400" />
            <span>Export C++</span>
          </button>

          {/* Add Step Button */}
          <button
            onClick={() => handleAddStep('phase')}
            className="inline-flex items-center gap-1.5 px-3.5 py-2 rounded-xl text-xs font-bold bg-cyan-600/20 hover:bg-cyan-600/30 text-cyan-300 border border-cyan-500/30 transition-all active:scale-95"
          >
            <Plus className="w-3.5 h-3.5" />
            <span>เพิ่มคำสั่ง (+)</span>
          </button>
        </div>
      </div>

      {/* Sequence Progress Bar (when running) */}
      {isSequenceRunning && (
        <div className="glass-panel-glow rounded-xl p-3.5 border border-cyan-500/40">
          <div className="flex items-center justify-between text-xs mb-1.5">
            <span className="font-semibold text-cyan-300 flex items-center gap-2">
              <Zap className="w-4 h-4 text-cyan-400 animate-spin" />
              กำลังรันสเต็ป #{activeStepIndex !== null ? activeStepIndex + 1 : 0} จาก {steps.length}: {steps[activeStepIndex ?? 0]?.title}
            </span>
            <div className="flex items-center gap-2 font-mono text-cyan-400 font-bold">
              {delayCountdown !== null && (
                <span className="text-amber-300 bg-amber-950/40 px-2 py-0.5 rounded border border-amber-500/30">
                  ⏳ พัก {(delayCountdown / 1000).toFixed(1)}s
                </span>
              )}
              <span>⏱️ รวม: {(totalElapsedMs / 1000).toFixed(2)}s</span>
            </div>
          </div>
          <div className="w-full bg-slate-800 h-2 rounded-full overflow-hidden">
            <div 
              className="bg-gradient-to-r from-cyan-400 to-indigo-500 h-full transition-all duration-300"
              style={{ width: `${(((activeStepIndex ?? 0) + 1) / steps.length) * 100}%` }}
            />
          </div>
        </div>
      )}

      {/* Steps List */}
      <div className="space-y-3">
        {steps.map((step, index) => {
          const isActive = isSequenceRunning && activeStepIndex === index;
          const isDone = step.status === 'done';

          return (
            <div
              key={step.id}
              className={`rounded-xl p-4 border transition-all ${
                isActive
                  ? 'bg-slate-900/95 border-cyan-500 shadow-glow-cyan'
                  : isDone
                  ? 'bg-slate-900/60 border-emerald-500/40'
                  : 'bg-slate-900/40 border-slate-800/80 hover:border-slate-700/80'
              }`}
            >
              <div className="flex flex-col gap-3">
                {/* Row 1: Step Number, Type Selector, Title Input, Move/Delete Controls */}
                <div className="flex flex-wrap items-center justify-between gap-2.5">
                  <div className="flex items-center gap-2.5 flex-1 min-w-[280px]">
                    <div className={`flex items-center justify-center w-7 h-7 rounded-lg font-mono text-xs font-bold border flex-shrink-0 ${
                      isActive
                        ? 'bg-cyan-500 text-slate-950 border-cyan-300'
                        : isDone
                        ? 'bg-emerald-950/80 text-emerald-300 border-emerald-500/50'
                        : 'bg-slate-800 text-slate-300 border-slate-700'
                    }`}>
                      #{index + 1}
                    </div>

                    {/* Step Type Selector */}
                    <select
                      value={step.type}
                      onChange={(e) => {
                        const newType = e.target.value as StepType;
                        handleUpdateStep(index, { 
                          type: newType, 
                          title: getStepDefaultTitle(newType),
                          val: newType === 'phase' || newType === 'phase_b' ? 25.0 : newType === 'pass' || newType === 'pass_b' ? 1 : newType === 'time' || newType === 'follow_notline' || newType === 'backward_notline' ? 1500 : 0,
                          delayMs: newType === 'delay' ? (step.delayMs || 500) : step.delayMs
                        });
                      }}
                      className="bg-slate-950 border border-slate-700 text-slate-200 text-xs rounded-lg px-2.5 py-1.5 outline-none font-semibold focus:border-cyan-500"
                    >
                      <optgroup label="เดินหน้าตามเส้น (Forward Line)">
                        <option value="phase">🚀 วิ่งตามเส้นตามระยะ (Phase cm)</option>
                        <option value="pass">🏁 วิ่งตามเส้นข้ามเส้นตัด (Pass)</option>
                        <option value="time">⏱️ วิ่งตามเส้นตามเวลา (Time ms)</option>
                        <option value="turnLeft">↩️ เลี้ยวซ้าย 90° (Turn Left)</option>
                        <option value="turnRight">↪️ เลี้ยวขวา 90° (Turn Right)</option>
                        <option value="turnLeft_by_stop">🛑 เลี้ยวซ้ายหยุดนิ่ง (Turn L by Stop)</option>
                        <option value="turnRight_by_stop">🛑 เลี้ยวขวาหยุดนิ่ง (Turn R by Stop)</option>
                        <option value="checkLine">🔍 วิ่งหาเส้นตัดหน้า (Check Line A3)</option>
                      </optgroup>
                      <optgroup label="ถอยหลังตามเส้น (Backward Line)">
                        <option value="phase_b">🔙 ถอยหลังตามเส้นตามระยะ (Phase Back cm)</option>
                        <option value="pass_b">🔙 ถอยหลังตามเส้นข้ามแยก (Pass Back)</option>
                        <option value="turnLeft_b">↩️ ถอยหลังเลี้ยวซ้าย (Turn Left B)</option>
                        <option value="turnRight_b">↪️ ถอยหลังเลี้ยวขวา (Turn Right B)</option>
                        <option value="checkLine_b">🔍 จัดเซนเซอร์หลังตรงเส้น (Check Line A10)</option>
                      </optgroup>
                      <optgroup label="วิ่งตรงไม่ดูเส้น (Not-Line)">
                        <option value="follow_notline">⬆️ วิ่งตรงไม่ดูเส้นตามเวลา (Follow Not-line ms)</option>
                        <option value="follow_notline_enc">⬆️ วิ่งตรงไม่ดูเส้นตาม Encoder (cm)</option>
                        <option value="backward_notline">⬇️ ถอยหลังตรงไม่ดูเส้นตามเวลา (ms)</option>
                      </optgroup>
                      <optgroup label="กลไกและระบบหยุด (Mechanisms & Flow)">
                        <option value="servo">🦾 สั่งองศาเซอร์โว S1 & S2 (แขน/กริปเปอร์)</option>
                        <option value="motor">⚡ หมุนมอเตอร์ 4WD แยก 4 ล้อ (ตามเวลา ms)</option>
                        <option value="delay">⏳ หน่วงเวลาพัก (Delay ms)</option>
                      </optgroup>
                    </select>

                    {/* Step Description Input */}
                    <input
                      type="text"
                      value={step.title}
                      onChange={(e) => handleUpdateStep(index, { title: e.target.value })}
                      placeholder="คำอธิบายขั้นตอน..."
                      className="flex-1 bg-slate-950/60 border border-slate-800 focus:border-cyan-500 rounded-lg text-xs text-slate-200 outline-none px-2.5 py-1.5"
                    />
                  </div>

                  {/* Move, Duplicate, Delete, Single Step Run Buttons */}
                  <div className="flex items-center gap-1.5 bg-slate-950/40 p-1 rounded-lg border border-slate-800">
                    <button
                      onClick={() => executeStep(step, index)}
                      disabled={!isConnected}
                      className="inline-flex items-center gap-1 px-2 py-1 rounded bg-cyan-950/50 hover:bg-cyan-900/60 text-cyan-300 font-mono text-xs border border-cyan-800/40 transition-colors disabled:opacity-40"
                      title="ส่งคำสั่งขั้นตอนนี้เดี่ยวๆ"
                    >
                      <Play className="w-3 h-3 fill-current" />
                      <span>รันขั้นนี้</span>
                    </button>
                    <button
                      onClick={() => handleMoveStep(index, 'up')}
                      disabled={index === 0}
                      className="p-1 rounded hover:bg-slate-800 text-slate-400 hover:text-slate-200 disabled:opacity-30"
                      title="เลื่อนขึ้น"
                    >
                      <ChevronUp className="w-3.5 h-3.5" />
                    </button>
                    <button
                      onClick={() => handleMoveStep(index, 'down')}
                      disabled={index === steps.length - 1}
                      className="p-1 rounded hover:bg-slate-800 text-slate-400 hover:text-slate-200 disabled:opacity-30"
                      title="เลื่อนลง"
                    >
                      <ChevronDown className="w-3.5 h-3.5" />
                    </button>
                    <button
                      onClick={() => handleDuplicateStep(index)}
                      className="p-1 rounded hover:bg-slate-800 text-slate-400 hover:text-slate-200"
                      title="ทำซ้ำขั้นตอนนี้"
                    >
                      <Copy className="w-3.5 h-3.5" />
                    </button>
                    <button
                      onClick={() => handleDeleteStep(index)}
                      className="p-1 rounded hover:bg-rose-950 text-slate-400 hover:text-rose-400"
                      title="ลบสเต็ปนี้"
                    >
                      <Trash2 className="w-3.5 h-3.5" />
                    </button>
                  </div>
                </div>

                {/* Row 2: Editable Numeric Parameters for this Step */}
                <div className="flex flex-wrap items-center gap-3 bg-slate-950/70 p-2.5 rounded-lg border border-slate-800/80 text-xs">
                  {/* If motion command: Speed */}
                  {step.type !== 'servo' && step.type !== 'delay' && step.type !== 'motor' && (
                    <div className="flex items-center gap-1.5">
                      <span className="text-slate-400">Speed:</span>
                      <input
                        type="number"
                        min="10"
                        max="100"
                        value={step.speed}
                        onChange={(e) => handleUpdateStep(index, { speed: parseInt(e.target.value) || 10 })}
                        className="w-16 bg-slate-900 border border-slate-700 rounded px-2 py-0.5 text-cyan-300 font-mono font-bold outline-none text-right focus:border-cyan-500"
                      />
                      <span className="text-slate-500 font-mono">%</span>
                    </div>
                  )}

                  {/* If phase / phase_b: Distance (cm) */}
                  {(step.type === 'phase' || step.type === 'phase_b' || step.type === 'follow_notline_enc') && (
                    <div className="flex items-center gap-1.5">
                      <span className="text-slate-400">ระยะทาง:</span>
                      <input
                        type="number"
                        step="0.5"
                        min="1"
                        max="500"
                        value={step.val}
                        onChange={(e) => handleUpdateStep(index, { val: parseFloat(e.target.value) || 0 })}
                        className="w-18 bg-slate-900 border border-slate-700 rounded px-2 py-0.5 text-emerald-300 font-mono font-bold outline-none text-right focus:border-emerald-500"
                      />
                      <span className="text-slate-500 font-mono">cm</span>
                    </div>
                  )}

                  {/* If pass / pass_b: Count */}
                  {(step.type === 'pass' || step.type === 'pass_b') && (
                    <div className="flex items-center gap-1.5">
                      <span className="text-slate-400">ข้ามเส้นตัด:</span>
                      <input
                        type="number"
                        min="1"
                        max="20"
                        value={step.val}
                        onChange={(e) => handleUpdateStep(index, { val: parseInt(e.target.value) || 1 })}
                        className="w-16 bg-slate-900 border border-slate-700 rounded px-2 py-0.5 text-emerald-300 font-mono font-bold outline-none text-right focus:border-emerald-500"
                      />
                      <span className="text-slate-500 font-mono">เส้น</span>
                    </div>
                  )}

                  {/* If time / follow_notline / backward_notline: Duration (ms) */}
                  {(step.type === 'time' || step.type === 'follow_notline' || step.type === 'backward_notline') && (
                    <div className="flex items-center gap-1.5">
                      <span className="text-slate-400">ระยะเวลา:</span>
                      <input
                        type="number"
                        step="100"
                        min="100"
                        max="30000"
                        value={step.val}
                        onChange={(e) => handleUpdateStep(index, { val: parseInt(e.target.value) || 1000 })}
                        className="w-20 bg-slate-900 border border-slate-700 rounded px-2 py-0.5 text-amber-300 font-mono font-bold outline-none text-right focus:border-amber-500"
                      />
                      <span className="text-slate-500 font-mono">ms</span>
                    </div>
                  )}

                  {/* If Delay: DELAY MS DIRECT NUMERIC INPUT (USER REQUESTED!) */}
                  {step.type === 'delay' && (
                    <div className="flex flex-wrap items-center gap-2">
                      <span className="text-slate-300 font-semibold flex items-center gap-1">
                        <Hourglass className="w-3.5 h-3.5 text-amber-400" />
                        หน่วงเวลาพัก (Delay Time):
                      </span>
                      <input
                        type="number"
                        step="50"
                        min="50"
                        max="10000"
                        value={step.delayMs ?? 500}
                        onChange={(e) => handleUpdateStep(index, { delayMs: parseInt(e.target.value) || 100 })}
                        className="w-24 bg-slate-900 border border-slate-700 rounded px-2 py-1 text-amber-300 font-mono font-bold outline-none text-right focus:border-amber-500"
                      />
                      <span className="text-slate-400 font-mono">ms</span>

                      {/* Quick Delay Presets */}
                      <div className="flex items-center gap-1 ml-2">
                        {[200, 500, 1000, 2000].map(d => (
                          <button
                            key={d}
                            type="button"
                            onClick={() => handleUpdateStep(index, { delayMs: d })}
                            className={`px-2 py-0.5 rounded text-[11px] font-mono transition-colors ${step.delayMs === d ? 'bg-amber-500 text-slate-950 font-bold' : 'bg-slate-800 hover:bg-slate-700 text-slate-300'}`}
                          >
                            {d}ms
                          </button>
                        ))}
                      </div>
                    </div>
                  )}

                  {/* If Servo: S1 and S2 angles */}
                  {step.type === 'servo' && (
                    <div className="flex flex-wrap items-center gap-3">
                      <div className="flex items-center gap-1.5">
                        <span className="text-slate-400">S1 (มือจับ):</span>
                        <input
                          type="number"
                          min="0"
                          max="180"
                          value={step.s1 ?? 90}
                          onChange={(e) => handleUpdateStep(index, { s1: parseInt(e.target.value) || 0 })}
                          className="w-16 bg-slate-900 border border-slate-700 rounded px-2 py-0.5 text-indigo-300 font-mono font-bold outline-none text-right focus:border-indigo-500"
                        />
                        <span className="text-slate-500 font-mono">&deg;</span>
                      </div>
                      <div className="flex items-center gap-1.5">
                        <span className="text-slate-400">S2 (แขนยก):</span>
                        <input
                          type="number"
                          min="0"
                          max="180"
                          value={step.s2 ?? 90}
                          onChange={(e) => handleUpdateStep(index, { s2: parseInt(e.target.value) || 0 })}
                          className="w-16 bg-slate-900 border border-slate-700 rounded px-2 py-0.5 text-indigo-300 font-mono font-bold outline-none text-right focus:border-indigo-500"
                        />
                        <span className="text-slate-500 font-mono">&deg;</span>
                      </div>

                      {/* Servo Quick Presets */}
                      <div className="flex items-center gap-1">
                        <button
                          type="button"
                          onClick={() => handleUpdateStep(index, { s1: 120, s2: 60 })}
                          className="px-2 py-0.5 rounded text-[11px] bg-slate-800 hover:bg-slate-700 text-slate-300"
                        >
                          หนีบ
                        </button>
                        <button
                          type="button"
                          onClick={() => handleUpdateStep(index, { s1: 50, s2: 146 })}
                          className="px-2 py-0.5 rounded text-[11px] bg-slate-800 hover:bg-slate-700 text-slate-300"
                        >
                          ปล่อย
                        </button>
                        <button
                          type="button"
                          onClick={() => handleUpdateStep(index, { s1: 140, s2: 140 })}
                          className="px-2 py-0.5 rounded text-[11px] bg-slate-800 hover:bg-slate-700 text-slate-300"
                        >
                          ยกขึ้น
                        </button>
                      </div>
                    </div>
                  )}

                  {/* If 4WD Motor: M1, M2, M3, M4 & Time */}
                  {step.type === 'motor' && (
                    <div className="flex flex-wrap items-center gap-2">
                      <span className="text-slate-400">M1:</span>
                      <input
                        type="number"
                        min="-100"
                        max="100"
                        value={step.m1 ?? 0}
                        onChange={(e) => handleUpdateStep(index, { m1: parseInt(e.target.value) || 0 })}
                        className="w-14 bg-slate-900 border border-slate-700 rounded px-1.5 py-0.5 text-cyan-300 font-mono text-right"
                      />
                      <span className="text-slate-400">M2:</span>
                      <input
                        type="number"
                        min="-100"
                        max="100"
                        value={step.m2 ?? 0}
                        onChange={(e) => handleUpdateStep(index, { m2: parseInt(e.target.value) || 0 })}
                        className="w-14 bg-slate-900 border border-slate-700 rounded px-1.5 py-0.5 text-cyan-300 font-mono text-right"
                      />
                      <span className="text-slate-400">M3:</span>
                      <input
                        type="number"
                        min="-100"
                        max="100"
                        value={step.m3 ?? 0}
                        onChange={(e) => handleUpdateStep(index, { m3: parseInt(e.target.value) || 0 })}
                        className="w-14 bg-slate-900 border border-slate-700 rounded px-1.5 py-0.5 text-cyan-300 font-mono text-right"
                      />
                      <span className="text-slate-400">M4:</span>
                      <input
                        type="number"
                        min="-100"
                        max="100"
                        value={step.m4 ?? 0}
                        onChange={(e) => handleUpdateStep(index, { m4: parseInt(e.target.value) || 0 })}
                        className="w-14 bg-slate-900 border border-slate-700 rounded px-1.5 py-0.5 text-cyan-300 font-mono text-right"
                      />
                      <span className="text-slate-400">เวลา:</span>
                      <input
                        type="number"
                        min="100"
                        max="10000"
                        step="100"
                        value={step.motor_time ?? 1000}
                        onChange={(e) => handleUpdateStep(index, { motor_time: parseInt(e.target.value) || 1000 })}
                        className="w-18 bg-slate-900 border border-slate-700 rounded px-1.5 py-0.5 text-amber-300 font-mono text-right"
                      />
                      <span className="text-slate-500 font-mono">ms</span>
                    </div>
                  )}

                  {/* Kp & Kd for PID steps */}
                  {(step.type.includes('phase') || step.type.includes('pass') || step.type === 'time') && (
                    <div className="flex items-center gap-3 border-l border-slate-800 pl-3">
                      <div className="flex items-center gap-1">
                        <span className="text-slate-400">Kp:</span>
                        <input
                          type="number"
                          step="0.01"
                          value={step.kp ?? 0.40}
                          onChange={(e) => handleUpdateStep(index, { kp: parseFloat(e.target.value) || 0.40 })}
                          className="w-16 bg-slate-900 border border-slate-700 rounded px-1.5 py-0.5 text-slate-200 font-mono text-right"
                        />
                      </div>
                      <div className="flex items-center gap-1">
                        <span className="text-slate-400">Kd:</span>
                        <input
                          type="number"
                          step="0.1"
                          value={step.kd ?? -1.0}
                          onChange={(e) => handleUpdateStep(index, { kd: parseFloat(e.target.value) || 0 })}
                          className="w-16 bg-slate-900 border border-slate-700 rounded px-1.5 py-0.5 text-slate-200 font-mono text-right"
                        />
                        <span className="text-[10px] text-slate-500 font-mono">(-1=Auto)</span>
                      </div>
                    </div>
                  )}

                  {/* Status Indicator */}
                  {step.elapsedMs && step.elapsedMs > 0 && (
                    <span className="ml-auto px-2 py-0.5 rounded font-mono text-xs bg-emerald-950/60 text-emerald-300 border border-emerald-500/40">
                      ⏱️ {(step.elapsedMs / 1000).toFixed(2)}s
                    </span>
                  )}
                </div>
              </div>
            </div>
          );
        })}
      </div>

      {/* Empty State */}
      {steps.length === 0 && (
        <div className="text-center py-12 glass-panel rounded-xl border border-dashed border-slate-800">
          <Bot className="w-10 h-10 text-slate-600 mx-auto mb-2" />
          <p className="text-sm text-slate-400 font-medium">ยังไม่มีขั้นตอนใน Sequence</p>
          <p className="text-xs text-slate-500 mb-4">กดปุ่มเพิ่มคำสั่งด้านล่างเพื่อเริ่มวางแผนเส้นทางการวิ่ง</p>
          <button
            onClick={() => handleAddStep('phase')}
            className="px-4 py-2 rounded-xl text-xs font-semibold bg-cyan-500 text-slate-950 hover:bg-cyan-400 transition-colors"
          >
            + เพิ่มสเต็ปแรก
          </button>
        </div>
      )}

      {/* Bottom Presets and Controls */}
      <div className="flex flex-wrap items-center justify-between gap-2 pt-2 border-t border-slate-800/80 text-xs">
        <div className="flex flex-wrap items-center gap-1.5 text-slate-400">
          <Sparkles className="w-3.5 h-3.5 text-cyan-400" />
          <span>เทมเพลตด่วน:</span>
          <button
            onClick={() => setSteps(DEFAULT_STEPS)}
            className="px-2.5 py-1 rounded-lg bg-slate-900 hover:bg-slate-800 text-slate-300 border border-slate-800 transition-colors"
          >
            ภารกิจ Logistics รุ่น 19 (7 สเต็ปครบ)
          </button>
          <button
            onClick={() => setSteps([
              { id: '1', type: 'phase', title: 'ตรงไป 30cm', speed: 65, val: 30, delayMs: 500, status: 'idle' },
              { id: '2', type: 'turnLeft', title: 'เลี้ยวซ้าย', speed: 50, val: 0, delayMs: 500, status: 'idle' },
              { id: '3', type: 'phase', title: 'ตรงไป 30cm', speed: 65, val: 30, delayMs: 500, status: 'idle' },
              { id: '4', type: 'turnRight', title: 'เลี้ยวขวา', speed: 50, val: 0, delayMs: 500, status: 'idle' },
            ])}
            className="px-2.5 py-1 rounded-lg bg-slate-900 hover:bg-slate-800 text-slate-300 border border-slate-800 transition-colors"
          >
            ทดสอบซิกแซก ซ้าย-ขวา
          </button>
        </div>

        <button
          onClick={() => {
            if (confirm('ต้องการล้างสเต็ปทั้งหมดใช่หรือไม่?')) {
              setSteps([]);
            }
          }}
          className="text-slate-500 hover:text-rose-400 transition-colors text-xs"
        >
          ล้างทั้งหมด
        </button>
      </div>

      {/* C++ Export Modal */}
      {showCodeModal && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/80 backdrop-blur-sm animate-in fade-in">
          <div className="bg-slate-900 border border-slate-700 rounded-2xl w-full max-w-3xl overflow-hidden shadow-2xl flex flex-col max-h-[85vh]">
            <div className="flex items-center justify-between px-5 py-3.5 border-b border-slate-800 bg-slate-950/50">
              <div className="flex items-center gap-2">
                <Code className="w-4 h-4 text-cyan-400" />
                <h3 className="text-sm font-bold text-white">
                  Export C++ Code (สำหรับวางใน main.ino)
                </h3>
              </div>
              <button
                onClick={() => setShowCodeModal(false)}
                className="text-slate-400 hover:text-white p-1 rounded-lg transition-colors text-lg"
              >
                &times;
              </button>
            </div>

            <div className="p-4 flex-1 overflow-auto">
              <pre className="bg-slate-950 p-4 rounded-xl text-xs font-mono text-cyan-300 overflow-x-auto border border-slate-800 leading-relaxed">
                {generateCppCode()}
              </pre>
            </div>

            <div className="flex items-center justify-between px-5 py-3 border-t border-slate-800 bg-slate-950/50">
              <span className="text-xs text-slate-400">
                {steps.length} ขั้นตอน &bull; พร้อมนำไปสั่งรันบนสนามแข่งขัน
              </span>
              <div className="flex items-center gap-2">
                <button
                  onClick={handleCopyCode}
                  className="inline-flex items-center gap-1.5 px-4 py-2 rounded-xl text-xs font-bold bg-cyan-500 hover:bg-cyan-400 text-slate-950 transition-all shadow-glow-cyan"
                >
                  {copiedCode ? <Check className="w-3.5 h-3.5" /> : <Copy className="w-3.5 h-3.5" />}
                  <span>{copiedCode ? 'คัดลอกเรียบร้อยแล้ว!' : 'คัดลอกโค้ด C++'}</span>
                </button>
                <button
                  onClick={() => setShowCodeModal(false)}
                  className="px-3.5 py-2 rounded-xl text-xs font-semibold bg-slate-800 hover:bg-slate-700 text-slate-300 transition-colors"
                >
                  ปิดหน้าต่าง
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

function getStepDefaultTitle(type: StepType): string {
  switch (type) {
    case 'phase': return 'วิ่งตามเส้นตามระยะทาง (cm)';
    case 'pass': return 'วิ่งตามเส้นข้ามเส้นตัด';
    case 'time': return 'วิ่งตามเส้นตามเวลา (ms)';
    case 'turnLeft': return 'เลี้ยวซ้าย 90 องศา';
    case 'turnRight': return 'เลี้ยวขวา 90 องศา';
    case 'turnLeft_by_stop': return 'เลี้ยวซ้ายแบบหยุดนิ่ง';
    case 'turnRight_by_stop': return 'เลี้ยวขวาแบบหยุดนิ่ง';
    case 'checkLine': return 'จัดกึ่งกลางเส้นด้านหน้า (A3)';
    case 'phase_b': return 'ถอยหลังตามเส้นตามระยะ (cm)';
    case 'pass_b': return 'ถอยหลังตามเส้นข้ามแยก';
    case 'turnLeft_b': return 'ถอยหลังเลี้ยวซ้าย';
    case 'turnRight_b': return 'ถอยหลังเลี้ยวขวา';
    case 'checkLine_b': return 'จัดกึ่งกลางเส้นด้านหลัง (A10)';
    case 'follow_notline': return 'วิ่งตรงไม่ต้องดูเส้นตามเวลา (ms)';
    case 'follow_notline_enc': return 'วิ่งตรงไม่ต้องดูเส้นตาม Encoder (cm)';
    case 'backward_notline': return 'ถอยหลังตรงไม่ต้องดูเส้นตามเวลา (ms)';
    case 'servo': return 'สั่งองศาเซอร์โว S1 & S2';
    case 'motor': return 'หมุนมอเตอร์ 4 ล้ออิสระ (4WD)';
    case 'delay': return 'หน่วงเวลาพักชั่วคราว (ms)';
    default: return 'ขั้นตอนการเคลื่อนที่';
  }
}
