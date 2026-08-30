# 975 现场恢复卡（2026-08-30 夜存档）

状态: 6184 决赛候选已处决 1972, 存活 4212(pins-resume.startord, 保序)。
公理 40748 条在 research/975/axioms.txt(重启后 /tmp 会丢, 先复制):

    cp research/975/axioms.txt /tmp/axioms_975.txt

恢复开火(28核, 断点续磨):

    cd /mnt/d/workspace/coil-solver && ulimit -s unlimited && \
    nohup env STARTORD=research/975/pins-resume.startord STARTONLY=1 STATS=1 TREELOG=1 \
      RESTART=1 RESTARTK=12 SWEEP=16000000 JOBS=28 FORCEEDGEFILE=/tmp/axioms_975.txt \
      /tmp/king-ax /mnt/d/workspace/coilbench/levels_all/975 \
      > research/975/fleet.sol 2> research/975/fleet3.log &

⚠ /tmp/king-ax 重启后也丢: cp evolve/solver.c /tmp/kb/ 用冠军policy重编(见 CASE-814 配方)。
出解后: coil_check 验证 -> 解入库 -> 提交 -> coil_ladder.py 放闸 -> 拉起 978/980/983(送1041)/987/994/996/997 队列(同配方)。
损失评估: 关机只丢在飞轮次的部分预算(~1小时当量), 处决账本/公理/名单全在盘上。

## 明早科研议程: 筛选2.0(Tron的99%在我们这的对应物)
现状: 975普通探针筛88.4%(53k->6184), Tron口径99%; 我们的直系武器=NESTPROBE强筛
(747战绩42178->453=98.9%, 从未在零信息池上基准过)。
1. A/B(降档廉价层)结果见 /tmp/ab634 或重跑: DEFAULT vs TIER2=0 vs TIER2=0 PROBE=300
2. NESTPROBE零信息基准: env NESTPROBE=1 + STARTORD=pins-resume(4191) + FUNNELDUMP,
   量: 强筛率/每起点成本/针存活(975针未知,以"体量下降幅度+889真值复测"双口径)
3. 若强筛率>90%: axcrack插入强筛层(廉价probe -> NESTPROBE strong -> 蒸馏 -> 舰队)
