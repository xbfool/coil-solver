// ============================================================================
// evolve/policy.h —— 唯一允许进化的文件（EVOLVE-PLAN P3）
//
// solver.c 是 v78-rpbj.c 的冻结副本，所有可进化决策都路由到本文件。
// 约束（对进化 LLM 的硬规则）：
//   1. 只许改 EVOLVE-BLOCK 标记之间的内容。
//   2. 必须保持 sound：这里只决定"先试哪个分支/多久触发一次演绎武器/预算多大"，
//      不许发明新剪枝（新剪枝=可能误杀解，必须走人工 REFSOL 审计流程）。
//   3. 函数签名和宏名不许改（solver.c 引用它们）。
//   4. 分数越小越先试；周期越小触发越密（越贵）。
// 基线 = v78 生产默认值逐字搬运，编译后与 v78 逐节点等价（已验证）。
// ============================================================================
#ifndef COIL_POLICY_H
#define COIL_POLICY_H

// EVOLVE-BLOCK-START

// ---- 分支排序：返回候选落点的优先分数（小者先试）----
// tier: swarm 策略梯队(0=保底 Warnsdorff, 1=温和抖动, 2=大抖动, 3=纯哈希序);
//       单进程/校准盘恒为 0。
// h: 与 (落点,深度,shard,restart轮) 绑定的杂凑（抖动源，可复现）
// freedeg_c: 落点的自由邻居数（Warnsdorff 核心信号）
// depth: 当前决策深度   len: 本次滑行长度   rem2: 走完后剩余格数
static inline int policy_branch_score(int tier, unsigned int h, int freedeg_c,
                                      int depth, int len, int rem2) {
    (void)depth; (void)len; (void)rem2;
    if (tier == 0) return freedeg_c * 32;
    if (tier == 1) return freedeg_c * 32 + (int)(h & 31);
    if (tier == 2) return freedeg_c * 32 + (int)(h & 95);
    return (int)(h & 1023);
}

// ---- 自适应武器周期（v78 ADAPT: region_parity / propagate_dyn 触发密度自调）----
#define POLICY_WP_INIT 128    // 初始周期（每 N 节点触发一次）
#define POLICY_WP_MIN  24     // 最密
#define POLICY_WP_MAX  8192   // 最疏
static inline int policy_wp_shrink(int p) { return p - p / 4 - 1; }  // 命中→加密
static inline int policy_wp_grow(int p)   { return p + p / 8 + 1; }  // 落空→放疏

// ---- 演绎武器默认开关/密度（0=关；生产配方 BJ=500 只在 env 显式给出时生效）----
#define POLICY_BJ_DEFAULT     0   // 帧入口 propagate_dyn 回跳剪枝周期
#define POLICY_ADAPT_DEFAULT  0   // 自适应武器组
#define POLICY_RESTART_DEFAULT 0  // Luby 随机重启轮

// ---- 探针预算（第一层证伪漏斗）----
#define POLICY_PROBE_ZERO 650     // 零信息类（全局阶段固定 0 边）的探针预算
#define POLICY_PROBE_INFO 2000    // 其余类
#define POLICY_PROBE2     32000   // 第二层重扫预算

// ---- 正式搜索的扫荡调度 ----
#define POLICY_SWEEP0    3000000  // 首轮预算（按赢家树尺度定标，不是怪物树）
#define POLICY_SWEEPMUL  8        // 逐轮倍增
#define POLICY_RESTARTK  24       // RESTART 模式的轮数

// ---- 传播深度自适应 ----
#define POLICY_SURVDEEP 38        // 第一层存活率 >= 此百分比才升深层传播
#define POLICY_DYNMIN   15        // 分支传播证伪率 < 此百分比则降回浅层

// EVOLVE-BLOCK-END

#endif
