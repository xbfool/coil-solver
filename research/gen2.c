// Coil 自建生成器 v2：贪心防死 + 自愿停步 + 按需落墙
//
// v1 教训：裸随机走法 2~15% 填充率就把自己画进死角。v2 策略：
//   - 候选 = (方向, 停点) 对：自然停（免费）+ 每个提前停位置（前方落墙）
//   - 打分 = 落点的自由度（防死：别把头塞进死角）+ 抖动；ε 概率纯随机（多样性）
//   - 放弃的区域自动变墙（不需要全覆盖），只求活得久 => 填充率高
// 参数：W H SEED EPS(随机%) 目标输出同 v1。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int W, H, N;
static unsigned char *cell;   // 0=未访问 1=路径 2=墙
static unsigned rng;
static unsigned rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

static int freedeg(int x, int y) {
    static int dx4[4] = { 1, -1, 0, 0 }, dy4[4] = { 0, 0, 1, -1 };
    int d = 0;
    for (int k = 0; k < 4; ++k) {
        int nx = x + dx4[k], ny = y + dy4[k];
        if (nx >= 0 && nx < W && ny >= 0 && ny < H && cell[ny * W + nx] == 0) ++d;
    }
    return d;
}

int main(int argc, char **argv) {
    W = argc > 1 ? atoi(argv[1]) : 36;
    H = argc > 2 ? atoi(argv[2]) : 36;
    rng = argc > 3 ? (unsigned)atoi(argv[3]) : 12345u;
    int eps = argc > 4 ? atoi(argv[4]) : 10;
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
        // 枚举全部 (方向, 停点)：沿射线逐格，停点=该格（若提前停则前方落墙）
        // 打分：停点自由度*4 + 滑行长度（轻微鼓励长滑）+ 抖动
        int bd = -1, bstop = -1, bscore = -1 << 30, bwall = -1;
        int nc = 0;
        for (int d = 0; d < 4; ++d) {
            int ddx = dx4[d], ddy = dy4[d];
            int cx = x, cy = y, len = 0;
            for (;;) {
                int nx = cx + ddx, ny = cy + ddy;
                if (nx < 0 || nx >= W || ny < 0 || ny >= H || cell[ny * W + nx] != 0) break;
                cx = nx; cy = ny; ++len;
                ++nc;
                // 停在 (cx,cy)：前方 (cx+ddx,cy+ddy) 若自由需落墙
                int fx = cx + ddx, fy = cy + ddy;
                int needwall = (fx >= 0 && fx < W && fy >= 0 && fy < H && cell[fy * W + fx] == 0);
                // 评估：先假掐——落点自由度要在「本滑行标记后」算，这里近似：
                // 落点四邻中减去来路方向（已走）即可近似
                int fd = 0;
                for (int k = 0; k < 4; ++k) {
                    int qx = cx + dx4[k], qy = cy + dy4[k];
                    if (qx < 0 || qx >= W || qy < 0 || qy >= H) continue;
                    if (cell[qy * W + qx] != 0) continue;
                    if (needwall && qx == fx && qy == fy) continue;   // 那格要变墙
                    // 来路格子会被标记
                    int onpath = 0;
                    if (k == (d ^ 1)) onpath = 1;                     // 正后方=来路
                    if (!onpath) ++fd;
                }
                int score = fd * 8 + len - (needwall ? 2 : 0) + (int)(rnd() % 5);
                if (score > bscore) { bscore = score; bd = d; bstop = len; bwall = needwall; }
            }
        }
        if (nc == 0) break;                          // 卡死
        if ((int)(rnd() % 100) < eps) {              // ε 随机：重选一个随机 (方向,停点)
            int pick = rnd() % nc, seen = 0;
            for (int d = 0; d < 4 && seen <= pick; ++d) {
                int ddx = dx4[d], ddy = dy4[d];
                int cx = x, cy = y, len = 0;
                for (;;) {
                    int nx = cx + ddx, ny = cy + ddy;
                    if (nx < 0 || nx >= W || ny < 0 || ny >= H || cell[ny * W + nx] != 0) break;
                    cx = nx; cy = ny; ++len;
                    if (seen == pick) {
                        bd = d; bstop = len;
                        int fx = cx + ddx, fy = cy + ddy;
                        bwall = (fx >= 0 && fx < W && fy >= 0 && fy < H && cell[fy * W + fx] == 0);
                    }
                    ++seen;
                    if (seen > pick) break;
                }
            }
        }
        // 执行
        int ddx = dx4[bd], ddy = dy4[bd];
        for (int k = 0; k < bstop; ++k) { x += ddx; y += ddy; cell[y * W + x] = 1; }
        if (bwall) cell[(y + ddy) * W + (x + ddx)] = 2;
        path[plen++] = dc4[bd];
    }
    path[plen] = 0;
    int nfree = 0;
    for (int i = 0; i < N; ++i) if (cell[i] == 1) ++nfree;
    printf("x=%d&y=%d&board=", W, H);
    for (int i = 0; i < N; ++i) putchar(cell[i] == 1 ? '.' : 'X');
    printf("\nSOL x=%d&y=%d&path=%s\n", sx, sy, path);
    printf("STAT free=%d fill=%.1f%% slides=%d end=%d,%d startdist=%d\n",
           nfree, 100.0 * nfree / N, plen, x, y, abs(x - sx) + abs(y - sy));
    return 0;
}
