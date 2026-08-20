// 纯贪心能走多远？（不回溯）
//
// 动机：搜索路线的天花板在 L350~450（节点数随格数指数增长，加机器只买到 +40 关）。
// 1000 关必须换近似线性的构造算法，而构造算法的可行性取决于一件事：
// 贪心走到底能覆盖百分之多少？覆盖 99%+ 说明只差局部修补；覆盖 60% 说明构造路线也不成立。
//
// 用法: greedy <关卡文件> [采样起点数]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int W, N, delta[4];
static unsigned char *g, *g0;
static int *deg;

static inline int freedeg(int c) { return g[c+delta[0]]+g[c+delta[1]]+g[c+delta[2]]+g[c+delta[3]]; }

int main(int argc, char **argv) {
    int samples = argc > 2 ? atoi(argv[2]) : 200;
    FILE *f = fopen(argv[1], "r");
    if (!f) { fprintf(stderr, "open failed\n"); return 1; }
    static char buf[1 << 23];
    size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);

    int w = atoi(strstr(buf, "x=") + 2), h = atoi(strstr(buf, "y=") + 2);
    char *pb = strstr(buf, "board=") + 6;
    W = w + 2; N = W * (h + 2);
    g = malloc(N); g0 = calloc(N, 1); deg = malloc(sizeof(int) * N);
    delta[0] = -1; delta[1] = -W; delta[2] = 1; delta[3] = W;

    int total = 0;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
        if (pb[(size_t)y * w + x] == '.') { g0[(y + 1) * W + (x + 1)] = 1; ++total; }

    int *starts = malloc(sizeof(int) * total), ns = 0;
    for (int c = 0; c < N; ++c) if (g0[c]) starts[ns++] = c;

    int best = 0;
    long long sum = 0; int cnt = 0;
    int step = ns / samples; if (step < 1) step = 1;
    for (int i = 0; i < ns; i += step) {
        memcpy(g, g0, N);
        int p = starts[i], covered = 1;
        g[p] = 0;
        for (;;) {
            int bd = -1, bs = 99, bend = -1, blen = 0;
            for (int d = 0; d < 4; ++d) {                 // Warnsdorff：走向出口最少的落点
                int dd = delta[d];
                if (!g[p + dd]) continue;
                int c = p, len = 0;
                while (g[c + dd]) { c += dd; g[c] = 0; ++len; }
                int sc = freedeg(c);
                for (int k = 0, b = c; k < len; ++k, b -= dd) g[b] = 1;
                if (sc < bs) { bs = sc; bd = d; bend = c; blen = len; }
            }
            if (bd < 0) break;
            int dd = delta[bd];
            for (int k = 0, q = p; k < blen; ++k) { q += dd; g[q] = 0; }
            p = bend; covered += blen;
        }
        if (covered > best) best = covered;
        sum += covered; ++cnt;
    }
    printf("%s: 自由格 %d  采样起点 %d\n", argv[1], total, cnt);
    printf("  贪心最好覆盖 %d (%.2f%%)   平均覆盖 %.2f%%\n",
           best, 100.0 * best / total, 100.0 * (double)sum / cnt / total);
    return 0;
}
