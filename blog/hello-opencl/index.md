---
title: "从 Vector Add 到 GEMM 的第一套 OpenCL 程序"
date: 2026-04-01T15:30:00+08:00
lastmod: 2026-04-15T21:20:00+08:00
draft: false
description: "先用最小 vector add 跑通 OpenCL，再把思路迁移到 GEMM"
slug: "hello-opencl"
tags: ["opencl"]
categories: ["opencl"]
comments: true
math: true
---

# 从 Vector Add 到 GEMM 的第一套 OpenCL 程序

当前目录里已经有一个最小的 `vector_add.cl`。这正好适合作为第一步，因为 OpenCL 真正难的往往不是数学，而是“Host 侧有没有把一次 kernel 调起来”。

## 1. 为什么不一上来就写 GEMM

因为 GEMM 会同时引入太多变量：

- 二维 NDRange
- 更复杂的索引
- local memory 分块
- 性能测试和正确性验证

如果你连最小的一维 kernel 都没跑通，直接上 GEMM 很容易把问题混在一起。

## 2. 第一阶段: 跑通最小 vector add

当前的最小 kernel 逻辑就是：

```c
__kernel void vector_add(
    __global const float *a,
    __global const float *b,
    __global float *c,
    const unsigned int n
) {
    int id = get_global_id(0);
    if (id < n) {
        c[id] = a[id] + b[id];
    }
}
```

这个例子足够教会你三件事：

- kernel 参数怎么传。
- `global_id` 怎么映射到数据索引。
- 输入输出 buffer 怎么在 Host 和 Device 间流动。

## 3. Host 侧要做的事情

即便是最小例子，Host 侧也要完整走一遍：

1. 枚举 platform 和 device。
2. 创建 context 和 command queue。
3. 读取 `.cl` 源码并 build。
4. 创建 buffer。
5. 设置 kernel 参数。
6. enqueue kernel。
7. 读回结果。

这一步真正的目标不是追求性能，而是把 OpenCL 的运行链路跑通。

## 4. 第二阶段: 从 vector add 迁移到 GEMM

等最小 demo 稳定后，再把问题升级到 GEMM 会自然很多。

你会新增三类复杂度：

### 4.1 索引从一维变二维

你要从：

```text
id
```

变成：

```text
row, col
```

### 4.2 每个 work-item 不再只做一次加法

而是要做一段长度为 `K` 的累加。

### 4.3 访存会成为主要问题

朴素 GEMM 的性能瓶颈几乎一定落在 global memory 访问上，所以后面必须引入：

- `local memory`
- `tiling`
- `barrier`

## 5. GEMM 版本你应该怎么分阶段写

我建议按下面顺序写，而不是一步到位：

1. 先写最朴素二维 GEMM。
2. 先确保结果正确。
3. 再做 `local memory` 分块。
4. 最后再做向量化和参数调优。

这样你每次只改一个层次，问题更容易定位。

## 6. 如何判断自己已经过了“入门关”

如果你已经能稳定做到下面几件事，就算过了第一关：

- 能把 `.cl` 源码编译成功。
- 能打印 build log 排错。
- 能用一个最小 kernel 跑出正确结果。
- 能解释 `global size / local size / work-group` 的关系。

这时候再去写 GEMM、卷积、image2d，效率会高很多。

## 7. 这一篇的结论

OpenCL 的第一套程序，不应该是“最炫的 kernel”，而应该是“最短但完整的一条链”。

先把最小 `vector add` 跑通，再把这条链迁移到 GEMM，你后面做 Android GPU 优化时会稳很多。
