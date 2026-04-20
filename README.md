# OPENCL_TUTORIAL

围绕 OpenCL / Android GPU 的中文教程仓库，当前内容已经按 `adb -s 127.0.0.1:40404` 这台真机环境补齐了实测命令。

## 目录

| 文章 | 说明 | 状态 |
|------|------|------|
| [OpenCL 介绍](blog/opencl-intro/index.md) | 梳理 `Platform` / `Device` / `NDRange` / 内存模型与 Host 调用流程 | ✅ 完成 |
| [Hello OpenCL](blog/hello-opencl/index.md) | 用最小 `Vector Add` 示例串起 `context`、`program`、`kernel`、`buffer` 和执行链路，并给出 Adreno 830 实测输出 | ✅ 完成 |
| [Qualcomm OpenCL](blog/opencl-qualcomm/index.md) | 基于 Qualcomm 官方文档整理 `Adreno` 上的 OpenCL 架构与优化要点 | 🚧 TODO |
| [MNN OpenCL MatMul](blog/mnn-opencl-matmul-buf/index.md) | 解释 `matmul_buf.cl` 的计算映射，以及 Host 侧 `.cpp` / `_mnn_cl.cpp` 的分工 | 🚧 TODO |

## 仓库结构

```text
blog/
├── opencl-intro/      # 概念与执行模型
├── hello-opencl/      # 文章 + 最小可编译示例
├── opencl-qualcomm/   # draft: Qualcomm / Adreno OpenCL 架构与优化
└── mnn-opencl-matmul-buf/ # draft: MNN OpenCL MatMul kernel 解析
```

## 参考资源

- [Khronos OpenCL Overview](https://www.khronos.org/opencl/)
- [Khronos OpenCL API Registry](https://registry.khronos.org/OpenCL/)
- Android 设备上的 `libOpenCL.so` 一般来自 GPU 厂商运行时，需要结合具体 SoC / ROM 确认路径和可用性。
