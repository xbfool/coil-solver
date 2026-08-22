#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""L101 唯一解的解剖：重放路径，输出
  1. 终点位置与颜色
  2. 每步滑行的（方向、长度、停步依靠：墙/边界/自身轨迹）
  3. 前 K 步与后 K 步的依靠类型序列——开局必然靠墙，何时开始靠轨迹？
  4. 访问顺序热力图（早=数字小），看路径的空间推进模式
"""
import re

raw = open("/mnt/d/workspace/coilbench/levels_all/101").read()
W = int(re.search(r"x=(\d+)", raw).group(1))
H = int(re.search(r"y=(\d+)", raw).group(1))
b = re.search(r"board=([.X]+)", raw).group(1)
free = set((x, y) for y in range(H) for x in range(W) if b[y * W + x] == ".")

sol = open("/tmp/l101sol.txt").read()
sx = int(re.search(r"x=(\d+)", sol).group(1))
sy = int(re.search(r"y=(\d+)", sol).group(1))
path = re.search(r"path=([UDLR]+)", sol).group(1)

DD = {"L": (-1, 0), "R": (1, 0), "U": (0, -1), "D": (0, 1)}
visited = {(sx, sy): 0}
order = 1
cur = (sx, sy)
slides = []
for ch in path:
    dx, dy = DD[ch]
    ln = 0
    while True:
        nxt = (cur[0] + dx, cur[1] + dy)
        if nxt not in free or nxt in visited:
            break
        cur = nxt
        visited[cur] = order
        order += 1
        ln += 1
    beyond = (cur[0] + dx, cur[1] + dy)
    if beyond not in free:
        backing = "WALL"
    elif beyond in visited:
        backing = f"TRAIL@{visited[beyond]}"
    else:
        backing = "??"
    slides.append((ch, ln, backing))

print(f"起点 ({sx},{sy}) 色={(sx+sy)&1}  终点 {cur} 色={(cur[0]+cur[1])&1}  滑行数 {len(slides)}  覆盖 {len(visited)}/{len(free)}")
wallbacked = sum(1 for s in slides if s[2] == "WALL")
print(f"靠墙停步 {wallbacked}/{len(slides)}（{100*wallbacked/len(slides):.0f}%）")
print("前 15 步:", [(c, l, bk if bk == "WALL" else "TRAIL") for c, l, bk in slides[:15]])
print("后 10 步:", [(c, l, bk if bk == "WALL" else "TRAIL") for c, l, bk in slides[-10:]])
# 第一次靠轨迹是第几步
for i, s in enumerate(slides):
    if s[2] != "WALL":
        print(f"第一次靠轨迹停步：第 {i+1} 步（共 {len(slides)} 步）")
        break
# 热力图（16 级灰度字符）
ramp = " .:-=+*#%@ABCDEF"
print("\n访问顺序图（越早越浅；#=墙）:")
for y in range(H):
    row = []
    for x in range(W):
        if (x, y) not in free:
            row.append("█")
        else:
            o = visited.get((x, y), -1)
            row.append(ramp[min(15, o * 16 // len(free))] if o >= 0 else "?")
    print("".join(row))
