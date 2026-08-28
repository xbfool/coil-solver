# EVOLVE-PLAN：把项目升级成算法发现实验场（2026-08-28）

> 缘起：GPT 建议把 Coil 塞进 OpenEvolve 风格的进化搜索框架。本文是对该建议的
> 评估结论 + 落地计划。评估的原始对话不入库，结论都在这里。

## 一、评估结论（先读这个）

### GPT 说对的
1. **稠密 fitness vector**：只看"过到第几关"信号太稀疏，要输出
   solved / time / nodes / backtracks / 各剪枝层命中数 的向量。✅ 采纳，是 P0。
2. **Cascade evaluator**：便宜盘先筛，贵盘只给精英。✅ 采纳——我们已有
   30s 奇数关 → 600s 全量的两级雏形，正规化即可。
3. **别只从冠军爬山**：v1→v78 的工作模式确实是 hill climbing from champion，
   要保留多条暂时稍差的谱系（islands / MAP-Elites）。✅ 采纳。
4. **行为多样性描述符**：不用代码差异做 diversity，用"每型盘上的 performance
   fingerprint"。✅ 采纳——junction.py 度数指纹、board_stats.py 就是现成原料。
5. **合成训练盘防过拟合**：✅ 采纳——experiments/gen.c、gen2.c、backbite.c
   已是雏形，缺的是和官方盘统计对齐的校准。
6. **失败记忆库**：✅ 我们早就有（notes.md 6273 行，每条死路带量化死因），
   GPT 不知道。缺的只是机器可读化，能自动喂进 mutation prompt。

### GPT 判断错的（重要，别被带偏）
1. **"828→1001 榜单断层 = 算法相变" 已被我们自己的数据证伪。**
   v78（BJ=500）已解 L1050（420×420，满核 <25min），767 破案后 790~814
   大多秒~分钟级。墙不是尺寸相变，是个别 deep-doom 实例（L664 型）。
   榜单断层更可能是"人肉玩家弃坑点"，不构成方法论转折的证据。
2. **低估 soundness 风险——这是本项目最贵的教训。**
   767 案形态：unsound 行为（栈溢出记账成"搜穷"）让求解器**看起来更快更果断**，
   骗过 499 关 REFSOL 回放 + 7 个邻居关逐条回放。LLM 变异产生的 unsound 剪枝
   会是同一形态：浅层全过、中盘更快、某关沉默失败。
   ⇒ cascade 第一关必须是 REFSOL/SOLWALK 回放 + postrust 阳性对照当**硬门**，
   崩过/违例即 TAINTED 淘汰，不给分。
3. **没算评估成本的账。** frontier 关一次 eval = 数百秒 × 26 核，繁殖率受限。
   fitness 主战场必须在秒级校准盘上。但历史数据表明**中盘提速与 frontier
   突破相关性弱**（probing 中盘 +20 关对墙无效；BJ=500 对 767 型 100×，
   对别型未验证）⇒ fitness 必须**按型分列**，不是单标量。

### 为什么仍然值得干
最近两次最大跃迁都不是新算法，是**参数/调度/位置的发现**：
- SWEEP 80.4M→400k（一个 276 倍的参数错误，值 L664）
- BJ=500（传播放对位置 + 调稀疏，767 型 81min→44s 单核）
这类东西正是盲搜+进化最擅长、人肉+CC 只有痛苦法证之后才找到的。
参数/调度空间里大概率还有低垂果实。算法级相变（如果存在）留给 architecture
island 晚期再开。

### 框架选择
先 **OpenEvolve** 一个，跑在 WSL。LLM4AD / MCTS-AHD / ReEvo 先不装——
ReEvo 的反思记忆思想用我们的 notes 机器化实现即可，没必要引第二套框架。
算力：本机 28 核为主；5090 服务器（111.17.197.107）CPU 可当第二 farm（待确认核数）。
**红线：解库是私有的（防 LLM 训练集污染），evolution loop 只喂代码+统计，永不喂解。**

## 二、已有资产盘点（别重建）

| GPT 以为要建 | 实际已有 |
| --- | --- |
| 可信 evaluator | check.c 官方模拟器、bench/bench-full、referee.py、postrust.sh、SOLWALK |
| instrumentation | DEATHSTAT、TREELOG、PROF、GDUMP、WINNER nodes（散落 stderr，未统一） |
| 失败记忆 | notes.md 全部死路+量化死因 |
| 盘面生成 | experiments/gen.c、gen2.c、backbite.c |
| 拓扑分型 | tools/junction.py（度数指纹）、experiments/board_stats.py |
| cascade 雏形 | 30s 奇数关代理 → 600s 全量两级 |

## 三、落地计划（按序，P0~P2 无论进化搞不搞都值得）

### P0：evaluator 硬化（地基）—— ✅ 2026-08-28 首版落地
- [x] v78 加 `STATS=1`：stderr 一行 JSON `{solved, wall_s, nodes_total, nodes_win,
  dth_liveend/estate/reach/dyn/flow/geom, maxrss_kb, start_x, start_y}`。
  nodes_total 是新增的跨起点累计器（nodes 每棵树清零）。⚠ swarm 多进程时每
  shard 各打一行 —— 做向量必须 JOBS=1。改动在 versions/v78-rpbj.c（stats_json）。
- [x] `tools/evalvec.py`：二进制+盘列表 → JSONL（含官方 check 验证；
  solved 但 check 拒绝 = 🚨 UNSOUND 退出码 2；崩溃按信号显式记账 tainted）
- [x] `tools/cascade.sh` T0+T1：
  - **T0 soundness 硬门**（calib/refsol.txt，8 关带真解回放）：断言违例("!!")/
    崩溃(信号)/未解/假解 一票否决。v78 基线全过。postrust 阳性对照暂未接（TODO）。
  - **T1 校准套件**（calib/list.txt，17 关占位版）：全 solved+verified 才 PASS，
    向量归档 results/vec/。v78 基线：17 关 21.2s，nodes_total 489 万
    → `results/vec/v78-baseline.jsonl`。
  - [ ] **T2 中档 600s / T3 frontier** 未接：先人工 ./bench 与 tools/solve.sh。
- [ ] 排名规则：先正确性（T0 一票否决），再 solved_count，再按型 log(nodes) 分列比
  （等 P1 分型后落地）

### P1：校准套件 + 分型 —— ✅ 2026-08-28 落地，两个重要发现
- [x] `tools/boardfeat.py`：结构特征提取（deg 分布/开阔度/走廊段/双色平衡，
  numpy 向量化），全量 1~1207 特征在 `results/boardfeat.jsonl`。
- [x] **发现①：官方盘结构高度同质，但存在两型**（残差 KMeans k=2 silhouette 0.617）：
  - **A 开阔型**（930 关）：deg4≈13.3%、open2x2≈31%
  - **B 致密走廊型**（218 关）：deg4≈7.5%、open2x2≈26%，两个来源——
    早期约每 30 关一张（353/383/413/533/563/593/623…）+ **L1013 起到 L1207 全部是 B 型**。
  - **战略含义：终盘战场 100% B 型**，中盘调优大多在 A 型上做，迁移性存疑，
    校准套件必须 B 型足量。分型表 `results/boardtypes.tsv`（deg4<0.105 规则，
    边界 9 关毛刺不影响尾块）。
  - GPT 设想的"많类型拓扑分类"对官方盘**不成立**——结构指纹几乎单一，
    真正的多样性要从**行为**（solver 统计向量）里来，MAP-Elites 描述符据此设计。
- [x] **发现②：行为特殊盘真实存在**：同尺寸同型下 L195(B) 要 316 万节点/14.7s，
  邻近 B 型只要 0.1s——结构特征完全解释不了，是"深树"行为型。
  这批盘（195/223/271/287/319/383）单列 `calib/hard.txt` 当 T1H 深树探针
  （非门禁，记录向量；271/287 v78 30s 仍超时，是现成的进化靶子）。
- [x] 校准套件 v2：`calib/list.txt` = A 型 10 关 + B 型 11 关尺寸阶梯，
  全过 12.3s；`calib/refsol.txt` = 13 关（含 5 关 B 型）。
  v78 基线向量 `results/vec/v78-baseline2{,-hard}.jsonl`。
- [ ] **合成盘：推迟（负结果先记账）**。research/gen2 产物墙密度 0.905 vs 官方
  0.334（免死走法填充率仅 ~9.5%），离官方分布太远；对齐生成器是独立课题。
  过拟合防线暂用官方 1013~1207 的 195 张 B 型（前沿未触及，天然测试集）。

### P2：知识库机器化 —— ✅ 2026-08-28 落地
- [x] `research/dead_ends.jsonl`：**62 条**，全部带量化死因（beam/割点/环模型/
  重启/iddeep/tiered/桥/pin/等价0/subtour0/增量传播节点相对障碍/PD128/SAC0/
  换数学空间/767 验尸教训/缝定律/SWEEP 标定错误 等全入册）
- [x] `research/insights.jsonl`：**39 条**（流 GAC/端点过滤/度不在连通性/
  BJ=500 机制/赢家树定标/墙四型诊断法 等）
- [x] `research/mutation_prompt.md`：模板含 fingerprint 字段表、dead_ends 注入
  规则（同模块/同盘型/soundness 三条常注入）、可证伪预言+按型收益声明要求
- 关键条目已内联进 evolve/config.yaml 的 system_message（policy 空间相关 6 条）

### P3：OpenEvolve 接入 —— ✅ 2026-08-28 搭建完成，待 API key 点火
- [x] 方案定为 **policy.h 模块化**（优于原计划的"v78 里散布 EVOLVE-BLOCK"）：
  `evolve/solver.c` = v78 冻结副本 + 钩子；`evolve/policy.h` = 唯一进化对象
  （分支打分函数 / 武器周期自调 / 全部预算调度宏）。prompt 只带 70 行头文件，
  冻结边界 by construction。**默认 policy 与 v78 校准 21 关逐节点相等（已断言）。**
- [x] `evolve/evaluator.py`：编译→T0 REFSOL 硬门(一票否决)→T1→T1H 三段级联，
  基线 combined_score 0.598（t1 全过 + hard 4/6，271/287 是现成靶子）
- [x] `evolve/config.yaml`：islands=3、MAP-Elites 行为描述符
  （deduction_frac × b_over_a，P1 结论：结构同质 ⇒ 多样性只能来自行为）、
  级联阈值 [0.3, 0.55]、dead_ends 关键条目内联 system_message
- [x] OpenEvolve 0.3.2 已装（WSL pip）；checkpoint/谱系是其内建功能
- [ ] **点火待办：需要 Anthropic API key**（WSL 里 export OPENAI_API_KEY，
  用 api.anthropic.com 的 OpenAI 兼容端点；跑法见 evolve/README.md）
- [ ] 探针排序/预算分配的第二批 policy 钩子（先看第一批收敛情况）

### P4：architecture island（P3 明显 plateau 后再开）
- 单独一个 island 准许大手术：范式级改动（状态表示、分解、按型切策略）。
- 目标形态参考 EoH-S：不是一个万能 heuristic，是**一组互补策略 + 按 fingerprint
  的 selector**（"if 盘型≈A 用策略A"）。我们已有按型数据基础。

## 四、成功判据（防自嗨）
- P0~P2 本身就该回本：cascade + 校准套件让**人肉迭代**也变快（现在每次 A/B 全靠手跑）。
- P3 的第一个里程碑不定"破新关"，定：**进化在校准套件上找到 ≥1 个
  人没试过的、经 T2 验证不退化的参数/排序改进**。找不到 ⇒ 参数空间已被
  人肉榨干，是有价值的负结果，architecture island 提前。
- 随时对照红线：跨机器绝对秒数不可比（A/B 必须同机）；官方口径 600s/关。
