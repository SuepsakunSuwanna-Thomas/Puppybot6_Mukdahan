#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PuppyBot6 - Automated Build & Upload Tool
Script สำหรับคอมไพล์และอัปโหลดโค้ดลงบอร์ด PuppyBot6 ผ่าน PlatformIO ด้วยภาษา Python
"""

import os
import sys
import shutil
import subprocess

# ป้องกันปัญหา UnicodeEncodeError บน Windows Console (cp1252 / cp874)
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# สีสำหรับแสดงผลใน Terminal (ANSI Escape Codes)
GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
CYAN = "\033[96m"
BOLD = "\033[1m"
RESET = "\033[0m"

def set_console_title(title: str):
    """ตั้งค่าหัวข้อหน้าต่าง Terminal บน Windows"""
    if os.name == "nt":
        try:
            os.system(f"title {title}")
        except BaseException:
            pass

def find_pio():
    """
    ค้นหาคำสั่งสำหรับรัน PlatformIO Core
    คืนค่าเป็น list คำสั่ง เช่น ['pio'] หรือ [python_path, '-m', 'platformio']
    """
    # 1. ค้นหาจาก PATH ในระบบ
    pio_path = shutil.which("pio")
    if pio_path:
        try:
            res = subprocess.run([pio_path, "--version"], capture_output=True, text=True)
            if res.returncode == 0:
                return [pio_path]
        except Exception:
            pass

    # 2. ค้นหาจากตำแหน่งติดตั้งเริ่มต้นของ PlatformIO
    home = os.path.expanduser("~")
    if os.name == "nt":
        penv_python = os.path.join(home, ".platformio", "penv", "Scripts", "python.exe")
        pio_exe = os.path.join(home, ".platformio", "penv", "Scripts", "pio.exe")

        # ตรวจสอบ python ใน penv ก่อน (ป้องกันปัญหา pio.exe โดน Windows Application Control บล็อก)
        if os.path.isfile(penv_python):
            try:
                res = subprocess.run([penv_python, "-m", "platformio", "--version"], capture_output=True, text=True)
                if res.returncode == 0:
                    return [penv_python, "-m", "platformio"]
            except Exception:
                pass

        if os.path.isfile(pio_exe):
            try:
                res = subprocess.run([pio_exe, "--version"], capture_output=True, text=True)
                if res.returncode == 0:
                    return [pio_exe]
            except Exception:
                pass
    else:
        candidates = [
            os.path.join(home, ".platformio", "penv", "bin", "pio"),
            os.path.join(home, ".platformio", "penv", "bin", "platformio"),
        ]
        for candidate in candidates:
            if os.path.isfile(candidate):
                return [candidate]

    return None

def run_upload():
    # เปลี่ยน Directory ไปยังโฟลเดอร์ของไฟล์นี้
    current_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(current_dir)

    set_console_title("PuppyBot6 - Build & Upload")

    print(f"{CYAN}{BOLD}{'=' * 60}{RESET}")
    print(f"{CYAN}{BOLD}   🚀 PUPPYBOT6 - BUILD & UPLOAD TOOL (Python){RESET}")
    print(f"{CYAN}{BOLD}{'=' * 60}{RESET}")
    print(f"Working Directory: {current_dir}\n")

    # ตรวจสอบ PlatformIO CLI
    pio_cmd = find_pio()
    if not pio_cmd:
        print(f"{RED}{BOLD}[ERROR] ไม่พบคำสั่ง 'pio' (PlatformIO CLI) ในระบบ!{RESET}")
        print("กรุณาติดตั้ง PlatformIO หรือเพิ่ม path ใน Environment Variable")
        if sys.stdin.isatty():
            try:
                input("\nกด Enter เพื่อออกจากโปรแกรม...")
            except (KeyboardInterrupt, EOFError):
                pass
        sys.exit(1)

    print(f"{GREEN}✔ พบ PlatformIO CLI: {' '.join(pio_cmd)}{RESET}\n")

    # ดึงจำนวน CPU Cores มาใช้ประมวลผล (จำกัดไม่เกิน 4 Threads บน Windows ป้องกันปัญหา WinError 1455: paging file / memory exhausted)
    detected_cores = os.cpu_count() or 4
    cpu_cores = min(detected_cores, 4)
    print(f"{GREEN}{BOLD}⚡ Multi-Core Acceleration: ใช้งาน CPU {cpu_cores} Cores!{RESET}")
    print(f"{GREEN}⚡ Turbo Caching: เปิดใช้งาน ccache ลดเวลาคอมไพล์สูงสุด{RESET}\n")
    print(f"{YELLOW}⏳ กำลังเริ่มทำการคอมไพล์แบบคู่ขนาน ({cpu_cores} Threads) และอัปโหลดโค้ดลงบอร์ด...{RESET}\n")

    # สั่งรันคำสั่ง pio run -j <cpu_cores> --target upload
    cmd = list(pio_cmd) + ["run", "-j", str(cpu_cores), "--target", "upload"]
    
    try:
        process = subprocess.run(cmd)
        return_code = process.returncode
        if return_code != 0 and cpu_cores > 1:
            # Fallback หากเกิดปัญหาเรื่อง memory / pagefile กับ multi-threading
            print(f"\n{YELLOW}[RETRY] กำลังลองคอมไพล์และอัปโหลดใหม่อีกครั้งแบบ Single Thread (-j 1)...{RESET}")
            cmd_single = list(pio_cmd) + ["run", "-j", "1", "--target", "upload"]
            process = subprocess.run(cmd_single)
            return_code = process.returncode
    except KeyboardInterrupt:
        print(f"\n{YELLOW}[CANCELLED] ผู้ใช้ยกเลิกการอัปโหลด{RESET}")
        sys.exit(0)
    except Exception as e:
        print(f"\n{RED}[EXCEPTION] เกิดข้อผิดพลาด: {e}{RESET}")
        return_code = 1

    print("\n" + "=" * 60)
    if return_code == 0:
        print(f"{GREEN}{BOLD}✅ [SUCCESS] อัปโหลดโค้ดลงบอร์ดสำเร็จเรียบร้อย!{RESET}")
        print(f"{GREEN}หุ่นยนต์ PuppyBot6 พร้อมเริ่มการทำงานแล้ว{RESET}")
    else:
        print(f"{RED}{BOLD}❌ [ERROR] การอัปโหลดไม่สำเร็จ (Exit code: {return_code}){RESET}")
        print(f"{YELLOW}คำแนะนำในการแก้ไขปัญหา:{RESET}")
        print("  1. ตรวจสอบว่าสาย USB เสียบแน่นกับคอมพิวเตอร์และบอร์ดแล้ว")
        print("  2. หากบอร์ดไม่ตอบสนอง: ให้กดปุ่ม BOOTSEL บนบอร์ดค้างไว้แล้วเสียบสาย USB ใหม่")
        print("  3. ปิดโปรแกรมหรือ Serial Monitor อื่นๆ ที่อาจกำลังเปิดพอร์ต COM ค้างไว้")
    print("=" * 60)

    # หากรันผ่านการดับเบิ้ลคลิกไฟล์ ให้รอการกด Enter ก่อนปิดหน้าต่าง
    if sys.stdin.isatty():
        try:
            input("\nกด Enter เพื่อปิดหน้าต่าง...")
        except (KeyboardInterrupt, EOFError):
            pass

if __name__ == "__main__":
    run_upload()
