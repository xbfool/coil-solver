// Coil GPU 原型 v1：树级并行限额扫荡
//
// 设计：一个线程磨一棵候选树（起点），滑动 DFS + 显式栈，节点预算统一（负载均衡）。
// 剪枝集（v1 精简版，全部 sound）：奇偶染色计数、滑行落点死路即回溯、预算封顶。
// 不带：传播、连通性检查（v2 再上）——v1 只回答一个问题：5090 的聚合节点速率是多少。
//
// 编译：nvcc -O3 -arch=compute_90 -code=compute_90 -o sweep sweep.cu
// 运行：./sweep <levelfile> [budget-per-tree] [maxtrees]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cuda_runtime.h>

#define CK(x) do { cudaError_t e = (x); if (e) { printf("CUDA err %s @%d\n", cudaGetErrorString(e), __LINE__); exit(1); } } while (0)

// 每棵树的持久状态（全局显存）
struct TreeState {
    long long nodes;      // 消耗的节点数
    int result;           // 0=预算尽 1=解出 2=搜穷（证伪）
    int start;            // 起点格
};

// 栈帧：每层一个滑行决策
// dirmask: 还没试过的方向位集；len: 当前已走滑行的长度（回溯用）；dir: 当前已走方向
struct Frame { unsigned char dirtried, dir; short len; };

__constant__ int cW, cH, cN, cTotalFree;
__constant__ int cDelta[4];

__global__ void sweep_kernel(const unsigned char *g0, const int *starts, int nstarts,
                             unsigned char *vis_all, Frame *stk_all,
                             TreeState *ts, long long budget, int *found_flag)
{
    int t = blockIdx.x * blockDim.x + threadIdx.x;
    if (t >= nstarts) return;
    unsigned char *vis = vis_all + (size_t)t * cN;         // 0=自由 1=墙/已走
    Frame *stk = stk_all + (size_t)t * (cTotalFree + 2);

    for (int i = 0; i < cN; ++i) vis[i] = g0[i] ? 0 : 1;

    int s = starts[t];
    int p = s;                     // 当前头
    vis[s] = 1;
    int remaining = cTotalFree - 1;
    // 奇偶染色计数（col = (x+y)&1）
    int cnt0 = 0, cnt1 = 0;
    for (int c = 0; c < cN; ++c) if (!vis[c]) { if (((c % cW) + (c / cW)) & 1) ++cnt1; else ++cnt0; }

    long long nodes = 0;
    int sp = 0;                    // 栈深
    stk[0].dirtried = 0; stk[0].dir = 0; stk[0].len = 0;
    int result = 0;

    // 方向次序按线程号旋转——不同树不同次序，白捡多样性
    int rot = t & 3;

    while (true) {
        if (*found_flag) { result = 0; break; }            // 别的树解出，全员收工
        if (remaining == 0) { result = 1; atomicExch(found_flag, t + 1); break; }
        if (nodes >= budget) { result = 0; break; }

        // Warnsdorff：没试过的方向里选「滑行落点自由度最小」的（好方向先走，坏方向当证伪材料）
        int tried = stk[sp].dirtried, d = -1, best = 1 << 30;
        for (int k = 0; k < 4; ++k) {
            int dk = (k + rot) & 3;
            if (tried & (1 << dk)) continue;
            if (vis[p + cDelta[dk]]) { tried |= (1 << dk); continue; }
            int dd2 = cDelta[dk], q2 = p;
            while (!vis[q2 + dd2]) q2 += dd2;
            int deg2 = 0;
            for (int k3 = 0; k3 < 4; ++k3) if (!vis[q2 + cDelta[k3]]) ++deg2;
            if (deg2 < best) { best = deg2; d = dk; }
        }
        stk[sp].dirtried = tried;

        if (d < 0) {
            // 本层穷尽 => 回溯
            if (sp == 0) { result = 2; break; }            // 整棵树搜穷（证伪）
            int len = stk[sp].len, bd = stk[sp].dir, dd = cDelta[bd];
            for (int k = 0; k < len; ++k) {
                vis[p] = 0;
                if (((p % cW) + (p / cW)) & 1) ++cnt1; else ++cnt0;
                p -= dd;
            }
            remaining += len;
            --sp;
            continue;
        }

        stk[sp].dirtried |= (1 << d);
        ++nodes;
        int dd = cDelta[d];
        // 滑行：走到撞墙/撞已走
        int q = p, len = 0;
        while (!vis[q + dd]) { q += dd; ++len; }
        // 标记
        int pp = p;
        for (int k = 0; k < len; ++k) {
            pp += dd; vis[pp] = 1;
            if (((pp % cW) + (pp / cW)) & 1) --cnt1; else --cnt0;
        }
        int rem2 = remaining - len;

        // 奇偶剪枝：剩余两色数差 >1 不可能（终点色未知的保守版）
        int diff = cnt0 - cnt1; if (diff < 0) diff = -diff;
        int prune = (diff > 1);
        // 落点死路剪枝：rem2>0 且落点四个方向全堵 => 回溯
        if (!prune && rem2 > 0) {
            int deg = 0;
            for (int k2 = 0; k2 < 4; ++k2) if (!vis[q + cDelta[k2]]) ++deg;
            if (deg == 0) prune = 1;
        }
        if (!prune && rem2 > 0) {                          // 滑行沿线的搁浅格：邻格 0 度且不是落点 => 死局
            int qq = p;
            for (int k2 = 0; k2 < len && !prune; ++k2) {
                qq += dd;
                for (int k3 = 0; k3 < 4 && !prune; ++k3) {
                    int nb = qq + cDelta[k3];
                    if (vis[nb] || nb == q) continue;
                    if (nb == q + cDelta[0] || nb == q + cDelta[1] ||
                        nb == q + cDelta[2] || nb == q + cDelta[3]) continue;   // 紧邻落点的格子可被下一步滑入，不算搁浅
                    int dg = 0;
                    for (int k4 = 0; k4 < 4; ++k4) if (!vis[nb + cDelta[k4]]) ++dg;
                    if (dg == 0) prune = 1;
                }
            }
        }
        if (prune) {
            for (int k = 0; k < len; ++k) {
                vis[pp] = 0;
                if (((pp % cW) + (pp / cW)) & 1) ++cnt1; else ++cnt0;
                pp -= dd;
            }
            continue;                                       // 同层试下一个方向
        }
        // 落子
        ++sp;
        stk[sp].dirtried = 0; stk[sp].dir = d; stk[sp].len = len;
        p = q; remaining = rem2;
    }
    ts[t].nodes = nodes; ts[t].result = result; ts[t].start = s;
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s <level> [budget] [maxtrees]\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("no level\n"); return 1; }
    static char buf[1 << 22];
    size_t nread = fread(buf, 1, sizeof buf - 1, f); buf[nread] = 0; fclose(f);
    int w = 0, h = 0; char *bp = 0;
    sscanf(strstr(buf, "x="), "x=%d", &w);
    sscanf(strstr(buf, "y="), "y=%d", &h);
    bp = strstr(buf, "board=") + 6;

    int W = w + 2, H = h + 2, N = W * H;
    unsigned char *g0 = (unsigned char *)calloc(N, 1);
    int total_free = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (bp[y * w + x] == '.') { g0[(y + 1) * W + (x + 1)] = 1; ++total_free; }

    long long budget = argc > 2 ? atoll(argv[2]) : 2000000;
    int maxtrees = argc > 3 ? atoi(argv[3]) : 1 << 30;

    int *starts = (int *)malloc(sizeof(int) * N), ns = 0;
    for (int c = 0; c < N && ns < maxtrees; ++c) if (g0[c]) starts[ns++] = c;
    printf("board %dx%d free=%d starts=%d budget=%lld\n", w, h, total_free, ns, budget);

    int delta[4] = { -1, -W, 1, W };
    CK(cudaMemcpyToSymbol(cW, &W, 4)); CK(cudaMemcpyToSymbol(cH, &H, 4));
    CK(cudaMemcpyToSymbol(cN, &N, 4)); CK(cudaMemcpyToSymbol(cTotalFree, &total_free, 4));
    CK(cudaMemcpyToSymbol(cDelta, delta, 16));

    unsigned char *d_g0; int *d_starts; unsigned char *d_vis; Frame *d_stk; TreeState *d_ts; int *d_found;
    CK(cudaMalloc(&d_g0, N));
    CK(cudaMalloc(&d_starts, sizeof(int) * ns));
    CK(cudaMalloc(&d_vis, (size_t)ns * N));
    CK(cudaMalloc(&d_stk, (size_t)ns * (total_free + 2) * sizeof(Frame)));
    CK(cudaMalloc(&d_ts, sizeof(TreeState) * ns));
    CK(cudaMalloc(&d_found, 4));
    CK(cudaMemcpy(d_g0, g0, N, cudaMemcpyHostToDevice));
    CK(cudaMemcpy(d_starts, starts, sizeof(int) * ns, cudaMemcpyHostToDevice));
    CK(cudaMemset(d_found, 0, 4));
    printf("gpu mem: vis %.1fMB stk %.1fMB\n", ns * (double)N / 1e6, ns * (double)(total_free + 2) * sizeof(Frame) / 1e6);

    cudaEvent_t t0, t1; cudaEventCreate(&t0); cudaEventCreate(&t1);
    cudaEventRecord(t0);
    int TPB = 128;
    sweep_kernel<<<(ns + TPB - 1) / TPB, TPB>>>(d_g0, d_starts, ns, d_vis, d_stk, d_ts, budget, d_found);
    CK(cudaGetLastError());
    cudaEventRecord(t1);
    CK(cudaEventSynchronize(t1));
    float ms; cudaEventElapsedTime(&ms, t0, t1);

    TreeState *ts = (TreeState *)malloc(sizeof(TreeState) * ns);
    CK(cudaMemcpy(ts, d_ts, sizeof(TreeState) * ns, cudaMemcpyDeviceToHost));
    int found = 0; CK(cudaMemcpy(&found, d_found, 4, cudaMemcpyDeviceToHost));
    long long tot = 0; int nsolve = 0, nexh = 0, nbud = 0;
    for (int i = 0; i < ns; ++i) {
        tot += ts[i].nodes;
        if (ts[i].result == 1) ++nsolve;
        else if (ts[i].result == 2) ++nexh;
        else ++nbud;
    }
    printf("wall %.2fs  nodes %lld  aggregate %.1fM nodes/s\n", ms / 1000.0, tot, tot / (ms / 1000.0) / 1e6);
    printf("solved=%d exhausted=%d budget-capped=%d found_flag=%d\n", nsolve, nexh, nbud, found);
    return 0;
}
