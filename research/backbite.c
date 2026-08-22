// backbite 退火：路径空间搜索滑动可行解（最小实现）
// 状态 = 哈密顿路径（order 数组）。移动 = backbite：取端点 u、其图邻居 v（非路径邻居），
// 加边 (u,v) 成 rho 形，断 v 的「朝 u 侧」路径边 => 新端点。O(段翻转) 实现：
//   若 v 在 u 所在端的方向 k 步处，翻转 order 的那一段即可（经典实现）。
// 损失 = min(两个走向的违约数)：违约 = 软垫背转弯的垫背在走向中晚于转弯。
// Metropolis 退火。损失 0 => 输出解。
// 用法：backbite <level> <lkh_tour> [seed] [maxsec] [T0]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

static int W, H, n;
static int *cells, *cid;
static int (*nb)[4]; static int *nbc;
static int *ord, *pos;
static unsigned rng;
static unsigned rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

static int violations(void) {
    // 走向 A：order[0..n-1]；走向 B：反向。违约：转弯 t（进方向 d），垫背 b=t+d 自由，
    // pos(b) > pos(t)（走向内）=> 违约。O(n)。
    int vA = 0, vB = 0;
    for (int i = 1; i < n - 1; ++i) {
        int c = ord[i], p = ord[i - 1], x = ord[i + 1];
        int cc = cells[c], pc = cells[p], xc = cells[x];
        int dix = (cc % W) - (pc % W), diy = (cc / W) - (pc / W);
        int dox = (xc % W) - (cc % W), doy = (xc / W) - (cc / W);
        if (dix == dox && diy == doy) continue;
        // 走向 A 在 c 转弯（从 p 来）：垫背 = c + di
        int bx = (cc % W) + dix, by = (cc / W) + diy;
        if (bx >= 0 && bx < W && by >= 0 && by < H && cid[by * W + bx] >= 0) {
            if (pos[cid[by * W + bx]] > i) ++vA;
        }
        // 走向 B 在 c 转弯（从 x 来）：垫背 = c + (-do)
        int b2x = (cc % W) - dox, b2y = (cc / W) - doy;
        if (b2x >= 0 && b2x < W && b2y >= 0 && b2y < H && cid[b2y * W + b2x] >= 0) {
            if (pos[cid[b2y * W + b2x]] < i) ++vB;   // 走向 B 的「更早」= 索引更大；晚 = 索引更小
        }
    }
    // 末端停步（走向 A 的最后一滑、走向 B 的最后一滑）由端点邻接自动满足或算violation?
    // 最后一滑停在 order[n-1]：垫背 = 越过端点的格——若自由且非 order[n-2] 方向重复……
    // 简化 v1：末端垫背额外检查
    {
        int c = ord[n - 1], p = ord[n - 2];
        int cc = cells[c], pc = cells[p];
        int dx = (cc % W) - (pc % W), dy = (cc / W) - (pc / W);
        int bx = (cc % W) + dx, by = (cc / W) + dy;
        if (bx >= 0 && bx < W && by >= 0 && by < H && cid[by * W + bx] >= 0) ++vA; // 末端垫背须墙/已访问；自由格=晚于一切=违约
        c = ord[0]; p = ord[1];
        cc = cells[c]; pc = cells[p];
        dx = (cc % W) - (pc % W); dy = (cc / W) - (pc / W);
        bx = (cc % W) + dx; by = (cc / W) + dy;
        if (bx >= 0 && bx < W && by >= 0 && by < H && cid[by * W + bx] >= 0) ++vB;
    }
    return vA < vB ? vA : vB;
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
    int maxsec = argc > 4 ? atoi(argv[4]) : 300;
    double T0 = argc > 5 ? atof(argv[5]) : 2.0;

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
    ord = malloc(sizeof(int) * n); pos = malloc(sizeof(int) * n);
    FILE *tf = fopen(argv[2], "rb");
    static char tb[1 << 22];
    nr = fread(tb, 1, sizeof tb - 1, tf); tb[nr] = 0; fclose(tf);
    char *p2 = strstr(tb, "TOUR_SECTION");
    p2 = strchr(p2, '\n') + 1;
    for (int i = 0; i < n; ++i) { int v; sscanf(p2, "%d", &v); ord[i] = v - 1; pos[ord[i]] = i; while (*p2 && *p2 != '\n') ++p2; ++p2; }

    int loss = violations(), best = loss;
    fprintf(stderr, "n=%d 初始违约 %d\n", n, loss);
    time_t t0 = time(0);
    long long iters = 0, acc = 0;
    double T = T0;
    while (loss > 0 && time(0) - t0 < maxsec) {
        ++iters;
        T = T0 * exp(-2.5 * (double)(time(0) - t0) / maxsec);
        // backbite：随机选一端
        int endside = rnd() & 1;                    // 0: 端=ord[0]，1: 端=ord[n-1]
        int u = endside ? ord[n - 1] : ord[0];
        int v = nb[u][rnd() % nbc[u]];
        int pv = pos[v];
        // v 不能是路径上的紧邻
        if (endside ? (pv == n - 2) : (pv == 1)) continue;
        // 翻转实现：端在右(n-1)：加边 (u,v)，断 (v, v+1)，翻转 [pv+1 .. n-1]
        //          端在左(0)：加边 (u,v)，断 (v-1, v)，翻转 [0 .. pv-1]
        int lo, hi;
        if (endside) { lo = pv + 1; hi = n - 1; }
        else { lo = 0; hi = pv - 1; }
        for (int x2 = lo, y2 = hi; x2 < y2; ++x2, --y2) {
            int t = ord[x2]; ord[x2] = ord[y2]; ord[y2] = t;
            pos[ord[x2]] = x2; pos[ord[y2]] = y2;
        }
        if (lo == hi) pos[ord[lo]] = lo;
        int nl = violations();
        int d = nl - loss;
        if (d <= 0 || ((double)rnd() / 16777216.0) < exp(-d / (T > 0.01 ? T : 0.01))) {
            loss = nl; ++acc;
            if (loss < best) best = loss;
        } else {
            for (int x2 = lo, y2 = hi; x2 < y2; ++x2, --y2) {
                int t = ord[x2]; ord[x2] = ord[y2]; ord[y2] = t;
                pos[ord[x2]] = x2; pos[ord[y2]] = y2;
            }
            if (lo == hi) pos[ord[lo]] = lo;
        }
        if ((iters & 0xFFFFF) == 0) fprintf(stderr, "it=%lldM acc=%lld loss=%d best=%d T=%.3f\n", iters >> 20, acc, loss, best, T);
    }
    fprintf(stderr, "终态 loss=%d best=%d iters=%lld\n", loss, best, iters);
    if (loss == 0) {
        printf("FEASIBLE-PATH");
        for (int i = 0; i < n; ++i) printf(" %d,%d", cells[ord[i]] % W, cells[ord[i]] / W);
        printf("\n");
        return 0;
    }
    return 1;
}
