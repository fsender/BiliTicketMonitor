#!/usr/bin/env python3
"""测试用: 在 test_grab 目录创建时间戳文件"""
import sys
from datetime import datetime

ts = datetime.now().strftime("%Y%m%d_%H%M%S")
filename = f"out{ts}.txt"
with open(filename, "w") as f:
    f.write(f"args: {sys.argv[1:]}\n")
    f.write(f"created: {ts}\n")
print(f"[test_grab] 已创建 {filename}")
