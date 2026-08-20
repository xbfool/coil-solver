// 数一关到底有多少个 Coil 解（不是找一个，是全枚举）
//
// 动机：空盘上解数是常数 32；而真实关卡是「照着有解」构造的（无度1格、墙的奇偶完全平衡）。
// 如果真实关卡的解数很少甚至唯一，那说明约束几乎把答案锁死了 —— 那正确的算法方向就是
// 「传播强制」而不是「搜索」。
//
// 用法: count <关卡文件> [节点预算]
// 输出: 有向解数（同一条路径正反走算 2 个）、几何上不同的解数 ≈ 有向数/2、是否被预算截断
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int W, N;
static unsigned char *g, *g0, *col;
static int *deg, cnt[2], low_cnt, zero_cnt, delta[4];
static int *fstack; static int32_t *seen, seen_id;
static long long nodes, budget, solutions;
static int truncated;

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

static void dfs(int p, int rem) {
    if (rem == 0) { ++solutions; return; }
    if (nodes++ >= budget) { truncated = 1; return; }
    if (truncated) return;
    for (int d = 0; d < 4; ++d) {
        int dd = delta[d];
        if (!g[p + dd]) continue;
        int c = p, len = 0;
        while (g[c + dd]) { c += dd; mark(c); ++len; }
        int rem2 = rem - len;
        if (rem2 == 0 || (cheap_ok(c, rem2) && reach_ok(c, rem2))) dfs(c, rem2);
        for (int i = 0, back = c; i < len; ++i, back -= dd) unmark(back);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: count <level> [budget]\n"); return 1; }
    budget = argc > 2 ? atoll(argv[2]) : 200000000LL;
    FILE *f = fopen(argv[1], "r");
    if (!f) { fprintf(stderr, "open failed\n"); return 1; }
    char buf[1 << 22]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); buf[n] = 0; fclose(f);

    int w = atoi(strstr(buf, "x=") + 2), h = atoi(strstr(buf, "y=") + 2);
    char *pb = strstr(buf, "board=") + 6;
    W = w + 2; N = W * (h + 2);
    g = calloc(N, 1); g0 = calloc(N, 1); col = calloc(N, 1); deg = malloc(sizeof(int) * N);
    fstack = malloc(sizeof(int) * N); seen = calloc(N, sizeof(int32_t));
    delta[0] = -1; delta[1] = -W; delta[2] = 1; delta[3] = W;
    for (int c = 0; c < N; ++c) col[c] = (unsigned char)(((c % W) + (c / W)) & 1);

    int total = 0;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
        if (pb[(size_t)y * w + x] == '.') { g0[(y + 1) * W + (x + 1)] = 1; ++total; }

    for (int s = 0; s < N; ++s) {
        if (!g0[s]) continue;
        memcpy(g, g0, N);
        cnt[0] = cnt[1] = low_cnt = zero_cnt = 0;
        for (int c = 0; c < N; ++c) if (g[c]) ++cnt[col[c]];
        for (int c = 0; c < N; ++c) if (g[c]) { deg[c] = freedeg(c);
            if (deg[c] <= 1) ++low_cnt; if (deg[c] == 0) ++zero_cnt; }
        mark(s);
        dfs(s, total - 1);
        if (truncated) break;
    }
    printf("%s: 自由格=%d 有向解=%lld 几何解≈%lld 节点=%lld%s\n",
           argv[1], total, solutions, solutions / 2, nodes, truncated ? " [超预算截断]" : "");
    return 0;
}
