// Coil solver — v23: 自适应分层（只在第一层存活率高时才上第二层大预算）
//
// 以下是 v21 的说明。
// Coil solver — v26: v25 的出方向版 —— 再挖一层白捡的有向信息
//
// v25 只用了「进入方向」。链定向同时也确定了每格的**后继方向** pin_out[c]，而滑行有两条强性质：
//   · run 的**中间格必须直穿**（进出同向）=> pin_out[q] 若不等于滑行方向，矛盾
//   · run 的**落点必然转弯或收尾**（前方已被挡住）=> pin_out[c] 若等于滑行方向，矛盾
// 两条都是 O(1)，和 v25 的进入方向检查走同一个循环。
//
// Coil solver — v25: 把链定向的结果**接进搜索**当有向剪枝
//
// 发现一处白扔的信息：orient_chains 对每条链算了两个行进方向可不可行（ok0/ok1），
// 但只用来判「两个都不行 => 矛盾」。**只有一个可行时，整条链的行进方向其实已经钉死了**，
// 这个有向信息一直没交给 DFS。而笔记里的诊断正说「缺的是有向信息，无向边装不下时序」。
//
// 做法：链定向时把每格的**强制进入方向** pin_in[c] 记下来（-1 表示未定），
// DFS 每次滑行穿过一格就 O(1) 校验一次进入方向对不对。PIN=0 可关掉做对照。
//
// Coil solver — v24: v23 + **动态桥规则**（可用边图里的桥必然被使用）
//
// 昨晚测到「初始网格图上一条桥都没有」（这批盘面 2 边连通）就把这条规则搁置了，
// 但传播禁掉一批边之后，剩余的**可用边图**上会冒出桥。而桥有一条比 in 方向蕴含式
// 更直接的用法：**可用边图里的桥必然被路径使用**——去掉它图就断成两块，
// 哈密顿路径要覆盖两边所有格子，唯一能跨过去的可用边就是它。
// 这条直接就是 estate=1，不需要 in 变量，能塞进现有的边传播，而且正是缺的那种「种子」。
// BRIDGE=0 可关掉做对照。
//
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>

static int shard = 0, nshard = 1;      // 本进程负责 starts 里下标 % nshard == shard 的那些
static int out_fd = -1;                // 子进程把答案写这里
static void emit(int s, const char *pathstr);

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
static long long nodes, node_limit;
static int best_rem;

// 找到解就写进管道（子进程）或直接打印（单进程模式）
static void emit(int s, const char *pathstr) {
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

static inline void mark(int c) {                 // 未访问 -> 已访问
    if (deg[c] <= 1) --low_cnt;
    if (deg[c] == 0) --zero_cnt;
    g[c] = 0; --cnt[col[c]];
    for (int d = 0; d < 4; ++d) {
        int n = c + delta[d];
        if (g[n]) {
            int o = deg[n]--;
            if (o == 2) ++low_cnt;
            if (o == 1) ++zero_cnt;
        }
    }
}

static inline void unmark(int c) {               // 已访问 -> 未访问（严格 LIFO 回滚）
    g[c] = 1; ++cnt[col[c]];
    for (int d = 0; d < 4; ++d) {
        int n = c + delta[d];
        if (n != c && g[n]) {
            int o = deg[n]++;
            if (o == 1) --low_cnt;
            if (o == 0) --zero_cnt;
        }
    }
    deg[c] = freedeg(c);
    if (deg[c] <= 1) ++low_cnt;
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
static signed char *pin_in;        // 每格被钉死的进入方向，-1 = 未定
static signed char *pin_out;       // 每格被钉死的后继方向，-1 = 未定
static int use_pin = 1;
static long long pins_set = 0, pin_hits = 0;   // 统计用

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
    if (use_pin) { memset(pin_in, -1, (size_t)N); memset(pin_out, -1, (size_t)N); pins_set = 0; }
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
        // 只有一个方向可行 => 整条链的行进方向钉死，把每格的进入方向记下来给 DFS 用
        if (use_pin && (ok0 != ok1)) {
            if (ok0) {                                      // 沿 seqbuf 正序走
                for (int i = 1; i < k; ++i) {
                    int diff = seqbuf[i] - seqbuf[i - 1];
                    for (int d = 0; d < 4; ++d) if (delta[d] == diff) {
                        pin_in[seqbuf[i]] = (signed char)d;
                        pin_out[seqbuf[i - 1]] = (signed char)d;   // 前一格的后继方向同时定了
                        ++pins_set; break;
                    }
                }
            } else {                                        // 沿 seqbuf 逆序走
                for (int i = k - 2; i >= 0; --i) {
                    int diff = seqbuf[i] - seqbuf[i + 1];
                    for (int d = 0; d < 4; ++d) if (delta[d] == diff) {
                        pin_in[seqbuf[i]] = (signed char)d;
                        pin_out[seqbuf[i + 1]] = (signed char)d;
                        ++pins_set; break;
                    }
                }
            }
        }
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
// ===== 动态桥规则 =====
// 在「未被禁用的边」构成的图上找桥。每找到一条未定的桥就判它必用。
// 迭代式 Tarjan（盘面可达 260 万格，递归会爆栈）。
static int *bdisc, *blow, *bstk_v, *bstk_pd, *bstk_di;
static int use_bridge = 1;
static long long bridges_found = 0;   // 统计用

static int bridge_pass(void) {
    if (!use_bridge || prop_bad) return 0;
    ++seen_id;
    int found = 0, timer = 0;
    for (int root = 0; root < N; ++root) {
        if (!g0[root] || seen[root] == seen_id) continue;
        seen[root] = seen_id; bdisc[root] = blow[root] = timer++;
        bstk_v[0] = root; bstk_pd[0] = -1; bstk_di[0] = 0;
        int sp = 1;
        while (sp) {
            int v = bstk_v[sp - 1];
            if (bstk_di[sp - 1] < 4) {
                int d = bstk_di[sp - 1]++;
                if (estate[v * 4 + d] == 2) continue;        // 已禁用的边不在图里
                int u = v + delta[d];
                if (!g0[u]) continue;
                if (seen[u] != seen_id) {
                    seen[u] = seen_id; bdisc[u] = blow[u] = timer++;
                    bstk_v[sp] = u; bstk_pd[sp] = d ^ 2; bstk_di[sp] = 0; ++sp;
                } else if (d != bstk_pd[sp - 1]) {           // 回父亲那条边不算回边
                    if (bdisc[u] < blow[v]) blow[v] = bdisc[u];
                }
            } else {
                --sp;
                if (sp) {
                    int par = bstk_v[sp - 1];
                    if (blow[v] < blow[par]) blow[par] = blow[v];
                    if (blow[v] > bdisc[par]) {              // (par, v) 是桥
                        int d = bstk_pd[sp] ^ 2;             // par -> v 的方向
                        if (estate[par * 4 + d] == 0) {
                            set_edge(par, d, 1);
                            ++found; ++bridges_found;
                            if (prop_bad) return found;
                        }
                    }
                }
            }
        }
    }
    return found;
}

static void prun_queue(void) {
    while (qhead != qtail && !prop_bad) {
        int c = pq[qhead++];
        if (qhead == N + 1) qhead = 0;
        inq[c] = 0;
        pprocess(c);
    }
}

static int propagate(int s) {
    memset(estate, 0, (size_t)N * 4);
    for (int c = 0; c < N; ++c) dsu[c] = c;
    memset(inq, 0, (size_t)N);
    qhead = qtail = 0;
    prop_bad = 0; prop_forced_end = -1; prop_start = s;
    prop_endcol = (total_free & 1) ? col[s] : (col[s] ^ 1);

    for (int c = 0; c < N; ++c) if (g0[c]) ppush(c);
    prun_queue();
    for (int it = 0; it < 6 && !prop_bad; ++it) {          // 桥 -> 传播 -> 可能又冒出新桥
        if (!bridge_pass()) break;
        prun_queue();
    }

    for (int round = 0; round < 8 && !prop_bad; ++round) {
        int nok = 0, okd = -1;
        for (int d = 0; d < 4; ++d) if (first_move_dir_ok(s, d)) { ++nok; okd = d; }
        if (nok == 0) { prop_bad = 1; break; }              // 四个方向都开不了局 => 起点证伪
        if (nok > 1) break;                                 // 还有得选，推不动了
        if (!force_first_line(s, okd)) break;               // 没有新信息，稳定了
        prun_queue();
        for (int it = 0; it < 6 && !prop_bad; ++it) {
            if (!bridge_pass()) break;
            prun_queue();
        }
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
static int propagate_strong(int s) {
    if (!propagate(s)) return 0;
    if (!do_probing(probe_rounds)) return 0;
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
    if (use_pin) {                                            // 有向剪枝
        for (int k = 0, q = p + dd; k < len; ++k, q += dd) {
            if (pin_in[q] >= 0 && pin_in[q] != d) { ++pin_hits; return 0; }   // 进入方向对不上
            if (k < len - 1) {                                 // 中间格必须直穿
                if (pin_out[q] >= 0 && pin_out[q] != d) { ++pin_hits; return 0; }
            } else {                                           // 落点必然转弯或收尾，不会再直行
                if (pin_out[q] == d) { ++pin_hits; return 0; }
            }
        }
    }
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
static int propagate_dyn(int p, int remaining) {
    if (remaining <= 0) return 1;
    memset(estate2, 0, (size_t)N * 4);
    memset(inq2, 0, (size_t)N);
    q2h = q2t = 0;
    p2_bad = 0; p2_end = -1; p2_start = p;
    p2_endcol = col[p] ^ (remaining & 1);

    g[p] = 1;                                  // p 已经走过了，但在路径上还欠一条出边
    for (int c = 0; c < N; ++c) if (g[c]) { dsu2[c] = c; ppush2(c); }
    while (q2h != q2t && !p2_bad) {
        int c = pq2[q2h++];
        if (q2h == N + 1) q2h = 0;
        inq2[c] = 0;
        pprocess2(c);
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

    ++seen_id;
    int top = 0, found = 1;
    fstack[top++] = srcbuf[0]; seen[srcbuf[0]] = seen_id;
    while (top) {
        int q = fstack[--top];
        for (int d = 0; d < 4; ++d) {
            int n = q + delta[d];
            if (g[n] && seen[n] != seen_id) {
                seen[n] = seen_id;
                if (srcmark[n] == src_id && ++found == nsrc) return 1;
                fstack[top++] = n;
            }
        }
    }
    return 0;
}

static int dfs(int p, int remaining, int depth) {
    if (remaining < best_rem) best_rem = remaining;
    if (remaining == 0) return 1;
    if (nodes++ >= node_limit) return -1;

    struct move { int dir, endp, len, score; } cand[4];
    int nc = 0;

    for (int d = 0; d < 4; ++d) {
        int dd = delta[d];
        if (!g[p + dd]) continue;
        int c = p, len = 0;
        while (g[c + dd]) { c += dd; ++len; }              // 先只量出滑到哪，不动盘面
        if (!estate_ok(p, d, dd, len, c)) continue;        // 传播定死的边直接否掉这个方向
        for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
        int rem2 = remaining - len;
        if (rem2 == 0 || (cheap_ok(c, rem2) && reach_local(p + dd, dd, len, c, rem2)
                          && (depth >= prop_depth || propagate_dyn(c, rem2)))) {
            cand[nc].dir = d; cand[nc].endp = c; cand[nc].len = len;
            cand[nc].score = freedeg(c);
            ++nc;
        }
        for (int i = 0, back = c; i < len; ++i, back -= dd) unmark(back);
    }

    for (int i = 1; i < nc; ++i)
        for (int j = i; j > 0 && cand[j].score < cand[j - 1].score; --j) {
            struct move t = cand[j]; cand[j] = cand[j - 1]; cand[j - 1] = t;
        }

    for (int i = 0; i < nc; ++i) {
        int dd = delta[cand[i].dir], len = cand[i].len, c = cand[i].endp;
        for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
        path[path_len++] = DCH[cand[i].dir];
        int r = dfs(c, remaining - len, depth + 1);
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
    bdisc = malloc(sizeof(int) * (size_t)N); blow = malloc(sizeof(int) * (size_t)N);
    bstk_v = malloc(sizeof(int) * (size_t)(N + 4)); bstk_pd = malloc(sizeof(int) * (size_t)(N + 4));
    bstk_di = malloc(sizeof(int) * (size_t)(N + 4));
    if (getenv("BRIDGE")) use_bridge = atoi(getenv("BRIDGE"));
    pin_in = malloc((size_t)N); memset(pin_in, -1, (size_t)N);
    pin_out = malloc((size_t)N); memset(pin_out, -1, (size_t)N);
    if (getenv("PIN")) use_pin = atoi(getenv("PIN"));
    chain_id = malloc(sizeof(int) * (size_t)N);
    chain_pos = malloc(sizeof(int) * (size_t)N);
    seqbuf = malloc(sizeof(int) * (size_t)N);
    estate2 = malloc((size_t)N * 4);
    dsu2 = malloc(sizeof(int) * (size_t)N);
    pq2 = malloc(sizeof(int) * (size_t)(N + 1));
    inq2 = malloc((size_t)N);
    bk_estate = malloc((size_t)N * 4);
    bk_dsu = malloc(sizeof(int) * (size_t)N);
    bk_inq = malloc((size_t)N);
    probe_rounds = getenv("PROBEROUNDS") ? atoi(getenv("PROBEROUNDS")) : 2;
    probe_all = getenv("PROBEALL") ? atoi(getenv("PROBEALL")) : 1;
    prop_depth = getenv("PROPDEPTH") ? atoi(getenv("PROPDEPTH")) : 2;

    // ---- --verify <解文件>：拿已知真解逐步验证两层传播的 soundness ----
    // 沿真解走一遍，每个滑行落点上跑一次传播。真解是可行的，所以传播**永远不许报矛盾**，
    // 静态那层的 estate_ok 也**永远不许否掉真解实际走的那个方向**。
    // 任何一次报警都说明规则本身是错的（而不是「运气不好」），必须先修再谈性能。
    if (argc > 3 && !strcmp(argv[2], "--verify")) {
        FILE *sf = fopen(argv[3], "r");
        if (!sf) { fprintf(stderr, "打不开 %s\n", argv[3]); return 1; }
        static char sb[1 << 22];
        size_t sn = fread(sb, 1, sizeof sb - 1, sf); sb[sn] = 0; fclose(sf);
        int sx = atoi(strstr(sb, "x=") + 2), sy = atoi(strstr(sb, "y=") + 2);
        const char *ps = strstr(sb, "path=") + 5;
        int s = (sy + 1) * W + (sx + 1);

        printf("真起点 (%d,%d), 自由格 %d\n", sx, sy, total_free);
        int pok = propagate(s);
        printf("静态传播: %s\n", pok ? "无矛盾 (对)" : "**矛盾 —— 静态层不 sound**");
        {   // 传播产出统计：判定了多少条边，其中多少条是桥规则贡献的
            long long nedge = 0, ndet = 0, nuse = 0, nban = 0;
            for (int c = 0; c < N; ++c) if (g0[c])
                for (int d = 2; d < 4; ++d) if (g0[c + delta[d]]) {   // 只数 R/D 免得重复
                    ++nedge;
                    if (estate[c * 4 + d] == 1) { ++ndet; ++nuse; }
                    else if (estate[c * 4 + d] == 2) { ++ndet; ++nban; }
                }
            printf("边 %lld，判定 %lld (%.1f%%)：必用 %lld，禁用 %lld；其中桥规则判出 %lld 条\n",
                   nedge, ndet, 100.0 * ndet / (nedge ? nedge : 1), nuse, nban, bridges_found);
            printf("链定向钉死进入方向 %lld / %d 格 (%.1f%%)\n",
                   pins_set, total_free, 100.0 * pins_set / total_free);
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
            if (!propagate_dyn(cur, rem)) {
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


    int *starts = malloc(sizeof(int) * (size_t)total_free);
    int ns = 0;
    memcpy(g, g0, N);
    for (int c = 0; c < N; ++c) if (g0[c]) starts[ns++] = c;
    for (int i = 1; i < ns; ++i) {
        int v = starts[i], dv = freedeg(v), j = i;
        while (j > 0 && freedeg(starts[j - 1]) > dv) { starts[j] = starts[j - 1]; --j; }
        starts[j] = v;
    }

    // ---- 分叉：起点分片，各干各的 ----
    nshard = getenv("JOBS") ? atoi(getenv("JOBS")) : (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (nshard < 1) nshard = 1;
    if (nshard > ns) nshard = ns;
    int *pfd = malloc(sizeof(int) * (size_t)nshard * 2);
    pid_t *kids = malloc(sizeof(pid_t) * (size_t)nshard);
    if (nshard > 1) {
        for (int k = 0; k < nshard; ++k) {
            if (pipe(&pfd[k * 2]) != 0) { nshard = 1; break; }
        }
    }
    if (nshard > 1) {
        for (int k = 0; k < nshard; ++k) {
            pid_t pid = fork();
            if (pid == 0) {                                 // 子进程
                // 管道在 fork 之前就全建好了，这个孩子继承了「所有」管道的两端。
                // 不属于自己的必须全关，否则别人的写端被我攥着，父进程永远收不到那条管道的 EOF。
                for (int j = 0; j < nshard; ++j) {
                    close(pfd[j * 2]);
                    if (j != k) close(pfd[j * 2 + 1]);
                }
                shard = k; out_fd = pfd[k * 2 + 1];
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
    long long probe = getenv("PROBE") ? atoll(getenv("PROBE")) : 8000;
    long long probe2 = getenv("PROBE2") ? atoll(getenv("PROBE2")) : 24000;
    int *sc = malloc(sizeof(int) * (size_t)ns);
    unsigned char **est_save = malloc(sizeof(unsigned char *) * (size_t)ns);
    int keep = 0;

    for (int i = 0; i < ns; ++i) {
        if (i % nshard != shard) continue;                 // 只做自己这一片
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
        int r = dfs(s, total_free - 1, 0);
        if (r == 1) { path[path_len] = 0; emit(s, path); }
        if (r == 0) continue;                              // 搜穷仍无解 => 永久剔除
        starts[keep] = s; sc[keep] = best_rem;
        est_save[keep] = malloc((size_t)N * 4);
        memcpy(est_save[keep], estate, (size_t)N * 4);      // 缓存，后两层直接复用
        ++keep;
    }

    if (!getenv("NORANK")) for (int i = 1; i < keep; ++i) {
        int vs = starts[i], vc = sc[i], j2 = i;
        unsigned char *ve = est_save[i];
        while (j2 > 0 && sc[j2 - 1] > vc) {
            starts[j2] = starts[j2 - 1]; sc[j2] = sc[j2 - 1]; est_save[j2] = est_save[j2 - 1]; --j2;
        }
        starts[j2] = vs; sc[j2] = vc; est_save[j2] = ve;
    }

    // 只在**需要**的时候才上第二层。
    // v21 无差别地给每一关都加一层大预算筛选，结果是「只赚尾部、赔了中盘」
    // （L239 342s→44s，但 L213 1.5s→23.9s，总分 237→221）——因为那层钱摊在了不需要它的关上。
    // 判据用**第一层的存活率**：存活率高就说明 PROBE 这个预算在这一关根本搜不穷子树、
    // 证伪不动，那才值得加钱；存活率低说明预算已经够用，直接进无限搜。
    int mine_total = 0;
    for (int i = 0; i < ns; ++i) if (i % nshard == shard) ++mine_total;
    int thresh = getenv("TIER2") ? atoi(getenv("TIER2")) : 30;   // 存活率(%)超过它才上第二层
    int use_tier2 = (mine_total > 0 && keep * 100 > mine_total * thresh);
    fprintf(stderr, "shard %d: 第一层存活 %d/%d = %d%%%%, 第二层 %s\n",
            shard, keep, mine_total, mine_total ? keep * 100 / mine_total : 0,
            use_tier2 ? "开" : "跳过");
    if (use_tier2)
    // 第二层：大预算再筛一遍，estate 直接复用，probing 不重算
    {
    int keep2 = 0;
    for (int i = 0; i < keep; ++i) {
        int s = starts[i];
        memcpy(estate, est_save[i], (size_t)N * 4);
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = 0; low_cnt = zero_cnt = 0;
        for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
        for (int c = 0; c < N; ++c) if (g[c]) { deg[c] = freedeg(c);
            if (deg[c] <= 1) ++low_cnt; if (deg[c] == 0) ++zero_cnt; }
        mark(s);
        path_len = 0; nodes = 0; node_limit = probe2; best_rem = total_free;
        int r = dfs(s, total_free - 1, 0);
        if (r == 1) { path[path_len] = 0; emit(s, path); }
        if (r == 0) { free(est_save[i]); continue; }
        starts[keep2] = s; sc[keep2] = best_rem; est_save[keep2] = est_save[i]; ++keep2;
    }
    if (!getenv("NORANK")) for (int i = 1; i < keep2; ++i) {
        int vs = starts[i], vc = sc[i], j2 = i;
        unsigned char *ve = est_save[i];
        while (j2 > 0 && sc[j2 - 1] > vc) {
            starts[j2] = starts[j2 - 1]; sc[j2] = sc[j2 - 1]; est_save[j2] = est_save[j2 - 1]; --j2;
        }
        starts[j2] = vs; sc[j2] = vc; est_save[j2] = ve;
    }
    keep = keep2;
    }
    fprintf(stderr, "probe: %d/%d 起点被证伪，剩 %d\n", ns - keep, ns, keep);

    // ---- 第三层：按顺序正式搜 ----
    node_limit = (long long)1 << 62;
    for (int i = 0; i < keep; ++i) {
        int s = starts[i];
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = 0; low_cnt = zero_cnt = 0;
        for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
        for (int c = 0; c < N; ++c) if (g[c]) {
            deg[c] = freedeg(c);
            if (deg[c] <= 1) ++low_cnt;
            if (deg[c] == 0) ++zero_cnt;
        }
        memcpy(estate, est_save[i], (size_t)N * 4);   // 复用第一层缓存好的 estate，不重算
        mark(s);
        // 归纳的底座：局部检查靠「上一层的剩余区域连通」，起点这一层得自己验。
        // 起点若是割点，去掉它剩余区域就断了。顺带白捡一条起点剪枝。
        if (!reach_ok(s, total_free - 1)) continue;

        path_len = 0; nodes = 0;
        if (dfs(s, total_free - 1, 0) == 1) { path[path_len] = 0; emit(s, path); }
    }
    fprintf(stderr, "no solution found\n");
    return 1;
}
