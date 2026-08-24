// Coil solver — v65: 漏斗动态取号（work stealing）—— 治「最慢分片决定总时间」
//
// 以下是 v64 的说明。
// Coil solver — v64: FUNNELIN（读回缓存的漏斗幸存者，跳过最贵的筛选）
//
// 以下是 v63 的说明。
// Coil solver — v63: 修 SWEEP 第一轮预算被 n 放大到近乎无限（防怪物垄断的机制因此失效）
//
// 以下是 v62 的说明。
// Coil solver — v62: v58 + PINSTART（只跑指定起点，用于拆解耗时）
//
// 以下是 v58 的说明。
// Coil solver — v58: v56 + 探针预算按类分家（零信息类 PROBE=650/SWEEP=3M，其余 2000/3M）
//
// 以下是 v53 的说明。
// Coil solver — v53: 升档目标 PD=16（平台点）+ 升档分片第三层多轮限额扫荡（怪物不再垄断队列）
//
// 以下是 v52 的说明。
// Coil solver — v52: 升档时第 1.5 层深层重探（复现 v50 的杀集与排序，成本只花在幸存者上）
//
// 以下是 v51 的说明。
// Coil solver — v51: 传播深度三段式（gfix强类深层｜浅探存活率>=45%升深层｜DYNMIN降档兜底）
//
// 以下是 v50 的说明。
// Coil solver — v50: 传播深度初值恒深层，降档交给 DYNMIN 自测（零信息类 22 倍）
//
// 以下是 v48 的说明。
// Coil solver — v47: 分支点上的流可行性检查（把最强的规则搬进搜索的浅层）
//
// 以下是 v45 的说明。
// Coil solver — v45: swarm 模式（候选极少时全员同攻一棵树，靠随机顺序搏超线性）
//
// 以下是 v44 的说明。
// Coil solver — v44: 死端必须是终点候选（tcand 的强用法；v43 的计数版太弱）
//
// 以下是 v43 的说明。
// Coil solver — v43: 终点候选剪枝（终点候选平均只剩 ~40 格，但这信息一直没进搜索）
//
// 以下是 v40 的说明。
// Coil solver — v40: 去掉 probing（流已经覆盖了它，而它烧掉 89% 的时间证伪 0）
//
// 以下是 v39 的说明。
// Coil solver — v39: 把链定向提到 probing 前面（0.3% 的代价证伪 98% 的起点）
//
// 以下是 v35 的说明。
// Coil solver — v23: 自适应分层（只在第一层存活率高时才上第二层大预算）
//
// 以下是 v21 的说明。
// Coil solver — v21: 分层探针（小预算 -> 大预算 -> 无限搜），propagate_strong 只算一次
//
// 以下是 v20 的说明。
// Coil solver — v20: v19 + probing（failed literal），只在正式搜索阶段的起点上做
//
// 以下是 v19 的说明。
// Coil solver — v19: v18 + 有向层（必用边链的定向 + 首步滑行推理）
//
// 以下是 v18 的说明。
// Coil solver — v18: v17 + 把传播搬进搜索内部，当浅层分支点的剪枝
//
// 以下是 v17 的说明。
// Coil solver — v17: v16 + 约束传播（每个起点开搜前跑一次，定死的边拿来剪枝）
//
// 以下是 v16 的说明。
// Coil solver — v16: 把每节点的全盘洪水填充换成等价的「局部」连通性检查
//
// v13 的每个搜索节点，对每个候选方向都做一次 reach_ok —— 从落点洪水填充整个剩余区域，
// 数够不够 remaining 个。这是最强的一条剪枝，但也是 O(剩余格数)：L195 开局就是三千格，
// 一个节点最多四个候选，光这一项就是每节点上万次访存。
//
// 但它可以严格等价地做成局部的。进入某节点时剩余区域 R 已知连通（上一层验过），
// 本次滑行吃掉的只是一条直线 S，新剩余 R' = R \ S。R' 要被切开，切口必然紧贴 S；
// 又因为 R 连通，R' 里任何一点都能在 R' 内走到某个「S 的自由邻居」。于是
//
//     R' 连通  ⟺  S 的自由邻居两两连通
//
// 这些邻居一旦连上就能立刻收工，不用把剩余区域数完。滑行终点 c 也在 S 里，
// 它的自由邻居同属这个集合，所以「从 c 出发能到全部剩余格」一并被保证。
//
// 归纳的底座要单独打：起点处剩余区域是「全盘去掉起点」，起点若是割点就不连通了，
// 所以每个起点开搜前照旧做一次完整的 reach_ok（顺带白捡一条起点剪枝）。
//
// 以下是 v13 的说明。
// Coil solver — v13: 修掉 v11 的并行失效 bug（emit() 写了却从未被调用）
//
// v11 的意图是「谁先解出就把答案写进管道退出，父进程收第一个成功的并杀掉其余」，为此写了
// emit()——但两处解出的地方走的都是 printf + return 0，emit 一次都没被调用（编译警告
// 'emit' defined but not used 就是证据）。后果：
//   · 子进程把答案直接打进继承来的 stdout，所以校验能过，成绩看着是对的；
//   · 父进程从管道只读到 0 字节，于是**不 kill 其余子进程**；
//   · evaluate.py 用 capture_output 抓 stdout，必须等所有持有该 fd 的进程退出才返回。
// 所以 v11 的实测耗时 = 最慢的那个 shard 把自己那片起点全部搜穷，而不是「最快找到解」。
// 28 核并行的收益基本被这一个 bug 吃掉了。
//
// 本版只改这一件事，好单独量出它值多少分：
//   (a) 两处解出都改走 emit()（写管道 + _exit(0)）；
//   (b) 子进程 fork 后关掉所有不属于自己的管道端；
//   (c) 父进程从 wait() 改成 poll()，第一个写出答案的孩子赢，立刻 kill 其余。
//
// 以下是 v11 的原始说明。
// Coil solver — v11: v10 + 起点分片并行（fork），32 核直接打在「起点彩票」这个瓶颈上
//
// 实测打脸：真实关卡的解极少（常常唯一），穷举整棵树也不大——L139 全枚举才 1680 万节点，
// 几秒跑完；而 v2~v4 的 Luby 重启到 4 万节点就换起点，等于把搜过的反复扔掉，同一关 30 秒超时。
// 所以这版删掉重启和随机化，老老实实系统搜索；连通性检查恢复成每节点都做（最强的一条，代价撑得住）。
//
// v3 的瓶颈：每个候选都做一次 O(剩余格数) 的洪水填充。盘子越大越贵，而大盘恰恰需要探更多节点。
// v4 把两条剪枝都降成 O(1)：
//   · 奇偶染色（v3 那条）——两色计数增量维护
//   · 死端/孤格——给每个未访问格维护「未访问邻居数」deg，标记访问时只更新 4 个邻居，
//     同时维护 deg<=1 的格数 low_cnt 和 deg==0 的格数 zero_cnt。当前位置 p 的相邻格能从 p 进来，
//     实际度要 +1，检查时扫 p 的 4 个邻居修正即可。
// 精确连通性（洪水填充）仍然保留，但只在剩余格数 <= FF_MAX 时做：残局时它便宜且最能砍，
// 开局时它最贵而 O(1) 那两条已经够用。
//
// 输入（stdin 或文件参数）：x=<w>&y=<h>&board=<w*h 个 . 或 X>
// 输出：x=<起点x>&y=<起点y>&path=<UDLR 序列>

// 瓶颈是「解唯一 => 起点唯一，但要试 n 个起点」。这件事天然并行：起点之间完全独立。
// 用 fork 而不是线程，是因为所有工作数组都是全局的——fork 之后每个子进程自带一份副本，
// 零重构。子进程 i 只处理下标 ≡ i (mod P) 的起点（探针和正式搜索都只做自己那份），
// 谁先解出就把答案写进自己的管道并退出 0，父进程收到第一个成功的就打印并杀掉其余。
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/prctl.h>

static int shard = 0, nshard = 1;
// ===== swarm 模式（v45）=====
// L319 的画像：全局过滤把起点砍到 94 个、探针杀到只剩 1 个幸存者 —— 起点彩票已经赢了，
// 但那 1 个起点的搜索由**单核**扛 271 秒，其余 27 个核全部退出闲置。
// 对策：候选总数 <= 4*核数 时放弃「按下标分片」，**每个 shard 都处理全部起点**，
// 但 DFS 的同分候选用 shard 专属的随机数打破平局 —— 28 种不同的探索顺序同时搏一棵树，
// 谁先撞到解谁赢（管道协议不变）。顺序多样性在针尖搜索上经常有超线性收益，
// 且这些核本来就是白扔的。shard 0 保持确定性顺序当保底。
static int swarm = 0;
static int force_shard_identity = -1;   // FORCESHARD：单进程复现指定 shard（gprof 用）
static inline int sident(void) { return force_shard_identity >= 0 ? force_shard_identity : shard; }
static int swarm_depth = 3;
static unsigned int shard_seed = 0;
// ===== 漏斗的动态取号（work stealing）=====
// 实测 L664：各分片漏斗耗时 最快113s / 中位124s / **最慢697s** —— 6 倍离散度。
// 总耗时由最慢那片决定，其余 27 个核在 124 秒后就闲着了。
// 静态分片（i % nshard == shard）下谁摊上怪物起点谁倒霉，别人帮不了忙。
// 换成共享原子计数器让子进程自己领号：总工作量 ≈ 4000 核·秒 / 28 ≈ 143 秒。
static volatile int *work_ctr = 0;       // mmap 共享，子进程 fetch_and_add 领号
static int use_steal = 1;
static int out_fd = -1;                // 子进程把答案写这里
static void emit(int s, const char *pathstr);
extern long long flowdyn_calls, flowdyn_refute;
static long long dyn_calls, dyn_refutes;

extern int flow_depth;

#ifndef FF_MAX
#define FF_MAX 1200                  // 剩余格数 <= 此值才做精确连通性检查
#endif

static int W, H, N;
static unsigned char *g;             // 1 = 空且未访问，0 = 墙或已访问
static unsigned char *g0;
static unsigned char *col;           // 棋盘染色
static int *deg;                     // 未访问格的未访问邻居数
static int cnt[2];                   // 未访问格的两色计数
static int low_cnt, zero_cnt;        // deg<=1 / deg==0 的未访问格数
static int delta[4];
static const char DCH[4] = { 'L', 'U', 'R', 'D' };
static char *path;
static int path_len;
static int total_free;
static int *fstack;
static int32_t *seen;
static int32_t *srcmark;   // 标记「S 的自由邻居」，配 src_id 使用
static int32_t src_id;
static int *srcbuf;
static int32_t seen_id;
static int *dsu0;          // estate 必用边的并查集基座（每起点建一次）
static int dsu0_valid;
static int seed_full;      // v55：升档（零信息类）分片恒用全量播种，保住 v52 的证伪语义
static int seed_mode = 1;  // v56 portfolio：0=v52语义(空底全播) 1=v54(底座滑播) 2=v55(底座全播)，按 shard%3 混编
static long long nodes, node_limit;
static int best_rem;

static time_t wall_t0;
static long long dth_liveend, dth_estate, dth_reach, dth_dyn, dth_flow, dth_geom;
// 找到解就写进管道（子进程）或直接打印（单进程模式）
static void emit(int s, const char *pathstr) {
    if (getenv("DEATHSTAT")) fprintf(stderr, "DEATH@WIN sh%d liveend=%lld estate=%lld reach=%lld dyn=%lld flow=%lld geom=%lld nodes=%lld\n",
        shard, dth_liveend, dth_estate, dth_reach, dth_dyn, dth_flow, dth_geom, nodes);
    if (flow_depth > 0)
        fprintf(stderr, "FLOWDYN shard%d: %lld calls, %lld refuted\n", shard, flowdyn_calls, flowdyn_refute);
    if (swarm) fprintf(stderr, "WINNER shard %d tier %d nodes %lld\n", shard, shard & 3, nodes);
    static char obuf[1 << 22];
    int n = snprintf(obuf, sizeof obuf, "x=%d&y=%d&path=%s\n", (s % W) - 1, (s / W) - 1, pathstr);
    if (out_fd >= 0) { ssize_t wr = write(out_fd, obuf, (size_t)n); (void)wr; _exit(0); }
    fputs(obuf, stdout);
    exit(0);
}

static uint64_t rng_s = 0x243F6A8885A308D3ULL;
static inline uint64_t rnd(void) {
    rng_s ^= rng_s << 13; rng_s ^= rng_s >> 7; rng_s ^= rng_s << 17;
    return rng_s;
}

static long long luby(long long i) {
    for (long long k = 1; k < 62; ++k) {
        if (i == (1LL << k) - 1) return 1LL << (k - 1);
        if (i >= (1LL << (k - 1)) && i < (1LL << k) - 1)
            return luby(i - (1LL << (k - 1)) + 1);
    }
    return 1;
}

static inline int freedeg(int c) {
    return g[c + delta[0]] + g[c + delta[1]] + g[c + delta[2]] + g[c + delta[3]];
}

// ===== 终点候选剪枝（v43）=====
// per-start 的流在 Régin/SCC 过滤时顺带把「谁还可能当终点」算了出来（哑弧两端不同 SCC
// 且无流量 => 那格绝不可能是终点）。实测在**恰恰是瓶颈**的那几关上，终点候选平均只剩
// ~40 个格子（L223 32 / L239 40 / L257 48，占自由格的 0.8~0.9%）—— 但这信息一直没进搜索。
//
// 用法：路径的终点必然是「当前还没访问的格子」之一（remaining>0 时），而终点必在候选集里。
// 于是维护 live_end = 未访问的候选数，mark/unmark 各一次 O(1) 增量，**清零即剪枝**。
// soundness：候选集是在比当前更松的约束下算的（起点一定就定了），真解的终点必在其中。
static unsigned char *tcand;
static int tcand_for = -1;                       // tcand 是为哪个起点算的（防串台）
static int live_end = -1;                        // <0 = 本起点没有候选信息，剪枝关闭
// v44：比「候选计数到零」强得多的用法 —— **死端格必须是终点候选**。
// 现在的死端规则允许 <=1 个死端（那是未来的终点），但终点必须落在 tcand 里；
// 一个不在候选集里的死端根本当不了终点，出现即矛盾。ntc_low 与 low_cnt 完全镜像维护。
static int ntc_low = 0;                          // 未访问、deg<=1、且**不是**终点候选的格子数
static int tc_on = 0;

static inline void mark(int c) {                 // 未访问 -> 已访问
    if (live_end >= 0 && tcand[c]) --live_end;
    if (tc_on && deg[c] <= 1 && !tcand[c]) --ntc_low;
    if (deg[c] <= 1) --low_cnt;
    if (deg[c] == 0) --zero_cnt;
    g[c] = 0; --cnt[col[c]];
    for (int d = 0; d < 4; ++d) {
        int n = c + delta[d];
        if (g[n]) {
            int o = deg[n]--;
            if (o == 2) { ++low_cnt; if (tc_on && !tcand[n]) ++ntc_low; }
            if (o == 1) ++zero_cnt;
        }
    }
}

static inline void unmark(int c) {               // 已访问 -> 未访问（严格 LIFO 回滚）
    if (live_end >= 0 && tcand[c]) ++live_end;
    g[c] = 1; ++cnt[col[c]];
    for (int d = 0; d < 4; ++d) {
        int n = c + delta[d];
        if (n != c && g[n]) {
            int o = deg[n]++;
            if (o == 1) { --low_cnt; if (tc_on && !tcand[n]) --ntc_low; }
            if (o == 0) --zero_cnt;
        }
    }
    deg[c] = freedeg(c);
    if (deg[c] <= 1) { ++low_cnt; if (tc_on && !tcand[c]) ++ntc_low; }
    if (deg[c] == 0) ++zero_cnt;
}

// O(1) 剪枝：奇偶 + 死端/孤格
static inline int cheap_ok(int p, int remaining) {
    if (cnt[col[p] ^ 1] != (remaining + 1) / 2) return 0;      // 奇偶被锁死
    int adj_low = 0, adj_zero = 0;
    for (int d = 0; d < 4; ++d) {
        int n = p + delta[d];
        if (g[n]) { if (deg[n] <= 1) ++adj_low; if (deg[n] == 0) ++adj_zero; }
    }
    if (zero_cnt - adj_zero > 0) return 0;                     // 有进不去的孤格
    if (tc_on) {                                               // v44：死端必须是终点候选
        int antc_low = 0, antc_zero = 0;
        for (int d = 0; d < 4; ++d) {
            int n = p + delta[d];
            if (g[n] && !tcand[n]) { if (deg[n] <= 1) ++antc_low; if (deg[n] == 0) ++antc_zero; }
        }
        if (ntc_low - antc_low + antc_zero > 0) return 0;      // 存在当不了终点的死端 => 矛盾
    }
    return low_cnt - adj_low + adj_zero <= 1;                  // 死端最多 1 个（路径终点）
}

// 精确连通性：剩余格必须都能从 p 走到
static int reach_ok(int p, int remaining) {
    int top = 0, count = 0;
    ++seen_id;
    for (int d = 0; d < 4; ++d) {
        int c = p + delta[d];
        if (g[c] && seen[c] != seen_id) { seen[c] = seen_id; fstack[top++] = c; }
    }
    while (top) {
        int c = fstack[--top];
        ++count;
        for (int d = 0; d < 4; ++d) {
            int n = c + delta[d];
            if (g[n] && seen[n] != seen_id) { seen[n] = seen_id; fstack[top++] = n; }
        }
    }
    // CHAMBERSTAT：连通死亡的隔断集测量（抽样 1/64）。
    // 死区 U = 未被 BFS 摸到的自由格；隔断 = 与 U 相邻的已访问格（g==0 的非墙格）。
    if (count != remaining && getenv("CHAMBERSTAT")) {
        static long long cs_n, cs_hist[9];   // 隔断大小 1..8+
        if ((++cs_n & 63) == 0) {
            ++seen_id;
            int t2 = 0, usz = 0;
            for (int c = 0; c < N; ++c)
                if (g[c] && seen[c] != seen_id) { /* 标记法重跑一遍拿摸到集 */ }
            // 上面留空：直接用上一次 BFS 的 seen（同 seen_id-1 不可用），改为重跑：
            --seen_id; ++seen_id;
            int sep = 0;
            static int *ub = 0; if (!ub) ub = malloc(sizeof(int) * (size_t)N);
            int un = 0;
            for (int c = 0; c < N; ++c) if (g[c] && seen[c] != seen_id - 1) ub[un++] = c;
            (void)usz; (void)t2;
            // 数隔断：死区格的邻居中「非墙且已访问」的格（去重粗略：计边不去重，上界）
            for (int i2 = 0; i2 < un; ++i2) {
                int c = ub[i2];
                for (int d2 = 0; d2 < 4; ++d2) {
                    int nn = c + delta[d2];
                    if (!g[nn] && g0 && g0[nn]) ++sep;
                }
            }
            if (un > 0) {
                int k = sep < 1 ? 1 : (sep > 8 ? 8 : sep);
                ++cs_hist[k];
                if ((cs_n >> 6) % 8 == 1)
                    fprintf(stderr, "CHAMBER n=%lld hist(1..8+):%lld %lld %lld %lld %lld %lld %lld %lld usz=%d\n",
                        cs_n, cs_hist[1], cs_hist[2], cs_hist[3], cs_hist[4], cs_hist[5], cs_hist[6], cs_hist[7], cs_hist[8], un);
            }
        }
    }
    return count == remaining;
}

// ===== 约束传播层（每个起点开搜前跑一次，结果整局有效）=====
//
// v9 当年空转的真正原因不是「环模型 vs 路径模型」，而是**传播没有起爆点**：
// 度 2 规则要「这一格恰好用 2 条边」才能锁死两条边，可路径有两个端点只用 1 条边，
// 端点是谁又不知道，于是每格都得留「我可能是端点」的活口，谁也锁不死谁。
//
// 种子：路径在两色间严格交替，**起点一旦假定，终点的颜色就被完全确定**
//   （自由格数 n 为偶 => 终点异色于起点；n 为奇 => 同色）。
// 于是一半的格子当场被证明「不可能是端点」，它们里的度 2 格必须用满两条边，
// 传播起爆，再沿强制边连锁。我们本来就在逐个枚举起点，这个假定是免费的。
//
// 传播出来的东西整局有效——它说的是**最终解**的性质，跟搜索走到哪一步无关。
// 已在 11 个已知真解上验过 soundness：真解用到的边一条也没被禁用。
static int probe_rounds;
static int probe_all;
static unsigned char *estate;      // estate[c*4+d]: 0 未定 1 必用 2 禁用，与反向边同步
static unsigned char *g_estate;    // 全局阶段（不假定起点）推出的边，所有起点共用的基线
static int *dsu;
static int *pq;
static unsigned char *inq;
static int qhead, qtail;
static int prop_bad, prop_start, prop_endcol, prop_forced_end;

static int dfind(int x) { while (dsu[x] != x) { dsu[x] = dsu[dsu[x]]; x = dsu[x]; } return x; }

static void ppush(int c) {
    if (inq[c]) return;
    inq[c] = 1; pq[qtail++] = c;
    if (qtail == N + 1) qtail = 0;
}

static void set_edge(int c, int d, int v) {
    int n = c + delta[d];
    unsigned char *a = &estate[c * 4 + d], *b = &estate[n * 4 + (d ^ 2)];
    if (*a == v) return;
    if (*a != 0) { prop_bad = 1; return; }
    if (v == 1) {
        int ra = dfind(c), rb = dfind(n);
        if (ra == rb) { prop_bad = 1; return; }   // 闭成环，而 Coil 的解是路径
        dsu[ra] = rb;
    }
    *a = *b = (unsigned char)v;
    ppush(c); ppush(n);
}

static void pprocess(int c) {
    if (prop_bad) return;

    for (int d = 0; d < 4; ++d) {                  // 连上去就成环的待定边先禁掉
        int n = c + delta[d];
        if (!g0[n] || estate[c * 4 + d] != 0) continue;
        if (dfind(c) == dfind(n)) { set_edge(c, d, 2); if (prop_bad) return; }
    }

    int used = 0, avail = 0;
    for (int d = 0; d < 4; ++d) {
        if (!g0[c + delta[d]]) continue;
        if (estate[c * 4 + d] == 1) ++used;
        else if (estate[c * 4 + d] == 0) ++avail;
    }

    int lo, hi;
    if (c == prop_start)             lo = hi = 1;        // 起点只挂 1 条边
    else if (prop_forced_end == c)   lo = hi = 1;        // 已认定的终点
    else if (prop_forced_end >= 0)   lo = hi = 2;        // 终点定在别处，这里必是中间格
    else if (col[c] == prop_endcol) { lo = 1; hi = 2; }  // 颜色对得上，还可能是终点
    else                             lo = hi = 2;        // 颜色不对，绝无可能是端点

    if (used > hi) { prop_bad = 1; return; }
    if (used + avail < lo) { prop_bad = 1; return; }

    if (used == hi && avail > 0) {
        for (int d = 0; d < 4; ++d)
            if (g0[c + delta[d]] && estate[c * 4 + d] == 0) { set_edge(c, d, 2); if (prop_bad) return; }
    } else if (used + avail == lo && avail > 0) {
        for (int d = 0; d < 4; ++d)
            if (g0[c + delta[d]] && estate[c * 4 + d] == 0) { set_edge(c, d, 1); if (prop_bad) return; }
    }

    if (c != prop_start && used + avail == 1 && hi == 2) {   // 最多挂 1 条边 => 只能是终点
        if (col[c] != prop_endcol) { prop_bad = 1; return; }
        if (prop_forced_end >= 0 && prop_forced_end != c) { prop_bad = 1; return; }
        if (prop_forced_end < 0) {
            prop_forced_end = c;
            for (int q = 0; q < N; ++q) if (g0[q] && col[q] == prop_endcol) ppush(q);
        }
    }
}

// ===== 有向层：链定向 + 首步滑行 =====
//
// 无向层只能说「这条边一定被用」，说不出方向。但必用边天然拼成一条条**链**（每格最多挂 2 条），
// 而且**每条链都是路径的一个连续段**，于是有三条能判定的规则：
//
// 1. 链一旦定向，链内先后顺序就全定了。链上某格拐弯 ⇒ 前方那格必须是墙、或更早被访问；
//    前方那格若也在本链上而且位置更晚 ⇒ 这个定向不可能。两个方向都排除 ⇒ 起点矛盾。
// 2. **含起点的那条链是全序里的第一段**，占据路径位置 0..k-1，任何不在它上面的格子位置都 >= k，
//    绝不可能更早。所以这条链上拐弯点的前方格只剩两种合法可能：是墙，或就在本链上且位置更早。
//    落在别的链上、或落在未定区域 —— 一律矛盾。（严格版，比第 1 条强得多）
// 3. 起点的第一次滑行是在**空盘面**上做的，撞不到任何已访问格，所以必然一路滑到墙。
//    那整条线因此就是路径的开头：线上每条边都要被用，中间格必须直穿。
//    四个方向全不可行 ⇒ 起点证伪；只剩一个方向 ⇒ 整条线当场定死，回灌传播再滚一轮。
//
// 三条都在已知真解上验过 soundness。实测证伪率：奇数关 44% → 58%，
// **偶数关只有 0% → 0.4%** —— 因为偶数关里起点终点异色，「假定起点」是把一格从 [2,2] 放松成
// [1,1]，放松永远推不出矛盾；奇数关是从 [1,2] 收紧成 [1,1]。详见 notes.md。
// evaluate.py 只跑奇数关号，那些关的自由格数恒为偶数，所以这三条在快速口径下基本不涨分，
// 留着是为了全量评测（奇+偶）里那一半的关。
static int *chain_id, *chain_pos, *seqbuf;

// strict 用于含起点的那条链，见上面第 2 条
static int chain_dir_ok(const int *seq, int k, int rev, int cid, int strict) {
    for (int i = 1; i + 1 < k; ++i) {
        int prev = rev ? seq[i + 1] : seq[i - 1];
        int next = rev ? seq[i - 1] : seq[i + 1];
        if (next - seq[i] == seq[i] - prev) continue;       // 直穿，滑行规则没意见
        int f = seq[i] + (seq[i] - prev);                   // 拐弯时「前方」那一格
        if (!g0[f]) continue;                               // 前方是墙，拐弯天然合法
        if (chain_id[f] != cid) { if (strict) return 0; continue; }
        int tf = rev ? (k - 1 - chain_pos[f]) : chain_pos[f];
        int ti = rev ? (k - 1 - i) : i;
        if (tf > ti) return 0;                              // 前方那格更晚 => 这个方向不可能
    }
    return 1;
}

// 含起点的链，开头那段必须是直线，直到撞墙才允许拐（第一次滑行滑到底）
static int first_run_ok(const int *seq, int k) {
    if (k < 2) return 1;
    int e = seq[1] - seq[0], limit = 1;
    while (g0[seq[0] + (limit + 1) * e]) ++limit;
    if (limit > k - 1) limit = k - 1;
    for (int i = 1; i < limit; ++i)
        if (seq[i + 1] - seq[i] != e) return 0;
    return 1;
}

static int orient_chains(int s) {
    for (int c = 0; c < N; ++c) chain_id[c] = -1;
    int nchain = 0;
    for (int c0 = 0; c0 < N; ++c0) {
        if (!g0[c0] || chain_id[c0] >= 0) continue;
        int fdeg = 0;
        for (int d = 0; d < 4; ++d)
            if (g0[c0 + delta[d]] && estate[c0 * 4 + d] == 1) ++fdeg;
        if (fdeg != 1) continue;                            // 只从链端起步（环已被传播禁掉）

        int k = 0, cur = c0, from = -1;
        for (;;) {
            seqbuf[k] = cur; chain_id[cur] = nchain; chain_pos[cur] = k; ++k;
            int nxt = -1;
            for (int d = 0; d < 4; ++d) {
                int n = cur + delta[d];
                if (g0[n] && estate[cur * 4 + d] == 1 && n != from) { nxt = n; break; }
            }
            if (nxt < 0) break;
            from = cur; cur = nxt;
        }
        if (k < 3) { ++nchain; continue; }                  // 太短，没有内部拐点可查

        int head_s = (seqbuf[0] == s), tail_s = (seqbuf[k - 1] == s);
        int ok0 = chain_dir_ok(seqbuf, k, 0, nchain, head_s);
        int ok1 = chain_dir_ok(seqbuf, k, 1, nchain, tail_s);
        if (head_s) ok1 = 0;                                // 起点只挂 1 条边，必是链端，路径从它出发
        if (tail_s) ok0 = 0;
        if (head_s && !first_run_ok(seqbuf, k)) return 0;
        if (tail_s) {
            int kk = k < 8192 ? k : 8192;
            static int rev[8192];
            for (int t = 0; t < kk; ++t) rev[t] = seqbuf[k - 1 - t];
            if (!first_run_ok(rev, kk)) return 0;
        }
        if (!ok0 && !ok1) return 0;                         // 两个方向都走不通 => 矛盾
        ++nchain;
    }
    return 1;
}

// 第一次滑行沿方向 d 是否还可能：线上不能有禁用边，中间格不能有垂直的必用边
static int first_move_dir_ok(int s, int d) {
    int dd = delta[d];
    if (!g0[s + dd]) return 0;
    int c = s;
    while (g0[c + dd]) {
        if (estate[c * 4 + d] == 2) return 0;
        c += dd;
        if (g0[c + dd]) {
            int e1 = (d + 1) & 3, e2 = (d + 3) & 3;
            if (estate[c * 4 + e1] == 1 || estate[c * 4 + e2] == 1) return 0;
        }
    }
    return 1;
}

static int force_first_line(int s, int d) {
    int dd = delta[d], c = s, changed = 0;
    while (g0[c + dd]) {
        if (estate[c * 4 + d] != 1) { set_edge(c, d, 1); changed = 1; if (prop_bad) return changed; }
        c += dd;
    }
    return changed;
}


// 返回 0 表示传播出矛盾 => 这个起点被证伪（是证明，不是启发式）
static void prun_queue(void) {
    while (qhead != qtail && !prop_bad) {
        int c = pq[qhead++];
        if (qhead == N + 1) qhead = 0;
        inq[c] = 0;
        pprocess(c);
    }
}

static int propagate(int s) {
    dsu0_valid = 0;
    memcpy(estate, g_estate, (size_t)N * 4);        // 从全局推论出发，而不是从零
    for (int c = 0; c < N; ++c) dsu[c] = c;
    memset(inq, 0, (size_t)N);
    qhead = qtail = 0;
    prop_bad = 0; prop_forced_end = -1; prop_start = s;
    prop_endcol = (total_free & 1) ? col[s] : (col[s] ^ 1);

    if (getenv("FORCEEDGES")) {                 // 研究注入口：外部假设边
        static char febuf[1 << 16];
        strncpy(febuf, getenv("FORCEEDGES"), sizeof febuf - 1);
        char *tok = strtok(febuf, ";");
        while (tok && !prop_bad) {
            int fc, fd, fv;
            if (sscanf(tok, "%d:%d:%d", &fc, &fd, &fv) == 3 && g0[fc] && g0[fc + delta[fd]]) {
                unsigned char *ea = &estate[fc * 4 + fd];
                unsigned char *eb = &estate[(fc + delta[fd]) * 4 + (fd ^ 2)];
                if (*ea != 0 && *ea != fv) { prop_bad = 1; break; }
                if (*ea == 0) { *ea = *eb = (unsigned char)fv; ppush(fc); ppush(fc + delta[fd]); }
            }
            tok = strtok(NULL, ";");
        }
    }
    for (int c = 0; c < N; ++c) if (g0[c]) ppush(c);
    prun_queue();

    for (int round = 0; round < 8 && !prop_bad; ++round) {
        int nok = 0, okd = -1;
        for (int d = 0; d < 4; ++d) if (first_move_dir_ok(s, d)) { ++nok; okd = d; }
        if (nok == 0) { prop_bad = 1; break; }              // 四个方向都开不了局 => 起点证伪
        if (nok > 1) break;                                 // 还有得选，推不动了
        if (!force_first_line(s, okd)) break;               // 没有新信息，稳定了
        prun_queue();
    }
    if (prop_bad) return 0;
    return orient_chains(s);
}

// ===== probing（failed literal）=====
//
// Tron 说的 recursive patterns + bounded brute force，本质就是这个：
// 对一条还没定的边试着假设「它必用」，传播一遍；矛盾的话这个假设就被证伪，那条边只能是禁用。
// 反过来同理。矛盾是**证明**，不是启发式。反复滚到不动点。
//
// 实测（experiments/probe.c）：在「假定起点」的基础上 probing 一轮，
// 必用边从 25% 涨到 35~38%，L195 一个起点花 0.05 秒。
//
// 但**它一个起点都证伪不了**（偶数关 0%，试过传播/probing/链定向三层叠加都是 0%）。
// 原因是纯哈密顿路径约束下每个起点都真的可行——哈密顿路径的数量是指数级的，
// 度约束根本区分不了它们；Coil 的滑行约束只能经由「定向」进场，而定向要靠必用边，
// 即使推到 35% 链定向也还是抓不到矛盾。所以 probing 在这里只当**更强的剪枝**用，不当证伪器。
//
// 成本因此必须绕开：**只在正式搜索阶段的起点上做**。探针那一段已经把大部分起点筛掉了，
// 正式搜索通常头几个起点就出解，多花的 0.05 秒相对于动辄几秒的搜索是划算的；
// 要是探针阶段每个起点都做，L195 一个 shard 就是 5 秒，比整关的时间还长。
static unsigned char *bk_estate, *bk_inq;
static int *bk_dsu;
static int bk_fe;

static void psnapshot(void) {
    memcpy(bk_estate, estate, (size_t)N * 4);
    memcpy(bk_dsu, dsu, sizeof(int) * (size_t)N);
    memcpy(bk_inq, inq, (size_t)N);
    bk_fe = prop_forced_end;
}

static void prestore(void) {
    memcpy(estate, bk_estate, (size_t)N * 4);
    memcpy(dsu, bk_dsu, sizeof(int) * (size_t)N);
    memcpy(inq, bk_inq, (size_t)N);
    prop_forced_end = bk_fe;
    qhead = qtail = 0; prop_bad = 0;
}

// 试一个假设，返回 1 = 这个假设导致矛盾（于是反面成立）
static int try_edge(int c, int d, int v) {
    psnapshot();
    set_edge(c, d, v);
    prun_queue();
    int r = prop_bad;
    prestore();
    return r;
}

// ===== 二部图度约束流 + Régin 残量图 SCC =====
//
// 网格是二部图（按染色分两侧），而「每格恰好用 2 条边、端点用 1 条」就是二部图上的
// **度约束子图**，整个可以建成流。这比局部度数传播严格更强 —— 它是该松弛上的 GAC。
//
// 端点的未知量怎么处理（建模的关键）：起点 s 已假定，其颜色记 X；自由格黑白数实测恒相等，
// 所以另一个端点必是 ~X 色但位置未知。加一个哑点 z：
//   · z 放在 X 一侧，连**所有 ~X 格**（容量 1），且源到 z 的容量为 1
//   · 于是恰好一个 ~X 格从 z 拿到 1 单位 => 它的**真实**边数是 2−1=1 => 它就是终点
//   · 其余 ~X 格真实边数 2；s 的 b=1，z 不连 X 侧所以不影响 s
// 两侧度数和正好配平，二部性也保住 —— 避开一般图的 blossom。
//
// 已定的边先扣掉：rb(c) = b(c) − c 上必用边数；禁用边不进图。
// **流不可行 => 这个起点直接被证伪**（比连通性强得多的矛盾检测器，也正是让 shaving 起作用的那个）。
// 找到可行流后建残量图跑 SCC：两端不同 SCC 的弧，其取值被锁死 —— 同时产必用和禁用。
// FLOW=0 可关掉做对照。
#define FSRC 0
#define FSNK 1
#define FDUM 2
#define FCELL(c) ((c) + 3)

static int *fhead, *fnext, *fto, *fcap, *flevel, *fiter, *fq, *fmap;
static int fcnt, fnodes;
static long long flow_bans = 0, flow_uses = 0, flow_refute = 0;
static int use_flow = 1;
static int use_chain = 1;   // CHAIN=0 关掉链定向做消融对照

static void fadd(int u, int v, int cap) {
    fto[fcnt] = v; fcap[fcnt] = cap; fnext[fcnt] = fhead[u]; fhead[u] = fcnt++;
    fto[fcnt] = u; fcap[fcnt] = 0;   fnext[fcnt] = fhead[v]; fhead[v] = fcnt++;
}

static int fbfs(void) {
    for (int i = 0; i < fnodes; ++i) flevel[i] = -1;
    int qh = 0, qt = 0;
    fq[qt++] = FSRC; flevel[FSRC] = 0;
    while (qh < qt) {
        int u = fq[qh++];
        for (int e = fhead[u]; e != -1; e = fnext[e])
            if (fcap[e] > 0 && flevel[fto[e]] < 0) { flevel[fto[e]] = flevel[u] + 1; fq[qt++] = fto[e]; }
    }
    return flevel[FSNK] >= 0;
}

static int fdfs(int u, int f) {
    if (u == FSNK) return f;
    for (int *pe = &fiter[u]; *pe != -1; *pe = fnext[*pe]) {
        int e = *pe, v = fto[e];
        if (fcap[e] <= 0 || flevel[v] != flevel[u] + 1) continue;
        int d = fdfs(v, f < fcap[e] ? f : fcap[e]);
        if (d > 0) { fcap[e] -= d; fcap[e ^ 1] += d; return d; }
    }
    return 0;
}

static int *fscc, *flow2, *fdfn, *fstk, *fon, *fcv, *fce;
static int *hdsu, *hcross, *hedge;
static int use_subtour = 1;
static long long sub_uses = 0, sub_refute = 0;
static long long end_cand = 0;
static int hfind(int x) { while (hdsu[x] != x) { hdsu[x] = hdsu[hdsu[x]]; x = hdsu[x]; } return x; }

// ===== 全局端点过滤（开局跑一次，不假定起点）=====
//
// 起点必然是路径的两个端点之一。上面 per-start 的流已经证明「端点在度约束松弛下是被强约束的」
// （固定起点后终点候选只剩 0.9~2.4%）。那就把起点也放开：两个端点都用哑点表示，跑**一次**流，
// 再用 Régin 过滤淘汰「不可能当端点」的格子 —— 活下来的才需要进起点循环。
//
// 建模（自由格两色计数 c0/c1，|c0−c1| > 1 直接无解）：
//   c0 == c1（偶数格）：两端点异色，左右各挂一个哑点，各带 1 个单位
//   c0 == c1+1（奇数格，颜色 0 是多数）：两端点同为颜色 0，右侧哑点带 2 个单位
//   c1 == c0+1：镜像
static unsigned char *end_ok;
static long long gfilter_kept = 0, gfix_new = 0, gfix_total = 0;
static int use_gfilter = 1;
#define FDUM2 (N + 3)

static int *blk;                  // 每格所属的块（分治用）
static int rstA = -1, rstB = -1;  // >=0 时把哑点的弧限制在该块内

static int filter_endpoints(void) {
    if (!use_gfilter) { for (int c = 0; c < N; ++c) end_ok[c] = 1; return 1; }
    int c0 = 0, c1 = 0;
    for (int c = 0; c < N; ++c) if (g0[c]) { if (col[c]) ++c1; else ++c0; }
    if (c0 - c1 > 1 || c1 - c0 > 1) return 0;             // |黑−白| > 1 => 无解

    fnodes = N + 4;
    for (int i = 0; i < fnodes; ++i) fhead[i] = -1;
    fcnt = 0;
    long long needL = 0, needR = 0;

    for (int c = 0; c < N; ++c) {
        if (!g0[c]) continue;
        if (col[c] == 0) { fadd(FSRC, FCELL(c), 2); needL += 2; }
        else             { fadd(FCELL(c), FSNK, 2); needR += 2; }
    }
    // rstA/rstB >= 0 时把哑点的弧限制在指定的块里 —— 用来做「两个端点分别落在这两块」的联合假设。
    // 单个端点的支撑性 Régin/SCC 已经做到完备了，所以在难关上一无所获；
    // 但**两个端点是联合约束的**：单独看每格都可能，组合起来未必。这就是分治的切入点。
#define OKA(c) (rstA < 0 || blk[c] == rstA)
#define OKB(c) (rstB < 0 || blk[c] == rstB)
#define OKAB(c) (rstA < 0 || blk[c] == rstA || blk[c] == rstB)
    if (c0 == c1) {                                       // 两端点异色，各挂一个
        fadd(FSRC, FDUM, 1); needL += 1;
        for (int c = 0; c < N; ++c) if (g0[c] && col[c] && OKA(c)) fadd(FDUM, FCELL(c), 1);
        fadd(FDUM2, FSNK, 1); needR += 1;
        for (int c = 0; c < N; ++c) if (g0[c] && !col[c] && OKB(c)) fadd(FCELL(c), FDUM2, 1);
    } else if (c0 > c1) {                                 // 两端点同为颜色 0
        fadd(FDUM2, FSNK, 2); needR += 2;
        for (int c = 0; c < N; ++c) if (g0[c] && !col[c] && OKAB(c)) fadd(FCELL(c), FDUM2, 1);
    } else {                                              // 两端点同为颜色 1
        fadd(FSRC, FDUM, 2); needL += 2;
        for (int c = 0; c < N; ++c) if (g0[c] && col[c] && OKAB(c)) fadd(FDUM, FCELL(c), 1);
    }
    for (int c = 0; c < N; ++c) {                         // 网格边：颜色 0 -> 颜色 1
        if (!g0[c] || col[c]) continue;
        for (int d = 0; d < 4; ++d) {
            int n = c + delta[d];
            if (!g0[n]) { fmap[c * 4 + d] = -1; continue; }
            fmap[c * 4 + d] = fcnt;
            fadd(FCELL(c), FCELL(n), 1);
        }
    }
    if (needL != needR) return 0;

    long long flow = 0;
    while (fbfs()) {
        for (int i = 0; i < fnodes; ++i) fiter[i] = fhead[i];
        int f;
        while ((f = fdfs(FSRC, 1 << 30)) > 0) flow += f;
    }
    if (flow != needL) return 0;                          // 度约束松弛都无解 => 整关无解

    for (int i = 0; i < fnodes; ++i) { fdfn[i] = -1; fscc[i] = -1; fon[i] = 0; }
    int timer = 0, sp = 0, nscc = 0, csp;
    for (int root = 0; root < fnodes; ++root) {
        if (fdfn[root] >= 0) continue;
        fcv[0] = root; fce[0] = fhead[root]; csp = 1;
        fdfn[root] = flow2[root] = timer++; fstk[sp++] = root; fon[root] = 1;
        while (csp) {
            int u = fcv[csp - 1];
            if (fce[csp - 1] != -1) {
                int e = fce[csp - 1]; fce[csp - 1] = fnext[e];
                if (fcap[e] <= 0) continue;
                int v = fto[e];
                if (fdfn[v] < 0) {
                    fdfn[v] = flow2[v] = timer++; fstk[sp++] = v; fon[v] = 1;
                    fcv[csp] = v; fce[csp] = fhead[v]; ++csp;
                } else if (fon[v] && fdfn[v] < flow2[u]) flow2[u] = fdfn[v];
            } else {
                --csp;
                if (csp) { int p = fcv[csp - 1]; if (flow2[u] < flow2[p]) flow2[p] = flow2[u]; }
                if (flow2[u] == fdfn[u]) {
                    for (;;) { int w = fstk[--sp]; fon[w] = 0; fscc[w] = nscc; if (w == u) break; }
                    ++nscc;
                }
            }
        }
    }

    for (int c = 0; c < N; ++c) end_ok[c] = 0;
    // 哑点的弧：当前有流量 => 它就是这个解里的端点；无流量但同 SCC => 换得过来，也可能
    for (int dum = 0; dum < 2; ++dum) {
        int dn = dum ? FDUM2 : FDUM;
        for (int e = fhead[dn]; e != -1; e = fnext[e]) {
            int v = fto[e];
            if (v == FSRC || v == FSNK) continue;
            int c2 = v - 3;
            if (c2 < 0 || c2 >= N || !g0[c2]) continue;
            // FDUM 侧遍历的是**正向**弧（容量 1，用掉后 fcap 变 0）；
            // FDUM2 侧遍历的是**反向**弧，它的 fcap 记的是**已用流量**（用掉后变 1）——两边判据相反。
            int used = dum ? (fcap[e] > 0) : (fcap[e] <= 0);
            if (used || fscc[dn] == fscc[v]) end_ok[c2] = 1;
        }
    }
    // 网格边的 Régin 过滤 —— 这些推论**不依赖起点假设**（只假设「存在一条哈密顿路径」），
    // 所以全局有效。固定下来后图更紧，下一轮流可能就能把哑弧分开了。
    gfix_new = 0;
    for (int c = 0; c < N; ++c) {
        if (!g0[c] || col[c]) continue;
        for (int d = 0; d < 4; ++d) {
            int e = fmap[c * 4 + d];
            if (e < 0 || estate[c * 4 + d] != 0) continue;
            int n = c + delta[d];
            if (fscc[FCELL(c)] == fscc[FCELL(n)]) continue;
            int a = c * 4 + d, b2 = n * 4 + (d ^ 2);
            estate[a] = estate[b2] = (unsigned char)(fcap[e] > 0 ? 2 : 1);
            ++gfix_new;
        }
    }
    gfilter_kept = 0;
    for (int c = 0; c < N; ++c) if (g0[c] && end_ok[c]) ++gfilter_kept;
    return 1;
}

// 全局阶段滚到不动点：流 -> 固定网格边 -> 再流 …… 这些推论对所有起点都成立，算一次全局复用
static int global_fixpoint(void) {
    memset(estate, 0, (size_t)N * 4);
    for (int it = 0; it < 8; ++it) {
        if (!filter_endpoints()) return 0;
        if (gfix_new == 0) break;
    }

    // ---- 分治：块对联合假设 ----
    // 单端点的支撑性已经完备了，所以在难关上过滤为 0。但两个端点是**联合**约束的：
    // 把格子按行带分成 K 块，对每一对块 (i,j) 假设「两端点分别落在这两块」跑一次流；
    // 不可行就杀掉这一对。某块与所有块的配对都不可行 => **整块淘汰**（可再对存活块递归细分）。
    int K = getenv("BLOCKS") ? atoi(getenv("BLOCKS")) : 12;
    if (K > 1 && gfilter_kept > total_free / 2) {          // 只在单端点过滤失效时才值得花这个钱
        int h2 = N / W;
        for (int c = 0; c < N; ++c) blk[c] = g0[c] ? ((c / W) * K / (h2 ? h2 : 1)) : -1;
        static unsigned char alive[128][128];
        memset(alive, 0, sizeof alive);
        int npair = 0, nok = 0;
        // ⚠ filter_endpoints 会覆写 estate 和 end_ok，受限运行的结果**不是全局有效的**，
        //    必须存下来再还原，否则最后留下的是「两端点都在最后一块」那次的结果（不 sound）。
        unsigned char *sv_e = malloc((size_t)N * 4), *sv_k = malloc((size_t)N);
        memcpy(sv_e, estate, (size_t)N * 4); memcpy(sv_k, end_ok, (size_t)N);
        for (int i = 0; i < K; ++i)
            for (int j = 0; j < K; ++j) {
                rstA = i; rstB = j;
                memcpy(estate, sv_e, (size_t)N * 4);
                int ok = filter_endpoints();
                ++npair;
                if (ok) { alive[i][j] = 1; ++nok; }
            }
        rstA = rstB = -1;
        memcpy(estate, sv_e, (size_t)N * 4); memcpy(end_ok, sv_k, (size_t)N);
        free(sv_e); free(sv_k);
        int killed = 0;
        for (int i = 0; i < K; ++i) {
            int any = 0;
            for (int j = 0; j < K; ++j) if (alive[i][j] || alive[j][i]) { any = 1; break; }
            if (!any) {                                    // 这一块不可能含任何端点
                for (int c = 0; c < N; ++c) if (g0[c] && blk[c] == i && end_ok[c]) { end_ok[c] = 0; ++killed; }
            }
        }
        gfilter_kept = 0;
        for (int c = 0; c < N; ++c) if (g0[c] && end_ok[c]) ++gfilter_kept;
        fprintf(stderr, "  分治：%d 块 %d 对，可行 %d 对，整块淘汰掉 %d 格\n", K, npair, nok, killed);
    }
    memcpy(g_estate, estate, (size_t)N * 4);

    // ---- SAC 测量（SACPROBE=1）：逐条未定边假设"必走"，传播看矛盾 => 全局禁边 ----
    // 目的：量 SAC 能否找出基础全局传播漏掉的新禁边（硬关全局仅 ~7 条）。纯测量，不改主流程。
    if (getenv("SACPROBE")) {
        unsigned char *snap = malloc((size_t)N * 4);
        int *dsnap = malloc(sizeof(int) * N);
        // 用当前必用边重建 dsu（全局阶段后 dsu 状态不可信）
        for (int c = 0; c < N; ++c) dsu[c] = c;
        for (int c = 0; c < N; ++c) if (g0[c]) for (int d = 0; d < 2; ++d)
            if (g0[c + delta[d]] && g_estate[c * 4 + d] == 1) {
                int ra = dfind(c), rb = dfind(c + delta[d]);
                if (ra != rb) dsu[ra] = rb;
            }
        long long tested = 0, newban = 0, base = 0;
        for (int c = 0; c < N; ++c) if (g0[c]) for (int d = 0; d < 4; ++d) {
            if (!g0[c + delta[d]]) continue;
            if (g_estate[c * 4 + d]) { if (d < 2) ++base; continue; }   // 已定，跳过
            if (d >= 2) continue;                                        // 每条无向边只测一次（右/下）
            ++tested;
            memcpy(snap, estate, (size_t)N * 4);
            memcpy(dsnap, dsu, sizeof(int) * N);
            memset(inq, 0, (size_t)N); qhead = qtail = 0; prop_bad = 0;
            set_edge(c, d, 1);
            if (!prop_bad) prun_queue();
            int bad = prop_bad;
            if (getenv("SACDIAG") && tested < 5) {
                long long chg = 0;
                for (int cc = 0; cc < N * 4; ++cc) if (estate[cc] != snap[cc]) ++chg;
                fprintf(stderr, "  SACDIAG edge(%d,%d)=1 => cascade %lld bad=%d\n", c, d, chg, bad);
            }
            memcpy(estate, snap, (size_t)N * 4);
            memcpy(dsu, dsnap, sizeof(int) * N);
            prop_bad = 0;
            if (bad) ++newban;
            if ((tested & 8191) == 0) fprintf(stderr, "  SAC progress %lld tested, %lld newban\n", tested, newban);
        }
        fprintf(stderr, "SACPROBE: base_used %lld, tested %lld undetermined, SAC_new_forbidden %lld\n", base, tested, newban);
        free(snap); free(dsnap);
    }
    return 1;
}

static int do_flow(int s) {
    if (!use_flow || prop_bad) return !prop_bad;
    int X = col[s];
    fnodes = N + 3;
    for (int i = 0; i < fnodes; ++i) fhead[i] = -1;
    fcnt = 0;
    long long needL = 0, needR = 0;

    for (int c = 0; c < N; ++c) {
        if (!g0[c]) continue;
        int b = (c == s) ? 1 : 2, nfix = 0;
        for (int d = 0; d < 4; ++d)
            if (g0[c + delta[d]] && estate[c * 4 + d] == 1) ++nfix;
        int rb = b - nfix;
        if (rb < 0) { prop_bad = 1; return 0; }
        if (col[c] == X) { if (rb) fadd(FSRC, FCELL(c), rb); needL += rb; }
        else             { if (rb) fadd(FCELL(c), FSNK, rb); needR += rb; }
    }
    // 哑点挂哪一侧取决于**终点的颜色**：自由格数为偶 => 终点异色于起点；为奇 => 同色。
    // （写死成「终点必是 ~X」会在奇数格的关卡上把模型建错 —— L235 就是这么 FAIL 的。）
    if (prop_endcol != X) {                              // 终点在 ~X 侧
        fadd(FSRC, FDUM, 1); needL += 1;
        for (int c = 0; c < N; ++c) if (g0[c] && col[c] != X) fadd(FDUM, FCELL(c), 1);
    } else {                                             // 终点在 X 侧（且不是起点自己）
        fadd(FDUM, FSNK, 1); needR += 1;
        for (int c = 0; c < N; ++c) if (g0[c] && col[c] == X && c != s) fadd(FCELL(c), FDUM, 1);
    }

    for (int c = 0; c < N; ++c) {
        if (!g0[c] || col[c] != X) continue;
        for (int d = 0; d < 4; ++d) {
            int n = c + delta[d];
            if (!g0[n] || estate[c * 4 + d] != 0) { fmap[c * 4 + d] = -1; continue; }
            fmap[c * 4 + d] = fcnt;
            fadd(FCELL(c), FCELL(n), 1);
        }
    }
    if (needL != needR) { prop_bad = 1; ++flow_refute; return 0; }

    long long flow = 0;
    while (fbfs()) {
        for (int i = 0; i < fnodes; ++i) fiter[i] = fhead[i];
        int f;
        while ((f = fdfs(FSRC, 1 << 30)) > 0) flow += f;
    }
    if (flow != needL) { prop_bad = 1; ++flow_refute; return 0; }

    for (int i = 0; i < fnodes; ++i) { fdfn[i] = -1; fscc[i] = -1; fon[i] = 0; }
    int timer = 0, sp = 0, nscc = 0, csp;
    for (int root = 0; root < fnodes; ++root) {
        if (fdfn[root] >= 0) continue;
        fcv[0] = root; fce[0] = fhead[root]; csp = 1;
        fdfn[root] = flow2[root] = timer++; fstk[sp++] = root; fon[root] = 1;
        while (csp) {
            int u = fcv[csp - 1];
            if (fce[csp - 1] != -1) {
                int e = fce[csp - 1]; fce[csp - 1] = fnext[e];
                if (fcap[e] <= 0) continue;                    // 残量图只走还有余量的弧
                int v = fto[e];
                if (fdfn[v] < 0) {
                    fdfn[v] = flow2[v] = timer++; fstk[sp++] = v; fon[v] = 1;
                    fcv[csp] = v; fce[csp] = fhead[v]; ++csp;
                } else if (fon[v] && fdfn[v] < flow2[u]) flow2[u] = fdfn[v];
            } else {
                --csp;
                if (csp) { int p = fcv[csp - 1]; if (flow2[u] < flow2[p]) flow2[p] = flow2[u]; }
                if (flow2[u] == fdfn[u]) {
                    for (;;) { int w = fstk[--sp]; fon[w] = 0; fscc[w] = nscc; if (w == u) break; }
                    ++nscc;
                }
            }
        }
    }

    for (int c = 0; c < N && !prop_bad; ++c) {
        if (!g0[c] || col[c] != X) continue;
        for (int d = 0; d < 4; ++d) {
            int e = fmap[c * 4 + d];
            if (e < 0 || estate[c * 4 + d] != 0) continue;
            int n = c + delta[d];
            if (fscc[FCELL(c)] == fscc[FCELL(n)]) continue;    // 同一 SCC => 两种取值都可行
            if (fcap[e] > 0) { set_edge(c, d, 2); ++flow_bans; }
            else             { set_edge(c, d, 1); ++flow_uses; }
            if (prop_bad) return 0;
        }
    }
    // ===== 白捡的一条：哑点 z 的弧同样受 Régin 过滤约束 =====
    // z 那一个单位挑中谁，谁就是终点。所以 z→c 这条弧若两端不同 SCC 且当前无流量，
    // 就说明**不存在任何度可行解让 c 当终点** => c 被排除出终点候选。
    // 我们本来就在算 SCC，之前只看了网格边，这些哑弧一直没查。
    end_cand = 0;
    memset(tcand, 0, (size_t)N);                 // 每次 do_flow 重算：后面的调用约束更紧，候选只会更小
    tcand_for = prop_start;
    for (int e = fhead[FDUM]; e != -1; e = fnext[e]) {
        if (fcap[e] == 0 && fcap[e ^ 1] == 0) continue;          // 不是 z 的正向弧
        int v = fto[e];
        if (v == FSRC || v == FSNK) continue;
        int c2 = v - 3;
        if (c2 < 0 || c2 >= N || !g0[c2]) continue;
        // v32 的老坑第三次出现：奇数关（终点色 == 起点色）时哑点挂在**汇侧**，
        // 弧是 cell→FDUM，从 fhead[FDUM] 遍历到的是**反向弧**，fcap 记的是已用流量 ——
        // 「这条弧用没用」的判据和偶数关（源侧，正向弧）正好相反。
        // 这段之前只算 end_cand 当展示，错了没人看出来；v44 拿它当剪枝，L39（奇）当场 FAIL。
        int used_arc = (prop_endcol != col[prop_start]) ? (fcap[e] <= 0) : (fcap[e] > 0);
        int possible = (fscc[FDUM] == fscc[v]) || used_arc;   // 同 SCC 可换，或当前就是它
        if (possible) {
            // 缝定律注入（SEAMR>=0 启用）：真解是近环（82/82 实证，L1 中位 7 最大 41），
            // 终点候选只留起点 L1<=SEAMR 球内的。远处 deg<=1 格随之变 ntc_low，牙口更硬。
            static int seamr = -2;
            if (seamr == -2) seamr = getenv("SEAMR") ? atoi(getenv("SEAMR")) : -1;
            if (seamr >= 0) {
                int ddx = c2 % W - prop_start % W, ddy = c2 / W - prop_start / W;
                if (ddx < 0) ddx = -ddx;
                if (ddy < 0) ddy = -ddy;
                if (ddx + ddy > seamr) possible = 0;
            }
        }
        if (possible) { ++end_cand; tcand[c2] = 1; }
    }

    prun_queue();
    if (prop_bad) return 0;

    // ===== 连通性加强（subtour elimination）=====
    // 度约束松弛上的 GAC 已经做到头了（迭代 3 轮就收敛，50~55%）。要再往上必须加强松弛本身，
    // 最自然的加强是**连通性**：流解出来的子图 H（必用边 ∪ 流量为 1 的边）是若干路径和圈的并。
    // 真解是一条覆盖全盘的路径，所以对 H 的每个连通块 C：
    //     跨出 C 的边必须 >= 2 条（C 里含一个端点则 >= 1 条，含两个则可以 0 条）
    // 于是（记 a = 跨出 C 的**待定**边数；必用边不会跨块，否则两块早连上了）：
    //     a == 0            => 矛盾（C 永远连不出去）
    //     a == 1            => 那条边**必用**；且 C 必然含端点（否则要 2 条）
    //     两个这样的块      => 矛盾（终点只有一个；起点那块另算）
    // 「C 必含端点」这一条顺带**定位终点** —— 正是我们最大的瓶颈。
    if (use_subtour) {
        for (int c = 0; c < N; ++c) hdsu[c] = c;
        for (int c = 0; c < N; ++c) {                    // 按 H 的边合并
            if (!g0[c]) continue;
            for (int d = 2; d < 4; ++d) {
                int n = c + delta[d];
                if (!g0[n]) continue;
                int inH = (estate[c * 4 + d] == 1);
                if (!inH && col[c] == X) { int e = fmap[c * 4 + d]; inH = (e >= 0 && fcap[e] <= 0); }
                if (!inH && col[c] != X) { int e = fmap[n * 4 + (d ^ 2)]; inH = (e >= 0 && fcap[e] <= 0); }
                if (!inH) continue;
                int ra = hfind(c), rb2 = hfind(n);
                if (ra != rb2) hdsu[ra] = rb2;
            }
        }
        // 统计每个块跨出去的待定边；顺手记下唯一那条，便于强制
        for (int c = 0; c < N; ++c) { hcross[c] = 0; hedge[c] = -1; }
        int nfreeroot = -1, nroots = 0;
        for (int c = 0; c < N; ++c) if (g0[c] && hfind(c) == c) { ++nroots; nfreeroot = c; }
        if (nroots > 1) {                                 // 只有一个块就没什么可推的
            for (int c = 0; c < N; ++c) {
                if (!g0[c]) continue;
                for (int d = 0; d < 4; ++d) {
                    int n = c + delta[d];
                    if (!g0[n] || estate[c * 4 + d] != 0) continue;
                    if (hfind(c) == hfind(n)) continue;   // 不跨块
                    int r = hfind(c);
                    ++hcross[r];
                    if (hcross[r] == 1) hedge[r] = c * 4 + d;
                }
            }
            int need_end = 0;
            for (int c = 0; c < N && !prop_bad; ++c) {
                if (!g0[c] || hfind(c) != c) continue;
                int sInC = (hfind(prop_start) == c);
                if (hcross[c] == 0) { prop_bad = 1; ++sub_refute; break; }   // 连不出去
                if (hcross[c] == 1) {
                    int cc = hedge[c] >> 2, dd2 = hedge[c] & 3;
                    if (estate[cc * 4 + dd2] == 0) { set_edge(cc, dd2, 1); ++sub_uses; }
                    if (!sInC && ++need_end > 1) { prop_bad = 1; ++sub_refute; break; } // 终点只有一个
                }
            }
            prun_queue();
        }
        (void)nfreeroot;
    }
    return !prop_bad;
}

static int do_probing(int rounds) {
    for (int r = 0; r < rounds && !prop_bad; ++r) {
        int changed = 0;
        for (int c = 0; c < N && !prop_bad; ++c) {
            if (!g0[c]) continue;
            for (int d = 2; d < 4; ++d) {                  // 只走 R/D，每条边只试一次
                int n = c + delta[d];
                if (!g0[n] || estate[c * 4 + d] != 0) continue;
                if (try_edge(c, d, 1)) {                   // 假设「必用」就矛盾 => 只能禁用
                    set_edge(c, d, 2); prun_queue(); changed = 1;
                    if (prop_bad) return 0;
                    continue;
                }
                if (try_edge(c, d, 2)) {                   // 假设「禁用」就矛盾 => 只能必用
                    set_edge(c, d, 1); prun_queue(); changed = 1;
                    if (prop_bad) return 0;
                }
            }
        }
        if (!changed) break;
    }
    return !prop_bad;
}

// 正式搜索阶段用的加强版：传播 + probing + 再过一遍链定向
// 按**规则代价**分层（不是按预算）：诊断显示慢关的瓶颈不在起点数量
// （L223 探针证伪掉 3892/3992 = 97.5%，只剩 100 个），而在为了筛掉那 3892 个所付的代价。
// 而 do_probing 是最贵的一环（几千次 try_edge，每次 O(N) 快照/恢复），do_flow 只是一次 max-flow
// 却同样能证伪。所以把流提到 probing 前面：绝大多数起点在便宜的两层就死了，
// 根本不该付 probing 的钱。
// v38 的分层计时把 v35 的排序打脸了。L223 一个 shard 的实测：
//
//     传播    0.7%  证伪 0        流1   6.5%  证伪 0
//     probing 62.1% 证伪 0        流2   3.8%  证伪 0
//     probe2  26.6% 证伪 0        链定向 0.3% 证伪 **976（98%）**
//
// **probing 吃掉 89% 的时间、一个起点都没证伪；链定向只花 0.3% 就证伪了 98%。**
// v35 按「规则代价」排序时，把链定向放在最后 —— 因为它原本是「产出定向信息」的收尾步骤，
// 没人拿它当证伪器看。但它其实是最强的那个：它带着**时序**（链是路径的连续段，
// 链上拐弯点的前方格若在本链更晚就矛盾），而 probing/流都只在结构层面打转。
//
// 排序原则要改一改：不是「按代价从低到高」，而是「按**单位代价的证伪量**从高到低」。
// 一条又便宜又强的规则被排在最后，等于让 98% 的起点先付了最贵的那笔钱。
// probing 在这一版里已经是**纯浪费**了 —— 它是 v20 时代加的，那会儿还没有流（v30）。
// v38 的分层计时暴露：probing + probe2 占 **89%** 的时间，**证伪 0 个起点**；
// 它唯一的产出（更多必用边）现在被 do_flow 的 Régin/SCC 过滤覆盖掉了。实测关掉之后：
//
//     L239 67.8s → 35.0s    L257 126.5s → 76.6s    L195 20.5s → 12.2s
//     L139 3.4s → 2.3s      其余全部略快，**16 关里一关都没回退**
//
// **教训（比这次提速更值钱）：加了新规则之后，要回头量旧规则还剩多少边际贡献。**
// 这个仓库里的规则是一层层叠上去的，从没做过反向检查 —— probing 白烧了 89% 的时间
// 烧了整整 10 个版本。PROBEROUNDS>0 可以把它开回来做对照。

// ===== 跨链时序偏序 + 环检测（v42 复活，XCHAIN=1 启用）=====
static int *xc_start, *xc_len, *xc_feas, *xc_es, *xc_ec, *xc_eall;
static int *xc_eh, *xc_en, *xc_et, xc_ecnt;
static unsigned char *xc_seen;
static int xc_nchain;

static int xc_scan(const int *seq, int k, int rev, int cid, int strict, int *bef, int *nb) {
    *nb = 0;
    for (int t = 1; t + 1 < k; ++t) {
        int prev = rev ? seq[t + 1] : seq[t - 1];
        int next = rev ? seq[t - 1] : seq[t + 1];
        if (next - seq[t] == seq[t] - prev) continue;      // 直穿，非拐点
        int f = seq[t] + (seq[t] - prev);                  // 拐点前方格
        if (!g0[f]) continue;
        int cf = chain_id[f];
        if (cf == cid) {
            int tf = rev ? (k - 1 - chain_pos[f]) : chain_pos[f];
            int tt = rev ? (k - 1 - t) : t;
            if (tf > tt) return 0;                         // 同链内前方格更晚 ⇒ 此方向不可行
        } else if (strict) {
            return 0;
        } else if (cf >= 0) {
            if (*nb < 500) bef[(*nb)++] = cf;
        }
    }
    return 1;
}

static int xc_reach(int from, int to) {
    if (from == to) return 1;
    for (int i = 0; i < xc_nchain; ++i) xc_seen[i] = 0;
    static int stk[8192];
    int top = 0;
    stk[top++] = from; xc_seen[from] = 1;
    while (top) {
        int v = stk[--top];
        for (int e = xc_eh[v]; e >= 0; e = xc_en[e]) {
            int w2 = xc_et[e];
            if (w2 == to) return 1;
            if (!xc_seen[w2] && top < 8190) { xc_seen[w2] = 1; stk[top++] = w2; }
        }
    }
    return 0;
}

static int xchain_check(int s) {
    if (!xc_start) {
        xc_start = malloc(sizeof(int) * (size_t)N); xc_len = malloc(sizeof(int) * (size_t)N);
        xc_feas = malloc(sizeof(int) * (size_t)N);
        xc_es = malloc(sizeof(int) * (size_t)N * 2); xc_ec = malloc(sizeof(int) * (size_t)N * 2);
        xc_eall = malloc(sizeof(int) * (size_t)N * 4);
        xc_eh = malloc(sizeof(int) * (size_t)N);
        xc_en = malloc(sizeof(int) * (size_t)N * 4); xc_et = malloc(sizeof(int) * (size_t)N * 4);
        xc_seen = malloc((size_t)N);
    }
    for (int c = 0; c < N; ++c) chain_id[c] = -1;
    xc_nchain = 0;
    int pos = 0;
    for (int c0 = 0; c0 < N; ++c0) {
        if (!g0[c0] || chain_id[c0] >= 0) continue;
        int fdeg = 0;
        for (int d = 0; d < 4; ++d)
            if (g0[c0 + delta[d]] && estate[c0 * 4 + d] == 1) ++fdeg;
        if (fdeg != 1) continue;                            // 链端点起走
        xc_start[xc_nchain] = pos;
        int cur = c0, from = -1;
        for (;;) {
            seqbuf[pos] = cur; chain_id[cur] = xc_nchain; chain_pos[cur] = pos - xc_start[xc_nchain]; ++pos;
            int nxt = -1;
            for (int d = 0; d < 4; ++d) {
                int n = cur + delta[d];
                if (g0[n] && estate[cur * 4 + d] == 1 && n != from) { nxt = n; break; }
            }
            if (nxt < 0) break;
            from = cur; cur = nxt;
        }
        xc_len[xc_nchain] = pos - xc_start[xc_nchain];
        ++xc_nchain;
    }
    if (xc_nchain < 2) return 1;
    int bpos = 0;
    xc_ecnt = 0;
    for (int i = 0; i < xc_nchain; ++i) { xc_eh[i] = -1; xc_feas[i] = 0; }
    for (int i = 0; i < xc_nchain; ++i) {
        const int *seq = seqbuf + xc_start[i];
        int k = xc_len[i];
        if (k < 3) { xc_feas[i] = 3; xc_ec[i * 2] = xc_ec[i * 2 + 1] = 0; continue; }
        int head_s = (seq[0] == s), tail_s = (seq[k - 1] == s);
        int feas = 0;
        for (int d = 0; d < 2; ++d) {
            if ((d == 1 && head_s) || (d == 0 && tail_s)) { xc_ec[i * 2 + d] = 0; continue; }
            int nb = 0;
            static int bef[512];
            int strict = d ? tail_s : head_s;
            if (!xc_scan(seq, k, d, i, strict, bef, &nb)) { xc_ec[i * 2 + d] = 0; continue; }
            feas |= 1 << d;
            xc_es[i * 2 + d] = bpos; xc_ec[i * 2 + d] = nb;
            for (int t = 0; t < nb && bpos < 4 * N; ++t) xc_eall[bpos++] = bef[t];
        }
        if (!feas) return 0;                                // 两方向皆不可行
        xc_feas[i] = feas;
    }
    for (int round = 0; round < 6; ++round) {
        int changed = 0;
        for (int i = 0; i < xc_nchain; ++i) {
            int f2 = xc_feas[i];
            if ((f2 & 3) != 1 && (f2 & 3) != 2) continue;
            if (f2 & 4) continue;
            int d = ((f2 & 3) == 1) ? 0 : 1;
            int st = xc_es[i * 2 + d], n = xc_ec[i * 2 + d];
            for (int t = 0; t < n; ++t) {
                int b = xc_eall[st + t];
                if (b == i) return 0;
                if (xc_reach(i, b)) return 0;               // 成环 ⇒ 矛盾
                xc_et[xc_ecnt] = i; xc_en[xc_ecnt] = xc_eh[b]; xc_eh[b] = xc_ecnt++;
            }
            xc_feas[i] |= 4; changed = 1;
        }
        if (!changed) break;
        for (int i = 0; i < xc_nchain; ++i) {
            int f2 = xc_feas[i] & 3;
            if (f2 != 3) continue;
            for (int d = 0; d < 2; ++d) {
                int st = xc_es[i * 2 + d], n = xc_ec[i * 2 + d];
                int bad2 = 0;
                for (int t = 0; t < n && !bad2; ++t)
                    if (xc_reach(i, xc_eall[st + t])) bad2 = 1;
                if (bad2) {
                    xc_feas[i] &= ~(1 << d);
                    if (!(xc_feas[i] & 3)) return 0;
                }
            }
        }
    }
    return 1;
}

static long long plainF_last;      // FRANK：纯传播层的 F（与 PROBEDUMP 普查同口径）
static int propagate_strong(int s) {
    if (!propagate(s)) return 0;                 // 第 1 层：最便宜
    {   // F 采样点：必须在 flow/probing 改写 estate 之前，保证与普查 F 逐位相等
        long long fp = 0;
        for (int c9 = 0; c9 < N; ++c9) if (g0[c9]) for (int d9 = 2; d9 < 4; ++d9) {
            if (!g0[c9 + delta[d9]]) continue;
            if (estate[c9 * 4 + d9]) ++fp;
        }
        plainF_last = fp;
    }
    if (!do_flow(s)) return 0;                   // 第 2 层：一次 max-flow
    if (use_chain && !orient_chains(s)) return 0; // 第 3 层：0.3% 的代价，98% 的证伪
    if (getenv("XCHAIN") && !xchain_check(s)) return 0;   // 第 3.5 层：跨链时序偏序（v42 复活）
    if (probe_rounds <= 0) return 1;             // 默认到此为止
    if (!do_probing(probe_rounds)) return 0;
    if (!do_flow(s)) return 0;
    if (!do_probing(1)) return 0;
    return orient_chains(s);
}


// 用传播结果卡一次滑行。滑行从 p 出发沿 dd 走 len 格到 c，它对最终解的边意味着：
//   · run 上那 len 条边**全都要用** —— 撞上任何一条禁用边就不可能；
//   · run 的中间格是直穿的，两条边都沿 dd，所以它的两条**垂直边一条都不用**
//     —— 那里若有必用边，矛盾；
//   · 落点 c 处路径要么转弯要么收尾，反正不会再往 dd 走一格，
//     所以边 (c, c+dd) 不用 —— 它若是必用边，矛盾。
//     （c+dd 此刻要么是墙，要么是更早访问过的格子；后者也不可能与 c 在路径上相邻，
//       因为 c 的前驱已经是 c-dd 了。）
static inline int estate_ok(int p, int d, int dd, int len, int c) {
    int od = d ^ 2;
    for (int k = 0, q = p; k < len; ++k, q += dd)
        if (estate[q * 4 + d] == 2) return 0;                 // 禁用边挡在 run 里
    for (int k = 1, q = p + dd; k < len; ++k, q += dd) {      // 中间格：垂直边必须不用
        int e1 = (d + 1) & 3, e2 = (d + 3) & 3;
        if (estate[q * 4 + e1] == 1 || estate[q * 4 + e2] == 1) return 0;
    }
    if (g0[c + dd] && estate[c * 4 + d] == 1) return 0;        // 落点不会再往前一格
    (void)od;
    return 1;
}


// ===== 搜索内部的传播：拿它当分支点剪枝 =====
//
// v12 试过在浅层分支点上给每个候选跑一次限预算 DFS 探针来证伪，想法是对的
// （证伪是证明不是启发式），但败在太贵：一次探针 2000 个节点。
// 传播只要 O(剩余格数)，大约相当于 40 个节点——**便宜五十倍**，同样的想法可以重做。
//
// 跟起点那次传播是同一套规则，只是作用在**剩余区域**上：
//   · 起点 = 当前位置 p（它已经走过了，但在路径上还欠一条出边，正好当起点用）；
//   · 终点色 = col(p) ^ (剩余格数 & 1) —— 从 p 再走 remaining 步，两色交替，终点色直接算出来；
//   · 其余照旧：非终点色的格子必须用满两条边，强制边不许闭成环。
// 一样是「证明无解」，矛盾就把这个候选当场砍掉。
static unsigned char *estate2;
static int *dsu2;
static int *pq2;
static unsigned char *inq2;
static int q2h, q2t;
static int p2_bad, p2_start, p2_endcol, p2_end;
static int prop_depth;

static int dfind2(int x) { while (dsu2[x] != x) { dsu2[x] = dsu2[dsu2[x]]; x = dsu2[x]; } return x; }

static int dfind0(int x) { while (dsu0[x] != x) { dsu0[x] = dsu0[dsu0[x]]; x = dsu0[x]; } return x; }
static void build_dsu0(void) {
    for (int c = 0; c < N; ++c) dsu0[c] = c;
    for (int c = 0; c < N; ++c) {
        if (!g0[c]) continue;
        for (int d = 2; d < 4; ++d) {                     // R/D 两个方向遍历每条边一次
            int n = c + delta[d];
            if (!g0[n] || estate[c * 4 + d] != 1) continue;
            int ra = dfind0(c), rb = dfind0(n);
            if (ra != rb) dsu0[ra] = rb;                  // 环冲突在起点传播时就该被证伪，这里不设防
        }
    }
    dsu0_valid = 1;
}

static void ppush2(int c) {
    if (inq2[c]) return;
    inq2[c] = 1; pq2[q2t++] = c;
    if (q2t == N + 1) q2t = 0;
}

static void set_edge2(int c, int d, int v) {
    int n = c + delta[d];
    unsigned char *a = &estate2[c * 4 + d], *b = &estate2[n * 4 + (d ^ 2)];
    if (*a == v) return;
    if (*a != 0) { p2_bad = 1; return; }
    if (v == 1) {
        int ra = dfind2(c), rb = dfind2(n);
        if (ra == rb) { p2_bad = 1; return; }
        dsu2[ra] = rb;
    }
    *a = *b = (unsigned char)v;
    ppush2(c); ppush2(n);
}

static void pprocess2(int c) {
    if (p2_bad) return;
    for (int d = 0; d < 4; ++d) {
        int n = c + delta[d];
        if (!g[n] || estate2[c * 4 + d] != 0) continue;
        if (dfind2(c) == dfind2(n)) { set_edge2(c, d, 2); if (p2_bad) return; }
    }
    int used = 0, avail = 0;
    for (int d = 0; d < 4; ++d) {
        if (!g[c + delta[d]]) continue;
        if (estate2[c * 4 + d] == 1) ++used;
        else if (estate2[c * 4 + d] == 0) ++avail;
    }
    int lo, hi;
    if (c == p2_start)          lo = hi = 1;
    else if (p2_end == c)       lo = hi = 1;
    else if (p2_end >= 0)       lo = hi = 2;
    else if (col[c] == p2_endcol) { lo = 1; hi = 2; }
    else                        lo = hi = 2;

    if (used > hi) { p2_bad = 1; return; }
    if (used + avail < lo) { p2_bad = 1; return; }
    if (used == hi && avail > 0) {
        for (int d = 0; d < 4; ++d)
            if (g[c + delta[d]] && estate2[c * 4 + d] == 0) { set_edge2(c, d, 2); if (p2_bad) return; }
    } else if (used + avail == lo && avail > 0) {
        for (int d = 0; d < 4; ++d)
            if (g[c + delta[d]] && estate2[c * 4 + d] == 0) { set_edge2(c, d, 1); if (p2_bad) return; }
    }
    if (c != p2_start && used + avail == 1 && hi == 2) {
        if (col[c] != p2_endcol) { p2_bad = 1; return; }
        if (p2_end >= 0 && p2_end != c) { p2_bad = 1; return; }
        if (p2_end < 0) {
            p2_end = c;
            for (int q = 0; q < N; ++q) if (g[q] && col[q] == p2_endcol) ppush2(q);
        }
    }
}

// 返回 0 = 传播出矛盾 => 这个候选走法在剩余区域上无解，当场砍掉
// ===== 分支点上的流可行性检查（v47）=====
//
// 消融实验里流是最强的规则（关掉慢 4~5 倍），但它只在「起点假设」时跑过一次。
// L413 的画像是「起点彩票已解决、树本身太深」—— 那就把流搬进搜索：在浅层分支点上，
// 对剩余区域建一次度约束流（当前头部当起点、哑点当终点），**不饱和 => 这个分支整棵剪掉**。
// 一次约 10~50ms，只在 depth < FLOWDEPTH 的分支点做（默认关，环境变量开）。
//
// 与 do_flow 的两个关键差别：
//   · 盘面用 g[]（未访问格）+ 当前头部，不是 g0；
//   · **只做可行性判定，不做 Régin 边过滤** —— 中途推出的边结论是「以当前前缀为条件」的，
//     写回全局 estate 会污染其它分支（v34 踩过同款坑：带假设跑的结果不是全局有效的）。
long long flowdyn_calls, flowdyn_refute;
int flow_depth = 0;

static int do_flow_dyn(int p, int remaining) {
    ++flowdyn_calls;
    int X = col[p];
    int endcol_dyn = (remaining & 1) ? (X ^ 1) : X;
    fnodes = N + 3;
    for (int i = 0; i < fnodes; ++i) fhead[i] = -1;
    fcnt = 0;
    long long needL = 0, needR = 0;

    // 头部 p：还欠 1 条出边；其余未访问格：欠 2 条减去已定死的
    g[p] = 1;                                            // 临时把头部当剩余格建模
    for (int c = 0; c < N; ++c) {
        if (!g[c]) continue;
        int b = (c == p) ? 1 : 2, nfix = 0;
        for (int d = 0; d < 4; ++d)
            if (g[c + delta[d]] && estate[c * 4 + d] == 1) ++nfix;
        int rb = b - nfix;
        if (rb < 0) { g[p] = 0; ++flowdyn_refute; return 0; }
        if (col[c] == X) { if (rb) fadd(FSRC, FCELL(c), rb); needL += rb; }
        else             { if (rb) fadd(FCELL(c), FSNK, rb); needR += rb; }
    }
    if (endcol_dyn != X) {
        fadd(FSRC, FDUM, 1); needL += 1;
        for (int c = 0; c < N; ++c) if (g[c] && col[c] != X) fadd(FDUM, FCELL(c), 1);
    } else {
        fadd(FDUM, FSNK, 1); needR += 1;
        for (int c = 0; c < N; ++c) if (g[c] && col[c] == X && c != p) fadd(FCELL(c), FDUM, 1);
    }
    for (int c = 0; c < N; ++c) {
        if (!g[c] || col[c] != X) continue;
        for (int d = 0; d < 4; ++d) {
            int n = c + delta[d];
            if (!g[n] || estate[c * 4 + d] != 0) continue;
            fadd(FCELL(c), FCELL(n), 1);
        }
    }
    g[p] = 0;
    if (needL != needR) { ++flowdyn_refute; return 0; }

    long long flow = 0;
    while (fbfs()) {
        for (int i = 0; i < fnodes; ++i) fiter[i] = fhead[i];
        int f;
        while ((f = fdfs(FSRC, 1 << 30)) > 0) flow += f;
    }
    if (flow != needL) { ++flowdyn_refute; return 0; }
    return 1;
}

static int propagate_dyn(int p, int remaining, int sfirst, int sdd, int slen) {
    if (remaining <= 0) return 1;
    q2h = q2t = 0;
    p2_bad = 0; p2_end = -1; p2_start = p;
    p2_endcol = col[p] ^ (remaining & 1);

    if (seed_mode == 0) {                      // v52 语义：空底座全播（不依赖 estate/dsu0）
        memset(estate2, 0, (size_t)N * 4);
        g[p] = 1;
        for (int c = 0; c < N; ++c) if (g[c]) { dsu2[c] = c; ppush2(c); }
    } else {
        if (!dsu0_valid) build_dsu0();
        memcpy(estate2, estate, (size_t)N * 4);    // 底座：起点缓存的 estate（终点色沿路径不变，永远有效且更强）
        memcpy(dsu2, dsu0, sizeof(int) * (size_t)N);
        g[p] = 1;                                  // p 已经走过了，但在路径上还欠一条出边
        ppush2(p);
        if (seed_mode == 2 || slen < 0) {                // 底座全播：恢复跨滑行组合结论
            for (int c = 0; c < N; ++c) if (g[c]) ppush2(c);
        } else
        for (int k = 0, q = sfirst; k < slen; ++k, q += sdd)   // 底座滑播：只播刚滑过的线的自由邻居
            for (int d2 = 0; d2 < 4; ++d2) {
                int n2 = q + delta[d2];
                if (g[n2]) ppush2(n2);
            }
    }
    while (q2h != q2t && !p2_bad) {
        int c = pq2[q2h++];
        if (q2h == N + 1) q2h = 0;
        inq2[c] = 0;
        pprocess2(c);
    }
    while (q2h != q2t) {                       // 提前矛盾退出时队列没干净，inq2 必须清
        inq2[pq2[q2h++]] = 0;
        if (q2h == N + 1) q2h = 0;
    }
    g[p] = 0;
    return !p2_bad;
}


// 局部连通性：跟 reach_ok 严格等价，但通常只探一小片就能收工。
//
// 归纳前提：进入本节点时剩余区域 R 是连通的（每个节点都验过，起点处另做一次完整验证）。
// 本次滑行吃掉的是一条直线 S，新剩余 R' = R \ S。R' 若被切开，切口必然紧贴 S；
// 而 R 连通意味着 R' 里任一点 x 都能在 R' 内走到某个「S 的自由邻居」
// （x 到 S-邻居的 R-路径若碰到 S，进入 S 之前的最后一点就是一个 S-邻居）。
// 所以 **R' 连通 ⟺ S 的自由邻居两两连通**——它们一连上就能立刻返回，
// 不必像 reach_ok 那样把整个剩余区域数一遍。滑行终点 c 属于 S，它的自由邻居也在这个集合里，
// 所以「从 c 出发全部可达」同时被保证。
//
// 代价：成功时 = 把这些邻居连起来所需探索的范围（通常就在 S 附近）；
// reach_ok 则不管怎样都是 O(剩余格数)，开局就是三千格。
// CHAMBERSTAT 诊断：连通死亡现场做全量 BFS，量死区与隔断（学习可行性测量，只在抽样时调用）
static void chamber_diag(int head) {
    static long long cs_n, cs_sep[10], cs_usz;
    if ((++cs_n & 3) != 0) return;                 // 抽样 1/16
    ++seen_id;
    int top = 0, cnt = 0;
    seen[head] = seen_id;
    for (int d = 0; d < 4; ++d) { int c = head + delta[d]; if (g[c] && seen[c] != seen_id) { seen[c] = seen_id; fstack[top++] = c; } }
    while (top) { int c = fstack[--top]; ++cnt; for (int d = 0; d < 4; ++d) { int n = c + delta[d]; if (g[n] && seen[n] != seen_id) { seen[n] = seen_id; fstack[top++] = n; } } }
    long long un = 0, sep = 0;
    for (int c = 0; c < N; ++c) if (g[c] && seen[c] != seen_id) {
        ++un;
        for (int d = 0; d < 4; ++d) { int n = c + delta[d]; if (!g[n] && g0[n]) ++sep; }
    }
    static long long cs_un0;
    if (un == 0) {
        if ((++cs_un0 & 1023) == 1) fprintf(stderr, "CHAMBER-UN0 %lld (局部口袋死,全局连通)\n", cs_un0);
        return;
    }
    cs_usz += un;
    int k = sep > 9 ? 9 : (int)sep;
    ++cs_sep[k];
    if ((cs_n >> 4) % 64 == 1)
        fprintf(stderr, "CHAMBER n=%lld sep(0..9+):%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld 平均死区 %lld\n",
            cs_n, cs_sep[0], cs_sep[1], cs_sep[2], cs_sep[3], cs_sep[4], cs_sep[5], cs_sep[6], cs_sep[7], cs_sep[8], cs_sep[9],
            cs_usz / (cs_n >> 4));
}

static int reach_local(int first, int dd, int len, int c, int rem2) {
    if (rem2 == 0) return 1;
    if (freedeg(c) == 0) return 0;                 // 滑到死胡同，后面没得走

    ++src_id;
    int nsrc = 0;
    for (int k = 0, q = first; k < len; ++k, q += dd)
        for (int d = 0; d < 4; ++d) {
            int n = q + delta[d];
            if (g[n] && srcmark[n] != src_id) { srcmark[n] = src_id; srcbuf[nsrc++] = n; }
        }
    if (nsrc == 0) return 0;                       // 还有格子没走，却没有一个挨着 S => 到不了
    if (nsrc == 1) return 1;                       // 只有一个邻居，两两连通平凡成立

    // BFS 而不是 DFS（v45.1）：gprof 实测 reach_local 占全部时间的 95.7%（1.2 亿次调用）。
    // 栈序洪水找「几个源是否连通」会乱走远路 —— 源们通常就在滑行线两侧、绕线头几步就接上了，
    // DFS 却可能先冲到盘面另一头。换队列序后，成功情形只探「最远源的距离」那个半径的区域，
    // 失败情形（真断开）和原来同价（都要灌满整块）。L319 实测 41s -> 10s（4 倍）。
    ++seen_id;
    int head = 0, tail = 0, found = 1;
    fstack[tail++] = srcbuf[0]; seen[srcbuf[0]] = seen_id;
    while (head < tail) {
        int q = fstack[head++];
        for (int d = 0; d < 4; ++d) {
            int n = q + delta[d];
            if (g[n] && seen[n] != seen_id) {
                seen[n] = seen_id;
                if (srcmark[n] == src_id && ++found == nsrc) return 1;
                fstack[tail++] = n;
            }
        }
    }
    return 0;
}

static int dfs(int p, int remaining, int depth) {
    if (live_end == 0 && remaining > 0) { ++dth_liveend; return 0; }    // 终点候选全被走掉了，后面必然收不了尾
    if (remaining < best_rem) best_rem = remaining;
    if (remaining == 0) return 1;
    if (nodes++ >= node_limit) return -1;
    if ((nodes & 0x7FFFFF) == 0 && getenv("DEATHSTAT"))
        fprintf(stderr, "DEATH sh%d liveend=%lld estate=%lld reach=%lld dyn=%lld flow=%lld geom=%lld\n",
                shard, dth_liveend, dth_estate, dth_reach, dth_dyn, dth_flow, dth_geom);


    struct move { int dir, endp, len, score; } cand[4];
    int nc = 0;

    for (int d = 0; d < 4; ++d) {
        int dd = delta[d];
        if (!g[p + dd]) continue;
        int c = p, len = 0;
        while (g[c + dd]) { c += dd; ++len; }              // 先只量出滑到哪，不动盘面
        if (!estate_ok(p, d, dd, len, c)) { ++dth_estate; continue; }        // 传播定死的边直接否掉这个方向
        for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
        int rem2 = remaining - len;
        int rok = (rem2 == 0 || (cheap_ok(c, rem2) && reach_local(p + dd, dd, len, c, rem2)));
        if (!rok) { ++dth_reach; static int cs_on = -1; if (cs_on < 0) cs_on = getenv("CHAMBERSTAT") != 0; if (cs_on) chamber_diag(c); }
        if (rok) {
            cand[nc].dir = d; cand[nc].endp = c; cand[nc].len = len;
            {
                // swarm 的策略梯队：温和抖动只把 271s 压到 84s 就见顶了 —— 真起点的树
                // 在「Warnsdorff 系」的所有顺序下都差不多深。按 shard%4 分四档拉开策略：
                //   0 档: 纯 Warnsdorff（保底，shard 0 落在这档）
                //   1 档: Warnsdorff + 同分抖动（原 v45 行为）
                //   2 档: 大幅抖动（允许翻转相邻名次）
                //   3 档: 纯哈希序（完全抛开 Warnsdorff，赌一个形状完全不同的树）
                int tier = swarm ? (sident() == 0 ? 0 : (sident() <= 7 ? 1 : (sident() <= 21 ? 2 : 3))) : 0;
                unsigned int h = shard_seed ^ (unsigned int)c * 2654435761u ^ (unsigned int)depth * 40503u;
                h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
                int sc2;
                if (!swarm || tier == 0)      sc2 = freedeg(c) * 32;
                else if (tier == 1)           sc2 = freedeg(c) * 32 + (int)(h & 31);
                else if (tier == 2)           sc2 = freedeg(c) * 32 + (int)(h & 95);
                else                          sc2 = (int)(h & 1023);
                cand[nc].score = sc2;
            }
            ++nc;
        }
        for (int i = 0, back = c; i < len; ++i, back -= dd) unmark(back);
    }

    // ===== 分支点过滤（v47.1）=====
    // 教训：把 dyn/flow 挂在候选生成里、用「决策深度」当窗口，会让窗口内**每个强制步**都
    // 付一次 O(N) 传播（强制链几百格长），实测 3~4 倍变慢。正确形态是：候选生成完、
    // 确认这**真是个分支点**（nc>1）之后，才对每个候选补跑传播/流过滤 ——
    // 强制步一分钱不付，真决策点上每个选项都被重锤。
    // FULLREACH：传播窗口外的分支点也做全局连通检查（怪物树深层裸奔的补刀）
    static int fullreach = -2;
    if (fullreach == -2) fullreach = getenv("FULLREACH") ? atoi(getenv("FULLREACH")) : 0;
    if (fullreach && nc > 1 && depth >= prop_depth) {
        int w = 0;
        for (int i = 0; i < nc; ++i) {
            int dd = delta[cand[i].dir], len = cand[i].len, c = cand[i].endp;
            for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
            int rem2 = remaining - len;
            int ok = (rem2 == 0) || reach_ok(c, rem2);
            for (int k = 0, back = c; k < len; ++k, back -= dd) unmark(back);
            if (!ok) ++dth_reach;
            if (ok) cand[w++] = cand[i];
        }
        nc = w;
        if (nc == 0) return 0;
    }
    if (nc > 1 && (depth < prop_depth || depth < flow_depth)) {
        int w = 0;
        for (int i = 0; i < nc; ++i) {
            int dd = delta[cand[i].dir], len = cand[i].len, c = cand[i].endp;
            for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
            int rem2 = remaining - len;
            int ok = 1;
            if (rem2 > 0) {
                if (depth < prop_depth) {
                    ++dyn_calls;
                    int fe = getenv("FULLEVERY") ? atoi(getenv("FULLEVERY")) : 0;
                    int fullseed = seed_full || (fe > 0 && depth % fe == 0);
                    if (!propagate_dyn(c, rem2, p + dd, dd, fullseed ? -1 : len)) { ok = 0; ++dyn_refutes; ++dth_dyn; }
                }
                if (ok && depth < flow_depth && !do_flow_dyn(c, rem2)) { ok = 0; ++dth_flow; }
            }
            for (int k = 0, back = c; k < len; ++k, back -= dd) unmark(back);
            if (ok) cand[w++] = cand[i];
        }
        nc = w;
        if (nc == 0) return 0;
    }

    if (nc == 0) ++dth_geom;
    for (int i = 1; i < nc; ++i)
        for (int j = i; j > 0 && cand[j].score < cand[j - 1].score; --j) {
            struct move t = cand[j]; cand[j] = cand[j - 1]; cand[j - 1] = t;
        }

    // swarm：浅层轮转 —— 抖动只能洗同分的牌，轮转是真的把树切开。
    // 前 3 层每层按 shard 的不同位选不同的首选分支，28 个 shard 覆盖 4^3 种前缀组合。
    int rot = 0;
    if (swarm && shard_seed && depth < swarm_depth && nc > 1)
        rot = (int)((shard_seed >> (5 * depth)) % (unsigned int)nc);
    for (int i0 = 0; i0 < nc; ++i0) {
        int i = (i0 + rot) % nc;
        int dd = delta[cand[i].dir], len = cand[i].len, c = cand[i].endp;
        for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
        path[path_len++] = DCH[cand[i].dir];
        int r = dfs(c, remaining - len, depth + (nc > 1 ? 1 : 0));   // depth = 分支决策数，强制步不计
        if (r) return r;
        --path_len;
        for (int k = 0, back = c; k < len; ++k, back -= dd) unmark(back);
    }
    return 0;
}

int main(int argc, char **argv) {
    FILE *in = stdin;
    if (argc > 1 && !(in = fopen(argv[1], "r"))) { fprintf(stderr, "open %s failed\n", argv[1]); return 1; }
    size_t cap = 1 << 20, n = 0;
    char *buf = malloc(cap);
    for (;;) {
        if (n + 65536 > cap) buf = realloc(buf, cap *= 2);
        size_t r = fread(buf + n, 1, 65536, in);
        if (!r) break;
        n += r;
    }
    buf[n] = 0;
    if (in != stdin) fclose(in);

    char *px = strstr(buf, "x="), *py = strstr(buf, "y="), *pb = strstr(buf, "board=");
    if (!px || !py || !pb) { fprintf(stderr, "bad input\n"); return 1; }
    int w = atoi(px + 2), h = atoi(py + 2);
    pb += 6;
    if (w <= 0 || h <= 0) { fprintf(stderr, "bad dims\n"); return 1; }

    W = w + 2; H = h + 2; N = W * H;
    g = calloc(N, 1); g0 = calloc(N, 1); col = calloc(N, 1);
    deg = malloc(sizeof(int) * (size_t)N);
    delta[0] = -1; delta[1] = -W; delta[2] = 1; delta[3] = W;
    for (int c = 0; c < N; ++c) col[c] = (unsigned char)(((c % W) + (c / W)) & 1);

    total_free = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (pb[(size_t)y * w + x] == '.') { g0[(y + 1) * W + (x + 1)] = 1; ++total_free; }
    if (total_free == 0) { printf("x=0&y=0&path=\n"); return 0; }

    path = malloc(total_free + 8);
    fstack = malloc(sizeof(int) * (size_t)N);
    seen = calloc(N, sizeof(int32_t));
    srcmark = calloc(N, sizeof(int32_t));
    srcbuf = malloc(sizeof(int) * (size_t)N);
    estate = malloc((size_t)N * 4);
    dsu = malloc(sizeof(int) * (size_t)N);
    pq = malloc(sizeof(int) * (size_t)(N + 1));
    inq = malloc((size_t)N);
    chain_id = malloc(sizeof(int) * (size_t)N);
    chain_pos = malloc(sizeof(int) * (size_t)N);
    seqbuf = malloc(sizeof(int) * (size_t)N);
    estate2 = malloc((size_t)N * 4);
    dsu2 = malloc(sizeof(int) * (size_t)N);
    dsu0 = malloc(sizeof(int) * (size_t)N);
    pq2 = malloc(sizeof(int) * (size_t)(N + 1));
    inq2 = malloc((size_t)N);
    {   int fe = 8 * N + 32;                            // 弧数上限：每格 4 条 + 源汇 + 哑点
        fhead = malloc(sizeof(int) * (size_t)(N + 8));
        fnext = malloc(sizeof(int) * (size_t)(2 * fe)); fto = malloc(sizeof(int) * (size_t)(2 * fe));
        fcap = malloc(sizeof(int) * (size_t)(2 * fe));
        flevel = malloc(sizeof(int) * (size_t)(N + 8)); fiter = malloc(sizeof(int) * (size_t)(N + 8));
        fq = malloc(sizeof(int) * (size_t)(N + 8)); fmap = malloc(sizeof(int) * (size_t)N * 4);
        fscc = malloc(sizeof(int) * (size_t)(N + 8)); flow2 = malloc(sizeof(int) * (size_t)(N + 8));
        fdfn = malloc(sizeof(int) * (size_t)(N + 8)); fstk = malloc(sizeof(int) * (size_t)(N + 8));
        fon = malloc(sizeof(int) * (size_t)(N + 8));
        fcv = malloc(sizeof(int) * (size_t)(N + 8)); fce = malloc(sizeof(int) * (size_t)(N + 8));
        hdsu = malloc(sizeof(int) * (size_t)(N + 8)); hcross = malloc(sizeof(int) * (size_t)(N + 8));
        hedge = malloc(sizeof(int) * (size_t)(N + 8));
    }
    if (getenv("FLOW")) use_flow = atoi(getenv("FLOW"));
    if (getenv("CHAIN")) use_chain = atoi(getenv("CHAIN"));
    if (getenv("SUBTOUR")) use_subtour = atoi(getenv("SUBTOUR"));
    bk_estate = malloc((size_t)N * 4);
    bk_dsu = malloc(sizeof(int) * (size_t)N);
    bk_inq = malloc((size_t)N);
    probe_rounds = getenv("PROBEROUNDS") ? atoi(getenv("PROBEROUNDS")) : 0;
    probe_all = getenv("PROBEALL") ? atoi(getenv("PROBEALL")) : 1;
    prop_depth = getenv("PROPDEPTH") ? atoi(getenv("PROPDEPTH")) : -1;   // -1 = 自适应
    flow_depth = getenv("FLOWDEPTH") ? atoi(getenv("FLOWDEPTH")) : 0;

    end_ok = malloc((size_t)N); g_estate = calloc((size_t)N * 4, 1);
    tcand = calloc((size_t)N, 1);
    blk = malloc(sizeof(int) * (size_t)N);

    // ---- --verify <解文件>：拿已知真解逐步验证两层传播的 soundness ----
    // 沿真解走一遍，每个滑行落点上跑一次传播。真解是可行的，所以传播**永远不许报矛盾**，
    // 静态那层的 estate_ok 也**永远不许否掉真解实际走的那个方向**。
    // 任何一次报警都说明规则本身是错的（而不是「运气不好」），必须先修再谈性能。
    // ---- PROBEDUMP：逐起点传播普查（不需要已知解）。REF=可靠证伪，OK=幸存；
    // 幸存者 estate 取交集 => GFIX 全局边事实（对一切解成立）。STRONG=1 用强传播。
    if (getenv("PROBEDUMP")) {
        unsigned char *i1 = malloc((size_t)N * 4), *i2 = malloc((size_t)N * 4);
        memset(i1, 1, (size_t)N * 4); memset(i2, 1, (size_t)N * 4);
        int strongp = getenv("STRONG") ? atoi(getenv("STRONG")) : 0;
        int nok = 0, nref = 0;
        for (int s0 = 0; s0 < N; ++s0) {
            if (!g0[s0]) continue;
            int pok = strongp ? propagate_strong(s0) : propagate(s0);
            if (!pok) { ++nref; printf("REF %d\n", s0); continue; }
            ++nok;
            long long fs = 0;
            for (int c = 0; c < N; ++c) if (g0[c]) for (int d = 2; d < 4; ++d) {
                if (!g0[c + delta[d]]) continue;
                if (estate[c * 4 + d]) ++fs;
                if (estate[c * 4 + d] != 1) i1[c * 4 + d] = 0;
                if (estate[c * 4 + d] != 2) i2[c * 4 + d] = 0;
            }
            printf("OK %d %lld\n", s0, fs);
            if ((nok & 2047) == 0) fprintf(stderr, "...ok=%d ref=%d\n", nok, nref);
        }
        long long nf1 = 0, nf2 = 0;
        for (int c = 0; c < N; ++c) if (g0[c]) for (int d = 2; d < 4; ++d) {
            if (!g0[c + delta[d]]) continue;
            if (i1[c * 4 + d]) { printf("GFIX %d %d 1\n", c, d); ++nf1; }
            else if (i2[c * 4 + d]) { printf("GFIX %d %d 2\n", c, d); ++nf2; }
        }
        fprintf(stderr, "PROBEDUMP: ok=%d ref=%d 必用=%lld 禁用=%lld\n", nok, nref, nf1, nf2);
        return 0;
    }
    if (argc > 3 && !strcmp(argv[2], "--verify")) {
        FILE *sf = fopen(argv[3], "r");
        if (!sf) { fprintf(stderr, "打不开 %s\n", argv[3]); return 1; }
        static char sb[1 << 22];
        size_t sn = fread(sb, 1, sizeof sb - 1, sf); sb[sn] = 0; fclose(sf);
        int sx = atoi(strstr(sb, "x=") + 2), sy = atoi(strstr(sb, "y=") + 2);
        const char *ps = strstr(sb, "path=") + 5;
        int s = (sy + 1) * W + (sx + 1);

        printf("真起点 (%d,%d), 自由格 %d\n", sx, sy, total_free);
        int strong = getenv("STRONG") ? atoi(getenv("STRONG")) : 0;
        int pok = strong ? propagate_strong(s) : propagate(s);
        fprintf(stderr, "PROPOK %d\n", pok);
        {   // 每起点确定率普查（多项式路线的靶场刻度）
            long long te2 = 0, f1 = 0, f2 = 0;
            for (int c = 0; c < N; ++c) if (g0[c]) for (int d = 2; d < 4; ++d) {
                if (!g0[c + delta[d]]) continue;
                ++te2;
                if (estate[c * 4 + d] == 1) ++f1;
                else if (estate[c * 4 + d] == 2) ++f2;
            }
            fprintf(stderr, "PERSTART 判定率 %.1f%%（必用 %lld 禁用 %lld / %lld 边）\n",
                    te2 ? 100.0 * (f1 + f2) / te2 : 0.0, f1, f2, te2);
            if (getenv("FIXDUMP")) {
                for (int c = 0; c < N; ++c) if (g0[c]) for (int d = 2; d < 4; ++d) {
                    if (!g0[c + delta[d]]) continue;
                    if (estate[c * 4 + d])
                        fprintf(stderr, "FIX %d %d %d\n", c, d, estate[c * 4 + d]);
                }
            }
        }
        printf("静态传播: %s\n", pok ? "无矛盾 (对)" : "**矛盾 —— 静态层不 sound**");
        {   long long ne = 0, nu = 0, nb = 0;
            for (int c = 0; c < N; ++c) if (g0[c])
                for (int d = 2; d < 4; ++d) if (g0[c + delta[d]]) {
                    ++ne;
                    if (estate[c * 4 + d] == 1) ++nu; else if (estate[c * 4 + d] == 2) ++nb;
                }
            printf("边 %lld：必用 %lld，禁用 %lld，判定率 %.1f%%（流GAC 禁用%lld/必用%lld；"
                   "连通性加强 必用%lld 证伪%lld）\n",
                   ne, nu, nb, 100.0 * (nu + nb) / (ne ? ne : 1),
                   flow_bans, flow_uses, sub_uses, sub_refute);
            printf("终点候选：流过滤后剩 %lld / %d 个自由格（%.1f%%）\n",
                   end_cand, total_free, 100.0 * end_cand / total_free);
        }

        memcpy(g, g0, N);
        g[s] = 0;
        int cur = s, rem = total_free - 1, step = 0, nbad = 0;
        for (const char *q = ps; *q && *q != '\n' && *q != '\r'; ++q) {
            int d = (*q == 'L') ? 0 : (*q == 'U') ? 1 : (*q == 'R') ? 2 : 3;
            int dd = delta[d], len = 0, c = cur;
            while (g[c + dd]) { c += dd; ++len; }
            if (len == 0) { printf("第 %d 步方向 %c 走不动，解或解析有问题\n", step, *q); break; }
            if (!estate_ok(cur, d, dd, len, c)) {
                printf("**静态层不 sound**：第 %d 步（方向 %c）真解走的路被 estate_ok 否掉了\n", step, *q);
                ++nbad;
            }
            for (int k = 0; k < len; ++k) { cur += dd; g[cur] = 0; }
            rem -= len;
            if (!propagate_dyn(cur, rem, cur - (len - 1) * dd, dd, len)) {
                printf("**动态层不 sound**：第 %d 步之后传播报矛盾（剩 %d 格）\n", step, rem);
                ++nbad;
                break;
            }
            ++step;
        }
        printf("走完 %d 步，剩 %d 格，报警 %d 次 —— %s\n", step, rem, nbad,
               (nbad == 0 && rem == 0) ? "两层都 sound" : "**有问题**");
        return nbad ? 1 : 0;
    }


    if (getenv("GFILTER")) use_gfilter = atoi(getenv("GFILTER"));
    if (!global_fixpoint()) { fprintf(stderr, "全局阶段：度约束松弛无解\n"); return 1; }
    for (int c = 0; c < N; ++c) if (g0[c]) for (int d = 2; d < 4; ++d)
        if (g0[c + delta[d]] && g_estate[c * 4 + d]) ++gfix_total;
    fprintf(stderr, "全局阶段：固定 %lld 条边，起点候选 %lld / %d (%.1f%%)\n",
            gfix_total, gfilter_kept, total_free, 100.0 * gfilter_kept / total_free);
    // ===== 自适应传播深度（v50 重做）=====
    // v48 用「全局定死边比例 >=10%」当开关：达标上全深度，否则只做前 2 个决策。
    // 那个规则把传播零信息类误杀了：L414 的 gfix=0 但探针阶段 DYN 证伪率 19~21%，
    // 全深度把死树从 ~10M 节点剪到 ~15K（600 倍），655s -> 30s。
    // estate 密度只是传播威力的一个来源；零信息类里传播吃的是**残余区域自身的窄缩**，
    // 全局边定不下来不代表分支点传播没肉。真正可靠的判据是 DYNMIN 自测
    // （探针阶段实测证伪率 <15% 才降浅层，L389 型由它兜底），初值恒为全深度。
    if (prop_depth < 0) {
        long long te = 0;
        for (int c = 0; c < N; ++c) if (g0[c]) for (int d = 2; d < 4; ++d)
            if (g0[c + delta[d]]) ++te;
        prop_depth = (te > 0 && gfix_total * 100 >= te * 10) ? 999999 : 2;
        fprintf(stderr, "adaptive PROPDEPTH=%d (fixed %lld / %lld edges; 第一层存活率>=SURVDEEP%%再升深层)\n", prop_depth, gfix_total, te);
    }

    if (!wall_t0) wall_t0 = time(0);
    int *starts = malloc(sizeof(int) * (size_t)total_free);
    int ns = 0;
    memcpy(g, g0, N);
    for (int c = 0; c < N; ++c) if (g0[c] && end_ok[c]) starts[ns++] = c;
    // PINSTART="x,y"：只跑这一个起点。用来把「总耗时」拆成「赢家自己的树」和「找到赢家之前的调度浪费」——
    // 已知答案的关（如 L664 离线解出、起点 (90,11)）上，这一刀能直接判断瓶颈是搜索还是调度。
    // FUNNELIN=<文件>：读回上次 FUNNELDUMP 存下的幸存起点名单，只跑这些。
    // 漏斗最贵的部分是筛掉 33486 个起点里的绝大多数（L664 满优先级 287 秒）；
    // 幸存者只有约 1650 个，重跑一遍只要 1/20 的时间。
    // 这样调 SWEEP 之类的第三层参数时，实验循环从 10 分钟降到 1 分钟。
    if (getenv("FUNNELIN")) {
        FILE *fi = fopen(getenv("FUNNELIN"), "r");
        if (fi) {
            unsigned char *keepset = calloc((size_t)N, 1);
            int cc, nread = 0;
            while (fscanf(fi, "%d", &cc) == 1) if (cc >= 0 && cc < N) { keepset[cc] = 1; ++nread; }
            fclose(fi);
            int k = 0;
            for (int i = 0; i < ns; ++i) if (keepset[starts[i]]) starts[k++] = starts[i];
            ns = k;
            free(keepset);
            fprintf(stderr, "FUNNELIN: 名单 %d 个，与本片起点交集 %d 个\n", nread, ns);
        }
    }
    if (getenv("PINSTART")) {
        int px = -1, py = -1;
        if (sscanf(getenv("PINSTART"), "%d,%d", &px, &py) == 2) {
            int want = (py + 1) * W + (px + 1), k = 0;
            for (int i = 0; i < ns; ++i) if (starts[i] == want) starts[k++] = starts[i];
            ns = k;
            fprintf(stderr, "PINSTART=(%d,%d) -> %s\n", px, py,
                    ns ? "命中，只跑这一个起点" : "**该起点已被全局过滤剔除**");
        }
    }
    for (int i = 1; i < ns; ++i) {
        int v = starts[i], dv = freedeg(v), j = i;
        while (j > 0 && freedeg(starts[j - 1]) > dv) { starts[j] = starts[j - 1]; --j; }
        starts[j] = v;
    }

    // ---- STARTORD：外部起点排序注入（PROBEDUMP 普查输出直喂）----
    // OK 行按 F 升序重排（真起点 F 异常低：81 关中位升序位 16.8%），REF 行可靠证伪直接剔除。
    if (getenv("STARTORD")) {
        FILE *sof = fopen(getenv("STARTORD"), "r");
        if (sof) {
            long long *fval = malloc(sizeof(long long) * (size_t)N);
            for (int c = 0; c < N; ++c) fval[c] = (1LL << 60);
            char lbuf[128];
            long long banned = 0;
            while (fgets(lbuf, sizeof lbuf, sof)) {
                long long cc, ff;
                if (sscanf(lbuf, "OK %lld %lld", &cc, &ff) == 2) { if (cc >= 0 && cc < N) fval[cc] = ff; }
                else if (sscanf(lbuf, "REF %lld", &cc) == 1) { if (cc >= 0 && cc < N) { fval[cc] = -1; ++banned; } }
            }
            fclose(sof);
            int w2 = 0;
            long long thr = getenv("STARTONLY") ? ((1LL << 60) - 1) : (1LL << 60);
            for (int i = 0; i < ns; ++i) if (fval[starts[i]] >= 0 && fval[starts[i]] <= thr) starts[w2++] = starts[i];
            ns = w2;
            for (int i = 1; i < ns; ++i) {
                int v = starts[i], j = i;
                long long fv = fval[v];
                while (j > 0 && fval[starts[j - 1]] > fv) { starts[j] = starts[j - 1]; --j; }
                starts[j] = v;
            }
            fprintf(stderr, "STARTORD: F 升序就位（%d 起点，证伪剔除 %lld）\n", ns, banned);
            free(fval);
        }
    }

    // ---- 分叉：起点分片，各干各的 ----
    nshard = getenv("JOBS") ? atoi(getenv("JOBS")) : (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (getenv("FORCESHARD")) { force_shard_identity = atoi(getenv("FORCESHARD")); nshard = 1; shard = 0; }
    if (getenv("FORCESHARD")) seed_mode = getenv("SEEDMODE") ? atoi(getenv("SEEDMODE")) : (force_shard_identity % 3);
    if (nshard < 1) nshard = 1;
    if (nshard > ns) nshard = ns;
    int *pfd = malloc(sizeof(int) * (size_t)nshard * 2);
    pid_t *kids = malloc(sizeof(pid_t) * (size_t)nshard);
    if (getenv("STEAL")) use_steal = atoi(getenv("STEAL"));
    if (use_steal) {                                      // fork 之前建好共享计数器
        void *m = mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (m != MAP_FAILED) { work_ctr = (volatile int *)m; *work_ctr = 0; }
    }
    if (nshard > 1) {
        for (int k = 0; k < nshard; ++k) {
            if (pipe(&pfd[k * 2]) != 0) { nshard = 1; break; }
        }
    }
    if (nshard > 1) {
        for (int k = 0; k < nshard; ++k) {
            pid_t pid = fork();
            if (pid == 0) {                                 // 子进程
                prctl(PR_SET_PDEATHSIG, SIGKILL);       // 父进程死（含 evaluate 超时杀）子进程跟着死，别留孤儿烧机器
                if (getppid() == 1) _exit(1);           // 关窗：prctl 生效前父进程已死的竞态
                // 管道在 fork 之前就全建好了，这个孩子继承了「所有」管道的两端。
                // 不属于自己的必须全关，否则别人的写端被我攥着，父进程永远收不到那条管道的 EOF。
                for (int j = 0; j < nshard; ++j) {
                    close(pfd[j * 2]);
                    if (j != k) close(pfd[j * 2 + 1]);
                }
                shard = k; out_fd = pfd[k * 2 + 1];
                seed_mode = getenv("SEEDMODE") ? atoi(getenv("SEEDMODE")) : (k % 3);
                goto child_work;
            }
            kids[k] = pid;
            close(pfd[k * 2 + 1]);
        }
        // 父进程：poll 所有读端。孩子有解就写管道；无解就自己退出，读端报 EOF。
        // 用 poll 而不是 wait，是因为大盘的解可能撑爆管道 64KB 缓冲——孩子会阻塞在 write 上
        // 等人来读，而 wait() 里的父进程要等它退出才读，那就互相等死了。
        struct pollfd *pf = malloc(sizeof(struct pollfd) * (size_t)nshard);
        for (int k = 0; k < nshard; ++k) { pf[k].fd = pfd[k * 2]; pf[k].events = POLLIN; }
        static char rbuf[1 << 22];
        int alive = nshard;
        while (alive > 0) {
            if (poll(pf, (nfds_t)nshard, -1) < 0) break;
            for (int k = 0; k < nshard; ++k) {
                if (pf[k].fd < 0 || pf[k].revents == 0) continue;
                size_t len = 0;
                ssize_t r;
                while ((r = read(pf[k].fd, rbuf + len, sizeof rbuf - 1 - len)) > 0) len += (size_t)r;
                if (len > 0) {                              // 这个 shard 赢了
                    rbuf[len] = 0;
                    fputs(rbuf, stdout);
                    fflush(stdout);
                    for (int j2 = 0; j2 < nshard; ++j2) if (kids[j2] > 0) kill(kids[j2], SIGKILL);
                    return 0;
                }
                close(pf[k].fd); pf[k].fd = -1; --alive;    // 这片搜穷了，无解
            }
        }
        for (int j2 = 0; j2 < nshard; ++j2) if (kids[j2] > 0) kill(kids[j2], SIGKILL);
        fprintf(stderr, "no solution found\n");
        return 1;
    }
  child_work:;

    {
        int swx = getenv("SWARMX") ? atoi(getenv("SWARMX")) : 6;   // L473 有 138 个候选卡在旧阈值 112 外面：87s -> 12.5s（7 倍）
        swarm = (ns <= nshard * swx) && (nshard > 1 || force_shard_identity >= 0);
    }
    swarm_depth = getenv("SWDEPTH") ? atoi(getenv("SWDEPTH")) : 3;
    { int ident = force_shard_identity >= 0 ? force_shard_identity : shard;
      shard_seed = swarm ? (unsigned int)(ident * 2654435761u) : 0u; }
    if (swarm) fprintf(stderr, "shard %d: swarm 模式（%d 个候选全员上，随机顺序）\n", shard, ns);

    // ---- 分层探针 ----
    // v20 里 propagate_strong（传播 + probing，每起点约 0.05 秒）被算了两遍：
    // 探针阶段一遍、正式搜之前又一遍。**把它缓存下来**，中间就能白捡一层大预算的筛选：
    //
    //   第一层：propagate_strong + 小预算(PROBE)   —— 便宜地干掉大部分起点
    //   第二层：复用缓存的 estate + 大预算(PROBE2) —— 不用重算 probing，纯赚
    //   第三层：复用缓存的 estate + 无限搜         —— 搜穷即永久剔除
    //
    // 为什么要分层：探针预算的最优值**因关而异**，而且差得离谱
    // （L217 在 PROBE=2000 要 18.6 秒、PROBE=16000 只要 0.28 秒；L223 反过来 PROBE=8000 最好）。
    // 单一预算总会在某些关上踩空，分层等于同时吃到小预算的便宜和大预算的证伪力。
    // 注意第三层的「无限搜」必须保留 —— v15 那次纯迭代加深之所以惨败（L193 7s→174s），
    // 就是因为错起点永远搜不穷、被反复重搜，而搜穷一次就能永久剔除才是真正的投资。
    // PROBE 的最优值**第二次翻转**了。历史：v13 时代 2000 最优 -> v20 加了 probing 之后
    // 8000 最优（+16 关）-> 现在 v40 去掉 probing，又变回 2000。
    // 每次底层一变，这个常数的最优点就跟着搬家：L239 23.9s->15.8s，L257 73.9s->54.9s，
    // L195 9.5s->4.9s，中小关全部持平。
    // **凡是「最优常数」，都要在底层改动之后重扫一遍**，这已经是同一个坑踩第二次了。
    // v58：探针预算按类分家。零信息类（gfix==0）的最优在 650（L475 实测 207s->26s 的主力刀），
    // 其余类保持 2000（L473 型 swarm 类在 650 下会炸：12.5s -> 31s+）。
    long long probe = getenv("PROBE") ? atoll(getenv("PROBE"))
        : (gfix_total == 0 ? (getenv("PROBEZERO") ? atoll(getenv("PROBEZERO")) : 650) : 2000);
    long long probe2 = getenv("PROBE2") ? atoll(getenv("PROBE2")) : 32000;   // v48 底座上重扫：L447 30.4->23.8s（进线），其余全绿
    // PROBE2 的最优点也搬家了（第三次验证「底层一动就重扫常数」）：v23 时代 48000 会让
    // L239 炸到 316s，BFS 版下它反而是甜点 —— L389 35s->10.6s，L377/L257 也更快，其余持平。
    int *sc = malloc(sizeof(int) * (size_t)ns);
    long long *fsv = calloc((size_t)ns, sizeof(long long));
    unsigned char **est_save = malloc(sizeof(unsigned char *) * (size_t)ns);
    unsigned char **tc_save = malloc(sizeof(unsigned char *) * (size_t)ns);
    int keep = 0;

    for (int ii = 0; ; ++ii) {
        int i;
        if (swarm) { i = ii; if (i >= ns) break; }
        else if (use_steal && work_ctr) {                 // 动态取号：谁先空谁多干
            i = __sync_fetch_and_add((int *)work_ctr, 1);
            if (i >= ns) break;
        } else {                                          // 退化成原来的静态分片
            i = ii;
            if (i >= ns) break;
            if (i % nshard != shard) continue;
        }
        int s = starts[i];
        if (!(probe_all ? propagate_strong(s) : propagate(s))) continue;
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = 0; low_cnt = zero_cnt = 0;
        for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
        for (int c = 0; c < N; ++c) if (g[c]) { deg[c] = freedeg(c);
            if (deg[c] <= 1) ++low_cnt; if (deg[c] == 0) ++zero_cnt; }
        mark(s);
        if (!reach_ok(s, total_free - 1)) continue;        // 起点是割点，整盘直接不连通
        path_len = 0; nodes = 0; node_limit = probe; best_rem = total_free;
        live_end = -1; tc_on = 0; ntc_low = 0;
        if (use_flow && tcand_for == s) {            // 候选集必须是**本起点**的，串台就关闭
            live_end = 0;
            for (int c2 = 0; c2 < N; ++c2) if (g[c2] && tcand[c2]) ++live_end;
            if (live_end == 0) live_end = -1;        // 流没跑成或候选为空就关掉，别误杀
            if (live_end > 0) {
                tc_on = 1;
                for (int c2 = 0; c2 < N; ++c2)
                    if (g[c2] && deg[c2] <= 1 && !tcand[c2]) ++ntc_low;
            }
        }
        int r = dfs(s, total_free - 1, 0);
        tc_on = 0; live_end = -1;
        if (r == 1) { path[path_len] = 0; emit(s, path); }
        if (r == 0) continue;                              // 搜穷仍无解 => 永久剔除
        fsv[keep] = plainF_last;
        starts[keep] = s; sc[keep] = best_rem;
        est_save[keep] = malloc((size_t)N * 4);
        memcpy(est_save[keep], estate, (size_t)N * 4);      // 缓存，后两层直接复用
        tc_save[keep] = malloc((size_t)N);
        memcpy(tc_save[keep], tcand, (size_t)N);            // 终点候选集也要随起点缓存，串台会误杀真解
        ++keep;
    }

    // FRANK：跑内 F 排序（1=降序 2=升序 3=奇偶双向对冲 4=自适应方向）
    // 自适应默认：大盘(n>=18000)系统性偏坏尾用降序(501-663全实证)，小盘用升序(L475升序31s)
    int frank_mode = getenv("FRANK") ? atoi(getenv("FRANK")) : 4;
    int frank_desc = (frank_mode == 1)
        || (frank_mode == 3 && (shard & 1) == 0)
        || (frank_mode == 4 && total_free >= 18000);
    if (frank_mode) {
        int desc = frank_desc;
        for (int i = 1; i < keep; ++i) {
            int vs = starts[i], vc = sc[i], j2 = i;
            long long vf = fsv[i];
            unsigned char *ve = est_save[i], *vt = tc_save[i];
            while (j2 > 0 && (desc ? (fsv[j2 - 1] < vf) : (fsv[j2 - 1] > vf))) {
                starts[j2] = starts[j2 - 1]; sc[j2] = sc[j2 - 1]; fsv[j2] = fsv[j2 - 1];
                est_save[j2] = est_save[j2 - 1]; tc_save[j2] = tc_save[j2 - 1]; --j2;
            }
            starts[j2] = vs; sc[j2] = vc; fsv[j2] = vf; est_save[j2] = ve; tc_save[j2] = vt;
        }
        fprintf(stderr, "FRANK shard%d: %s" "\n", shard, desc ? "F降序" : "F升序");
    } else if (!getenv("NORANK")) for (int i = 1; i < keep; ++i) {
        int vs = starts[i], vc = sc[i], j2 = i;
        unsigned char *ve = est_save[i], *vt = tc_save[i];
        while (j2 > 0 && sc[j2 - 1] > vc) {
            starts[j2] = starts[j2 - 1]; sc[j2] = sc[j2 - 1];
            est_save[j2] = est_save[j2 - 1]; tc_save[j2] = tc_save[j2 - 1]; --j2;
        }
        starts[j2] = vs; sc[j2] = vc; est_save[j2] = ve; tc_save[j2] = vt;
    }

    // 只在**需要**的时候才上第二层。
    // v21 无差别地给每一关都加一层大预算筛选，结果是「只赚尾部、赔了中盘」
    // （L239 342s→44s，但 L213 1.5s→23.9s，总分 237→221）——因为那层钱摊在了不需要它的关上。
    // 判据用**第一层的存活率**：存活率高就说明 PROBE 这个预算在这一关根本搜不穷子树、
    // 证伪不动，那才值得加钱；存活率低说明预算已经够用，直接进无限搜。
    int mine_total = 0;
    if (!(use_steal && work_ctr) || swarm) { for (int i = 0; i < ns; ++i) if (i % nshard == shard) ++mine_total; }
    int thresh = getenv("TIER2") ? atoi(getenv("TIER2")) : 30;   // 存活率(%)超过它才上第二层
    int sweep3 = 0;
    int use_tier2 = (mine_total > 0 && keep * 100 > mine_total * thresh);
    // v51：零信息类的真判据是「浅层探针杀不杀得动」——存活率高说明每棵树都巨大，
    // 深层传播的每节点成本能靠 600 倍的剪树回本（L414 死树 10M -> 15K 节点）。
    // 存活率低说明树本来就小（L381 浅层 6.5s 通关），深层纯付成本。
    {
        int sdthr = getenv("SURVDEEP") ? atoi(getenv("SURVDEEP")) : 38;
        if (prop_depth <= 2 && mine_total > 0 && keep * 100 >= mine_total * sdthr) {
            prop_depth = getenv("PDPROMO") ? atoi(getenv("PDPROMO")) : (total_free >= 18000 ? 64 : 16);   // 自适应：大盘路径长需更深窗口(501+实测64)，小盘16是平台
            sweep3 = 1;                                                      // 升档分片的第三层用限额扫荡
            seed_full = 1;                                                   // 零类的证伪吃跨滑行组合结论，末滑行播种亏 1/3 证伪率
            fprintf(stderr, "shard %d: 第一层存活 %d%% >= %d%%，升深度传播(PD=%d)\n",
                    shard, keep * 100 / mine_total, sdthr, prop_depth);
            // ---- 第 1.5 层：深层重探幸存者（v52）----
            // 浅层第一层的 sc/杀集在大树类上质量差（v51 的 L475 200s->400s+ 未解出）。
            // 用同样的 PROBE 预算深层重探一遍：深层证伪 ⊇ 浅层证伪、estate 缓存两者通用，
            // 重探后的幸存集与排序精确等于 v50 全程深层的第一层，成本却只花在幸存者上。
            int keep15 = 0;
            for (int i = 0; i < keep; ++i) {
                int s = starts[i];
                memcpy(estate, est_save[i], (size_t)N * 4); dsu0_valid = 0;
                memcpy(tcand, tc_save[i], (size_t)N);
                tcand_for = s;
                memcpy(g, g0, N);
                cnt[0] = cnt[1] = 0; low_cnt = zero_cnt = 0;
                for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
                for (int c = 0; c < N; ++c) if (g[c]) { deg[c] = freedeg(c);
                    if (deg[c] <= 1) ++low_cnt; if (deg[c] == 0) ++zero_cnt; }
                mark(s);
                path_len = 0; nodes = 0; node_limit = probe; best_rem = total_free;
                live_end = -1; tc_on = 0; ntc_low = 0;
                if (use_flow && tcand_for == s) {
                    live_end = 0;
                    for (int c2 = 0; c2 < N; ++c2) if (g[c2] && tcand[c2]) ++live_end;
                    if (live_end == 0) live_end = -1;
                    if (live_end > 0) {
                        tc_on = 1;
                        for (int c2 = 0; c2 < N; ++c2)
                            if (g[c2] && deg[c2] <= 1 && !tcand[c2]) ++ntc_low;
                    }
                }
                int r15 = dfs(s, total_free - 1, 0);
                tc_on = 0; live_end = -1;
                if (r15 == 1) { path[path_len] = 0; emit(s, path); }
                if (r15 == 0) { free(est_save[i]); free(tc_save[i]); continue; }
                starts[keep15] = s; sc[keep15] = best_rem; fsv[keep15] = fsv[i];
                est_save[keep15] = est_save[i]; tc_save[keep15] = tc_save[i];
                ++keep15;
            }
            keep = keep15;
            if (frank_mode || !getenv("NORANK")) for (int i = 1; i < keep; ++i) {
                int vs = starts[i], vc = sc[i], j2 = i;
                long long vf = fsv[i];
                unsigned char *ve = est_save[i], *vt = tc_save[i];
                while (j2 > 0 && (frank_mode ? (frank_desc ? fsv[j2 - 1] < vf : fsv[j2 - 1] > vf) : sc[j2 - 1] > vc)) {
                    starts[j2] = starts[j2 - 1]; sc[j2] = sc[j2 - 1]; fsv[j2] = fsv[j2 - 1];
                    est_save[j2] = est_save[j2 - 1]; tc_save[j2] = tc_save[j2 - 1]; --j2;
                }
                starts[j2] = vs; sc[j2] = vc; fsv[j2] = vf; est_save[j2] = ve; tc_save[j2] = vt;
            }
            fprintf(stderr, "shard %d: 1.5 层深层重探后剩 %d\n", shard, keep);
            use_tier2 = (mine_total > 0 && keep * 100 > mine_total * thresh);
        }
    }
    fprintf(stderr, "shard %d: 第一层存活 %d/%d = %d%%%%, 第二层 %s\n",
            shard, keep, mine_total, mine_total ? keep * 100 / mine_total : 0,
            use_tier2 ? "开" : "跳过");
    if (use_tier2)
    // 第二层：大预算再筛一遍，estate 直接复用，probing 不重算
    {
    int keep2 = 0;
    // swarm：起点顺序也要错开 —— 探针后幸存 ~11 个候选，28 个 shard 若用同一份排序，
    // 全体都在先磕同一个（可能错误的）起点，DFS 抖动只是同一棵树里换姿势。
    // 每个 shard 从排序表的不同位置起轮，候选层面就被真正摊开了。
    int srot = swarm ? (shard % (keep > 0 ? keep : 1)) : 0;
    unsigned char *drop2 = calloc((size_t)(keep > 0 ? keep : 1), 1);
    for (int i0 = 0; i0 < keep; ++i0) {
        int i = (i0 + srot) % keep;
        int s = starts[i];
        memcpy(estate, est_save[i], (size_t)N * 4); dsu0_valid = 0;
        memcpy(tcand, tc_save[i], (size_t)N);
        tcand_for = s;
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = 0; low_cnt = zero_cnt = 0;
        for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
        for (int c = 0; c < N; ++c) if (g[c]) { deg[c] = freedeg(c);
            if (deg[c] <= 1) ++low_cnt; if (deg[c] == 0) ++zero_cnt; }
        mark(s);
        path_len = 0; nodes = 0; node_limit = probe2; best_rem = total_free;
        live_end = -1; tc_on = 0; ntc_low = 0;
        if (use_flow && tcand_for == s) {            // 候选集必须是**本起点**的，串台就关闭
            live_end = 0;
            for (int c2 = 0; c2 < N; ++c2) if (g[c2] && tcand[c2]) ++live_end;
            if (live_end == 0) live_end = -1;        // 流没跑成或候选为空就关掉，别误杀
            if (live_end > 0) {
                tc_on = 1;
                for (int c2 = 0; c2 < N; ++c2)
                    if (g[c2] && deg[c2] <= 1 && !tcand[c2]) ++ntc_low;
            }
        }
        int r = dfs(s, total_free - 1, 0);
        tc_on = 0; live_end = -1;
        if (r == 1) { path[path_len] = 0; emit(s, path); }
        if (r == 0) { drop2[i] = 1; continue; }
        (void)keep2;
    }
    {   // 轮转遍历后再顺序压缩（压缩不能在轮转循环里做，会把没跑到的项挤掉）
        int w = 0;
        for (int i = 0; i < keep; ++i) {
            if (drop2[i]) { free(est_save[i]); free(tc_save[i]); continue; }
            starts[w] = starts[i]; sc[w] = sc[i]; fsv[w] = fsv[i];
            est_save[w] = est_save[i]; tc_save[w] = tc_save[i]; ++w;
        }
        keep2 = w;
    }
    if (frank_mode || !getenv("NORANK")) for (int i = 1; i < keep2; ++i) {
        int vs = starts[i], vc = sc[i], j2 = i;
        long long vf = fsv[i];
        unsigned char *ve = est_save[i], *vt = tc_save[i];
        while (j2 > 0 && (frank_mode ? (frank_desc ? fsv[j2 - 1] < vf : fsv[j2 - 1] > vf) : sc[j2 - 1] > vc)) {
            starts[j2] = starts[j2 - 1]; sc[j2] = sc[j2 - 1]; fsv[j2] = fsv[j2 - 1];
            est_save[j2] = est_save[j2 - 1]; tc_save[j2] = tc_save[j2 - 1]; --j2;
        }
        starts[j2] = vs; sc[j2] = vc; fsv[j2] = vf; est_save[j2] = ve; tc_save[j2] = vt;
    }
    keep = keep2;
    if (getenv("FUNNELDUMP")) {
        char fdn[512];
        snprintf(fdn, sizeof fdn, "%s.%d", getenv("FUNNELDUMP"), shard);
        FILE *fdf = fopen(fdn, "w");
        if (fdf) { for (int i = 0; i < keep; ++i) fprintf(fdf, "%d\n", starts[i]); fclose(fdf); }
    }
    free(drop2);
    }
    fprintf(stderr, "probe[t=%llds]: %d/%d 起点被证伪，剩 %d\n", (long long)(time(0) - wall_t0), ns - keep, ns, keep);
    if (getenv("DEATHSTAT")) fprintf(stderr, "DEATH liveend=%lld estate=%lld reach=%lld dyn=%lld flow=%lld geom=%lld\n",
        dth_liveend, dth_estate, dth_reach, dth_dyn, dth_flow, dth_geom);
    if (dyn_calls > 50) {
        fprintf(stderr, "DYN shard%d: %lld calls, %lld refuted (%.0f%%)\n", shard, dyn_calls, dyn_refutes, 100.0 * dyn_refutes / dyn_calls);
        // 自测式自适应：探针阶段的分支传播证伪率太低 => 正式搜索降回浅层，别再付这个钱
        if (prop_depth > 100 && dyn_refutes * 100 < dyn_calls * (long long)(getenv("DYNMIN") ? atoi(getenv("DYNMIN")) : 15))
            prop_depth = 2;
    }

    // ---- 第三层：按顺序正式搜（v53：升档分片用多轮限额扫荡）----
    // L501@PD16 的树成本重尾（中位 ~15 万节点、尾部 3.5 亿）：无限搜让怪物垄断队列，
    // 而可解树往往便宜（L414 赢家 2 万节点）。低预算轮先收割软柿子，
    // r==0 搜穷永久剔除，-1 预算尽留下一轮，最后一轮无限搜保底。
    long long sweep0 = getenv("SWEEP") ? atoll(getenv("SWEEP")) : 3000000;
    // ⚠ 原来这里按 n 线性放大（(n-18000)*5000+3M），L664 算出 **8040 万**节点。
    // 但 L664 赢家树实测只有 **291,193 节点**（PINSTART=90,11 直接量的）——预算是它的 276 倍，
    // 等于第一轮就给每个起点近乎无限的额度，「多轮限额扫荡防怪物垄断」这个机制**完全失效**：
    // 怪物起点一个人就能把 600 秒烧光，赢家永远轮不上。
    // 那个「664→~78M 实测标定」标错了对象（标的是怪物树/决赛者总量，不是赢家树）。
    // 改回小额度起步，放大交给本来就有的倍增阶梯（SWEEPMUL=8，4 轮 = 3M/24M/192M/1.5G）——
    // 大盘赢家真需要 18M 的话第二轮就接住了，代价只是多跑一轮，而不是把整关烧掉。
    int sweep_mul = getenv("SWEEPMUL") ? atoi(getenv("SWEEPMUL")) : 8;
    int nrounds3 = (sweep3 && !getenv("NOSWEEP")) ? 4 : 1;
    unsigned char *dead3 = calloc((size_t)(keep > 0 ? keep : 1), 1);
    for (int round3 = 0; round3 < nrounds3; ++round3) {
    long long budget3 = ((long long)1 << 62);
    if (round3 < nrounds3 - 1) { budget3 = sweep0; for (int r2 = 0; r2 < round3; ++r2) budget3 *= sweep_mul; }
    int srot3 = swarm && keep > 0 ? (shard % keep) : 0;
    for (int i0 = 0; i0 < keep; ++i0) {
        int i = (i0 + srot3) % keep;
        if (dead3[i]) continue;
        int s = starts[i];
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = 0; low_cnt = zero_cnt = 0;
        for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
        for (int c = 0; c < N; ++c) if (g[c]) {
            deg[c] = freedeg(c);
            if (deg[c] <= 1) ++low_cnt;
            if (deg[c] == 0) ++zero_cnt;
        }
        memcpy(estate, est_save[i], (size_t)N * 4); dsu0_valid = 0;   // 复用第一层缓存好的 estate，不重算
        memcpy(tcand, tc_save[i], (size_t)N);
        tcand_for = s;
        mark(s);
        // 归纳的底座：局部检查靠「上一层的剩余区域连通」，起点这一层得自己验。
        // 起点若是割点，去掉它剩余区域就断了。顺带白捡一条起点剪枝。
        if (!reach_ok(s, total_free - 1)) continue;

        path_len = 0; nodes = 0; node_limit = budget3;
        live_end = -1; tc_on = 0; ntc_low = 0;
        if (use_flow && tcand_for == s) {
            live_end = 0;
            for (int c2 = 0; c2 < N; ++c2) if (g[c2] && tcand[c2]) ++live_end;
            if (live_end == 0) live_end = -1;
            if (live_end > 0) {
                tc_on = 1;
                for (int c2 = 0; c2 < N; ++c2)
                    if (g[c2] && deg[c2] <= 1 && !tcand[c2]) ++ntc_low;
            }
        }
        int t3r = dfs(s, total_free - 1, 0);
        if (getenv("TREELOG")) fprintf(stderr, "T3[t=%llds] shard%d r%d #%d cell=(%d,%d) sc=%d nodes=%lld r=%d\n",
            (long long)(time(0) - wall_t0), shard, round3, i0, s % W - 1, s / W - 1, sc[i], nodes, t3r);
        if (t3r == 1) { path[path_len] = 0; emit(s, path); }
        if (t3r == 0) dead3[i] = 1;
        tc_on = 0; live_end = -1;
    }
    if (nrounds3 > 1) fprintf(stderr, "sweep shard%d round%d done (budget %lld)\n", shard, round3, budget3);
    }
    fprintf(stderr, "no solution found\n");
    return 1;
}
