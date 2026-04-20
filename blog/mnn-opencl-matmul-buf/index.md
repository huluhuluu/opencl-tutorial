---
title: "MNN OpenCL `matmul_buf.cl` 解析"
date: 2026-04-19T15:20:00+08:00
lastmod: 2026-04-19T15:20:00+08:00
draft: true
description: "围绕 MNN OpenCL buffer MatMul 实现，解释 `matmul_buf.cl` 的计算映射，以及 `MatmulBufExecution.cpp`、`matmul_buf_mnn_cl.cpp` 在 Host 侧分别负责什么。"
slug: "mnn-opencl-matmul-buf"
tags: ["opencl", "mnn", "matmul"]
categories: ["opencl"]
comments: true
math: true
---

# MNN OpenCL `matmul_buf.cl` 解析

这篇解释三份文件之间的关系：

- `MNN-batch/source/backend/opencl/execution/cl/matmul_buf.cl`
- `MNN-batch/source/backend/opencl/execution/buffer/MatmulBufExecution.cpp`
- `MNN-batch/source/backend/opencl/execution/cl/matmul_buf_mnn_cl.cpp`

如果只看 `matmul_buf.cl`，很容易以为它就是全部实现；实际上它只负责设备侧计算。真正把这个 kernel 变成一次可执行 OpenCL 调用，还需要 Host 侧的 `.cpp` 去：

- 根据 `M / N / K` 和转置信息选 kernel 变体
- 拼编译宏
- build kernel
- 设置参数
- 决定 `global work size / local work size`

## 1. `matmul_buf.cl` 解决的是什么问题

这份 kernel 是 `buffer` 路径下的一个通用 `MatMul` 回退实现，目标可以概括成：

```text
C[M, N] = A[M, K] x B[K, N]
```

它支持几类常见变化：

- `A` 是否转置：`TRANSPOSE_A`
- `B` 是否转置：`TRANSPOSE_B`
- 是否带 bias：`BIAS`
- `M / N / K` 不是 4 的整数倍时的尾块处理：`M_LEAVE`、`N_LEAVE`、`K_LEAVE`

之所以说它是“通用回退实现”，是因为 `MNN` 在 Host 侧并不是永远都选它。`MatmulBufExecution.cpp` 会优先尝试更强的 tile kernel：

- 大 tile：`matmul_params_buf`
- 小 tile + local memory：`matmul_local_buf`
- 都不满足时，才退回 `matmul_buf`

所以 `matmul_buf.cl` 的定位不是“最快”，而是“泛化最好、边界最全、能兜底”。

## 2. 一个 work-item 实际上在算一个 `4x4` 输出块

这个 kernel 的全局映射很关键：

```cpp
mGlobalWorkSize = {static_cast<uint32_t>(N_4), static_cast<uint32_t>(M_4)};
```

其中：

- `N_4 = UP_DIV(N, 4)`
- `M_4 = UP_DIV(M, 4)`

也就是说，`global id` 不是逐元素对应输出矩阵，而是按 `4x4` tile 对应。

kernel 一开始就这么取索引：

```c
int2 pos = (int2)(get_global_id(0), get_global_id(1)); // N M
const int idn = pos.x << 2;
const int idm = pos.y << 2;
```

这里：

- `pos.x` 表示当前 work-item 负责第几个 `N/4` 列块
- `pos.y` 表示当前 work-item 负责第几个 `M/4` 行块
- `idn = pos.x * 4`
- `idm = pos.y * 4`

所以一个 work-item 最终负责输出矩阵里从 `(idm, idn)` 开始的一个 `4x4` 子块。

这也是后面 `out[4]` 的来源：

```c
COMPUTE_FLOAT4 out[4];
```

它不是 4 个随便的向量，而是：

- `out[0]`：第 0 行的 4 个输出
- `out[1]`：第 1 行的 4 个输出
- `out[2]`：第 2 行的 4 个输出
- `out[3]`：第 3 行的 4 个输出

也就是一整个 `4x4` 小块。

## 3. 主循环怎么把 `K` 维累加起来

这份 kernel 的核心思想是把 `K` 维按 4 个元素一组展开。

```c
const int K4 = (K + 3) / 4;
for (int k = 0; k < loop_end; ++k) {
    int kindex = k << 2;
    COMPUTE_FLOAT4 A[4];
    COMPUTE_FLOAT4 B[4];
    ...
}
```

这里的直觉可以理解成：

- 从 `A` 里取出 4 行 x 4 个 `K` 元素
- 从 `B` 里取出 4 个 `K` 元素 x 4 列
- 然后做一次 `4x4` 的小矩阵乘

### 3.1 `A[4]` 和 `B[4]` 分别代表什么

在不考虑转置宏时：

- `A[0]` 是第 `idm + 0` 行在 `K` 方向上的 4 个连续元素
- `A[1]` 是第 `idm + 1` 行在 `K` 方向上的 4 个连续元素
- `A[2]` 是第 `idm + 2` 行在 `K` 方向上的 4 个连续元素
- `A[3]` 是第 `idm + 3` 行在 `K` 方向上的 4 个连续元素

而 `B[0] ~ B[3]` 则对应这 4 个 `K` 位置上，每个位置跨 4 列的值。

也就是说，代码在逻辑上构造的是：

```text
A_tile: 4 x 4
B_tile: 4 x 4
```

然后把它们累加到 `out[4]` 里。

### 3.2 为什么是四次 `mad`

核心计算是这几行：

```c
out[vec_m] = mad((COMPUTE_FLOAT4)A[vec_m].x, B[0], out[vec_m]);
out[vec_m] = mad((COMPUTE_FLOAT4)A[vec_m].y, B[1], out[vec_m]);
out[vec_m] = mad((COMPUTE_FLOAT4)A[vec_m].z, B[2], out[vec_m]);
out[vec_m] = mad((COMPUTE_FLOAT4)A[vec_m].w, B[3], out[vec_m]);
```

对固定的 `vec_m` 来说：

- `A[vec_m].x` 是这一行第 1 个 `K` 分量
- `B[0]` 是这个 `K` 分量对应的 4 列值
- 乘完之后直接加到当前行的 4 个输出上

再对 `y / z / w` 做同样事情，相当于把 `K` 方向这 4 个分量全部累加完。

所以这不是普通的“向量点乘”，而是：

- 每次用一个标量广播乘一个 `float4`
- 把 `4x4` 小块按外积方式积累起来

这类写法很适合 `OpenCL` 的 `float4 + mad` 模式。

## 4. 为什么有这么多 `LEAVE` 和 `TRANSPOSE` 宏

这份 kernel 最显眼的地方，就是一堆编译宏：

- `TRANSPOSE_A`
- `TRANSPOSE_B`
- `BIAS`
- `M_LEAVE`
- `N_LEAVE`
- `K_LEAVE`
- `M_LEAVE_NUM`
- `N_LEAVE_NUM`

这些宏的作用不是“代码可读性更差”，而是为了把慢分支尽量移到编译期。

### 4.1 转置不是运行时 if，而是直接换加载方式

例如 `A` 的起始地址：

```c
#ifdef TRANSPOSE_A
__global const FLOAT* input_a_offset = input_a + idm; // K x M
#else
__global const FLOAT* input_a_offset = input_a + idm * K; // M x K
#endif
```

这意味着：

- 同一个数学运算
- 但底层内存布局不同
- `MNN` 直接 build 两份不同版本的 kernel

这样做的好处是：

- 热路径里不需要反复判断转置状态
- 编译器可以对访存表达式做更激进的优化

### 4.2 尾块处理也是编译期特化

当 `M / N / K` 不是 4 的倍数时，kernel 不会用一套统一的慢逻辑处理全部情况，而是：

- `M_LEAVE_NUM=1/2/3` 时分别生成不同尾块代码
- `N_LEAVE_NUM=1/2/3` 时也分别生成
- `K_LEAVE` 单独处理最后一组不足 4 的 `K` 元素

这就是为什么你会看到很多 `vload2`、`vload3`、标量补零的分支。它们的目的都是一样的：

- 主体路径尽量保持 `4x4` 向量化
- 只在边界块上退到更小粒度

### 4.3 `BIAS` 也是直接编译成两版

如果有 bias，开头就直接读：

```c
COMPUTE_FLOAT4 bias = CONVERT_COMPUTE_FLOAT4(vload4(0, input_c + idn));
```

然后把 4 行输出都先初始化成这个 bias。

如果没有 bias，就直接从 0 初始化。

这比运行时传一个“是否有 bias”的标志再 if 判断更直接，因为：

- 少了热路径分支
- 编译器更容易做死代码消除

## 5. 最后的写回为什么也分两条路径

回写部分同样有快路径和边界路径。

快路径是：

```c
vstore4(CONVERT_FLOAT4(out[vec_m]), 0, output_c + out_offset + vec_m * N);
```

也就是整行 4 个元素一次写回。

边界路径则是：

- 如果 `M` 不满 4 行，只写剩余行数
- 如果 `N` 不满 4 列，只逐个标量写剩余列数

这样做的收益仍然是同一个原则：

- 主体路径尽量保持 `vstore4`
- 只有最后一小块才退化成标量写回

## 6. `MatmulBufExecution.cpp` 到底在干什么

如果说 `matmul_buf.cl` 是设备侧计算模板，那么
`MNN-batch/source/backend/opencl/execution/buffer/MatmulBufExecution.cpp`
就是 Host 侧的“发射器”。

它的工作可以拆成四步。

### 6.1 先看形状，决定用哪一类 kernel

`onEncode()` 里先根据输入 shape 和转置标志算出：

- `M`
- `N`
- `K`

然后它不是无脑用 `matmul_buf`，而是走一层策略选择：

- 满足大 tile 条件，用 `matmul_params_buf`
- 否则满足小 tile 条件，用 `matmul_local_buf`
- 再不满足，才用 `matmul_buf`

也就是说，`matmul_buf.cl` 在整个调度体系里其实是 fallback kernel。

### 6.2 再拼 build options，把一份 `.cl` 编成多个特化版本

`MatmulBufExecution.cpp` 会根据当前 case 动态添加：

- `-DTRANSPOSE_A`
- `-DTRANSPOSE_B`
- `-DBIAS`
- `-DM_LEAVE`
- `-DM_LEAVE_NUM=...`
- `-DN_LEAVE`
- `-DN_LEAVE_NUM=...`
- `-DK_LEAVE`

这一步非常关键，因为它决定了：

- 同一个 `matmul_buf.cl`
- 最终会被编译成很多不同的小变体

所以你可以把 `.cl` 看成“模板”，把 `.cpp` 看成“实例化参数的提供者”。

### 6.3 `.cpp` 负责设置参数和 `gws/lws`

选到 `matmul_buf` 之后，`MatmulBufExecution.cpp` 会设置：

- `global_size_dim0`
- `global_size_dim1`
- `input0`
- `input1`
- `bias`（如果有）
- `output`
- `M`
- `N`
- `K`

并且设置：

```cpp
mGlobalWorkSize = {UP_DIV(N, 4), UP_DIV(M, 4)};
```

然后再通过 `localWS2DDefault(...)` 去调一个合适的 `local work size`。

所以 `.cpp` 的职责不是“参与矩阵乘”，而是：

- 描述这次 launch 要跑哪个 kernel
- 这次 kernel 的静态宏是什么
- 参数是什么
- 发多少个 work-item
- 本地工作组怎么分

### 6.4 `.cpp` 是 Host 侧调度逻辑，不是设备侧数学逻辑

这点可以直接记成一句话：

```text
.cl 负责算
.cpp 负责让它以当前 shape 和当前参数被正确地算起来
```

## 7. 那 `matmul_buf_mnn_cl.cpp` 又是干什么的

这个文件最容易让人误解成“又写了一份 C++ 版 kernel”，其实不是。

它本质上是：

- 由 `opencl_codegen.py` 从 `.cl` 自动生成出来的
- 内容是把 kernel 源码变成 `const char*` 字符串
- 再注册进 `OpenCLProgramMap`

例如生成脚本就在：

- `MNN-batch/source/backend/opencl/execution/cl/opencl_codegen.py`

脚本会把：

```text
matmul_buf.cl
```

转成：

```text
matmul_buf_mnn_cl.cpp
```

同时更新：

```text
opencl_source_map.hpp
```

### 7.1 运行时是怎么用到它的

`OpenCLRuntime::loadProgram()` 会按 `programName` 从 `OpenCLProgramMap` 里找源码：

```cpp
auto it_source = OpenCLProgramMap.find(programName);
std::string source(it_source->second);
sources.push_back(source);
*program = cl::Program(context(), sources);
```

也就是说，`MNN` 运行时并不是在手机文件系统里再去找 `matmul_buf.cl`，而是：

- 程序编译时就把 `.cl` 源码嵌进二进制
- 运行时直接从内存里的字符串构建 `cl::Program`

### 7.2 这样做的好处是什么

主要有四个好处：

- 不依赖运行时文件路径，移动端部署更简单。
- 所有 kernel 源码都能跟着库一起发，不需要额外拷 `.cl` 文件。
- 可以通过 `OpenCLProgramMap` 统一管理源码名到程序名的映射。
- 方便用编译宏做大量特化版本。

### 7.3 这个文件一般不应该手改

因为它是生成物，所以正常工作流应该是：

```text
改 matmul_buf.cl
-> 跑 opencl_codegen.py
-> 重新生成 matmul_buf_mnn_cl.cpp
```

而不是直接手改 `_mnn_cl.cpp`。

否则你下次重新生成时，手改内容会被覆盖掉。

### 7.4 `MNN_OPENCL_BUFFER_CLOSED` 又是什么意思

你会看到这些 `_buf` 相关的源码和声明前面经常有：

```cpp
#ifndef MNN_OPENCL_BUFFER_CLOSED
```

这表示：

- 如果构建时打开了 size cut 之类的配置
- `MNN` 可以把 buffer 路径 kernel 整体裁掉
- 用来减小 OpenCL 代码体积

所以 `_mnn_cl.cpp` 不只是“嵌源码”，还是整套 OpenCL program 注册和裁剪机制的一部分。

## 8. 一句话总结

如果只想先抓住主线，可以记这四句：

- `matmul_buf.cl` 是一个以 `4x4` 输出 tile 为基本单位的通用 buffer MatMul kernel。
- `MatmulBufExecution.cpp` 负责选择 kernel 变体、拼编译宏、设置参数、决定 `gws/lws`。
- `matmul_buf_mnn_cl.cpp` 不是手写算子实现，而是把 `.cl` 源码嵌进二进制的自动生成文件。
- 真正要改算法和设备侧逻辑，改的是 `.cl`；真正要改调度策略和 launch 规则，改的是 Host 侧 `.cpp`。

## 9. 参考资料

- `MNN-batch/source/backend/opencl/execution/cl/matmul_buf.cl`
- `MNN-batch/source/backend/opencl/execution/buffer/MatmulBufExecution.cpp`
- `MNN-batch/source/backend/opencl/execution/cl/matmul_buf_mnn_cl.cpp`
- `MNN-batch/source/backend/opencl/execution/cl/opencl_codegen.py`
- `MNN-batch/source/backend/opencl/core/runtime/OpenCLRuntime.cpp`
