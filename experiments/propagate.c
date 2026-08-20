// 约束传播可行性实验 —— 重做 v9，这次找到了能启动传播的种子
//
// v9 当年空转的真正原因不是「环模型 vs 路径模型」，而是**传播根本没有起爆点**：
// 度 2 规则要「这一格恰好用 2 条边」才能锁死两条边，可路径有两个端点只用 1 条边，
// 而端点是谁不知道，于是每个格子都得留「我可能是端点」的活口，谁也锁不死谁。
//
// 种子在这里：**路径在棋盘两色间严格交替，所以起点一旦假定，终点的颜色就被完全确定**
//   · 自由格数 n 为偶数 => 起点终点异色 => 终点色 = 1 - col(起点)
//   · n 为奇数         => 起点终点同色 => 终点色 = col(起点)
// 于是**一半的格子当场被证明「不可能是端点」**，它们里面的度 2 格必须用满两条边，
// 传播就能起爆，再沿强制边连锁下去。而我们的求解器本来就在逐个枚举起点，这个假定是免费的。
//
// 本程序只测「哈密顿路径层」的传播（无向边 + 度约束 + 不成环），先看这个种子有多大威力：
//   · 对真起点：能定死多少条边？（顺便验证 soundness —— 真解用的边一条都不许被禁用）
//   · 对错起点：能证伪多少？（这是拿来替代 DFS 探针的关键指标）
// Coil 特有的滑行/时序约束（Tron 的有向「进入方向」）还没加，那是下一层。
//
// 用法:
//   propagate <关卡文件> [--sol <解文件>]      # 用解文件里的起点，做 soundness 检查
//   propagate <关卡文件> --all                 # 对所有候选起点跑，统计证伪率
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static int W, H, N;
static unsigned char *g0;          // 1 = 自由格
static unsigned char *col;
static int delta[4];               // L U R D
static int total_free;

// 边状态：estate[c*4+d]  0=未定 1=必用 2=禁用。与 estate[(c+delta[d])*4 + (d^2)] 保持同步。
static unsigned char *estate;
static int *dsu;
static int *queue_, qhead, qtail;
static unsigned char *inq;
static int bad;                    // 传播出矛盾
static int start_cell, end_col, forced_end;
// 终点只有一个。凡是已经挂满 2 条边的格子都不可能是终点——把它们从候选里划掉，
// 候选一旦收缩到 1，终点就定了，终点色那一整半格子立刻从「可能只挂 1 条」塌成「必须挂 2 条」。
// 这一步是偶数关能不能起爆的关键：偶数关里起点终点异色，「假定起点」是把一格放松，
// 放松永远推不出矛盾；真正的收紧只能靠把终点候选逼死。
static unsigned char *not_end;
static int end_cand;

static int find(int x) { while (dsu[x] != x) { dsu[x] = dsu[dsu[x]]; x = dsu[x]; } return x; }
static void unite(int a, int b) { dsu[find(a)] = find(b); }

static void push(int c) {
    if (inq[c]) return;
    inq[c] = 1; queue_[qtail++] = c;
    if (qtail == N + 1) qtail = 0;
}
static int pop(void) {
    int c = queue_[qhead++];
    if (qhead == N + 1) qhead = 0;
    inq[c] = 0;
    return c;
}

static void set_edge(int c, int d, int v) {
    int n = c + delta[d];
    unsigned char *a = &estate[c * 4 + d], *b = &estate[n * 4 + (d ^ 2)];
    if (*a == v) return;
    if (*a != 0) { bad = 1; return; }
    if (v == 1) {
        if (find(c) == find(n)) { bad = 1; return; }   // 会闭成环，而 Coil 的解是路径
        unite(c, n);
    }
    *a = *b = (unsigned char)v;
    push(c); push(n);
}

// 处理一格：算出它已用/待定的边数，套上「这一格该用几条边」的区间，能定就定
static void process(int c) {
    if (bad) return;

    // 先把「连上去就成环」的待定边禁掉
    for (int d = 0; d < 4; ++d) {
        int n = c + delta[d];
        if (!g0[n] || estate[c * 4 + d] != 0) continue;
        if (find(c) == find(n)) { set_edge(c, d, 2); if (bad) return; }
    }

    int used = 0, avail = 0;
    for (int d = 0; d < 4; ++d) {
        int n = c + delta[d];
        if (!g0[n]) continue;
        if (estate[c * 4 + d] == 1) ++used;
        else if (estate[c * 4 + d] == 0) ++avail;
    }

    // 这一格允许用几条边
    int lo, hi;
    if (c == start_cell)        lo = hi = 1;           // 起点只用 1 条
    else if (forced_end == c)   lo = hi = 1;           // 已确认的终点
    else if (forced_end >= 0)   lo = hi = 2;           // 终点已定在别处，这里必是中间格
    else if (col[c] == end_col && !not_end[c]) { lo = 1; hi = 2; }  // 还可能是终点
    else                        lo = hi = 2;           // 颜色不对，绝不可能是端点

    if (used > hi) { bad = 1; return; }
    if (used + avail < lo) { bad = 1; return; }

    if (used == hi && avail > 0)
        for (int d = 0; d < 4; ++d) {
            int n = c + delta[d];
            if (g0[n] && estate[c * 4 + d] == 0) { set_edge(c, d, 2); if (bad) return; }
        }
    else if (used + avail == lo && avail > 0)
        for (int d = 0; d < 4; ++d) {
            int n = c + delta[d];
            if (g0[n] && estate[c * 4 + d] == 0) { set_edge(c, d, 1); if (bad) return; }
        }

    // 挂满 2 条边 => 不可能是终点，从候选里划掉
    if (col[c] == end_col && c != start_cell && !not_end[c] && used == 2) {
        not_end[c] = 1;
        if (--end_cand == 0) { bad = 1; return; }         // 没格子能当终点了
        if (end_cand == 1 && forced_end < 0) {
            for (int q = 0; q < N; ++q)
                if (g0[q] && col[q] == end_col && q != start_cell && !not_end[q]) { forced_end = q; break; }
            for (int q = 0; q < N; ++q) if (g0[q] && col[q] == end_col) push(q);
        }
    }

    // 最多只能挂 1 条边的格子，只能是终点（起点已单列）
    if (c != start_cell && used + avail == 1 && hi == 2) {
        if (col[c] != end_col) { bad = 1; return; }
        if (forced_end >= 0 && forced_end != c) { bad = 1; return; }   // 终点只有一个
        if (forced_end < 0) {
            forced_end = c;
            for (int q = 0; q < N; ++q) if (g0[q] && col[q] == end_col) push(q);
        }
    }
}

static void run_queue(void) {
    while (qhead != qtail && !bad) process(pop());
}

// 起点的第一次滑行是在**空盘面**上做的，撞不到任何已访问格，所以必然一路滑到墙。
// 那整条线因此就是路径的开头，于是：线上每条边都要被用（撞上禁用边 => 这个方向不可能），
// 线上的中间格是直穿的（垂直边一条都不用 => 那里有必用边就矛盾）。
// 这是偶数关唯一能让「起点位置」真正收紧约束的入口——单说「起点只挂 1 条边」是**放松**，
// 放松永远推不出矛盾，这也正是偶数关证伪率一直是 0 的原因。
static int first_move_dir_ok(int s, int d) {
    int dd = delta[d];
    if (!g0[s + dd]) return 0;
    int c = s;
    while (g0[c + dd]) {
        if (estate[c * 4 + d] == 2) return 0;              // 线上有禁用边
        c += dd;
        if (g0[c + dd]) {                                  // c 是中间格，必须直穿
            int e1 = (d + 1) & 3, e2 = (d + 3) & 3;
            if (estate[c * 4 + e1] == 1 || estate[c * 4 + e2] == 1) return 0;
        }
    }
    return 1;
}

// 第一次滑行的方向若已唯一，把那整条线定死，回灌给传播再滚一轮
static int force_first_line(int s, int d) {
    int dd = delta[d], c = s, changed = 0;
    while (g0[c + dd]) {
        if (estate[c * 4 + d] != 1) { set_edge(c, d, 1); changed = 1; if (bad) return changed; }
        c += dd;
    }
    return changed;
}

static void propagate(int s) {
    memset(estate, 0, (size_t)N * 4);
    for (int c = 0; c < N; ++c) dsu[c] = c;
    memset(inq, 0, (size_t)N);
    memset(not_end, 0, (size_t)N);
    qhead = qtail = 0;
    bad = 0; forced_end = -1; start_cell = s;
    end_col = (total_free & 1) ? col[s] : (col[s] ^ 1);
    end_cand = 0;
    for (int c = 0; c < N; ++c) if (g0[c] && col[c] == end_col && c != s) ++end_cand;

    for (int c = 0; c < N; ++c) if (g0[c]) push(c);
    run_queue();

    for (int round = 0; round < 8 && !bad; ++round) {
        int nok = 0, okd = -1;
        for (int d = 0; d < 4; ++d) if (first_move_dir_ok(s, d)) { ++nok; okd = d; }
        if (nok == 0) { bad = 1; break; }                  // 四个方向都开不了局 => 起点证伪
        if (nok > 1) break;                                // 还有得选，推不动了
        if (!force_first_line(s, okd)) break;              // 没有新信息，稳定了
        run_queue();
    }
}

// ===== 有向层：必用边链的定向 =====
//
// 无向层只能告诉我们「这条边一定被用」，说不出走的方向。但必用边天然拼成一条条**链**
// （每格最多挂 2 条必用边），而**链一旦定向，链内所有格子的先后顺序就完全确定了**。
// 有了先后顺序，Coil 的滑行规则第一次能被判定：
//
//   在链上的某格拐弯 ⇒ 「前方那格」必须是墙，或者**更早**被访问过。
//   如果前方那格恰好也在这条链上、而且位置**更晚** —— 这个定向不可能，排除。
//
// 两个方向都被排除 ⇒ 整个假设（当前这个起点）矛盾 ⇒ **起点被证伪**。
// 只剩一个方向 ⇒ 链定向确定。
//
// 链越蜿蜒，擦到自己的次数越多，这条规则越强 —— 而 33% 墙密度的盘面上链就是蜿蜒的。
// 另外含起点的链方向本来就是确定的（起点只挂 1 条边，必是链端，且路径从它出发），
// 那种链直接就能查，不用二选一。
static int *chain_id, *chain_pos, *chain_len_of, *seqbuf;

// 检查链 seq[0..k-1] 按给定方向走是否与滑行规则相容。
// rev=0: seq[0] 更早; rev=1: seq[k-1] 更早。
// strict=1 用于**含起点的那条链**：它是路径的第一段，占据位置 0..k-1，
// 所以任何不在它上面的格子位置都 >= k，绝不可能比链上的拐弯点更早。
// 于是前方那格只剩两种合法可能：是墙，或者就在本链上且位置更早。
static int chain_dir_ok(const int *seq, int k, int rev, int cid, int strict) {
    for (int i = 1; i + 1 < k; ++i) {
        int prev = rev ? seq[i + 1] : seq[i - 1];
        int next = rev ? seq[i - 1] : seq[i + 1];
        if (next - seq[i] == seq[i] - prev) continue;      // 直穿，滑行规则没意见
        int f = seq[i] + (seq[i] - prev);                  // 拐弯时「前方」那一格
        if (!g0[f]) continue;                              // 前方是墙，拐弯天然合法
        if (chain_id[f] != cid) { if (strict) return 0; continue; }
        int tf = rev ? (k - 1 - chain_pos[f]) : chain_pos[f];
        int ti = rev ? (k - 1 - i) : i;
        if (tf > ti) return 0;                             // 前方那格更晚 => 这个方向不可能
    }
    return 1;
}

// 起点的第一次滑行是在空盘面上做的，撞不到任何已访问格，所以**必然一路滑到墙**。
// 含起点的链开头那一段因此必须是直线，直到撞墙才允许拐。
static int first_run_ok(const int *seq, int k) {
    if (k < 2) return 1;
    int e = seq[1] - seq[0];
    int limit = 1;
    while (g0[seq[0] + (limit + 1) * e]) ++limit;          // 从起点沿 e 能滑到第几格
    if (limit > k - 1) limit = k - 1;
    for (int i = 1; i < limit; ++i)
        if (seq[i + 1] - seq[i] != e) return 0;            // 没撞墙就拐了
    return 1;
}

// 返回 0 = 矛盾（这个起点被证伪）。oriented/total 回填被定死方向的链数。
static int orient_chains(int s, int *oriented, int *total) {
    for (int c = 0; c < N; ++c) chain_id[c] = -1;
    int nchain = 0, ndet = 0;

    for (int c0 = 0; c0 < N; ++c0) {
        if (!g0[c0] || chain_id[c0] >= 0) continue;
        int fdeg = 0;
        for (int d = 0; d < 4; ++d) if (g0[c0 + delta[d]] && estate[c0 * 4 + d] == 1) ++fdeg;
        if (fdeg != 1) continue;                           // 只从链端起步（环已被传播禁掉）

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
        if (k < 3) { ++nchain; continue; }                 // 太短，没有内部拐点可查

        int has_s = (chain_id[s] == nchain);
        int ok0 = chain_dir_ok(seqbuf, k, 0, nchain, has_s && seqbuf[0] == s);
        int ok1 = chain_dir_ok(seqbuf, k, 1, nchain, has_s && seqbuf[k - 1] == s);
        // 含起点的链方向是硬的：起点只挂 1 条边，必是链端，且路径从它出发
        if (seqbuf[0] == s)     ok1 = 0;
        if (seqbuf[k - 1] == s) ok0 = 0;
        if (has_s) {
            int rev[4096];
            const int *sq = seqbuf;
            if (seqbuf[k - 1] == s) {                      // 让 s 排在序列开头再查第一段
                int kk = k < 4096 ? k : 4096;
                for (int t = 0; t < kk; ++t) rev[t] = seqbuf[k - 1 - t];
                sq = rev;
                if (!first_run_ok(sq, kk)) return 0;
            } else if (!first_run_ok(sq, k)) return 0;
        }
        if (!ok0 && !ok1) return 0;                        // 两个方向都走不通 => 矛盾
        if (!ok0 && !ok1) return 0;                        // 两个方向都走不通 => 矛盾
        if (ok0 != ok1) ++ndet;
        ++nchain;
    }
    *oriented = ndet; *total = nchain;
    return 1;
}


// ---- 关卡 / 解 的读入 ----
static char *slurp(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "打不开 %s\n", path); exit(1); }
    size_t cap = 1 << 20, n = 0;
    char *b = malloc(cap);
    for (;;) {
        if (n + 65536 > cap) b = realloc(b, cap *= 2);
        size_t r = fread(b + n, 1, 65536, f);
        if (!r) break;
        n += r;
    }
    b[n] = 0; fclose(f);
    return b;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "用法: propagate <关卡> [--sol <解>] [--all]\n"); return 1; }
    const char *solpath = NULL;
    int do_all = 0;
    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--sol") && i + 1 < argc) solpath = argv[++i];
        else if (!strcmp(argv[i], "--all")) do_all = 1;
    }

    char *buf = slurp(argv[1]);
    int w = atoi(strstr(buf, "x=") + 2), h = atoi(strstr(buf, "y=") + 2);
    const char *pb = strstr(buf, "board=") + 6;

    W = w + 2; H = h + 2; N = W * H;
    g0 = calloc(N, 1); col = calloc(N, 1);
    delta[0] = -1; delta[1] = -W; delta[2] = 1; delta[3] = W;
    for (int c = 0; c < N; ++c) col[c] = (unsigned char)(((c % W) + (c / W)) & 1);
    total_free = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (pb[(size_t)y * w + x] == '.') { g0[(y + 1) * W + (x + 1)] = 1; ++total_free; }

    estate = malloc((size_t)N * 4);
    dsu = malloc(sizeof(int) * (size_t)N);
    queue_ = malloc(sizeof(int) * (size_t)(N + 1));
    inq = malloc((size_t)N);
    not_end = malloc((size_t)N);
    chain_id = malloc(sizeof(int) * (size_t)N);
    chain_pos = malloc(sizeof(int) * (size_t)N);
    chain_len_of = malloc(sizeof(int) * (size_t)N);
    seqbuf = malloc(sizeof(int) * (size_t)N);

    int free_edges = 0;
    for (int c = 0; c < N; ++c) if (g0[c])
        for (int d = 0; d < 4; ++d) if (g0[c + delta[d]]) ++free_edges;
    free_edges /= 2;

    printf("%s: %dx%d, 自由格 %d (%s), 网格边 %d\n", argv[1], w, h, total_free,
           (total_free & 1) ? "奇" : "偶", free_edges);

    // ---- 用真解跑：soundness 检查 + 定死率 ----
    if (solpath) {
        char *sb = slurp(solpath);
        int sx = atoi(strstr(sb, "x=") + 2), sy = atoi(strstr(sb, "y=") + 2);
        const char *ps = strstr(sb, "path=") + 5;
        int s = (sy + 1) * W + (sx + 1);

        // 沿真解走一遍，记下它真正用到的边
        unsigned char *truth = calloc((size_t)N * 4, 1);
        unsigned char *vis = calloc(N, 1);
        int cur = s, nvis = 1;
        vis[cur] = 1;
        for (const char *p = ps; *p && *p != '\n'; ++p) {
            int d = (*p == 'L') ? 0 : (*p == 'U') ? 1 : (*p == 'R') ? 2 : 3;
            int dd = delta[d];
            while (g0[cur + dd] && !vis[cur + dd]) {
                truth[cur * 4 + d] = 1; truth[(cur + dd) * 4 + (d ^ 2)] = 1;
                cur += dd; vis[cur] = 1; ++nvis;
            }
        }
        printf("  真解: 起点 (%d,%d), 覆盖 %d/%d %s\n", sx, sy, nvis, total_free,
               nvis == total_free ? "" : "  <-- 解不完整！");

        propagate(s);

        int forced = 0, banned = 0, undecided = 0, unsound = 0;
        for (int c = 0; c < N; ++c) if (g0[c])
            for (int d = 0; d < 4; ++d) {
                if (!g0[c + delta[d]]) continue;
                unsigned char e = estate[c * 4 + d];
                if (e == 1) { ++forced; if (!truth[c * 4 + d]) ++unsound; }
                else if (e == 2) { ++banned; if (truth[c * 4 + d]) ++unsound; }
                else ++undecided;
            }
        printf("  传播(真起点): %s | 必用 %d (%.1f%%), 禁用 %d, 未定 %d\n",
               bad ? "**矛盾**" : "无矛盾",
               forced / 2, 100.0 * (forced / 2) / free_edges, banned / 2, undecided / 2);
        printf("  soundness: %s (与真解冲突的边 %d 条)\n", unsound ? "**不成立**" : "OK", unsound / 2);
        {
            int od = 0, tot = 0;
            int ok = orient_chains(s, &od, &tot);
            printf("  链定向: %d 条链, 方向被定死 %d 条 (%.0f%%), %s\n", tot, od,
                   tot ? 100.0 * od / tot : 0.0, ok ? "无矛盾" : "**矛盾 —— 对真起点不该发生**");
        }
        if (forced_end >= 0)
            printf("  传播还直接定出了终点: (%d,%d)\n", forced_end % W - 1, forced_end / W - 1);
    }

    // ---- 对所有候选起点跑：证伪率 ----
    if (do_all) {
        int need_col = -1;
        if (total_free & 1) {
            int c0 = 0, c1 = 0;
            for (int c = 0; c < N; ++c) if (g0[c]) { if (col[c]) ++c1; else ++c0; }
            need_col = (c0 > c1) ? 0 : 1;
        }
        int cand = 0, killed = 0, killed_chain = 0;
        clock_t t0 = clock();
        for (int c = 0; c < N; ++c) {
            if (!g0[c]) continue;
            if (need_col >= 0 && col[c] != need_col) continue;
            ++cand;
            propagate(c);
            if (bad) { ++killed; continue; }
            { int od, tot; if (!orient_chains(c, &od, &tot)) { ++killed; ++killed_chain; } }
        }
        double el = (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("  全起点传播: %d 个候选, 证伪 %d (%.1f%%) [其中链定向贡献 %d], 剩 %d, 用时 %.2fs (%.0f 起点/秒)\n",
               cand, killed, 100.0 * killed / cand, killed_chain, cand - killed, el, cand / (el > 0 ? el : 1e-9));
    }
    return 0;
}
