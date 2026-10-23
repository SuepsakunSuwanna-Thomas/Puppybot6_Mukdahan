import os
import shutil
Import("env")

# ค้นหา ccache จาก C:\ccache\ccache.exe หรือจาก System PATH
ccache_path = "C:\\ccache\\ccache.exe"
if not os.path.exists(ccache_path):
    ccache_path = shutil.which("ccache")

try:
    if ccache_path and os.path.exists(ccache_path):
        current_cc = str(env.get("CC", ""))
        # ป้องกันการต่อคำสั่งซ้ำซ้อน (Prevent Double Wrapping)
        if "ccache" not in current_cc.lower():
            env.Replace(CC=f"{ccache_path} {env.get('CC')}")
            env.Replace(CXX=f"{ccache_path} {env.get('CXX')}")
            env.Replace(AS=f"{ccache_path} {env.get('AS')}")
            print(f"--- [SUCCESS] ccache accelerated build enabled ({ccache_path}) ---")
except Exception as e:
    print(f"--- [WARNING] ccache hook skipped: {e} ---")

