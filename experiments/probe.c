// 全局 probing 实验 —— 不假定起点，靠 probing 自己造种子
//
// 背景：v17~v19 的传播都要「先假定一个起点」才能起爆，因为度 2 规则要「这一格恰好用 2 条边」
// 才能锁死，而路径的两个端点只用 1 条、端点是谁又不知道。但那条路撞了一堵墙：
// 自由格数 n 为偶数时起点和终点异色，「假定起点」是把一格从 [2,2] **放松**成 [1,1]，
// 放松永远推不出矛盾 —— 偶数关证伪率因此只有 0.4%（奇数关 58%）。
//
// Tron 说的 recursive patterns + bounded brute force，本质是 **probing / failed literal**：
// 假设某条边被用（或不被用），传播，矛盾就把这个假设消掉，拿结果再滚一轮到不动点。
// **probing 自己造种子，不需要起点当锚**，而且只跑一次，结果对所有起点共享
// （每个起点跑一次就是 O(n^3)，付不起）。
//
// 不假定起点的基础模型靠「端点数」的基数约束撑住：
//   · 路径两色交替 => 设两色自由格数 cntA/cntB，|cntA-cntB|>1 直接无解；
//   · cntA = cntB+1 => 两个端点都在多数色上，**少数色的格子全部 [2,2]**（这就是奇数关的免费种子）；
//   · cntA = cntB   => 两色各恰好一个端点。
// 再把基数约束做成传播：某格挂满 2 条边 => 它不是端点，该色候选数 -1；
// 某色的端点名额用完 => 该色其余格子全部塌成 [2,2]；候选数不够填名额 => 矛盾。
//
// 用法:
//   probe <关卡> [--sol <解>]   # 跑全局传播 + probing，报告定死率；给了解则做 soundness 检查
//   probe <关卡> --rounds N     # 限制 probing 轮数
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static int W, H, N;
static unsigned char *g0, *col;
static int delta[4];
static int total_free;

static unsigned char *estate;      // 0 未定 1 必用 2 禁用
static int *dsu;
static int *pq;
static unsigned char *inq;
static int qhead, qtail;
static int bad;

static unsigned char *is_ep, *not_ep;   // 已确定是端点 / 已确定不是端点
static int ep_cap[2], ep_used[2], ep_cand[2];

static int dfind(int x) { while (dsu[x] != x) { dsu[x] = dsu[dsu[x]]; x = dsu[x]; } return x; }

static void push(int c) {
    if (inq[c]) return;
    inq[c] = 1; pq[qtail++] = c;
    if (qtail == N + 1) qtail = 0;
}

static void set_edge(int c, int d, int v) {
    int n = c + delta[d];
    unsigned char *a = &estate[c * 4 + d], *b = &estate[n * 4 + (d ^ 2)];
    if (*a == v) return;
    if (*a != 0) { bad = 1; return; }
    if (v == 1) {
        int ra = dfind(c), rb = dfind(n);
        if (ra == rb) { bad = 1; return; }      // 闭成环，而 Coil 的解是路径
        dsu[ra] = rb;
    }
    *a = *b = (unsigned char)v;
    push(c); push(n);
}

static void push_color(int k) {
    for (int q = 0; q < N; ++q) if (g0[q] && col[q] == k) push(q);
}

// 某格被判定为端点 / 非端点，走基数约束
static void mark_endpoint(int c) {
    if (is_ep[c]) return;
    if (not_ep[c]) { bad = 1; return; }
    int k = col[c];
    is_ep[c] = 1;
    if (++ep_used[k] > ep_cap[k]) { bad = 1; return; }
    if (ep_used[k] == ep_cap[k]) push_color(k);     // 名额用完，该色其余全塌成 [2,2]
    push(c);
}

static void mark_not_endpoint(int c) {
    if (not_ep[c]) return;
    if (is_ep[c]) { bad = 1; return; }
    int k = col[c];
    not_ep[c] = 1;
    --ep_cand[k];
    if (ep_cand[k] < ep_cap[k] - ep_used[k]) { bad = 1; return; }   // 候选不够填名额
    if (ep_cand[k] == ep_cap[k] - ep_used[k]) {                     // 剩下的候选只能全是端点
        for (int q = 0; q < N; ++q)
            if (g0[q] && col[q] == k && !not_ep[q] && !is_ep[q]) { mark_endpoint(q); if (bad) return; }
    }
    push(c);
}

static void process(int c) {
    if (bad) return;

    for (int d = 0; d < 4; ++d) {                  // 连上去就成环的待定边先禁掉
        int n = c + delta[d];
        if (!g0[n] || estate[c * 4 + d] != 0) continue;
        if (dfind(c) == dfind(n)) { set_edge(c, d, 2); if (bad) return; }
    }

    int used = 0, avail = 0;
    for (int d = 0; d < 4; ++d) {
        if (!g0[c + delta[d]]) continue;
        if (estate[c * 4 + d] == 1) ++used;
        else if (estate[c * 4 + d] == 0) ++avail;
    }

    int k = col[c];
    int lo, hi;
    if (is_ep[c])                         lo = hi = 1;
    else if (not_ep[c] || ep_used[k] == ep_cap[k]) lo = hi = 2;
    else                                  { lo = 1; hi = 2; }

    if (used > hi) { bad = 1; return; }
    if (used + avail < lo) { bad = 1; return; }

    if (used == hi && avail > 0) {
        for (int d = 0; d < 4; ++d)
            if (g0[c + delta[d]] && estate[c * 4 + d] == 0) { set_edge(c, d, 2); if (bad) return; }
    } else if (used + avail == lo && avail > 0) {
        for (int d = 0; d < 4; ++d)
            if (g0[c + delta[d]] && estate[c * 4 + d] == 0) { set_edge(c, d, 1); if (bad) return; }
    }

    if (used == 2 && !not_ep[c]) { mark_not_endpoint(c); if (bad) return; }      // 挂满 2 条 => 不是端点
    if (used + avail == 1 && !is_ep[c]) { mark_endpoint(c); if (bad) return; }   // 最多挂 1 条 => 是端点
}

static void run_queue(void) {
    while (qhead != qtail && !bad) {
        int c = pq[qhead++];
        if (qhead == N + 1) qhead = 0;
        inq[c] = 0;
        process(c);
    }
}

static void base_init(void) {
    memset(estate, 0, (size_t)N * 4);
    memset(inq, 0, (size_t)N);
    memset(is_ep, 0, (size_t)N);
    memset(not_ep, 0, (size_t)N);
    for (int c = 0; c < N; ++c) dsu[c] = c;
    qhead = qtail = 0; bad = 0;

    int cnt[2] = {0, 0};
    for (int c = 0; c < N; ++c) if (g0[c]) ++cnt[col[c]];
    ep_cand[0] = cnt[0]; ep_cand[1] = cnt[1];
    ep_used[0] = ep_used[1] = 0;
    int diff = cnt[0] - cnt[1];
    if (diff > 1 || diff < -1) { bad = 1; return; }        // 两色差超过 1 => 无哈密顿路径
    if (diff == 0)      { ep_cap[0] = 1; ep_cap[1] = 1; }  // 两端点异色
    else if (diff == 1) { ep_cap[0] = 2; ep_cap[1] = 0; }  // 都在多数色上
    else                { ep_cap[0] = 0; ep_cap[1] = 2; }

    for (int c = 0; c < N; ++c) if (g0[c]) push(c);
    run_queue();
}

// ---- probing：对每条未定边试「必用」和「禁用」，哪边一试就矛盾，另一边就是定论 ----
static unsigned char *bk_estate, *bk_isep, *bk_notep, *bk_inq;
static int *bk_dsu;
static int bk_epu[2], bk_epc[2];

static void snapshot(void) {
    memcpy(bk_estate, estate, (size_t)N * 4);
    memcpy(bk_dsu, dsu, sizeof(int) * (size_t)N);
    memcpy(bk_isep, is_ep, (size_t)N);
    memcpy(bk_notep, not_ep, (size_t)N);
    memcpy(bk_inq, inq, (size_t)N);
    bk_epu[0] = ep_used[0]; bk_epu[1] = ep_used[1];
    bk_epc[0] = ep_cand[0]; bk_epc[1] = ep_cand[1];
}
static void restore(void) {
    memcpy(estate, bk_estate, (size_t)N * 4);
    memcpy(dsu, bk_dsu, sizeof(int) * (size_t)N);
    memcpy(is_ep, bk_isep, (size_t)N);
    memcpy(not_ep, bk_notep, (size_t)N);
    memcpy(inq, bk_inq, (size_t)N);
    ep_used[0] = bk_epu[0]; ep_used[1] = bk_epu[1];
    ep_cand[0] = bk_epc[0]; ep_cand[1] = bk_epc[1];
    qhead = qtail = 0; bad = 0;
}

// 试一个假设，返回 1 = 这个假设导致矛盾
static int try_assume(int c, int d, int v) {
    snapshot();
    set_edge(c, d, v);
    run_queue();
    int r = bad;
    restore();
    return r;
}

static long long probe_calls;

static int probing(int max_rounds) {
    for (int round = 0; round < max_rounds; ++round) {
        int changed = 0;
        for (int c = 0; c < N && !bad; ++c) {
            if (!g0[c]) continue;
            for (int d = 2; d < 4; ++d) {                  // 只走 R/D，每条边算一次
                int n = c + delta[d];
                if (!g0[n] || estate[c * 4 + d] != 0) continue;
                probe_calls += 2;
                if (try_assume(c, d, 1)) {                 // 假设「必用」矛盾 => 只能禁用
                    set_edge(c, d, 2); run_queue(); changed = 1;
                    if (bad) return 0;
                    continue;
                }
                if (try_assume(c, d, 2)) {                 // 假设「禁用」矛盾 => 只能必用
                    set_edge(c, d, 1); run_queue(); changed = 1;
                    if (bad) return 0;
                }
            }
        }
        if (!changed) break;
    }
    return !bad;
}

// ===== 有向层：必用边链的定向（从 propagate.c 搬来）=====
// probing 把必用边从 25% 推到 35%，链因此更长——链越长，链内可判先后的拐点越多。
static int *chain_id, *chain_pos, *chain_len_of, *seqbuf;
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

static void report(const char *tag, unsigned char *truth) {
    int forced = 0, banned = 0, undec = 0, unsound = 0;
    for (int c = 0; c < N; ++c) if (g0[c])
        for (int d = 0; d < 4; ++d) {
            if (!g0[c + delta[d]]) continue;
            unsigned char e = estate[c * 4 + d];
            if (e == 1) { ++forced; if (truth && !truth[c * 4 + d]) ++unsound; }
            else if (e == 2) { ++banned; if (truth && truth[c * 4 + d]) ++unsound; }
            else ++undec;
        }
    int tot = (forced + banned + undec) / 2;
    printf("  %s: 必用 %d (%.1f%%), 禁用 %d (%.1f%%), 未定 %d (%.1f%%)%s",
           tag, forced / 2, 100.0 * forced / 2 / tot, banned / 2, 100.0 * banned / 2 / tot,
           undec / 2, 100.0 * undec / 2 / tot, bad ? "  **矛盾**" : "");
    if (truth) printf("  soundness: %s", unsound ? "**不成立**" : "OK");
    printf("\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "用法: probe <关卡> [--sol <解>] [--rounds N]\n"); return 1; }
    const char *solpath = NULL;
    int rounds = 4;
    int assume = 0;   // 用解文件里的真起点做「假定起点」，看更紧的基础上 probing 能推多远
    int do_all = 0;   // 对每个候选起点跑「假定起点 + probing」，量证伪率
    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--sol") && i + 1 < argc) solpath = argv[++i];
        else if (!strcmp(argv[i], "--rounds") && i + 1 < argc) rounds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--assume")) assume = 1;
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

    estate = malloc((size_t)N * 4); dsu = malloc(sizeof(int) * (size_t)N);
    pq = malloc(sizeof(int) * (size_t)(N + 1)); inq = malloc((size_t)N);
    is_ep = malloc((size_t)N); not_ep = malloc((size_t)N);
    chain_id = malloc(sizeof(int)*(size_t)N); chain_pos = malloc(sizeof(int)*(size_t)N);
    seqbuf = malloc(sizeof(int)*(size_t)N);
    bk_estate = malloc((size_t)N * 4); bk_dsu = malloc(sizeof(int) * (size_t)N);
    bk_isep = malloc((size_t)N); bk_notep = malloc((size_t)N); bk_inq = malloc((size_t)N);

    unsigned char *truth = NULL;
    int start_cell = -1;
    if (solpath) {
        char *sb = slurp(solpath);
        int sx = atoi(strstr(sb, "x=") + 2), sy = atoi(strstr(sb, "y=") + 2);
        const char *ps = strstr(sb, "path=") + 5;
        truth = calloc((size_t)N * 4, 1);
        unsigned char *vis = calloc(N, 1);
        int cur = (sy + 1) * W + (sx + 1);
        start_cell = cur;
        vis[cur] = 1;
        for (const char *p = ps; *p && *p != '\n' && *p != '\r'; ++p) {
            int d = (*p == 'L') ? 0 : (*p == 'U') ? 1 : (*p == 'R') ? 2 : 3;
            int dd = delta[d];
            while (g0[cur + dd] && !vis[cur + dd]) {
                truth[cur * 4 + d] = 1; truth[(cur + dd) * 4 + (d ^ 2)] = 1;
                cur += dd; vis[cur] = 1;
            }
        }
    }

    printf("%s: %dx%d, 自由格 %d (%s)\n", argv[1], w, h, total_free,
           (total_free & 1) ? "奇" : "偶");

    if (do_all) {
        int need_col = -1;
        {   int cnt[2] = {0,0};
            for (int c = 0; c < N; ++c) if (g0[c]) ++cnt[col[c]];
            if (cnt[0] != cnt[1]) need_col = cnt[0] > cnt[1] ? 0 : 1;   // 奇数关：两端点必是多数色
        }
        int cand = 0, k_base = 0, k_probe = 0, k_chain = 0;
        clock_t tt = clock();
        for (int c = 0; c < N; ++c) {
            if (!g0[c]) continue;
            if (need_col >= 0 && col[c] != need_col) continue;
            ++cand;
            base_init();
            if (!bad) mark_endpoint(c);
            if (!bad) run_queue();
            if (bad) { ++k_base; continue; }
            probing(rounds);
            if (bad) { ++k_probe; continue; }
            { int od, tot; if (!orient_chains(c, &od, &tot)) ++k_chain; }
        }
        double el = (double)(clock() - tt) / CLOCKS_PER_SEC;
        printf("  全起点: %d 个候选, 光传播证伪 %d (%.1f%%), probing 再证伪 %d, 链定向再证伪 %d, 合计 %d (%.1f%%), 用时 %.1fs\n",
               cand, k_base, 100.0*k_base/cand, k_probe, k_chain, k_base+k_probe+k_chain,
               100.0*(k_base+k_probe+k_chain)/cand, el);
        return 0;
    }

    clock_t t0 = clock();
    base_init();
    double t_base = (double)(clock() - t0) / CLOCKS_PER_SEC;
    if (assume && start_cell >= 0) {
        // 假定起点：它占掉本色唯一的端点名额，同色其余格子于是全部塌成「必须挂 2 条边」
        mark_endpoint(start_cell);
        run_queue();
        report("假定起点后", truth);
    }
    report("全局传播(不假定起点)", truth);
    printf("    端点名额 [%d,%d], 已定端点 [%d,%d], 候选 [%d,%d], 用时 %.3fs\n",
           ep_cap[0], ep_cap[1], ep_used[0], ep_used[1], ep_cand[0], ep_cand[1], t_base);

    t0 = clock();
    probing(rounds);
    double t_probe = (double)(clock() - t0) / CLOCKS_PER_SEC;
    report("+ probing", truth);
    printf("    probing 试探 %lld 次, 用时 %.2fs\n", probe_calls, t_probe);
    return 0;
}
