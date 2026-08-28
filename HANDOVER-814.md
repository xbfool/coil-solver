# HANDOVER-814：下班交接（2026-08-28 16:30）

> 状态：814 未解，但**全名单+新处方的首次完整攻击**弹药已备齐。
> 今天全部结论已入 git（notes.md 进化战役节 / research/814/CASE.md / insights+dead_ends.jsonl）。

## 一句话现状
814 = 零信息×有向双重失明盘（稀有病）。两次事故（旧路径空枪/名单 6/26 切片）+
三次火力不足后，**尚未被"全名单+足深度"打满过一次**。用户先验（<1h 应解）依然成立。

## 回家开火 runbook（按序）

```bash
git pull   # 数据都在库里: research/814/{surv.*, startord-ordered.txt, funnel4.log}
wsl -e bash -lc "cd /mnt/d/works/coil/coil-solver && cc -std=gnu99 -O3 -march=native -funroll-loops evolve/solver.c -Ievolve -o /tmp/s.o 2>/dev/null; mkdir -p /tmp/kb && cp evolve/solver.c /tmp/kb/ && cp evolve/openevolve_output2/best/best_program.h /tmp/kb/policy.h 2>/dev/null || cp evolve/champions/2026-08-28-adapt-z10-warm16.h /tmp/kb/policy.h && cc -std=gnu99 -O3 -march=native -funroll-loops /tmp/kb/solver.c -o bin/king && echo built"
```
⚠ 家里笔记本 CPU 弱,跨机器秒数不可比;JOBS 按核数调。

**磨（主线）**：全名单（~1400 候选）+ 保序（勿再 sort -un!）+ 深阶梯：
```bash
wsl -e bash -lc "cd /mnt/d/works/coil/coil-solver && ulimit -s unlimited && \
  env STARTORD=research/814/startord-ordered.txt STARTONLY=1 STATS=1 DEATHSTAT=1 TREELOG=1 \
  RESTART=1 RESTARTK=40 SWEEP=50000 JOBS=<核数-2> bin/king \
  /mnt/d/works/coil/coilbench/levels_all/814 > research/814/814.sol 2> research/814/grind2.log"
```
- 出解 → 官方 check → 解入私库 → SOLVELOG/CASE 销案。
- 这次开了 TREELOG:全灭/久磨时 grind2.log 里有每钉死因,别再盲磨。

## ⚠ 名单覆盖说明（16:37 定稿）
19/20 切片、**1780 幸存者**（shard 9 超时未交,其 ~2500 起点未筛 = 5% 盲区）。
磨到全灭时头号嫌疑 = shard 9 切片,补筛后再下结论。startord-ordered.txt 已保序
（漏斗排名,勿 sort!）。

## 悬案清单（回家可继续的科研）
1. **去偏彩票终审**：h&7(e2e 19s) vs h&15(300s 超时) 证明单盘 e2e 是顺序彩票。
   干净核跨盘 A/B: `bin/champ1` vs `bin/champ2-debias` 在 634/736/744/748 各一枪
   （无竞跑!),多数盘赢才算真。判据已预注册(CASE.md)。
2. **针位信号实验**（名单加速的头号希望）：阶梯盘(634/736/748,真起点已知)跑
   FUNNELDUMP,看真起点在 dump 序里的名次分布。系统性靠前→名单可流式产出。
3. **战役#2 收尾**：还剩 ~10 代。跑法照旧(evolve/config2.yaml,manual 档答题)。
   e2e 已降权 0.25(彩票教训)。已收官轴别重试:见 config2 system_message + dead_ends。
4. **流水线化**：shard 交卷即开磨(生产者-消费者),省 22 分钟串行等待。工程活。

## 今日战果速览（详见 notes.md / CASE.md）
- 进化战役#1: 冠军 AD+z10+温16(0.7825),零信息族探针层撤销(架构级),
  271/287 单核 47/43s 破(历史多核 2400/6200 核秒)。
- 战役#2(进行中): +探针波64k/天花板3072,628 e2e 162→19.4s(彩票成分待审)。
- 814 病理全谱: 零信息×有向;漏斗 22min→51/shard;全名单 ~1400。
- 三大情报: L1013+复用系列(9 免费解)/零信息 13 实验体阶梯/两亚族划分。
- 事故两起入册: 名单 6/26 切片(验份数!)、排序彩票(单盘 e2e 不可比)。

## 在场进程（下班时会留守台式机,可 kill）
- 重导(b4hhhp3xj → funnel4.log): ~16:35 完成后自动带扫荡至 3300s 超时,期间可能出解。
- 战役#2 主进程(setsid 分离): 杀= pkill -f 'openevolve.cli.*config2'
