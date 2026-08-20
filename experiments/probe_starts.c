// 起点探针：给每个候选起点跑一个小预算的搜索，按「探到多深」排序，看真起点排第几。
//
// 动机：解通常唯一 => 起点也唯一，而系统搜索平均要浪费 n/2 个起点。
// 如果一个廉价探针就能把真起点排进前 1%，那就是白捡两个数量级。
//
// 用法: probe <关卡文件> <真起点x> <真起点y> [每个起点的节点预算]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int W, N, delta[4];
static unsigned char *g, *g0, *col;
static int *deg, cnt[2], low_cnt, zero_cnt;
static int *fstack; static int32_t *seen, seen_id;
static long long nodes, budget;
static int best_rem;                       // 本次探针探到的最小剩余格数

static inline int freedeg(int c) { return g[c+delta[0]]+g[c+delta[1]]+g[c+delta[2]]+g[c+delta[3]]; }
static inline void mark(int c) {
    if (deg[c] <= 1) --low_cnt;
    if (deg[c] == 0) --zero_cnt;
    g[c] = 0; --cnt[col[c]];
    for (int d = 0; d < 4; ++d) { int n = c + delta[d];
        if (g[n]) { int o = deg[n]--; if (o == 2) ++low_cnt; if (o == 1) ++zero_cnt; } }
}
static inline void unmark(int c) {
    g[c] = 1; ++cnt[col[c]];
    for (int d = 0; d < 4; ++d) { int n = c + delta[d];
        if (g[n]) { int o = deg[n]++; if (o == 1) --low_cnt; if (o == 0) --zero_cnt; } }
    deg[c] = freedeg(c);
    if (deg[c] <= 1) ++low_cnt;
    if (deg[c] == 0) ++zero_cnt;
}
static inline int cheap_ok(int p, int rem) {
    if (cnt[col[p] ^ 1] != (rem + 1) / 2) return 0;
    int al = 0, az = 0;
    for (int d = 0; d < 4; ++d) { int n = p + delta[d];
        if (g[n]) { if (deg[n] <= 1) ++al; if (deg[n] == 0) ++az; } }
    if (zero_cnt - az > 0) return 0;
    return low_cnt - al + az <= 1;
}
static int reach_ok(int p, int rem) {
    int top = 0, count = 0; ++seen_id;
    for (int d = 0; d < 4; ++d) { int c = p + delta[d];
        if (g[c] && seen[c] != seen_id) { seen[c] = seen_id; fstack[top++] = c; } }
    while (top) { int c = fstack[--top]; ++count;
        for (int d = 0; d < 4; ++d) { int n = c + delta[d];
            if (g[n] && seen[n] != seen_id) { seen[n] = seen_id; fstack[top++] = n; } } }
    return count == rem;
}
static int dfs(int p, int rem) {
    if (rem < best_rem) best_rem = rem;
    if (rem == 0) return 1;
    if (nodes++ >= budget) return -1;
    struct move { int dir, endp, len, score; } cd[4];
    int nc = 0;
    for (int d = 0; d < 4; ++d) {
        int dd = delta[d];
        if (!g[p + dd]) continue;
        int c = p, len = 0;
        while (g[c + dd]) { c += dd; mark(c); ++len; }
        int r2 = rem - len;
        if (r2 == 0 || (cheap_ok(c, r2) && reach_ok(c, r2))) {
            cd[nc].dir = d; cd[nc].endp = c; cd[nc].len = len; cd[nc].score = freedeg(c); ++nc; }
        for (int i = 0, b = c; i < len; ++i, b -= dd) unmark(b);
    }
    for (int i = 1; i < nc; ++i)
        for (int j = i; j > 0 && cd[j].score < cd[j-1].score; --j) {
            struct move t = cd[j]; cd[j] = cd[j-1]; cd[j-1] = t; }
    for (int i = 0; i < nc; ++i) {
        int dd = delta[cd[i].dir], len = cd[i].len, c = cd[i].endp;
        for (int k = 0, q = p; k < len; ++k) { q += dd; mark(q); }
        int r = dfs(c, rem - len);
        if (r) return r;
        for (int k = 0, b = c; k < len; ++k, b -= dd) unmark(b);
    }
    return 0;
}

int main(int argc, char **argv) {
    budget = argc > 4 ? atoll(argv[4]) : 300;
    FILE *f = fopen(argv[1], "r");
    char *buf = malloc(1 << 24); size_t n = fread(buf, 1, (1 << 24) - 1, f); buf[n] = 0; fclose(f);
    int w = atoi(strstr(buf, "x=") + 2), h = atoi(strstr(buf, "y=") + 2);
    char *pb = strstr(buf, "board=") + 6;
    W = w + 2; N = W * (h + 2);
    g = calloc(N,1); g0 = calloc(N,1); col = calloc(N,1); deg = malloc(sizeof(int)*N);
    fstack = malloc(sizeof(int)*N); seen = calloc(N, sizeof(int32_t));
    delta[0] = -1; delta[1] = -W; delta[2] = 1; delta[3] = W;
    for (int c = 0; c < N; ++c) col[c] = (unsigned char)(((c % W) + (c / W)) & 1);
    int total = 0;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
        if (pb[(size_t)y*w+x] == '.') { g0[(y+1)*W+(x+1)] = 1; ++total; }

    int truth = (atoi(argv[3]) + 1) * W + (atoi(argv[2]) + 1);
    int ns = 0, truth_score = -1, nref = 0, truth_ref = 0;
    unsigned char *refuted = calloc(N, 1);
    int *score = malloc(sizeof(int) * N);
    for (int s = 0; s < N; ++s) {
        if (!g0[s]) continue;
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = low_cnt = zero_cnt = 0;
        for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
        for (int c = 0; c < N; ++c) if (g[c]) { deg[c] = freedeg(c);
            if (deg[c] <= 1) ++low_cnt; if (deg[c] == 0) ++zero_cnt; }
        mark(s);
        nodes = 0; best_rem = total;
        int r = dfs(s, total - 1);
        score[s] = best_rem;                        // 越小=探得越深
        if (r == 0) { refuted[s] = 1; ++nref; }     // 子树搜穷仍无解 => 此起点被证伪
        if (s == truth) { truth_score = best_rem; truth_ref = (r == 0); }
        ++ns;
    }
    int better = 0, equal = 0;
    for (int s = 0; s < N; ++s) {
        if (!g0[s] || refuted[s]) continue;
        if (score[s] < truth_score) ++better;
        else if (score[s] == truth_score) ++equal;
    }
    printf("%s: 候选起点 %d  预算 %lld 节点/起点\n", argv[1], ns, budget);
    printf("  真起点探到剩余 %d（总格 %d，即走了 %.0f%%）\n",
           truth_score, total, 100.0 * (total - truth_score) / total);
    printf("  **被证伪的起点 %d / %d = %.1f%%**%s\n", nref, ns, 100.0 * nref / ns,
           truth_ref ? "   !!! 真起点也被证伪，有 bug" : "");
    printf("  幸存者中排名：更优 %d，并列 %d → 最差第 %d / %d（幸存者前 %.1f%%）\n",
           better, equal, better + equal, ns - nref,
           (ns - nref) ? 100.0 * (better + equal) / (ns - nref) : 0.0);
    return 0;
}
