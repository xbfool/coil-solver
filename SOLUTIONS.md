# 解的银行（solution bank）

**解不在这个仓库里，在 https://github.com/xbfool/coil-solutions（private）。**

## 为什么单独放、而且是 private

上游 `background.md` 明确写了偶数关加密的动机：

> "if solutions are generated, then put publicly online, they may become part of future
> LLM training sets. This doesn't appear to have happened yet but may in the future."

而这个 benchmark **现在正被用来评测模型**（adum 的 coilbench 就是干这个的）。
把几百个解推到公开仓库，等于替它完成那件它一直在防的事。
所以：**代码和笔记公开，解走 private 仓库**。备份的目的（别丢机时）一样达到。

⚠️ 已知的一处例外：`research/l101sol.txt` 里有 L101 的完整解，在本仓库变成 public 之前就提交了。
只此一关，量级上无所谓，但如果要彻底干净就得改写历史。

## 为什么必须备份

打榜成绩线（现在 694）**完全依赖本地缓存的解**。这些解是几百小时机时换来的，
有的单关要跑几十分钟（L664 离线 2432 秒）。它们只存在于跑夜班那台机器的本地磁盘上，
且被 `.gitignore` 排除，**一次误删或一块坏盘就全没了，而且没法快速重现**。

## 在有解的那台机器上执行

```bash
cd /path/to/coil-solver
git clone git@github.com:xbfool/coil-solutions.git bank
cp -a solutions/* bank/
cd bank && git add -A && git commit -m "solution bank: N 个已校验的解" && git push
```

之后每晚流水线跑完追加一次即可。建议在 bank 里放一个 `INDEX.md`，
记清楚每关的**起点坐标 + 求解耗时 + 是 LEGAL 还是 OFFLINE**，
这样不用解开文件就能看出哪些关是"合规能打"的，哪些只能靠缓存上榜。
