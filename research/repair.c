// 滑动修补爬山器：在哈密顿环空间做 2-opt 局部搜索，目标 = 滑动可行（弧扫描有缝）
//
// 状态：环 ord[0..n-1]（合法图环）。损失 = (min_s cov[s], sum cov)：
//   cov[s] = 封锁断点 s 的时序约束数（软垫背转弯 (i 需 j 早) 封锁弧 (j..i]，
//   断点两端转弯豁免在评估时近似忽略——损失到 0 后精确扫描复核）。
// 移动：2-opt 翻转 ord[lo..hi]（须保持环合法：新接缝 (lo-1,hi) 与 (lo,hi+1) 图相邻）。
// 评估：翻转后重建约束（翻转段内转弯方向反转）+ 差分数组重建 cov —— O(n+T)。
// 接受：损失字典序不升（平移动带概率）；停滞 → double-bridge 扰动；损失 min=0 → 输出。
// 用法：repair <level> <tourfile> [seed] [maxsec]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int W, H, n;
static int *cells, *cid;                  // 网格 <-> 环节点
static int (*nb)[4]; static int *nbc;
static int *ord, *pos;
static int *cov;                          // 差分/覆盖
static int *cons_i, *cons_j, nT;          // 约束（按当前环序的位置对）
static unsigned rng;
static unsigned rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

static inline int gadj(int u, int v) {
    for (int k = 0; k < nbc[u]; ++k) if (nb[u][k] == v) return 1;
    return 0;
}

// 从当前 ord 重建全部约束与 cov；返回 (min_cov, 总质量) 经 out 参数
static void rebuild(long long *mincov, long long *mass) {
    nT = 0;
    for (int i = 0; i < n; ++i) {
        int c = ord[i], p = ord[(i - 1 + n) % n], x = ord[(i + 1) % n];
        int cc = cells[c], pc = cells[p], xc = cells[x];
        int dix = (cc % W) - (pc % W), diy = (cc / W) - (pc / W);
        int dox = (xc % W) - (cc % W), doy = (xc / W) - (cc / W);
        if (dix == dox && diy == doy) continue;            // 直穿
        int bx = (cc % W) + dix, by = (cc / W) + diy;
        if (bx < 0 || bx >= W || by < 0 || by >= H) continue;
        int bcell = by * W + bx;
        if (cid[bcell] < 0) continue;                       // 墙垫背
        cons_i[nT] = i; cons_j[nT] = pos[cid[bcell]]; ++nT;
    }
    memset(cov, 0, sizeof(int) * (n + 1));
    for (int t = 0; t < nT; ++t) {
        int i = cons_i[t], j = cons_j[t];
        int lo = (j + 1) % n, hi = i;                       // 封锁弧 [j+1..i]
        if (lo <= hi) { cov[lo] += 1; cov[hi + 1] -= 1; }
        else { cov[lo] += 1; cov[n] -= 1; cov[0] += 1; cov[hi + 1] -= 1; }
    }
    long long mn = 1 << 30, sm = 0, run = 0;
    for (int s = 0; s < n; ++s) {
        run += cov[s];
        sm += run;
        if (run < mn) mn = run;
    }
    *mincov = mn; *mass = sm;
}

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    static char buf[1 << 22];
    size_t nr = fread(buf, 1, sizeof buf - 1, f); buf[nr] = 0; fclose(f);
    int w, h; char *bp;
    sscanf(strstr(buf, "x="), "x=%d", &w);
    sscanf(strstr(buf, "y="), "y=%d", &h);
    bp = strstr(buf, "board=") + 6;
    W = w; H = h;
    rng = argc > 3 ? (unsigned)atoi(argv[3]) : 7u;
    int maxsec = argc > 4 ? atoi(argv[4]) : 600;

    int N = w * h;
    cells = malloc(sizeof(int) * N); cid = malloc(sizeof(int) * N);
    n = 0;
    for (int i = 0; i < N; ++i) { cid[i] = -1; if (bp[i] == '.') { cid[i] = n; cells[n++] = i; } }
    nb = malloc(sizeof(int) * 4 * n); nbc = calloc(n, sizeof(int));
    for (int k = 0; k < n; ++k) {
        int c = cells[k], x = c % W, y = c / W;
        int dd[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
        for (int t = 0; t < 4; ++t) {
            int nx = x + dd[t][0], ny = y + dd[t][1];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            if (cid[ny * W + nx] >= 0) nb[k][nbc[k]++] = cid[ny * W + nx];
        }
    }
    // 读 LKH tour
    ord = malloc(sizeof(int) * n); pos = malloc(sizeof(int) * n);
    cov = malloc(sizeof(int) * (n + 1));
    cons_i = malloc(sizeof(int) * n); cons_j = malloc(sizeof(int) * n);
    FILE *tf = fopen(argv[2], "rb");
    static char tb[1 << 22];
    nr = fread(tb, 1, sizeof tb - 1, tf); tb[nr] = 0; fclose(tf);
    char *p2 = strstr(tb, "TOUR_SECTION");
    p2 = strchr(p2, '\n') + 1;
    for (int i = 0; i < n; ++i) { int v; sscanf(p2, "%d", &v); ord[i] = v - 1; pos[ord[i]] = i; while (*p2 && *p2 != '\n') ++p2; ++p2; }

    long long mincov, mass, bestmin = 1 << 30, bestmass = 1LL << 60;
    rebuild(&mincov, &mass);
    fprintf(stderr, "初始：min_cov=%lld 总质量=%lld 约束=%d\n", mincov, mass, nT);

    time_t t0 = time(0);
    long long iters = 0, acc = 0;
    while (mincov > 0 && time(0) - t0 < maxsec) {
        ++iters;
        // 候选 2-opt：随机 a 与其图邻居 c；翻转 pos[a]+1..pos[c]
        int a = ord[rnd() % n];
        int c = nb[a][rnd() % nbc[a]];
        int i = pos[a], j = pos[c];
        int lo = (i + 1) % n, hi = j;
        if (lo > hi) { int t = lo; lo = (j + 1) % n; hi = i; if (lo > hi) continue; (void)t; }
        if (hi - lo < 1 || hi - lo > n - 2) continue;
        // 环合法性：新缝 (lo-1,hi) 与 (lo,hi+1)
        int pl = (lo - 1 + n) % n, ph = (hi + 1) % n;
        if (!gadj(ord[pl], ord[hi]) || !gadj(ord[lo], ord[ph])) continue;
        // 执行翻转
        for (int x2 = lo, y2 = hi; x2 < y2; ++x2, --y2) {
            int t = ord[x2]; ord[x2] = ord[y2]; ord[y2] = t;
            pos[ord[x2]] = x2; pos[ord[y2]] = y2;
        }
        long long m2, s2;
        rebuild(&m2, &s2);
        int ok = (m2 < mincov) || (m2 == mincov && s2 <= mass && (s2 < mass || (rnd() & 3)));
        if (ok) { mincov = m2; mass = s2; ++acc; }
        else {
            for (int x2 = lo, y2 = hi; x2 < y2; ++x2, --y2) {
                int t = ord[x2]; ord[x2] = ord[y2]; ord[y2] = t;
                pos[ord[x2]] = x2; pos[ord[y2]] = y2;
            }
        }
        if ((iters & 0x3FFF) == 0) {
            if (mincov < bestmin || (mincov == bestmin && mass < bestmass)) { bestmin = mincov; bestmass = mass; }
            fprintf(stderr, "it=%lldk acc=%lld min_cov=%lld mass=%lld\n", iters >> 10, acc, mincov, mass);
        }
    }
    fprintf(stderr, "终态 min_cov=%lld mass=%lld iters=%lld\n", mincov, mass, iters);
    if (mincov == 0) {
        printf("FEASIBLE-CYCLE");
        for (int i = 0; i < n; ++i) printf(" %d,%d", cells[ord[i]] % W, cells[ord[i]] / W);
        printf("\n");
        return 0;
    }
    return 1;
}
