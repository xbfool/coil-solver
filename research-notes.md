# 别人怎么解的（网上调研，2026-08-20）

**最重要的一条：我把 v9「边约束传播」判成死路是错的——第一个通关全部 1206 关的人用的正是这个方法。**
我错在把它建成了**环模型**（每格用 2 条边），而 Coil 是**路径**，有 2 个端点只用 1 条边，
全盘强制度 2 必然自相矛盾（正好解释我实测的"10 关里 6 关直接矛盾"）。方法没错，是我的建模错了。

调研范围：hacker.org 官方论坛 f=6 全部 38 帖、1 篇通关者复盘、1 篇 top-10 博客、约 16 个 GitHub 仓库。
**Reddit / HN / StackOverflow / CodeGolf 零内容；中文、日文、俄文写作均无**
（中文只有 pongba 组 2008 年一个老帖，最高 72 关）。没有任何论文研究 Coil 的**求解**方向。

关卡尺寸标定（拟合官方数据 ≈ 1.1 × level^0.75）：**L165≈50×50，L333≈85×85，L1000=337×336，L1206=2000×2000**。
官方榜通关者：Tron、adum、LudemeGames、KarlisS（均 1206）；captainfox（=博客作者 Kleemans）1098。

---

## 一、可信度最高：唯一一份通关者自述 —— Tron 的边约束传播

**出处**：https://www.hacker.org/f3/viewtopic.php?t=2294 （2011-10-03，通关当天发的复盘）
**可信度：最高**——官方榜可验证他是第一个 1206。

原文（我已亲自抓取核实）：

> "The basic idea is to determine and remove **infeasible moves**, e.g. '**the field at y,x will not be entered from the left side**'."

> "These methods include **local patterns, recursive patterns and bounded brute force**.
> Only after application of these pre-calculations I employ a **simple recursive brute force search**."

> "the solver cannot determine, which two sides are used, to about **0.3%**"

> "It is **4,500 lines of C**, about 1,000 are debug code... Solving level 1206 (2000x2000) took **37 minutes**."

**要点**：
1. 消除的对象是**「从某个方向进入某格」这件事不可行**——比"这条边用不用"更细（每格 4 个进入方向，有向）。
   这正好能装下 Coil 的**时序约束**（转向时前方必须是墙或已访问），而纯哈密顿的度约束装不下。
2. 不是"先搜再剪"，而是**先把搜索几乎消灭掉**：预处理后只剩 **0.3%** 的格子还不确定用哪两条边，
   最后才跑一个**朴素**递归搜索。
3. 他没提分区、也没提端点定位——**纯靠传播**就够了。

**对我们的直接启示**：v9 要重做，但要改三处——
(a) 按**路径**建模（2 个度 1 端点），别用环模型；
(b) 消除的对象改成**有向的「进入方向」**，不是无向边；
(c) 加上 local / recursive pattern 和 bounded brute force 这三层消除手段。

---

## 二、有硬证据的第二条路：瓶颈分区（做到第 332 关，代码可查）

**出处**：https://github.com/ericgopak/Mortal-Coil （C++，2014）
**可信度：高**——仓库里 committed 了 `solution0…solution332`，`Level333` 存在但无解，即**实测到 332 关**。

**切割判据只有 5 行**（`src/Simulator.cpp:8`），按「宽度为 1 的瓶颈」切：
```c
return (left->isObstacle() || frontLeft->isObstacle())
    && (right->isObstacle() || frontRight->isObstacle());
```

**主搜索完全不做格子级回溯**：离线穷举每个 component 的全部穿越方式，每条方式带三个位掩码
`mustBeBlockedMask` / `mustBeFreeMask` / `stateChangeMask`，主搜索只在 component 层面走，
**两次位运算判兼容**。这些掩码正是让分区**正确**而非启发式的关键——它解决了 2008 年 Brazzy 提出的
经典反驳（"入口会被路径经过而失效"）。

三个配套技巧值得单独抄：
- **按「出入口序列」等价归并**（`Analyzer.cpp:292`）：两条内部走法不同但进出口顺序相同的解，
  外部不可区分，只留一条。**这是让解目录不爆炸的关键。**
- **SPECIAL component**：穿越解数为 **0** 的区域**必然含起点或终点**。恰好 2 个时，
  起点候选直接从 O(格子数) 塌缩到几十个——**比"度 1 端点"强太多**（我们的盘面根本没有度 1 格）。
- **障碍接触一致性检查**（`Simulator.cpp:438`）：每个 8 连通墙块带计数，再次接触时要求
  "绕该墙块的相邻格已有被占"。**近 O(1)，不是割点检测**，可当 flood fill 的廉价替身。
  （⚠ 未验证是否严格 sound，要先小盘对拍。）

**⚠ 两个必须知道的坑**：
1. 抽样它 150–330 关的耗时：中位数约 19 秒，但**极不单调**——L290 = 0.9 秒，
   **L165 = 7322 秒（2 小时）**，L321 = 10.6 小时。**在我们现在 30 秒能过的 165 关上，它比我们慢两个数量级。**
   → **分区法是改变尾部形状，不是全面加速。** 别指望它顺手把中盘也变快。
2. `floodInnerSubcomponents()`（环形 / "donut" component，包住内部障碍的情况）是**空 stub**，
   作者自己没做出来（commit 原话："Tried implementing inner component absorbation - did not succeed yet"）。

---

## 三、同一思路的人话版：top-10 玩家博客

**出处**：https://www.kleemans.ch/mortal-coil （2024-04-09，Adrianus Kleemans = 榜上 captainfox，1098 关）

cave 分区法的图文详解：怎么切、为什么某些开口是**强制的**、怎么在 cave 间传播约束收缩解集。
关键原文：

> "entries and exits are sometimes **mandatory** (like the lower left), because else a **dead end is created
> in the adjacent cave**. The other two entries/exits are optional, as there exist solutions in the other
> caves that seal the entry so it isn't used at all."

> "Try a new opening. Move until either a dead end is found (in which case we roll back).
> Or, if another opening is found, exit the cave and start from 1."

> "Once all caves are 'solved' in isolation ... start to connect them to their neighbors and see what
> solutions are still feasible ... reducing the amount of possibilities"

端点定位（三种情形）：单开口的 cave 必为起/终点；**孤立求解无解的 cave 必含端点**；全有解则只能穷举。

**注意**：切分算法本身他没公开，兼容性传播也没有伪代码。他放出的 Kotlin 代码只是暴力基线。

---

## 四、论坛点名、但无人公开代码的进阶技巧

出处 https://www.hacker.org/f3/viewtopic.php?t=1093 （2008–2016）。**可信度：中**——自述无产物，但发言人都在榜上。

**tails**（当时 604 关，后到 1020）：
> "partitioning is practical and very effective. I think it's a **must-do in high levels**"
> "finding the endpoints is another essence to do. And it works well together with partitioning.
> **If you find two parts with endpoints inside, then all the other parts can be solved almost independently**"

**Hippo**（细节最多）：
- **conditional shortcuts**：先做度 2 链收缩，再加"**假设起点不在本区域**"前提下才成立的捷径。
  他说这是过 160 关的关键，也警告"转成条件捷径后 bug 多到调了一个月"，
  并给了回归测试关：**第 88 关**（正解的终点恰好落在相邻捷径的假设集里）。
- **按「违反约束条数」做迭代加深**（不是按深度）——假设"即使超大盘，失败约束数也很小"。别处没见过。
- **Euler-Tour Tree / Top Tree** 维护动态连通性，O(log n) 找路径上第一个分叉点，替代反复 flood fill。
- "parity excluding starts at some levels"、"degree one square dynamic detection"、"zero degree detection"
- "work on graph based on grid, but with **path shortcuts** ... researching paths with all vertices with
  degree 2 is surely waste of time"

**gfoot**：
> "The thing I've always found awkward with Mortal Coil is the endpoints, and generally
> **the fact that the order of path segments matters**"

**ernie**（1127 关）自述三阶段：
> "naive gets to around 50 or 60 / rewrite 1 gets to around 250 (python) or 500+ /
> rewrite 2 gets to 500 or so (python) or the top of the score list"

**别人踩过的坑（省得我们再踩）**：
- Brazzy 试过哈希表找重复模式——"太罕见，没用"，即**置换表在这题不划算**
- PSO 跑 4 小时只到 32 关（t=2331）
- Hierholzer 算法被 Hippo 直接否掉（t=2614）

---

## 五、关卡是构造出来的，不是对抗性随机

**出处**：https://github.com/ernop/coil-levels （生成器源码）、https://fuseki.net/home/Mortal-Coil.html

生成方式：先随机自避走一条 coil（`InitialWander`），再在**线段层面**做局部搜索反复 tweak
（coil 存成 `Seg` 链表，一次 tweak 把一段替换成更长的绕行），最后没走到的格子变成墙。
——**这印证了我们自己测出的"盘面是照着有解构造的"**（无度 1 格、墙的两色数量完全相等）。

有用统计：
- 官方关卡约 **60% 空格**（我们实测 67%）
- **撞墙后 80%+ 的情况根本没有选择**（我们实测 87%，独立吻合）
- 小盘多解，大盘通常只有一个或少数几个解（我们实测 L99/L139 各 1 个有向解，吻合）

两条推论：
(a) 解的自然搜索单元是**极大直线段**，不是格子；
(b) 1107–1206 关用 tails 的生成器，Tron 和 ernie 都说"结构上更简单"——
    **最难的关不是最大的关**（Tron 卡最久的是 1132，1260×1260，花了两周 CPU 时间）。

---

## 六、通用大规模哈密顿路径技术：坦白说没有能直接用的

- **Umans & Lenhart（FOCS 1997）** 多项式解 solid grid graph —— 硬性前提是**无洞**，Coil 满盘是洞，
  且 O(n⁴) 在 400 万格上本就不可用。无公开实现。 https://dl.acm.org/doi/10.5555/795663.796334
- **Backbite / Pósa rotation**（https://arxiv.org/abs/cond-mat/0508094 ，
  实现 https://clisby.net/projects/hamiltonian_path/ ）：O(1) 一次移动，但是**采样器不是求解器**；
  已发表实验只到 N=2500（50×50），**无 10⁶ 规模跑分**，且假设无洞点阵，装不下时序约束。
- **生成树加倍 / STC**（Gabriely & Rimon 2001）：唯一 O(N) 且显式支持障碍的构造法，
  但要求区域能在粗格上按 2×2 分解，**障碍边界会丢格子**——对必须恰好覆盖每个空格的 Coil 不成立。
- **插头 DP**：实用宽度上限约 12–15，1000+ 宽度无望；但**在分区法内部验证小区域**是对的工具
  （能处理洞、还能精确计数）。
- **FHCP Challenge**（https://arxiv.org/abs/1902.10352 ）：66–9528 顶点的困难 HCP 实例**至今无人全解**，
  通用启发式天花板在 10⁴ 顶点量级。
- 唯一对口的论文 **"Ice sliding games"**（https://arxiv.org/abs/1507.00559 ）研究的正是"滑到撞墙"机制下的
  覆盖问题，但是**生成方向**（最少放多少块砖能全覆盖），不是求解方向。**求解方向确实没人研究过。**

---

## 六点五、第二轮深挖（2026-08-21）：定标、结案、两条正确性警告

### ⭐ 最重要的一条：定标

| 人 | 到哪 | 参考耗时 |
| --- | --- | --- |
| **Tron** | 1206（通关） | **L910 只要 90 秒**；L1058 3m10s；L1206 (2000×2000) 37 分钟 |
| ericgopak（代码可查） | ~332 | L165 要 2 小时 |
| **我们（v23）** | **287** | L287 220 秒 |
| Hippo（论坛技术贴最多的那位） | **卡在 233** | — |

关卡尺寸校准：**L741 已经是 250×250，L1000 是 337×336**。

**所以：我们已经超过 Hippo，逼近公开代码最高的 ericgopak；但和 Tron 之间不是常数因子，是算法代差。**
他在 L910（比我们大一个数量级的盘）上只花 90 秒。

### 差距被量化了：传播的判定率

| | 判定率 | 出处 |
| --- | --- | --- |
| Tron | **99.7%**（"cannot determine which two sides are used, to about 0.3%"） | p20026 |
| 我们（传播+probing+配对） | **~48%**（L215 5215 条边判定 2538） | v28 实测 |

**他的搜索几乎不用搜（0.3% 未定后跑朴素递归），我们的搜索要面对一半未定的边。**
这就是全部差距所在，比任何单条剪枝规则都重要。

### 任务一正式结案：Tron 没留下更多

子代理把 **Tron 全部 30 个帖子** + Mortal Coil 子版 **全部 38 个主题**都读完了。
**全站只有 p20026 那一段描述求解器内部结构，没有第二处；论坛上不存在任何一个具体的
「这条边不可能」的模式实例。** local / recursive patterns 和 bounded brute force 他从未展开。
（论坛全文搜索索引是坏的，只能靠作者搜索逐帖遍历——这条路走到头了。）

### ⚠️ 两条正确性警告

**一、死端剪枝必须允许恰好 1 个。** gfoot 2008（[p7812](https://www.hacker.org/f3/viewtopic.php?p=7812#p7812)）：
> "creating a dead-end, because **that dead-end might be the one you're meant to end on**"

**我们已审计：`cheap_ok` 里是 `low_cnt - adj_low + adj_zero <= 1`，允许 1 个，没问题。**
ericgopak 也是这么做的（Solver.cpp:74 `temporaryEnds.size() > 1` 才剪）。

**二、朴素分区法是错的，mask 不是优化而是正确性条件。** brazzy 2008（[p7803](https://www.hacker.org/f3/viewtopic.php?p=7803#p7803)）：
> "an entry point into such a territory **can be eliminated by the coil passing it**"

区域入口会被路径「滑过」而消灭。ericgopak 的 `mustBeBlockedMask`/`mustBeFreeMask` 正是对这个的解答——
每条区域解显式记录它依赖哪些出口此刻仍空/哪些已被占，再用两次位运算核验。
**要做分区就必须带这两个 mask。**

### 新增的可用信息

- **T 型路口才是真决策点**：Tron 的 qpath 压缩格式（[p19597](https://www.hacker.org/f3/viewtopic.php?p=19597#p19597)）只在 T-crossing 记方向、
  拐角处省略——因为**撞墙后只有一侧能转时那是强制走法，不是分支**。
  （我们的 DFS 在 nc==1 时本来就不分支，这条已经满足。）
- **奇偶排除起点彻底死了**：实测 5 关（L100/200/280/300/330）**黑白格数差全部为 0**，
  只能推出「两端点异色」，推不出任何起点排除。Hippo 说的 "parity excluding starts at **some** levels"
  指的是不平衡的关卡，tails 这个生成器产的不是。（但 |黑−白| > 1 ⇒ 无解 是 O(n) 白送的检查。）
- **「先定端点」有三方独立背书**：tails、Hippo、ericgopak 的 SPECIAL component 机制。
- **conditional shortcut 别碰**：Hippo 自述「几乎用不上」，还给了坑关 L88。
- **置换表不划算**：brazzy 实测重复模式太罕见。

## 七、下一步该做什么（按性价比）

1. **重做 v9：边约束传播，但按路径建模、消除对象改成有向的「进入方向」**。
   这是**唯一被验证能到 2000×2000** 的方法，而且直接对症我上次失败的原因。
   配 local patterns / recursive patterns / bounded brute force 三层消除。
2. **瓶颈分区 + 每区域解目录 + must-be-blocked/must-be-free 掩码**（有 332 关的 committed 证据）。
   注意它**只改善尾部**，中盘可能反而更慢。
3. **零穿越解区域 ⇒ 起终点定位** —— 几乎免费，且直接打我们最大的瓶颈（起点彩票）。
   我们的盘面没有度 1 格，所以这条比"度 1 端点"实用得多。
4. **度 2 链收缩** + **障碍接触一致性**（O(1) 替代 flood fill，需先对拍验证 soundness）。
5. **奇偶排除起点**：两色数量差 1 时两端点必同色，一半格子当场排除。O(1)，还没做。
6. Hippo 的 **conditional shortcuts** 和 **按违反约束数迭代加深** —— 无人公开实现，风险高，
   但正好卡在我们现在的 165 关瓶颈上。记得用**第 88 关**做回归测试。


---

# 八、专攻「怎么产生禁用」的调研（2026-08-21 第三轮）

## 核心诊断被独立证实

> 「必用」推论来自**向上计数到界**（某格只剩 2 条可用边 => 都得用）。
> 「禁用」推论只有三个来源：**① 奇偶代数 ② 割/分离子论证 ③ 假设-反驳**。
> 你现有的规则（度数计数、成环排除）**几乎不对「加入一条边」这个动作产生反应** ——
> 强制一条边进去只带来局部度数变化，不级联；强制它出去反而立刻触发「只剩 2 个候选」的级联。

这和我们自己量出来的机制完全一致，也说明我们缺的不是调参，是**缺整类规则**。

## ⭐ 四条能直接用的（按性价比）

### 1. 割奇偶（cut parity）—— 路径版，日文叫「偶端定理」

对**任意**自由格集合 S：

    Σ_{e ∈ δ(S)} x_e  ≡  S 内端点个数  (mod 2)

证明：Σ_{v∈S} deg(v) = 2|E(S)| + |δ_used(S)|，内点 deg=2、端点 deg=1。

**用法（这才是关键）**——维护一个「区域库」，对每个区域看 δ(S) 里还有几条未定边：
- 未定 = 1 => **奇偶直接决定它**（约一半情况是禁用）← 正是我们最缺的
- 未定 = 2 且 S 必须被访问 => 两条都强制 => 触发四个格的度数级联
- 未定 = 0 且奇偶不符 => 矛盾
- **全部定完 => 反过来读出「端点在不在 S 里」** ← 这直接打我们最大的瓶颈（起点彩票）

区域库放什么：所有轴对齐矩形（前缀和 O(1) 增量）、墙的连通分量∪边界、当前部分链诱导的口袋、每个割点的两侧。
单格的情形退化成我们已有的度数规则 —— **威力全在大区域上**。

### 2. 顶点分离子 —— Hooker Corollary 2（**这条解释了我们的桥为什么找不到**）

Kaya & Hooker, *A Filter for the Circuit Constraint*, CP 2006：
> **Corollary 1.** If |L_S| > |S|, then G is nonhamiltonian.
> **Corollary 2.** If |L_S| = |S|, then **no edge connecting vertices of S is hamiltonian**.

Corollary 2 是纯否定的。他们报告：消掉了可行实例中约 **1/3** 的冗余取值。

**为什么这条能绕开我们的困境**：我们找不到桥，是因为这批盘面 **2 边连通**。
而分离子切的是**顶点不是边**，完全绕开这个问题 —— 带墙网格上分离子极好找
（每个单格门洞、每个墙角夹缝都是）。

配套的割点端口预算：割点 v 把残量图分成 C₁…C_k，则 **k ≤ 2**（k≥3 直接矛盾）；
k=2 且两侧都无端点时，v 到每侧**恰好用 1 条边** => **某侧一条边被选中，该侧其余边立即禁用**
—— 哪怕 v 离饱和还差得远。

### 3. 二部图度约束流 + Régin 残量图 SCC（一次计算同时给必用和禁用）

网格图是二部图，「每格恰好 2 条边、端点 1 条」就是二部图上的度约束子图，可以建成流：
源 →(下界=上界=2)→ 偶格 →(容量1)→ 奇格 →(2)→ 汇；端点用哑点处理且**保持二部性**（避开一般图的 blossom）。

求一个可行流 f，建残量图，跑一次 Tarjan SCC：

| 条件 | 结论 |
| --- | --- |
| f(e)=0 且两端**不同** SCC | **e 禁用** |
| f(e)=1 且两端**不同** SCC | e 必用 |
| 同一 SCC | 未定 |

调研方**实测过**（随机 Coil 式盘面，根节点）：边消除率比纯局部度数传播强 **1.2~2.6 倍**
（20×20@12% 是 2.62% → 6.87%，和我们量的 1.8% 同量级）。
但**真正的价值在死局检测**：15×15@10% 这种开阔盘面，廉价规则只能杀 132/357，
**流检测杀 314/357** —— 多出来的 182 个死局正是连通性和度数规则完全看不见的。
**它专门在我们现有传播「变瞎」的场合起作用。**

代价一次 max-flow，建议只在廉价规则到不动点时跑一轮。

### 4. Shaving 要配 3 才有用

Hertel 的 Look-Ahead Theorem：*forcing 一条边后图非哈密顿 => 该边可剪*。
Caseau & Laburthe（ICLP'97）实测「搜索开始前就能去掉**约一半**的边」。

**但关键在于**：单靠度数计数，「强制一条边」几乎不会失败 —— 这正是我们 v28 配对 probing
只挖出 6~10 条的原因。**接上第 3 条的流可行性检查之后，强制才会真正失败。**
这两个技术是互补的，单独任何一个都不够。

## 明确排除的（省得再花时间）

- **TSP 的 edge elimination（Jonker-Volgenant / Hougardy-Schroeder）**：判据是**欧氏度量**的
  （双曲线/锥形区域、圆盘半径），我们是单位权可行性问题，判据直接退化为空。
- **LP reduced cost fixing / α-nearness（LKH）**：都需要目标函数和上界；可行性问题下全零费用，
  reduced cost 全为 0。**但**它的正确类比被指出来了：可行性问题里 reduced-cost fixing 的对应物
  是「把 e 强制进去后松弛还有没有解」，而这在我们这里是**多项式可解**的 —— 就是上面第 3 条。
- **Slitherlink 的内外染色（带翻转位并查集）**：闭环才是 Jordan 曲线，
  **开放路径不能直接搬**，要加 T-join 修正（固定一条连接两端点的参考路径）。
  端点没钉死之前别碰，先用不需要参考路径的割奇偶。
- **滑行/冰面的求解算法文献**：再次确认**基本空白**。只有复杂度结果
  （MIT Hardness Group arXiv:2405.08377 证了 Icelom / Simple Loop 是 ASP-完全）。
- **奇偶排除起点**：我们已实测 |黑−白| 恒为 0，无效。

## 一个日文命名很到位

fff 的「理論に基づくスリザーリンク手筋集」把两类规则分得很清楚：
> 「**通過点理論**は小さなヒント数字に対して**×をつけることができる**」

**通過点理論 = 产「×」（禁用）的引擎，最大数理論 = 产「线」（必用）的引擎。**
我们手上全是后者，缺的是前者 —— 这就是那个 7:1 的来源。
