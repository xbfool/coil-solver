#!/bin/bash
# GPU v2 内核构建（WSL nvcc 12.4 出 compute_90 PTX，驱动在 sm_120 上 JIT）
cd "$(dirname "$0")"
nvcc -O3 -arch=compute_90 -code=compute_90 -o sweep2 sweep2.cu
# 差分测试基准（CPU v16 语义，逐起点打印 x y result nodes）
gcc -O2 -o oracle16 oracle16.c
