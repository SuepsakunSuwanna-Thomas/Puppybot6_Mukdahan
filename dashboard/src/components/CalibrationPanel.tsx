import React, { useState } from 'react';
import { serial } from '../services/serialService';
import { CalibData } from '../types';
import { 
  Wrench, 
  RotateCcw, 
  Save, 
  CheckCircle2, 
  Layers, 
  Eye, 
  Ruler, 
  Sun, 
  Compass, 
  Maximize2,
  RefreshCw,
  HelpCircle,
  Sparkles
} from 'lucide-react';

interface CalibrationPanelProps {
  calib: CalibData | null;
  isConnected: boolean;
}

export const CalibrationPanel: React.FC<CalibrationPanelProps> = ({ calib, isConnected }) => {
  const [encScale, setEncScale] = useState<number>(calib?.enc_scale ?? 50.0);
  const [lastRefreshed, setLastRefreshed] = useState<string>('');

  const handleStartCalib = async (target: string, promptText: string) => {
    if (!isConnected) {
      alert('กรุณาเชื่อมต่อ COM Port ก่อนเริ่มคาลิเบรต!');
      return;
    }
    if (confirm(promptText)) {
      await serial.sendCommand({
        cmd: 'CALIB',
        target,
      });
    }
  };

  const handleSaveEncScale = async () => {
    if (!isConnected) {
      alert('กรุณาเชื่อมต่อ COM Port ก่อน!');
      return;
    }
    if (isNaN(encScale) || encScale <= 0) {
      alert('กรุณากรอกค่า Pulses/cm ที่ถูกต้อง (> 0)');
      return;
    }
    await serial.sendCommand({
      cmd: 'SET_ENC_SCALE',
      val: encScale,
    });
  };

  const handleResetEncoder = async () => {
    if (!isConnected) return;
    await serial.sendCommand({ cmd: 'RESET_ENC' });
  };

  const handleFetchCalib = async () => {
    if (!isConnected) return;
    await serial.sendCommand({ cmd: 'GET_CALIB' });
    const now = new Date();
    setLastRefreshed(now.toLocaleTimeString('th-TH'));
  };

  const frontMin = calib?.front_min ?? [0, 0, 0, 0, 0, 0, 0];
  const frontMax = calib?.front_max ?? [0, 0, 0, 0, 0, 0, 0];
  const backMin = calib?.back_min ?? [0, 0, 0, 0, 0, 0, 0];
  const backMax = calib?.back_max ?? [0, 0, 0, 0, 0, 0, 0];
  const ir5cm = calib?.ir_5cm ?? 0;
  const ir10cm = calib?.ir_10cm ?? 0;
  const scale = calib?.enc_scale ?? encScale;

  return (
    <div className="glass-panel rounded-xl p-5 border border-slate-800 space-y-6">
      {/* Header */}
      <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 border-b border-slate-800 pb-4">
        <div>
          <h3 className="text-base font-bold text-white flex items-center gap-2">
            <Wrench className="w-4 h-4 text-cyan-400" />
            <span>ศูนย์กลางการคาลิเบรตเซนเซอร์ครบวงจร (Sensor Calibration Center)</span>
          </h3>
          <p className="text-xs text-slate-400 mt-0.5">
            กดเลือกหมวดที่ต้องการคาลิเบรตจากหน้าเว็บ แล้วทำตามขั้นตอนที่หน้าจอ LCD/TFT ของหุ่นยนต์ หรือดูผลลัพธ์ผ่านเว็บได้ทันที
          </p>
        </div>
        <button
          onClick={handleFetchCalib}
          disabled={!isConnected}
          className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-xl bg-slate-800 hover:bg-slate-700 text-cyan-300 text-xs font-semibold border border-slate-700 transition-colors disabled:opacity-40"
        >
          <RefreshCw className="w-3.5 h-3.5" />
          <span>ดึงค่าล่าสุดจาก EEPROM</span>
        </button>
      </div>

      {/* 7 Calibration Cards Grid (100% of the Original Website) */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3.5">
        {/* Card 1: Front Sensors */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 flex flex-col justify-between hover:border-cyan-500/40 transition-all">
          <div>
            <div className="flex items-center justify-between mb-2">
              <span className="text-[10px] font-mono font-bold px-2 py-0.5 rounded bg-cyan-950/60 text-cyan-400 border border-cyan-800/40">
                STEP 1 & 2
              </span>
              <Sun className="w-4 h-4 text-cyan-400" />
            </div>
            <h4 className="text-xs font-bold text-slate-200">
              1. เซนเซอร์แสงหน้า 7 ตัว (Front ADC 0-6)
            </h4>
            <p className="text-[11px] text-slate-400 mt-1 leading-relaxed">
              คาลิเบรตค่าแสงพื้นขาว และเส้นดำ เพื่อหาจุดตัดกึ่งกลางเส้นที่แม่นยำ
            </p>
            <div className="bg-slate-950/60 p-2 rounded-lg border border-slate-800/60 my-2.5 text-[10px] text-slate-300 font-mono space-y-0.5">
              <div>1. วางบนพื้นขาว &rarr; กดสวิตช์ SW บนหุ่น</div>
              <div>2. วางบนเส้นดำ &rarr; กดสวิตช์ SW เพื่อบันทึก</div>
            </div>
          </div>
          <button
            onClick={() => handleStartCalib('front', 'ต้องการเริ่มคาลิเบรต "เซนเซอร์แสงหน้า 7 ตัว" ใช่หรือไม่?\n\nกรุณาเตรียมวางหุ่นยนต์บนพื้นขาวและเส้นดำตามขั้นตอนบนหน้าจอหุ่นยนต์')}
            disabled={!isConnected}
            className="w-full py-2 rounded-xl text-xs font-bold bg-cyan-500 hover:bg-cyan-400 text-slate-950 transition-all shadow-glow-cyan disabled:opacity-40"
          >
            เริ่มคาลิเบรตเซนเซอร์หน้า
          </button>
        </div>

        {/* Card 2: Back Sensors */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 flex flex-col justify-between hover:border-indigo-500/40 transition-all">
          <div>
            <div className="flex items-center justify-between mb-2">
              <span className="text-[10px] font-mono font-bold px-2 py-0.5 rounded bg-indigo-950/60 text-indigo-400 border border-indigo-800/40">
                STEP 1 & 2
              </span>
              <Layers className="w-4 h-4 text-indigo-400" />
            </div>
            <h4 className="text-xs font-bold text-slate-200">
              2. เซนเซอร์แสงหลัง 7 ตัว (Back ADC 7-13)
            </h4>
            <p className="text-[11px] text-slate-400 mt-1 leading-relaxed">
              คาลิเบรตค่าแสงพื้นขาว และเส้นดำ สำหรับการวิ่งถอยหลังตามเส้น
            </p>
            <div className="bg-slate-950/60 p-2 rounded-lg border border-slate-800/60 my-2.5 text-[10px] text-slate-300 font-mono space-y-0.5">
              <div>1. วางบนพื้นขาว &rarr; กดสวิตช์ SW บนหุ่น</div>
              <div>2. วางบนเส้นดำ &rarr; กดสวิตช์ SW เพื่อบันทึก</div>
            </div>
          </div>
          <button
            onClick={() => handleStartCalib('back', 'ต้องการเริ่มคาลิเบรต "เซนเซอร์แสงหลัง 7 ตัว" ใช่หรือไม่?\n\nกรุณาเตรียมวางหุ่นยนต์บนพื้นขาวและเส้นดำตามขั้นตอนบนหน้าจอหุ่นยนต์')}
            disabled={!isConnected}
            className="w-full py-2 rounded-xl text-xs font-bold bg-indigo-600 hover:bg-indigo-500 text-white transition-all disabled:opacity-40"
          >
            เริ่มคาลิเบรตเซนเซอร์หลัง
          </button>
        </div>

        {/* Card 3: Floor Color Sensor */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 flex flex-col justify-between hover:border-amber-500/40 transition-all">
          <div>
            <div className="flex items-center justify-between mb-2">
              <span className="text-[10px] font-mono font-bold px-2 py-0.5 rounded bg-amber-950/60 text-amber-400 border border-amber-800/40">
                FLOOR 4 COLORS
              </span>
              <Eye className="w-4 h-4 text-amber-400" />
            </div>
            <h4 className="text-xs font-bold text-slate-200">
              3. เซนเซอร์สีพื้นสนาม TCS34725 (Floor)
            </h4>
            <p className="text-[11px] text-slate-400 mt-1 leading-relaxed">
              บันทึกค่า RGB อ้างอิง 4 สีพื้นสนาม: แดง (RED), เขียว (GREEN), น้ำเงิน (BLUE), เหลือง (YELLOW)
            </p>
            <div className="bg-slate-950/60 p-2 rounded-lg border border-slate-800/60 my-2.5 text-[10px] text-slate-300 font-mono">
              วางเหนือสีพื้นสนามที่หน้าจอบอก แล้วกดปุ่ม SW บันทึกทีละสี
            </div>
          </div>
          <button
            onClick={() => handleStartCalib('color_floor', 'ต้องการเริ่มคาลิเบรต "เซนเซอร์สีพื้นสนาม TCS34725 ด้านหน้า" ใช่หรือไม่?\n\nกรุณาเตรียมวางหุ่นเหนือสีพื้น: แดง (RED), เขียว (GREEN), น้ำเงิน (BLUE), เหลือง (YELLOW) ตามที่หน้าจอบอก')}
            disabled={!isConnected}
            className="w-full py-2 rounded-xl text-xs font-bold bg-amber-500 hover:bg-amber-400 text-slate-950 transition-all disabled:opacity-40"
          >
            เริ่มคาลิเบรตสีพื้นสนาม
          </button>
        </div>

        {/* Card 4: Side Color Sensor (Can / Object) */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 flex flex-col justify-between hover:border-purple-500/40 transition-all">
          <div>
            <div className="flex items-center justify-between mb-2">
              <span className="text-[10px] font-mono font-bold px-2 py-0.5 rounded bg-purple-950/60 text-purple-400 border border-purple-800/40">
                SIDE (CAN/OBJECT)
              </span>
              <Eye className="w-4 h-4 text-purple-400" />
            </div>
            <h4 className="text-xs font-bold text-slate-200">
              4. เซนเซอร์สีกระป๋องข้าง TCS34725 (Side Can)
            </h4>
            <p className="text-[11px] text-slate-400 mt-1 leading-relaxed">
              บันทึกค่า RGB เซนเซอร์ยื่นด้านข้างสำหรับอ่านสีแก้ว/กระป๋อง/วัตถุ 4 สี
            </p>
            <div className="bg-slate-950/60 p-2 rounded-lg border border-slate-800/60 my-2.5 text-[10px] text-slate-300 font-mono">
              ยื่นเซนเซอร์จ่อสีกระป๋องด้านข้างตามที่จอบอก แล้วกดปุ่ม SW
            </div>
          </div>
          <button
            onClick={() => handleStartCalib('color_side', 'ต้องการเริ่มคาลิเบรต "เซนเซอร์สีกระป๋อง/วัตถุด้านข้าง (Side TCS)" ใช่หรือไม่?\n\nกรุณาเตรียมสีกระป๋อง: แดง (RED), เขียว (GREEN), น้ำเงิน (BLUE), เหลือง (YELLOW) ตามที่หน้าจอบอก')}
            disabled={!isConnected}
            className="w-full py-2 rounded-xl text-xs font-bold bg-purple-600 hover:bg-purple-500 text-white transition-all disabled:opacity-40"
          >
            เริ่มคาลิเบรตสีกระป๋องข้าง
          </button>
        </div>

        {/* Card 5: Motor Encoder Calibration */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 flex flex-col justify-between hover:border-cyan-500/40 transition-all">
          <div>
            <div className="flex items-center justify-between mb-2">
              <span className="text-[10px] font-mono font-bold px-2 py-0.5 rounded bg-cyan-950/60 text-cyan-400 border border-cyan-800/40">
                DISTANCE SCALE
              </span>
              <Ruler className="w-4 h-4 text-cyan-400" />
            </div>
            <h4 className="text-xs font-bold text-slate-200">
              5. เอ็นโค้ดเดอร์วัดระยะทาง (Encoder Pulses/cm)
            </h4>
            <p className="text-[11px] text-slate-400 mt-1 leading-relaxed">
              ปรับอัตราทดพัลส์ต่อระยะทาง (cm) หรือใช้ระบบหมุนล้อวัดระยะ 20 ซม. บันทึกลง EEPROM
            </p>

            <div className="flex items-center gap-2 my-2">
              <input
                type="number"
                step="0.1"
                min="1.0"
                max="500.0"
                value={encScale}
                onChange={(e) => setEncScale(parseFloat(e.target.value) || 50.0)}
                className="flex-1 bg-slate-950 border border-slate-700 rounded-lg px-2.5 py-1.5 text-xs text-cyan-300 font-mono font-bold text-right outline-none focus:border-cyan-500"
              />
              <button
                onClick={handleSaveEncScale}
                disabled={!isConnected}
                className="px-3 py-1.5 rounded-lg text-xs font-bold bg-cyan-500 hover:bg-cyan-400 text-slate-950 transition-colors disabled:opacity-40 whitespace-nowrap"
              >
                บันทึก Scale
              </button>
            </div>
          </div>

          <div className="space-y-1.5">
            <button
              onClick={() => handleStartCalib('encoder', 'เริ่มโหมดคาลิเบรต Encoder หมุนล้อ 20 ซม. บนหน้าจอหุ่นยนต์?\n\n1. วางหุ่นยนต์ตรงจุดเริ่มต้น\n2. กดสวิตช์ SW บนหุ่นเพื่อ Reset Pulse เป็น 0\n3. เข็นหุ่นไปข้างหน้าตรงๆ เป็นระยะทาง 20 ซม. พอดี\n4. กดสวิตช์ SW บนหุ่นอีกครั้งเพื่อคำนวณและบันทึกลง EEPROM')}
              disabled={!isConnected}
              className="w-full py-1.5 rounded-lg text-xs font-semibold bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 transition-colors disabled:opacity-40"
            >
              หมุนล้อ 20 ซม. คาลิเบรตบนหุ่น
            </button>
            <button
              onClick={handleResetEncoder}
              disabled={!isConnected}
              className="w-full py-1 rounded-lg text-[11px] font-mono bg-slate-950 hover:bg-slate-900 text-slate-400 hover:text-slate-200 border border-slate-800 transition-colors"
            >
              Zero / รีเซ็ตตัวนับ Encoder เป็น 0
            </button>
          </div>
        </div>

        {/* Card 6: Sharp IR Distance */}
        <div className="bg-slate-900/60 p-4 rounded-xl border border-slate-800 flex flex-col justify-between hover:border-emerald-500/40 transition-all">
          <div>
            <div className="flex items-center justify-between mb-2">
              <span className="text-[10px] font-mono font-bold px-2 py-0.5 rounded bg-emerald-950/60 text-emerald-400 border border-emerald-800/40">
                5cm & 10cm
              </span>
              <Maximize2 className="w-4 h-4 text-emerald-400" />
            </div>
            <h4 className="text-xs font-bold text-slate-200">
              6. เซนเซอร์วัดระยะ Sharp IR (GP2Y0A41)
            </h4>
            <p className="text-[11px] text-slate-400 mt-1 leading-relaxed">
              วัดค่าแรงดันที่ระยะ 5 ซม. และ 10 ซม. เพื่อแปลงระยะเป็นเซนติเมตรที่เที่ยงตรง
            </p>
            <div className="bg-slate-950/60 p-2 rounded-lg border border-slate-800/60 my-2.5 text-[10px] text-slate-300 font-mono space-y-0.5">
              <div>1. วางห่าง 5 ซม. &rarr; กดสวิตช์ SW บันทึก</div>
              <div>2. วางห่าง 10 ซม. &rarr; กดสวิตช์ SW บันทึก</div>
            </div>
          </div>
          <button
            onClick={() => handleStartCalib('ir', 'ต้องการเริ่มคาลิเบรต "เซนเซอร์วัดระยะ Sharp IR" ใช่หรือไม่?\n\nกรุณาเตรียมวัตถุขวางที่ระยะ 5 ซม. และ 10 ซม. ตามลำดับ')}
            disabled={!isConnected}
            className="w-full py-2 rounded-xl text-xs font-bold bg-emerald-600 hover:bg-emerald-500 text-white transition-all disabled:opacity-40"
          >
            เริ่มคาลิเบรตวัดระยะ IR
          </button>
        </div>

        {/* Card 7: Full Onboard Menu */}
        <div className="bg-slate-900/80 p-4 rounded-xl border border-cyan-500/40 flex flex-col justify-between shadow-glow-cyan md:col-span-2 lg:col-span-3">
          <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-3 mb-2">
            <div>
              <div className="flex items-center gap-2 mb-1">
                <span className="text-[10px] font-mono font-bold px-2 py-0.5 rounded bg-cyan-950 text-cyan-300 border border-cyan-500/40">
                  FULL 6 ONBOARD MENUS
                </span>
                <h4 className="text-xs font-bold text-white">
                  7. เปิดเมนูคาลิเบรตหลักบนจอหุ่นยนต์ (Full TFT Calibration Menu)
                </h4>
              </div>
              <p className="text-[11px] text-slate-300">
                เปิดเมนูหลัก 6 ข้อบนหน้าจอ LCD/TFT ของหุ่นยนต์ และควบคุมด้วยปุ่มบนบอร์ด ครอบคลุมทั้งเซนเซอร์เส้น, สีพื้น, สีกระป๋อง, เอ็นโค้ดเดอร์ และ IR
              </p>
            </div>
            <button
              onClick={() => handleStartCalib('menu', 'เปิดเมนูคาลิเบรตหลัก 6 ข้อบนหน้าจอ TFT ของหุ่นยนต์?')}
              disabled={!isConnected}
              className="px-5 py-2.5 rounded-xl text-xs font-bold bg-cyan-500 hover:bg-cyan-400 text-slate-950 shadow-glow-cyan active:scale-95 transition-all disabled:opacity-40 whitespace-nowrap"
            >
              เปิดหน้าจอเมนูคาลิเบรตบนหุ่น
            </button>
          </div>
        </div>
      </div>

      {/* EEPROM Live Values Table (100% of Old Web) */}
      <div className="space-y-3 pt-2 border-t border-slate-800">
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-2">
          <h4 className="text-xs font-bold text-slate-200 flex items-center gap-2">
            <CheckCircle2 className="w-4 h-4 text-emerald-400" />
            <span>ตารางค่าคาลิเบรตที่บันทึกอยู่ใน EEPROM ปัจจุบัน (7 Channels)</span>
          </h4>
          {lastRefreshed && (
            <span className="text-[11px] text-emerald-400 font-mono">
              ✓ โหลดข้อมูลล่าสุดเมื่อ {lastRefreshed}
            </span>
          )}
        </div>

        <div className="overflow-x-auto rounded-xl border border-slate-800 bg-slate-950/70">
          <table className="w-full text-left text-xs font-mono">
            <thead className="bg-slate-950 text-slate-400 text-[11px] border-b border-slate-800">
              <tr>
                <th className="py-2.5 px-3">กลุ่มเซนเซอร์</th>
                {Array.from({ length: 7 }).map((_, i) => (
                  <th key={i} className="py-2.5 px-2 text-center">
                    {i === 3 ? <span className="text-cyan-400 font-bold">Ch 3★</span> : `Ch ${i}`}
                  </th>
                ))}
              </tr>
            </thead>
            <tbody className="divide-y divide-slate-800/60 text-[11px]">
              <tr>
                <td className="py-2 px-3 text-cyan-300 font-sans font-medium">หน้า Min (ขาว)</td>
                {frontMin.map((v, i) => (
                  <td key={i} className="py-2 px-2 text-center text-slate-200">{v}</td>
                ))}
              </tr>
              <tr>
                <td className="py-2 px-3 text-cyan-300 font-sans font-medium">หน้า Max (ดำ)</td>
                {frontMax.map((v, i) => (
                  <td key={i} className="py-2 px-2 text-center text-slate-200">{v}</td>
                ))}
              </tr>
              <tr>
                <td className="py-2 px-3 text-indigo-300 font-sans font-medium">หลัง Min (ขาว)</td>
                {backMin.map((v, i) => (
                  <td key={i} className="py-2 px-2 text-center text-slate-200">{v}</td>
                ))}
              </tr>
              <tr>
                <td className="py-2 px-3 text-indigo-300 font-sans font-medium">หลัง Max (ดำ)</td>
                {backMax.map((v, i) => (
                  <td key={i} className="py-2 px-2 text-center text-slate-200">{v}</td>
                ))}
              </tr>
            </tbody>
          </table>
        </div>

        {/* EEPROM Extra Metrics Pills */}
        <div className="flex flex-wrap items-center gap-3 pt-1">
          <div className="bg-slate-900/60 px-3 py-1.5 rounded-lg border border-slate-800 text-xs flex items-center gap-2">
            <span className="text-slate-400">Sharp IR 5cm Raw:</span>
            <strong className="text-cyan-400 font-mono font-bold">{ir5cm}</strong>
          </div>
          <div className="bg-slate-900/60 px-3 py-1.5 rounded-lg border border-slate-800 text-xs flex items-center gap-2">
            <span className="text-slate-400">Sharp IR 10cm Raw:</span>
            <strong className="text-cyan-400 font-mono font-bold">{ir10cm}</strong>
          </div>
          <div className="bg-slate-900/60 px-3 py-1.5 rounded-lg border border-slate-800 text-xs flex items-center gap-2">
            <span className="text-slate-400">Encoder Scale:</span>
            <strong className="text-emerald-400 font-mono font-bold">{scale.toFixed(1)} p/cm</strong>
          </div>
        </div>
      </div>
    </div>
  );
};
