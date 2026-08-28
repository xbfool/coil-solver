# evolve/ —— OpenEvolve 进化实验场（EVOLVE-PLAN P3）

## 结构

| 文件 | 角色 | 谁能改 |
| --- | --- | --- |
| `policy.h` | **唯一进化对象**：分支排序、武器周期、预算调度 | 进化 LLM（只许改 EVOLVE-BLOCK 内） |
| `solver.c` | v78-rpbj.c 冻结副本 + policy 钩子 | ❄ 冻结（重生成用 tools 里的替换脚本思路） |
| `evaluator.py` | 冻结 harness：编译→T0 REFSOL 硬门→T1 校准→T1H 深树 | ❄ 冻结 |
| `config.yaml` | islands=3、MAP-Elites 行为描述符、级联阈值 | 人 |

基线等价性：默认 policy 下 `solver.c` 与 `bin/v78stats` 在校准 21 关**逐节点相等**（已验证）。
改 solver.c 或重生成后必须重跑这个断言。

## 跑

```bash
# WSL，评测器自测（不花钱）：
python3 evolve/evaluator.py            # 基线应得 combined_score≈0.60

# 进化（要 API key；Anthropic 的 OpenAI 兼容端点）：
cd evolve && export OPENAI_API_KEY=<key>
python3 -m openevolve.cli policy.h evaluator.py --config config.yaml --iterations 100
```

产出在 `evolve/openevolve_output/`（gitignore；好的 policy 手工提拔进 git + notes.md）。

## 评分（evaluator.py）

- **T0 一票否决**：13 关 REFSOL 回放，断言违例 / 崩溃(信号验尸) / 假解(官方 check 拒绝) → 0 分。
  767 教训：unsound 的表现形式是"更快更果断"，绝不能只看速度。
- **combined** = 0.35·T1全过率 + 0.20·节点分(基线持平=0.5) + 0.45·深树探针分。
  探针里 **L271/L287 是 v78 30s 解不动的现成靶子**，解出即大幅得分。
- **MAP-Elites 描述符**（P1 结论：盘面结构同质，多样性必须来自行为）：
  `deduction_frac`（演绎击杀占比：传播派↔几何派）、
  `b_over_a`（B/A 型节点比：致密盘特化↔开阔盘特化）。

## 红线

1. 解库私有：进化管线只见代码+统计，**永不喂解**（REFSOL 路径只进 env，不进 prompt/metrics）。
2. 新剪枝不走进化：policy.h 只管排序/密度/预算（sound by construction）。
   想进化剪枝本体 = P4 architecture island，须人工 REFSOL 审计流程伴跑。
3. A/B 必须同机；跨机器绝对秒数不可比。

## 基线成绩（2026-08-28，台式机 WSL）

```
combined_score 0.598 | t1_frac 1.0 | t1_node_score 0.5 (定义如此)
hard_solved 4/6 (271/287 未解) | deduction_frac 0.545 | b_over_a 0.353
```
