// Coil solver — v1: 位置滑动 DFS + 连通性剪枝 + 死端计数剪枝 + Warnsdorff 排序
//
// 规则回顾：选一个空格出发，每次选方向后一直滑到撞墙/撞边界/撞已访问才停，要求每个空格恰好走一次。
// 所以路径本质是空格集合上的哈密顿路径，只是「拐弯点」被滑动规则限死了。
// 哈密顿路径的必要条件都能拿来剪枝（连通性、死端数），这是 v1 的主要武器。
//
// 输入（stdin 或文件参数）：x=<w>&y=<h>&board=<w*h 个 . 或 X>
// 输出：x=<起点x>&y=<起点y>&path=<UDLR 序列>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int W, H, N;              // 加了一圈哨兵墙后的宽高
static unsigned char *g;         // 1 = 空且未访问，0 = 墙或已访问
static unsigned char *g0;        // 原始棋盘副本，每次换起点时还原
static int delta[4];
static const char DCH[4] = { 'L', 'U', 'R', 'D' };
static char *path;
static int path_len;
static int total_free;
static int *fstack;              // 洪水填充用的显式栈
static int32_t *seen;            // 时间戳法，免去每次清零
static int32_t seen_id;
static long long nodes, node_limit;

static inline int freedeg(int c) {
    return g[c + delta[0]] + g[c + delta[1]] + g[c + delta[2]] + g[c + delta[3]];
}

// 从当前位置 p 出发做一次洪水填充，同时统计死端数量。
// 剪枝依据：1) 剩余空格必须全部还能从 p 连通到达；2) 只允许存在 1 个死端（路径终点）。
static int prune_ok(int p, int remaining) {
    int top = 0, count = 0, deadends = 0;
    ++seen_id;
    for (int d = 0; d < 4; ++d) {
        int c = p + delta[d];
        if (g[c] && seen[c] != seen_id) { seen[c] = seen_id; fstack[top++] = c; }
    }
    while (top) {
        int c = fstack[--top];
        ++count;
        int deg = 0;
        for (int d = 0; d < 4; ++d) {
            int nb = c + delta[d];
            if (g[nb]) {
                ++deg;
                if (seen[nb] != seen_id) { seen[nb] = seen_id; fstack[top++] = nb; }
            } else if (nb == p) {
                ++deg;                        // 还能从当前位置进来
            }
        }
        if (deg <= 1 && ++deadends > 1) return 0;
    }
    return count == remaining;
}

// 返回 1 = 解出，0 = 此分支无解，-1 = 触到节点上限，放弃
static int dfs(int p, int remaining) {
    if (remaining == 0) return 1;
    if (nodes++ >= node_limit) return -1;

    struct move { int dir, endp, len, score; } cand[4];
    int nc = 0;

    for (int d = 0; d < 4; ++d) {
        int dd = delta[d];
        if (!g[p + dd]) continue;
        int c = p, len = 0;
        while (g[c + dd]) { c += dd; g[c] = 0; ++len; }   // 滑到底
        int rem2 = remaining - len;
        if (rem2 == 0 || prune_ok(c, rem2)) {
            cand[nc].dir = d; cand[nc].endp = c; cand[nc].len = len;
            cand[nc].score = freedeg(c);                  // Warnsdorff：优先走向出口少的地方
            ++nc;
        }
        for (int i = 0, back = c; i < len; ++i, back -= dd) g[back] = 1;
    }

    for (int i = 1; i < nc; ++i) {                        // 插入排序，n<=4
        for (int j = i; j > 0 && cand[j].score < cand[j - 1].score; --j) {
            struct move t = cand[j];
            cand[j] = cand[j - 1]; cand[j - 1] = t;
        }
    }

    for (int i = 0; i < nc; ++i) {
        int dd = delta[cand[i].dir], len = cand[i].len, c = cand[i].endp;
        for (int k = 0, q = p; k < len; ++k) { q += dd; g[q] = 0; }
        path[path_len++] = DCH[cand[i].dir];
        int r = dfs(c, remaining - len);
        if (r) return r;                                  // 1 解出 / -1 放弃，都直接上抛
        --path_len;
        for (int k = 0, back = c; k < len; ++k, back -= dd) g[back] = 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    // ---- 读入 ----
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

    int w = 0, h = 0;
    char *px = strstr(buf, "x="), *py = strstr(buf, "y="), *pb = strstr(buf, "board=");
    if (!px || !py || !pb) { fprintf(stderr, "bad input\n"); return 1; }
    w = atoi(px + 2); h = atoi(py + 2);
    pb += 6;
    if (w <= 0 || h <= 0) { fprintf(stderr, "bad dims\n"); return 1; }

    // ---- 建带哨兵边框的棋盘 ----
    W = w + 2; H = h + 2; N = W * H;
    g = calloc(N, 1); g0 = calloc(N, 1);
    delta[0] = -1; delta[1] = -W; delta[2] = 1; delta[3] = W;
    total_free = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            char ch = pb[(size_t)y * w + x];
            if (ch == '.') { g0[(y + 1) * W + (x + 1)] = 1; ++total_free; }
        }
    if (total_free == 0) { printf("x=0&y=0&path=\n"); return 0; }

    path = malloc(total_free + 8);
    fstack = malloc(sizeof(int) * (size_t)N);
    seen = calloc(N, sizeof(int32_t));

    // ---- 起点候选：度数小的优先（度 1 的格子必然是路径端点）----
    int *starts = malloc(sizeof(int) * (size_t)total_free);
    int ns = 0;
    memcpy(g, g0, N);
    for (int c = 0; c < N; ++c) if (g0[c]) starts[ns++] = c;
    for (int i = 1; i < ns; ++i) {                        // 按度数插入排序
        int v = starts[i], dv = freedeg(v), j = i;
        while (j > 0 && freedeg(starts[j - 1]) > dv) { starts[j] = starts[j - 1]; --j; }
        starts[j] = v;
    }

    // ---- 迭代放宽节点上限：先每个起点浅搜一遍，都不行再加深 ----
    for (long long lim = 300000;; lim *= 8) {
        for (int i = 0; i < ns; ++i) {
            memcpy(g, g0, N);
            int s = starts[i];
            g[s] = 0;
            path_len = 0; nodes = 0; node_limit = lim;
            if (dfs(s, total_free - 1) == 1) {
                path[path_len] = 0;
                printf("x=%d&y=%d&path=%s\n", (s % W) - 1, (s / W) - 1, path);
                return 0;
            }
        }
        if (lim > (1LL << 42)) break;
    }
    fprintf(stderr, "no solution found\n");
    return 1;
}
