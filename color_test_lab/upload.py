#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PuppyBot6 - Standalone Color Lab Upload Tool (Python)
สคริปต์สำหรับคอมไพล์และอัปโหลดโปรแกรมทดสอบเซนเซอร์สี (Color Lab) ลงบอร์ด PuppyBot6
"""

import os
import sys
import shutil
import subprocess

# รหัสสี ANSI สำหรับแสดงผลใน Terminal
GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
CYAN = "\033[96m"
MAGENTA = "\033[95m"
BOLD = "\033[1m"
RESET = "\033[0m"

def set_console_title(title: str):
    """ตั้งชื่อหน้าต่าง Console บน Windows"""
    if os.name == "nt":
        os.system(f"title {title}")

def check_pio():
    """ตรวจสอบคำสั่ง PlatformIO (pio) ในระบบ"""
    return shutil.which("pio") is not None

def run_upload():
    # กำหนดตำแหน่งโฟลเดอร์ให้เป็น color_test_lab
    current_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(current_dir)

    set_console_title("PuppyBot6 - Color Lab Uploader")

    print(f"{CYAN}{BOLD}{'=' * 65}{RESET}")
    print(f"{CYAN}{BOLD}   🎨 PUPPYBOT6 - COLOR LAB UPLOAD TOOL (Python){RESET}")
    print(f"{MAGENTA}   โปรแกรมทดสอบและคาลิเบรตเซนเซอร์สี TCS34725 (SDA=4, SCL=5){RESET}")
    print(f"{CYAN}{BOLD}{'=' * 65}{RESET}")
    print(f"Working Directory: {current_dir}\n")

    # ตรวจสอบ PlatformIO CLI
    if not check_pio():
        print(f"{RED}{BOLD}[ERROR] ไม่พบคำสั่ง 'pio' (PlatformIO CLI) ในระบบ!{RESET}")
        print("กรุณาตรวจสอบว่าได้ติดตั้ง PlatformIO หรือเปิด VS Code / PlatformIO Terminal แล้ว")
        if sys.stdin.isatty():
            input("\nกด Enter เพื่อออกจากโปรแกรม...")
        sys.exit(1)

    cpu_cores = os.cpu_count() or 8
    print(f"{GREEN}{BOLD}⚡ Multi-Core Acceleration: ประมวลผลด้วย CPU {cpu_cores} Cores เต็มกำลัง!{RESET}")
    print(f"{YELLOW}⏳ กำลังเริ่มทำการคอมไพล์และอัปโหลดเฟิร์มแวร์ลงบอร์ด...{RESET}\n")

    # สั่งรันคำสั่ง pio run -j <cpu_cores> --target upload ในโฟลเดอร์ color_test_lab
    cmd = ["pio", "run", "-j", str(cpu_cores), "--target", "upload"]

    try:
        process = subprocess.run(cmd, shell=True)
        return_code = process.returncode
    except KeyboardInterrupt:
        print(f"\n{YELLOW}[CANCELLED] ผู้ใช้ยกเลิกการอัปโหลด{RESET}")
        sys.exit(0)
    except Exception as e:
        print(f"\n{RED}[EXCEPTION] เกิดข้อผิดพลาด: {e}{RESET}")
        return_code = 1

    print("\n" + "=" * 65)
    if return_code == 0:
        print(f"{GREEN}{BOLD}✅ [SUCCESS] อัปโหลดโปรแกรม Color Lab สำเร็จเรียบร้อย!{RESET}")
        print(f"{GREEN}บอร์ดพร้อมใช้งาน: กดปุ่มเลือกเมนู [1. Live Test] หรือ [2. Calibrate] ได้เลย{RESET}")
    else:
        print(f"{RED}{BOLD}❌ [ERROR] การอัปโหลดไม่สำเร็จ (Exit code: {return_code}){RESET}")
        print(f"{YELLOW}คำแนะนำในการแก้ไขปัญหา:{RESET}")
        print("  1. ตรวจสอบว่าสาย USB เชื่อมต่อกับบอร์ด PuppyBot6 แน่นดีแล้ว")
        print("  2. หากบอร์ดไม่ตอบสนอง: ให้กดปุ่ม BOOTSEL บนบอร์ดค้างไว้แล้วเสียบสาย USB ใหม่อีกครั้ง")
        print("  3. ปิดโปรแกรมหรือ Serial Monitor ที่อาจกำลังเปิดพอร์ต COM ค้างไว้")
    print("=" * 65)

    if sys.stdin.isatty():
        try:
            input("\nกด Enter เพื่อปิดหน้าต่าง...")
        except (KeyboardInterrupt, EOFError):
            pass

if __name__ == "__main__":
    run_upload()
