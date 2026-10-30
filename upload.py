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
import json

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
    """ค้นหา path คำสั่ง PlatformIO CLI"""
    # 1. ค้นหาจากตำแหน่งติดตั้งเริ่มต้นของ PlatformIO ก่อน (เสถียรกว่า)
    home = os.path.expanduser("~")
    if os.name == "nt":
        pio_exe = os.path.join(home, ".platformio", "penv", "Scripts", "platformio.exe")
        if not os.path.isfile(pio_exe):
            pio_exe = os.path.join(home, ".platformio", "penv", "Scripts", "pio.exe")
        penv_python = os.path.join(home, ".platformio", "penv", "Scripts", "python.exe")

        if os.path.isfile(pio_exe):
            return [pio_exe]
        if os.path.isfile(penv_python):
            return [penv_python, "-m", "platformio"]

    # 2. ค้นหาจาก PATH ในระบบ
    pio_path = shutil.which("pio") or shutil.which("platformio")
    if pio_path:
        return [pio_path]

    # 3. ค้นหาจากโฟลเดอร์สำหรับ Linux / macOS
    if os.name != "nt":
        candidates = [
            os.path.join(home, ".platformio", "penv", "bin", "pio"),
            os.path.join(home, ".platformio", "penv", "bin", "platformio"),
        ]
        for candidate in candidates:
            if os.path.isfile(candidate):
                return [candidate]

    return None

def find_upload_port(pio_cmd):
    """ค้นหาพอร์ต COM ของบอร์ด RP2350 อัตโนมัติ"""
    try:
        res = subprocess.run(list(pio_cmd) + ["device", "list", "--json-output"],
                             capture_output=True, text=True, timeout=5)
        if res.returncode == 0:
            devices = json.loads(res.stdout)
            for d in devices:
                port = d.get("port", "")
                hwid = d.get("hwid", "")
                desc = d.get("description", "")
                if "239A" in hwid.upper() or "RP2350" in desc.upper() or "SERIAL" in desc.upper():
                    return port
            if devices and "port" in devices[0]:
                return devices[0]["port"]
    except Exception:
        pass
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

    # ค้นหาพอร์ต COM อัตโนมัติ
    upload_port = find_upload_port(pio_cmd)
    if upload_port:
        print(f"{GREEN}✔ ตรวจพบพอร์ตของบอร์ด: {BOLD}{upload_port}{RESET}\n")
    else:
        print(f"{YELLOW}ℹ ไม่พบพอร์ตระบุเจาะจง กำลังค้นหาบอร์ดอัตโนมัติ...{RESET}\n")

    # ดึงจำนวน CPU Cores มาใช้ประมวลผล (จำกัดไม่เกิน 4 Threads บน Windows ป้องกันปัญหา WinError 1455: paging file / memory exhausted)
    detected_cores = os.cpu_count() or 4
    cpu_cores = min(detected_cores, 4)
    print(f"{GREEN}{BOLD}⚡ Multi-Core Acceleration: ใช้งาน CPU {cpu_cores} Cores!{RESET}")
    print(f"{GREEN}⚡ Turbo Caching: เปิดใช้งาน ccache ลดเวลาคอมไพล์สูงสุด{RESET}\n")
    print(f"{YELLOW}⏳ กำลังเริ่มทำการคอมไพล์แบบคู่ขนาน ({cpu_cores} Threads) และอัปโหลดโค้ดลงบอร์ด...{RESET}\n")

    # สั่งรันคำสั่ง pio run -e adafruit_metro_rp2350 -j <cpu_cores> --target upload
    cmd = list(pio_cmd) + ["run", "-e", "adafruit_metro_rp2350", "-j", str(cpu_cores), "--target", "upload"]
    if upload_port:
        cmd += ["--upload-port", upload_port]
    
    try:
        process = subprocess.run(cmd)
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
