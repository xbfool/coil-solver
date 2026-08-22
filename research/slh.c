// 土方 SLH：网格子图哈密顿环的 2-opt 爬山
// 圈序 order[0..n-1]，缺陷 = 圈上相邻但图上不相邻的对数。
// 移动：对缺陷对 (a,b)=order[i],order[i+1]，找 a 的图邻居 c=order[j]，
// 翻转 order[i+1..j]（2-opt）——净缺陷不增则接受（平移动带随机性防抖）。
// 停滞则 double-bridge 扰动。缺陷=0 => 哈密顿环，输出格子序列。
// 用法：slh <level> [seed] [maxsec]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int W, H, N, n;
static int *cells, *cid;          // cells[k]=格子线性号；cid[格子]=k 或 -1
static int nbr[ /*n*/ 1 ][ 4 ];   // 占位，实际动态
static int (*nb)[4]; static int *nbc;
static int *ord, *pos;
static unsigned rng;
static unsigned rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

static inline int adj(int u, int v) {
    for (int k = 0; k < nbc[u]; ++k) if (nb[u][k] == v) return 1;
    return 0;
}
static inline int defect(int i) {                 // 圈位 i 与 i+1 之间是否坏缝
    return !adj(ord[i], ord[(i + 1) % n]);
}

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    static char buf[1 << 22];
    size_t nread = fread(buf, 1, sizeof buf - 1, f); buf[nread] = 0; fclose(f);
    int w, h; char *bp;
    sscanf(strstr(buf, "x="), "x=%d", &w);
    sscanf(strstr(buf, "y="), "y=%d", &h);
    bp = strstr(buf, "board=") + 6;
    W = w; H = h; N = w * h;
    rng = argc > 2 ? (unsigned)atoi(argv[2]) : 42u;
    int maxsec = argc > 3 ? atoi(argv[3]) : 120;

    cells = malloc(sizeof(int) * N); cid = malloc(sizeof(int) * N);
    n = 0;
    for (int i = 0; i < N; ++i) { cid[i] = -1; if (bp[i] == '.') { cid[i] = n; cells[n++] = i; } }
    nb = malloc(sizeof(int) * 4 * n); nbc = calloc(n, sizeof(int));
    for (int k = 0; k < n; ++k) {
        int c = cells[k], x = c % W, y = c / W;
        int dd[4][3] = { {1,0,1}, {-1,0,1}, {0,1,0}, {0,-1,0} };
        for (int t = 0; t < 4; ++t) {
            int nx2 = x + dd[t][0], ny2 = y + dd[t][1];
            if (nx2 < 0 || nx2 >= W || ny2 < 0 || ny2 >= H) continue;
            int c2 = ny2 * W + nx2;
            if (cid[c2] >= 0) nb[k][nbc[k]++] = cid[c2];
        }
    }
    ord = malloc(sizeof(int) * n); pos = malloc(sizeof(int) * n);
    // 初始圈：蛇形行序（同行左右交替）——天然大量好缝
    int idx = 0;
    for (int y = 0; y < H; ++y) {
        if (y & 1) { for (int x = W - 1; x >= 0; --x) { int c = y * W + x; if (cid[c] >= 0) ord[idx++] = cid[c]; } }
        else       { for (int x = 0; x < W; ++x)      { int c = y * W + x; if (cid[c] >= 0) ord[idx++] = cid[c]; } }
    }
    for (int i = 0; i < n; ++i) pos[ord[i]] = i;
    long long ndef = 0;
    for (int i = 0; i < n; ++i) ndef += defect(i);
    fprintf(stderr, "n=%d 初始缺陷 %lld\n", n, ndef);

    time_t t0 = time(0);
    long long iters = 0, best = ndef;
    while (ndef > 0 && time(0) - t0 < maxsec) {
        ++iters;
        // 找一个缺陷位置（随机起点线性探测）
        int i = rnd() % n;
        int guard = 0;
        while (!defect(i) && guard++ < n) i = (i + 1) % n;
        if (!defect(i)) break;
        int a = ord[i];
        // a 的随机图邻居 c=ord[j]，做 2-opt 翻转 (i+1..j)
        int c = nb[a][rnd() % nbc[a]];
        int j = pos[c];
        if (j == i || j == (i + 1) % n) continue;
        // 环上翻转 i+1..j（处理环绕：旋转视角——为简单，仅当 j>i 时翻转，否则换端点）
        int lo = (i + 1) % n, hi = j;
        if (lo > hi) { // 翻转另一半等价：翻 hi+1..lo-1 的补段太绕，简化：翻转 (j+1..i) 反向配对
            lo = (j + 1) % n; hi = i;
            if (lo > hi) continue;
            // 此时缺陷对在尾端；等价 2-opt
        }
        // 评分：受影响的缝 = (lo-1,lo) 与 (hi,hi+1)
        int p1 = (lo - 1 + n) % n, p2 = hi;
        int before = defect(p1) + defect(p2);
        // 翻转
        for (int x2 = lo, y2 = hi; x2 < y2; ++x2, --y2) {
            int t = ord[x2]; ord[x2] = ord[y2]; ord[y2] = t;
            pos[ord[x2]] = x2; pos[ord[y2]] = y2;
        }
        pos[ord[lo]] = lo; pos[ord[hi]] = hi;
        int after = defect(p1) + defect(p2);
        int delta = after - before;
        if (delta > 0 || (delta == 0 && (rnd() & 7))) {
            // 拒绝：翻回来
            for (int x2 = lo, y2 = hi; x2 < y2; ++x2, --y2) {
                int t = ord[x2]; ord[x2] = ord[y2]; ord[y2] = t;
                pos[ord[x2]] = x2; pos[ord[y2]] = y2;
            }
        } else {
            ndef += delta;
            if (ndef < best) { best = ndef; }
        }
        if ((iters & 0xFFFFF) == 0) fprintf(stderr, "iter=%lldM 缺陷=%lld\n", iters >> 20, ndef);
    }
    fprintf(stderr, "终态缺陷 %lld（iters=%lld）\n", ndef, iters);
    if (ndef == 0) {
        printf("CYCLE");
        for (int i = 0; i < n; ++i) {
            int c = cells[ord[i]];
            printf(" %d,%d", c % W, c / W);
        }
        printf("\n");
        return 0;
    }
    return 1;
}
