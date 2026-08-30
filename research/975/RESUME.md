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
