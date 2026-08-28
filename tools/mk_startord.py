# -*- coding: utf-8 -*-
# 生成完整 STARTORD 文件：候选 OK 行 + 其余自由格全部 REF（STARTORD 只排序不过滤，REF 才剔除）
import re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
lv = sys.argv[1]
board_path = str(ROOT.parent / "coilbench" / "levels_all" / lv)
cand_path = f"/tmp/cand_{lv}.txt"
out_path = f"/tmp/so_{lv}.txt"
t = open(board_path).read().strip()
m = re.match(r"x=(\d+)&y=(\d+)&board=(.*)$", t, re.S)
w, h, b = int(m.group(1)), int(m.group(2)), m.group(3).strip()
W = w + 2
ok = set()
oklines = open(cand_path).readlines()
for L in oklines:
    p = L.split()
    if p and p[0] == "OK":
        ok.add(int(p[1]))
with open(out_path, "w", newline="\n") as out:
    for L in oklines:
        out.write(L if L.endswith("\n") else L + "\n")
    n = 0
    for y in range(h):
        for x in range(w):
            if b[y * w + x] == ".":
                c = (y + 1) * W + (x + 1)
                if c not in ok:
                    out.write(f"REF {c}\n")
                    n += 1
print("OK", len(ok), "REF", n)
