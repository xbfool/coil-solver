// Coil solver — v6: 束搜索（宽度受限的宽搜），不回溯
//
// 实测依据（experiments/branching.py）：沿真解走，**87% 的决策点只有 1 个候选能活过剪枝**，
// 13% 是二选一，三选一几乎不存在。也就是说路径几乎被逼死，偶尔一个岔路。
// 这种剖面下深度优先回溯是最差的选择——一次早期猜错要等到几百步后才暴露，然后整棵子树白搜；
// 而束搜索把所有活着的假设并行往前推，岔路自然被后面的剪枝淘汰，永远不需要回头。
// 还有个实际好处：内存和时间都是线性可控的，不像回溯会指数爆炸。
//
// 每个状态 = 位图(未访问格) + 当前位置 + 两色计数 + 指向路径树的一个节点。
// 扩展时先用 O(1) 的奇偶剪枝过一遍，再按启发式排序，只对排前面的做 O(剩余) 的连通性+死端检查，
// 收满 K 个就停 —— 把最贵的检查限制在幸存者上。
//
// 用法: v6-beam [关卡文件]   环境变量 BEAM=宽度（默认自适应：从 64 开始，失败翻倍重来）

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int W, N, nw;                 // 带哨兵边框的宽、格数、位图字数
static unsigned char *col;
static int delta[4];
static const char DCH[4] = { 'L', 'U', 'R', 'D' };
static int *fstack;
static int32_t *seen, seen_id;

static inline int getb(const uint64_t *v, int c) { return (v[c >> 6] >> (c & 63)) & 1; }
static inline void clrb(uint64_t *v, int c) { v[c >> 6] &= ~(1ULL << (c & 63)); }
static inline void setb(uint64_t *v, int c) { v[c >> 6] |= 1ULL << (c & 63); }

// 路径树：每步只记 (父节点, 方向)，最后回溯出整条路径，避免每个状态都存一份路径
typedef struct { int parent; int dir; } Node;
static Node *arena;
static int arena_n, arena_cap;

static int new_node(int parent, int dir) {
    if (arena_n == arena_cap) {
        arena_cap = arena_cap ? arena_cap * 2 : (1 << 16);
        arena = realloc(arena, sizeof(Node) * (size_t)arena_cap);
    }
    arena[arena_n].parent = parent; arena[arena_n].dir = dir;
    return arena_n++;
}

typedef struct { uint64_t *vis; int pos, rem, c0, c1, node, score; } State;

// 连通性 + 死端：剩余格必须都能从 p 到达，且最多 1 个死端（路径终点）
static int deep_ok(const uint64_t *vis, int p, int rem) {
    int top = 0, count = 0, dead = 0;
    ++seen_id;
    for (int d = 0; d < 4; ++d) {
        int c = p + delta[d];
        if (getb(vis, c) && seen[c] != seen_id) { seen[c] = seen_id; fstack[top++] = c; }
    }
    while (top) {
        int c = fstack[--top];
        ++count;
        int deg = 0;
        for (int d = 0; d < 4; ++d) {
            int n = c + delta[d];
            if (getb(vis, n)) {
                ++deg;
                if (seen[n] != seen_id) { seen[n] = seen_id; fstack[top++] = n; }
            } else if (n == p) ++deg;
        }
        if (deg <= 1 && ++dead > 1) return 0;
    }
    return count == rem;
}

static inline int freedeg(const uint64_t *v, int c) {
    return getb(v, c + delta[0]) + getb(v, c + delta[1]) + getb(v, c + delta[2]) + getb(v, c + delta[3]);
}

int main(int argc, char **argv) {
    FILE *in = stdin;
    if (argc > 1 && !(in = fopen(argv[1], "r"))) { fprintf(stderr, "open failed\n"); return 1; }
    size_t cap = 1 << 20, rd = 0;
    char *buf = malloc(cap);
    for (;;) {
        if (rd + 65536 > cap) buf = realloc(buf, cap *= 2);
        size_t r = fread(buf + rd, 1, 65536, in);
        if (!r) break;
        rd += r;
    }
    buf[rd] = 0;
    if (in != stdin) fclose(in);

    int w = atoi(strstr(buf, "x=") + 2), h = atoi(strstr(buf, "y=") + 2);
    char *pb = strstr(buf, "board=") + 6;
    W = w + 2; N = W * (h + 2); nw = (N + 63) / 64;

    col = malloc(N); fstack = malloc(sizeof(int) * (size_t)N); seen = calloc(N, sizeof(int32_t));
    delta[0] = -1; delta[1] = -W; delta[2] = 1; delta[3] = W;
    for (int c = 0; c < N; ++c) col[c] = (unsigned char)(((c % W) + (c / W)) & 1);

    uint64_t *base = calloc(nw, 8);
    int total = 0, tc[2] = { 0, 0 };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (pb[(size_t)y * w + x] == '.') {
                int c = (y + 1) * W + (x + 1);
                setb(base, c); ++total; ++tc[col[c]];
            }
    if (!total) { printf("x=0&y=0&path=\n"); return 0; }

    int K0 = getenv("BEAM") ? atoi(getenv("BEAM")) : 64;

    for (int K = K0; K <= (1 << 20); K *= 4) {          // 失败就加宽重来
        int CK = K * 4;                                  // 候选池
        State *beam = malloc(sizeof(State) * (size_t)K);
        State *cand = malloc(sizeof(State) * (size_t)CK);
        uint64_t *bbuf = malloc((size_t)K * nw * 8);
        uint64_t *cbuf = malloc((size_t)CK * nw * 8);
        int *ord = malloc(sizeof(int) * (size_t)CK);
        for (int i = 0; i < K; ++i) beam[i].vis = bbuf + (size_t)i * nw;
        for (int i = 0; i < CK; ++i) cand[i].vis = cbuf + (size_t)i * nw;
        arena_n = 0;

        // ---- 播种：扫全盘所有起点，按 freedeg 分桶收集，保证起点不偏向左上角 ----
        int nb = 0, nc = 0;
        uint64_t *tmp = malloc((size_t)nw * 8);
        for (int bucket = 0; bucket <= 4 && nc < CK; ++bucket) {
            for (int s = 0; s < N && nc < CK; ++s) {
                if (!getb(base, s)) continue;
                for (int d = 0; d < 4 && nc < CK; ++d) {
                    int dd = delta[d], c = s, len = 0;
                    while (getb(base, c + dd) && c + dd != s) { c += dd; ++len; }
                    if (!len) continue;
                    int c0 = tc[0], c1 = tc[1];
                    for (int k = 0, q = s; k <= len; ++k) { if (col[q] == 0) --c0; else --c1; q += dd; }
                    int rem = total - 1 - len;
                    if (rem) {
                        int need = (rem + 1) / 2;
                        if ((col[c] ^ 1) ? (c1 != need) : (c0 != need)) continue;
                    }
                    memcpy(tmp, base, (size_t)nw * 8);
                    clrb(tmp, s);
                    for (int k = 0, q = s; k < len; ++k) { q += dd; clrb(tmp, q); }
                    if (freedeg(tmp, c) != bucket) continue;      // 只收本桶的
                    memcpy(cand[nc].vis, tmp, (size_t)nw * 8);
                    cand[nc].pos = c; cand[nc].rem = rem; cand[nc].c0 = c0; cand[nc].c1 = c1;
                    cand[nc].node = new_node(-1 - s, d);
                    cand[nc].score = bucket;
                    ++nc;
                }
            }
        }
        free(tmp);

        int solved = -1;
        for (int i = 0; i < nc; ++i) ord[i] = i;
        for (int i = 1; i < nc; ++i) {
            int t = ord[i], j = i;
            while (j > 0 && cand[ord[j - 1]].score > cand[t].score) { ord[j] = ord[j - 1]; --j; }
            ord[j] = t;
        }
        for (int i = 0; i < nc && nb < K; ++i) {
            State *cd = &cand[ord[i]];
            if (cd->rem == 0) { solved = cd->node; break; }
            if (!deep_ok(cd->vis, cd->pos, cd->rem)) continue;
            memcpy(beam[nb].vis, cd->vis, (size_t)nw * 8);
            beam[nb].pos = cd->pos; beam[nb].rem = cd->rem;
            beam[nb].c0 = cd->c0; beam[nb].c1 = cd->c1;
            beam[nb].node = cd->node;
            ++nb;
        }

        // ---- 逐轮推进 ----
        while (solved < 0 && nb > 0) {
            nc = 0;
            for (int i = 0; i < nb; ++i) {
                State *st = &beam[i];
                int kids = 0;
                for (int d = 0; d < 4 && nc < CK && kids < 3; ++d) {
                    int dd = delta[d];
                    if (!getb(st->vis, st->pos + dd)) continue;
                    uint64_t *v = cand[nc].vis;
                    memcpy(v, st->vis, (size_t)nw * 8);
                    int c = st->pos, len = 0, c0 = st->c0, c1 = st->c1;
                    while (getb(v, c + dd)) {
                        c += dd; clrb(v, c); ++len;
                        if (col[c] == 0) --c0; else --c1;
                    }
                    int rem = st->rem - len;
                    if (rem) {                                  // O(1) 奇偶剪枝
                        int need = (rem + 1) / 2;
                        if ((col[c] ^ 1) ? (c1 != need) : (c0 != need)) continue;
                    }
                    cand[nc].pos = c; cand[nc].rem = rem; cand[nc].c0 = c0; cand[nc].c1 = c1;
                    cand[nc].node = new_node(st->node, d);
                    ++kids;
                    cand[nc].score = rem ? rem * 8 + freedeg(v, c) : -1;   // 进度优先，Warnsdorff 破并列
                    ++nc;
                }
            }
            // 排索引而不是搬位图
            for (int i = 0; i < nc; ++i) ord[i] = i;
            for (int i = 1; i < nc; ++i) {
                int t = ord[i], j = i;
                while (j > 0 && cand[ord[j - 1]].score > cand[t].score) { ord[j] = ord[j - 1]; --j; }
                ord[j] = t;
            }
            int nn = 0;
            for (int i = 0; i < nc && nn < K; ++i) {
                State *cd = &cand[ord[i]];
                if (cd->rem == 0) { solved = cd->node; break; }
                if (!deep_ok(cd->vis, cd->pos, cd->rem)) continue;
                memcpy(beam[nn].vis, cd->vis, (size_t)nw * 8);
                beam[nn].pos = cd->pos; beam[nn].rem = cd->rem;
                beam[nn].c0 = cd->c0; beam[nn].c1 = cd->c1;
                beam[nn].node = cd->node;
                ++nn;
            }
            nb = nn;
        }

        if (solved >= 0) {
            int len = 0;
            for (int t = solved; t >= 0; t = arena[t].parent) ++len;   // 到负数编码的起点为止
            char *out = malloc(len + 1);
            int s = -1, i = len;
            out[len] = 0;
            for (int t = solved; ; t = arena[t].parent) {
                out[--i] = DCH[arena[t].dir];
                if (arena[t].parent < 0) { s = -1 - arena[t].parent; break; }
            }
            printf("x=%d&y=%d&path=%s\n", (s % W) - 1, (s / W) - 1, out);
            return 0;
        }
        free(beam); free(cand); free(bbuf); free(cbuf); free(ord);
        fprintf(stderr, "beam K=%d 走空，加宽重来\n", K);
    }
    fprintf(stderr, "no solution found\n");
    return 1;
}
