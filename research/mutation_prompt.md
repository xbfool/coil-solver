# Coil 求解器 mutation prompt 模板

> 用途：EVOLVE-PLAN P2/P3。由 harness 填充占位符后喂给 LLM，产出一个**可实验证伪的算法改动**。
> 产出会被机器 apply → compile → cascade（T0 soundness 硬门 → T1 校准套件 → T2/T3），结果回填谱系。
> 红线：只喂代码+统计，**永不喂解**（解库私有，防训练集污染）。

---

## 一、Parent 代码 / diff

```
{{PARENT_CODE_OR_DIFF}}
```

<!-- harness 填充：EVOLVE-BLOCK 标注的代码段（v78-rpbj.c 的分支排序 / 剪枝调度参数
     BJ/SWEEP/PROBE/梯队预算/restart 策略 / 探针排序与预算分配 三类区域之一），
     或 parent→child 的完整 diff。harness 冻结清单（parser、check 调用、cascade、
     统计输出、REFSOL 回放）不在可改范围内，也不要建议改它们。 -->

## 二、Parent 的 performance fingerprint

```
{{PERF_FINGERPRINT}}
```

<!-- harness 填充：results/vec/<parent>.jsonl 的相关行（或按型聚合摘要）。 -->

**格式说明**（`tools/evalvec.py` 产出，每行一个关卡的 JSON 对象）：

| 字段 | 含义 |
| --- | --- |
| `level`, `w`, `h` | 关号与盘面宽高 |
| `solved`, `verified`, `timeout` | 是否解出 / 官方 check 是否通过 / 是否超时（solved 但 check 拒绝 = UNSOUND，直接淘汰） |
| `wall_ms` / `wall_s` | 墙钟（同机才可比；跨机绝对秒数不可比） |
| `nodes_total` | 跨起点累计搜索节点数（⚠ swarm 多进程时每 shard 各打一行，做向量必须 JOBS=1） |
| `nodes_win` | 赢家树节点数（判断墙型的关键：赢家便宜=调度/排序病，赢家贵=树价病） |
| `dth_liveend / dth_estate / dth_reach / dth_dyn / dth_flow / dth_geom` | 各剪枝层的死因计数（死因经济学：哪层在杀树） |
| `maxrss_kb`, `start_x`, `start_y` | 内存峰值 / 赢家起点 |

读法提示：比较 parent 与基线时，先看**按型分列**的 `nodes_total`/死因构成，再看墙钟。
中盘提速与 frontier 突破相关性弱（历史实证：probing 中盘 +20 关对墙无效；BJ=500 对
767 型 100×、对别型未验证），**单标量结论不采信**。

## 三、相关 dead ends（别重新发明这些死路）

```
{{RELEVANT_DEAD_ENDS}}
```

<!-- harness 填充规则（从 research/dead_ends.jsonl 挑选注入，每条一行 JSON）：
     1) 同模块：本次变异目标所在模块的全部条目——
        传播/规则层 → cycle-model-propagation, pin-*, equivalence-reasoning, subtour-*,
                      bridge-rule, sac-global-layer, dirlayer-rules-only, regpar-* 等；
        流/全局过滤 → milp-cutting-planes, pairblock-*, global-probing-no-start;
        搜索/排序   → beam-search, sc2-deepprobe-rank, f-signal-universal-rank,
                      start-geometry-features, multi-source-bfs, reverse-search-from-end;
        调度/预算   → start-iddeep, uniform-tiered-probe, budget-portfolio,
                      startcap-single-limit, pecon-budget-cap, sweep-mislabeled-calibration;
        增量/工程   → trail-undo-memcpy, incremental-propagation-node-relative,
                      incremental-connectivity-dual, flow-hotstart-endtoend, chamber-nogood-reuse。
     2) 同盘型：本次变异预期改善的盘型的全部条目（零信息针尖类 / 传播强富矿类 /
        厚墙类 / 767 重尾类），按 scope 字段匹配。
     3) 永远注入 soundness 三条：stack-overflow-false-exhaustion,
        two-sample-causality, entry-engine-accounting-illusion。
     相关 insights（research/insights.jsonl）可按同样规则附在其后作为正面参照。 -->

**约束**：如果你的想法与上面某条 dead end 本质相同，**不要提出它**——除非你能明确说明
该条 `lesson` 字段里写的重试前提条件现在已被满足（说出具体是哪条、为什么满足）。

## 四、任务

基于 Parent 代码与 fingerprint，提出**恰好一个**算法改动。要求：

### 1. 可实验证伪

- 改动必须是一个可编译、可 A/B 的具体 diff，不是方向性建议。
- 附带**预期机制**：为什么这个改动会起作用（因果链，不是"可能有帮助"）。
- 附带**可证伪预言**：改动后哪些具体指标会怎么变
  （例：某型关的 `nodes_total` 降 ≥2×；某死因计数占比下降；某关墙钟从 X 到 Y）。
  预言落空即判负结果——负结果也有价值，但含糊的预言没有。

### 2. 按型声明预期收益

- 明确说出**预期在哪类盘上改善**：零信息针尖类（gfix=0、候选未收窄）/
  传播强富矿类 / 厚墙类（万里挑一候选+大树）/ 767 型重尾类 / 全型。
- 同时声明**预期在哪类盘上可能变差**（历史上多数改动一型赚一型赔，
  假装全型净赚的提案不可信）。
- 参数/调度类改动优先：本项目最近两次最大跃迁（SWEEP 276 倍参数错误的修正、
  BJ=500 的位置+稀疏度）都出自这个空间，不是新算法。

### 3. soundness 论证（硬性要求，参考 767 教训）

任何新剪枝/新证伪路径必须论证**不误杀解**：

- 写明剪枝的数学依据：为什么被剪的分支**不可能**通向任何解
  （必要条件违反 / 搜穷证明 / 已证规则的复合）。"经验上没出错"不算论证。
- 特别警惕 767 案的形态：**unsound 的行为会让求解器看起来更快更果断**
  （浅层测试全过、中盘提速、某关沉默失败）——栈溢出被记账成"搜穷"就骗过了
  499 关回放。你的改动会经过 REFSOL/SOLWALK 真解回放 + postrust 阳性对照硬门，
  崩溃/断言违例/假解 = TAINTED 一票否决，不给分。
- 若改动引入"证伪失败即放行"的软路径，说明放行方向是保守的（少剪不会错剪）。
- 若改动触碰以下高危区，逐条说明如何规避：
  共享可变数组+多起点重入（默认用自己的数组）；按起点算又被缓存复用的状态
  （检查恢复路径）；带假设跑一遍的函数（跑完还原全局状态）；
  残量图正/反向弧判据（依赖哑点在哪侧，三次栽过）。

### 4. 输出格式

```
## 改动名（kebab-case）
## Diff
<统一 diff 或替换后的 EVOLVE-BLOCK 代码>
## 机制
<因果链：改了什么 → 为什么树/时间/命中率会变>
## 预期改善盘型 + 可证伪预言
<型别 + 具体数字预言；以及预期风险盘型>
## soundness 论证
<为什么不误杀解；触碰高危区时的规避措施>
## 与 dead_ends 的关系
<声明与注入条目均不同质；若是重试，指明满足了哪条 lesson 的前提>
```

---

## 附：评估流程提醒（harness 行为，供你校准提案粒度）

- T0 soundness 硬门：calib/refsol.txt 真解回放，断言违例("!!")/崩溃(信号)/未解/假解一票否决；
- T1 校准套件：17 个秒级关全 solved+verified 才 PASS，向量归档 results/vec/；
- 排名：先正确性（T0 一票否决），再 solved_count，再按型 log(nodes) 分列比；
- 评估成本：frontier 关一次 eval = 数百秒 × 26 核——**你的主战场是让秒级校准盘上的
  按型向量变好**，不是直接赌 frontier；
- A/B 必须同机同时同核数；跨机器绝对秒数不可比；重尾关单次跑测不出 20% 量级差异
  （±200s 方差实测），预言请落在节点数/死因构成这类稳定量上。
