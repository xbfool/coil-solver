# coil-solver

给 [adum/coilbench](https://github.com/adum/coilbench) 写的 Coil 解题程序。**上游仓库里没有任何答案**
（关卡文件只有棋盘，校验器 `check.c` 是纯模拟器，走一遍数还剩几格没走到），所以这里是从零写的。

## 题目

网格里选一个空格出发，每次选一个方向就**一直滑到撞墙/撞边界/撞已访问格子**才停，然后重新选方向。
要求每个空格恰好走一次。本质是空格集合上的哈密顿路径，只是拐弯时机被滑动规则限死。

评测是**一个程序打全部关卡**：评测脚本按关号依次把棋盘喂给 `./run_solver`（stdin 进、stdout 出，
每关 600 秒），失败即停，成绩 = 通过的最高关号。程序看不到关号，只看到棋盘，所以没法按关特判。
难度只有一个维度——棋盘大小：L1 是 3×3，L1207 是 1990×1990。

## 用法

需要 WSL（Windows 侧没有 C 编译器，且上游脚本是 POSIX sh）：

```bash
wsl -e bash -lc "cd /mnt/d/works/coil-solver && ./bench --start 1 --timeout 60"
```

`bench` 会编译 `solver.c` → 拷进 coilbench 当 `./solver`（`run_solver` 会优先跑它）→ 调官方
`evaluate.py`。coilbench 的位置默认 `/mnt/d/works/coilbench`，可用 `COILBENCH_DIR` 覆盖。

跑全量（奇+偶关）：`COIL_FULL_PASSWORD=local ./evaluate_full.py`（口令是本机下载关卡包时设的）。

## 版本

`versions/v1-dfs.c` — 滑动 DFS：

- **滑动 DFS**：状态 = 位置 + 已访问集合，每步枚举 4 个方向滑到底
- **连通性剪枝**：每个节点从当前位置洪水填充，剩余空格必须全部可达，否则剪掉
- **死端剪枝**：统计只剩 ≤1 个出口的未访问格子，超过 1 个必然无解（路径只有一个终点）
- **Warnsdorff 排序**：优先走向出口最少的落点
- **起点排序**：度数小的格子优先（度 1 的格子必然是路径端点）
- **迭代放宽**：先给每个起点浅搜（30 万节点），全失败再把上限 ×8 重来，避免卡死在坏起点上

每个版本单独一个文件放在 `versions/`（`v1-dfs.c`、`v2-*.c` …），跑 `./bench <版本名>` 直接对比，
不用翻 git 历史。成绩表和迭代观察见 `notes.md`。
