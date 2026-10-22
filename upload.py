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
        os.system(f"title {title}")

def check_pio():
    """ตรวจสอบว่าคำสั่ง pio (PlatformIO Core) ติดตั้งอยู่ในระบบหรือไม่"""
    return shutil.which("pio") is not None

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
    if not check_pio():
        print(f"{RED}{BOLD}[ERROR] ไม่พบคำสั่ง 'pio' (PlatformIO CLI) ในระบบ!{RESET}")
        print("กรุณาติดตั้ง PlatformIO หรือเพิ่ม path ใน Environment Variable")
        input("\nกด Enter เพื่อออกจากโปรแกรม...")
        sys.exit(1)

    print(f"{YELLOW}⏳ กำลังเริ่มทำการคอมไพล์และอัปโหลดโค้ดลงบอร์ด...{RESET}\n")

    # สั่งรันคำสั่ง pio run --target upload แบบแสดงผลสด (Live output)
    cmd = ["pio", "run", "--target", "upload"]
    
    try:
        process = subprocess.run(cmd, shell=True)
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
