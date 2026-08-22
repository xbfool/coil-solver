// Coil 自建生成器 v1：自愿停步走法 + 按需落墙（构造即证明有解）
//
// 原理：蛇在 W×H 空盘上走。每步选方向 d，沿 d 的自由射线选一个停点：
//   - 停在自然阻挡前（边界/已访问/已落墙）：免费；
//   - 提前停：把停点前方那格立刻落为墙（它从此永不可访问）。
// 走到卡死（四方向都动不了）为止，未访问格全部变墙。
// 生成走法在成品盘上按滑动规则重放即为合法解（停步语义逐步一致）。
//
// 参数：W H SEED STOPP(提前停概率%) 输出：
//   x=..&y=..&board=... 一行 + SOL x=..&y=..&path=... 一行 + STAT 行
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int W, H, N;
static unsigned char *cell;   // 0=未访问 1=路径 2=墙
static unsigned rng;
static unsigned rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

int main(int argc, char **argv) {
    W = argc > 1 ? atoi(argv[1]) : 36;
    H = argc > 2 ? atoi(argv[2]) : 36;
    rng = argc > 3 ? (unsigned)atoi(argv[3]) : 12345u;
    int stopp = argc > 4 ? atoi(argv[4]) : 30;      // 提前停概率（%）每个可停位置
    N = W * H;
    cell = calloc(N, 1);
    int dx4[4] = { 1, -1, 0, 0 }, dy4[4] = { 0, 0, 1, -1 };
    char dc4[4] = { 'R', 'L', 'D', 'U' };

    int sx = rnd() % W, sy = rnd() % H;
    int x = sx, y = sy;
    cell[y * W + x] = 1;
    static char path[1 << 20];
    int plen = 0;

    for (;;) {
        // 收集可行方向（至少能走一格）
        int cand[4], nc = 0;
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx4[d], ny = y + dy4[d];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            if (cell[ny * W + nx] != 0) continue;
            cand[nc++] = d;
        }
        if (nc == 0) break;                          // 卡死 = 终点
        int d = cand[rnd() % nc];
        int ddx = dx4[d], ddy = dy4[d];
        // 沿射线走；每过一格掷骰子决定要不要提前停（前方格落墙）
        for (;;) {
            x += ddx; y += ddy;
            cell[y * W + x] = 1;
            int nx = x + ddx, ny = y + ddy;
            int blocked = (nx < 0 || nx >= W || ny < 0 || ny >= H) || cell[ny * W + nx] != 0;
            if (blocked) break;
            if ((int)(rnd() % 100) < stopp) {        // 提前停：前方落墙
                cell[ny * W + nx] = 2;
                break;
            }
        }
        path[plen++] = dc4[d];
    }
    path[plen] = 0;
    int nfree = 0, nwall = 0;
    for (int i = 0; i < N; ++i) {
        if (cell[i] == 1) ++nfree;
        else ++nwall;                                // 未访问 + 落墙 都算墙
    }
    printf("x=%d&y=%d&board=", W, H);
    for (int i = 0; i < N; ++i) putchar(cell[i] == 1 ? '.' : 'X');
    printf("\nSOL x=%d&y=%d&path=%s\n", sx, sy, path);
    printf("STAT free=%d wall=%d walldens=%.1f%% slides=%d end=%d,%d startdist=%d\n",
           nfree, nwall, 100.0 * nwall / N, plen, x, y,
           abs(x - sx) + abs(y - sy));
    return 0;
}
