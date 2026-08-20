# 别人怎么解的（网上调研，2026-08-20）

结论先行：**公开资料极少，但高分玩家的共识非常一致——「分区 + 定位端点」是高关卡的必经之路。**
而且这正好命中我实测出来的瓶颈（起点彩票）。另外有几条具体技巧我还没实现。

调研范围：英文/中文/日文/俄文/韩文/德文/法文/西班牙文/意大利文 约 20 组关键词。
**非英文资料基本为零**（知乎/CSDN/博客园/Habr/Qiita 全部零命中），Hacker News / Reddit / Stack Overflow 也零。

## 一、最有价值的一手资料

### 论坛主帖 —— https://www.hacker.org/f3/viewtopic.php?t=1093

技术含量最高的一处。摘关键原文：

**tails**（排行榜 #10，到过第 1020 关，东京）：
> "partitioning is practical and very effective. I think it's a **must-do in high levels**"
> "finding the endpoints is another essence to do. And it works well together with partitioning.
> **If you find two parts with endpoints inside, then all the other parts can be solved almost independently**"

**adum**（站长）——关卡规模参考：
> Level 1000 约 250×250；Level 1010 约 500×500

**ernie**（排行榜 #7，到过第 1127 关）——他自己的三个阶段：
> "naive gets to around 50 or 60 / rewrite 1 gets to around 250 (python) or 500+ / rewrite 2 gets to 500 or so (python) or the top of the score list"

**Hippo**（技术细节最多的一位）：
> "parity excluding starts at some levels"
> "degree one square dynamic detection to stop searching in earlier stages"
> "work on graph based on grid, but with **path shortcuts** ... researching paths with all vertices with degree 2 is surely waste of time"
> "added zero degree detection"
> "heuristic for choosing starting point rather near places which were rarely visited on best fills"
> "**EulerTrees** of Monika Rauch Henzinger and Tarjan variant of **Top Trees**" ——用途："maintaining ET for a minimum spanning tree would guarantee the connectivity"

**gfoot / therethinker** —— 都说分区可行但不愿细说：
> "The thing I've always found awkward with Mortal Coil is the endpoints, and generally **the fact that the order of path segments matters**"（gfoot）
> "partitions are possible. The trick is in how you create them & define them... I don't want to reveal too much"（therethinker）

### 唯一一篇正经博客 —— https://www.kleemans.ch/mortal-coil

Adrianus Kleemans，进过 Top 10。他的「cave（洞穴）分区法」：

1. 把棋盘切成若干 cave，cave 之间由**开口**相连
2. 开口分**必需/可选**：
   > "entries and exits are sometimes mandatory (like the lower left), because else a dead end is created
   > in the adjacent cave. The other two entries/exits are optional, as there exist solutions in the other
   > caves that seal the entry so it isn't used at all."
3. 每个 cave 先**孤立求解**，枚举它所有可能的走法集合：
   > "Try a new opening. Move until either a dead end is found (in which case we roll back).
   > Or, if another opening is found, exit the cave and start from 1."
4. 再做**跨 cave 的兼容性传播**剪枝：
   > "Once all caves are 'solved' in isolation ... start to connect them to their neighbors and see what
   > solutions are still feasible ... reducing the amount of possibilities"
5. **端点推导**（三种情形）：
   - 只有一个开口的 cave → 必然含起点或终点
     > "The orange cave on top only has one opening, so it has to be either the start or end cave."
   - 孤立求解无解的 cave → 必然含端点
   - 全都有解 → 只能穷举试

注意：**切分算法本身他没公开**（没说是按割点、最小割还是别的），兼容性传播也没有伪代码。
他放出来的 Kotlin 代码（mortal-coil-starter）只是个暴力基线，不是他的真解法。

### ernie 的项目页 —— https://fuseki.net/home/Mortal-Coil.html

内容很薄，但有一条统计**独立验证了我的测量**：
> "Most (**80%+**) of the time you hit a wall, you don't actually have a decision"

我自己测的是 87%（L99 401/462，L139 726/840）——对上了。
他的方法概括为 "Brute force with backtracking" + "board split" + "too many deadends" 两类剪枝。

## 二、公开代码的水平

找到 14 个 GitHub 仓库，**全部 star 数 ≤8，且都只到第 100 关左右**：

| 仓库 | 语言 | 星 | 声称水平 |
| --- | --- | --- | --- |
| gerard/mortal-coil | C | 8 | 未声称 |
| ericgopak/Mortal-Coil | C++ | 3 | 未声称 |
| Xsitsu/Mortal-Coil-Solver | C++ | 2 | "a little over level 100" |
| szge/CoilSolver | Python | 1 | 每步 flood-fill 剪枝 |
| 其余 10 个 | — | 0 | 多为玩具/生成器 |

**也就是说本仓库现在的 v11（第 165 关）已经超过所有公开代码。** 排行榜上限是 1206
（Tron、adum、LudemeGames、KarlisS 到顶）。

## 三、对我们的直接启示（按性价比排序）

1. **分区（cave partitioning）+ 端点定位** —— 两位到过 1020/1127 关的人都说这是必经之路，
   而且**两者是配套的**：先用「单开口 cave 必含端点」定位起终点，其余分区就能近乎独立求解。
   这恰好解决我实测出的最大瓶颈——起点彩票（解唯一 ⇒ 起点唯一，但要试 n 个起点）。
   我之前想的「按强制边拆」失败了（环模型不成立），但**按窄开口拆**是另一回事，值得做。

2. **度 2 顶点收缩（path shortcuts）** —— Hippo："researching paths with all vertices with degree 2
   is surely waste of time"。我实测度 2 的格子占 30%，收缩掉能显著缩小搜索图。**我还没实现。**

3. **动态连通性（ET-trees / Top Trees）** —— 用来替掉我现在每节点 O(剩余格数) 的洪水填充。
   这是我 v3→v4 一直在绕的开销，Hippo 直接点名了正确的数据结构。

4. **奇偶排除起点** —— Hippo："parity excluding starts at some levels"。
   当自由格两色数量差 1 时，两个端点必然同色，**一半的格子当场被排除**。O(1) 实现，我还没做。

5. **起点启发式** —— Hippo：优先选「在历史最好尝试里很少被走到」的位置附近作为起点。

## 四、没找到 / 存疑

- **切分算法的具体做法始终没人公开**（tails、therethinker、gfoot 都明确表示不细说）。这是要自己想的部分。
- 非英文世界基本没有讨论；tails 是东京人但没有任何日文写作。
- 一个搜索片段提到「parity 缩小起点集合 + degree-one 动态检测」，但**无法定位到具体帖子**，标记为未证实。
- https://www.hacker.org/f3/viewtopic.php?t=994 内容像是 Mortal Coil（提到 coderT 第 513 关、
  gfoot "any level under two seconds"），但抓取时标题显示为 "Runaway Robot"，**归属存疑**。
