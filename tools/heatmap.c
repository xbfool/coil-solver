// heatmap.c —— 生长式 backbite 采样器：无种子长哈密顿路 + 端点漂移热图
// (2026-08-29 端点定针富集38×实验的产线化; 排序自由/裁剪必须证明 —— 本工具只产排序)
//
// 用法: heatmap <board> <out.heat> [grow_timeout_s=600] [walk_moves=300000] [seed=1]
// 输出: 每行 "x y heat", 按热度降序; stderr 报进度与摘要。
// 机制: 生长期 = 尖端有未访问邻居则延伸,否则 backbite 换向;铺满全盘后进入
//       游走期,每 256 次接受移动采样两端点计热。刚性把端点挤进柔性区(针区)。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int w, h, nfree;
static char *cells;
static int *buf, cap, head, tail;   // 路径占 buf[head..tail]
static int *pos;                    // cell -> buf 下标 (-1 不在路上)
static unsigned long long rng;
static unsigned long long xr(void) {
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng;
}
static int freec(int x, int y) {
    return x >= 0 && y >= 0 && x < w && y < h && cells[y * w + x] == '.';
}
static int nbr(int c, int k) {   // 第 k 个邻居 cell 或 -1
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    int x = c % w + dx[k], y = c / w + dy[k];
    return freec(x, y) ? y * w + x : -1;
}
static void recenter(void) {
    int len = tail - head + 1, nh = (cap - len) / 2;
    memmove(buf + nh, buf + head, sizeof(int) * (size_t)len);
    head = nh; tail = nh + len - 1;
    for (int i = head; i <= tail; ++i) pos[buf[i]] = i;
}
static void rev(int a, int b) {   // 反转 buf[a..b] 并同步 pos
    while (a < b) {
        int t = buf[a]; buf[a] = buf[b]; buf[b] = t;
        pos[buf[a]] = a; pos[buf[b]] = b; ++a; --b;
    }
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "用法: heatmap <board> <out> [grow_s] [walk_moves] [seed]\n"); return 2; }
    FILE *bf = fopen(argv[1], "r");
    if (!bf) { fprintf(stderr, "打不开盘面 %s\n", argv[1]); return 2; }
    static char raw[1 << 22];
    size_t n = fread(raw, 1, sizeof raw - 1, bf); raw[n] = 0; fclose(bf);
    w = atoi(strstr(raw, "x=") + 2); h = atoi(strstr(raw, "y=") + 2);
    cells = strstr(raw, "board=") + 6;
    int grow_s = argc > 3 ? atoi(argv[3]) : 600;
    long long walk_moves = argc > 4 ? atoll(argv[4]) : 300000;
    rng = argc > 5 ? (unsigned long long)atoll(argv[5]) * 2654435761ull + 88172645463325252ull
                   : 88172645463325252ull;
    nfree = 0;
    for (int i = 0; i < w * h; ++i) if (cells[i] == '.') ++nfree;
    cap = nfree * 2 + 8;
    buf = malloc(sizeof(int) * (size_t)cap);
    pos = malloc(sizeof(int) * (size_t)(w * h));
    long long *heat = calloc((size_t)(w * h), sizeof(long long));
    for (int i = 0; i < w * h; ++i) pos[i] = -1;
    // ---- 生长期改 Warnsdorff 贪心蛇填 + 卡死重启（backbite 生长有走廊钉扎, 见 dead_ends）----
    int *freelist = malloc(sizeof(int) * (size_t)nfree);
    int nf2 = 0;
    for (int i = 0; i < w * h; ++i) if (cells[i] == '.') freelist[nf2++] = i;
    time_t t0 = time(0);
    int len = 0, attempts = 0, bestlen = 0;
    while (time(0) - t0 <= (argc > 3 ? atoi(argv[3]) : 600)) {
        ++attempts;
        for (int i = 0; i < w * h; ++i) pos[i] = -1;
        int s0 = freelist[(xr() >> 32) % (unsigned)nfree];
        head = tail = cap / 2; buf[head] = s0; pos[s0] = head; len = 1;
        long long stuck_rescues = 0;
        while (len < nfree) {
            int at_head = (int)((xr() >> 33) & 1);
            int tip = at_head ? buf[head] : buf[tail];
            int adj2 = (len > 1) ? (at_head ? buf[head + 1] : buf[tail - 1]) : -1;
            // Warnsdorff: 未访问邻居里选"自身未访问邻居数最少"者
            int bestc = -1, bestd = 9;
            for (int k = 0; k < 4; ++k) {
                int c = nbr(tip, k);
                if (c < 0 || pos[c] >= 0) continue;
                int d = 0;
                for (int k2 = 0; k2 < 4; ++k2) { int c2 = nbr(c, k2); if (c2 >= 0 && pos[c2] < 0) ++d; }
                if (d < bestd || (d == bestd && ((xr() >> 32) & 1))) { bestd = d; bestc = c; }
            }
            if (bestc >= 0) {
                if (at_head) { if (head == 0) recenter(); buf[--head] = bestc; pos[bestc] = head; }
                else { if (tail == cap - 1) recenter(); buf[++tail] = bestc; pos[bestc] = tail; }
                ++len; continue;
            }
            // 本端卡死: 试 backbite 抢救(换向后可能开出新延伸口), 限额防死循环
            if (++stuck_rescues > (long long)nfree * 8) break;
            int vis2[4], nv2 = 0;
            for (int k = 0; k < 4; ++k) {
                int c = nbr(tip, k);
                if (c >= 0 && c != adj2 && pos[c] >= 0) vis2[nv2++] = c;
            }
            if (!nv2) { if (len >= nfree) break; else { /* 此端完全死, 换端 */ continue; } }
            int c = vis2[(xr() >> 32) % (unsigned)nv2], j = pos[c];
            if (at_head) rev(head, j - 1); else rev(j + 1, tail);
        }
        if (len > bestlen) bestlen = len;
        if (len == nfree) break;
    }
    fprintf(stderr, "生长: %d 次尝试, 最佳覆盖 %d/%d (%.1f%%)%s\n", attempts, bestlen, nfree,
            100.0 * bestlen / nfree, len == nfree ? " ✅铺满" : "");
    if (len < nfree) { fprintf(stderr, "种子未长成, 退出\n"); return 1; }
    long long moves = 0, acc = 0, samples = 0;
    int full_at_move = -1;
    while (1) {
        if (len < nfree && time(0) - t0 > grow_s) break;
        if (len == nfree) {
            if (full_at_move < 0) {
                full_at_move = 1;
                fprintf(stderr, "铺满全盘: %lld 步 %llds\n", moves, (long long)(time(0) - t0));
                moves = 0;
            }
            if (moves >= walk_moves) break;
        }
        ++moves;
        if ((moves & ((1 << 24) - 1)) == 0) {
            int bad = 0, cnt = tail - head + 1;
            for (int i = head; i < tail; ++i) {
                int a = buf[i], b2 = buf[i + 1], ok = 0;
                for (int k = 0; k < 4; ++k) if (nbr(a, k) == b2) ok = 1;
                if (!ok) { ++bad; }
            }
            fprintf(stderr, "  moves=%lldM len=%d(实%d) acc=%lld 坏边=%d 头(%d,%d) 尾(%d,%d)\n",
                    moves >> 20, len, cnt, acc, bad,
                    buf[head] % w, buf[head] / w, buf[tail] % w, buf[tail] / w);
        }
        int at_head = (int)((xr() >> 33) & 1);
        int tip = at_head ? buf[head] : buf[tail];
        int adj = (len > 1) ? (at_head ? buf[head + 1] : buf[tail - 1]) : -1;
        int ext[4], ne = 0, vis[4], nv = 0;
        for (int k = 0; k < 4; ++k) {
            int c = nbr(tip, k);
            if (c < 0 || c == adj) continue;
            if (pos[c] < 0) ext[ne++] = c; else vis[nv++] = c;
        }
        if (len < nfree && ne && (nv == 0 || ((xr() >> 32) & 3))) {   // 生长(25%让位给换向,破贪婪螺旋)
            int c = ext[(xr() >> 32) % (unsigned)ne];
            if (at_head) { if (head == 0) recenter(); buf[--head] = c; pos[c] = head; }
            else { if (tail == cap - 1) recenter(); buf[++tail] = c; pos[c] = tail; }
            ++len; ++acc;
        } else if (nv) {                             // backbite 换向
            int c = vis[(xr() >> 32) % (unsigned)nv], j = pos[c];
            if (at_head) rev(head, j - 1); else rev(j + 1, tail);
            ++acc;
            if (len == nfree && (acc & 255) == 0) {  // 游走期采样端点
                ++heat[buf[head]]; ++heat[buf[tail]]; ++samples;
            }
        }
    }
    fprintf(stderr, "覆盖 %d/%d (%.1f%%), 游走采样 %lld 次\n",
            len, nfree, 100.0 * len / nfree, samples);
    if (len < nfree && len * 100 < nfree * 95) {
        fprintf(stderr, "⚠ 覆盖不足95%%, 热图弃用\n");
        return 1;
    }
    FILE *of = fopen(argv[2], "w");
    // 简单选择排序太慢; 输出全部非零热,由调用方排序
    for (int i = 0; i < w * h; ++i)
        if (heat[i]) fprintf(of, "%d %d %lld\n", i % w, i / w, heat[i]);
    fclose(of);
    return 0;
}
