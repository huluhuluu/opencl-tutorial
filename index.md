---
title: "OpenCL 教程"
date: 2026-04-01T15:00:00+08:00
lastmod: 2026-04-20T10:00:00+08:00
draft: false
description: "围绕 OpenCL 核心概念与最小 Vector Add 实例的中文教程"
slug: "opencl-tutorial"
tags: ["opencl"]
categories: ["opencl"]
build:
  list: never
comments: true
math: true
---

# OpenCL 教程

围绕 OpenCL / Android GPU 的中文教程仓库，当前内容已经结合 `adb -s 127.0.0.1:40404` 这台设备补充了实测构建和运行流程。

| 文章 | 说明 | 状态 |
|------|------|------|
| [OpenCL 介绍](/p/opencl-intro/) | 梳理 `Platform`、`Device`、`NDRange`、内存模型和 Host 调用顺序 | ✅ 完成 |
| [Hello OpenCL](/p/hello-opencl/) | 用最小 `Vector Add` 示例串起 `program build`、`kernel execute` 和结果校验，并给出 Adreno 830 实测结果 | ✅ 完成 |
| [Qualcomm OpenCL](/p/opencl-qualcomm/) | 基于 Qualcomm 官方文档整理 `Adreno` 上的 OpenCL 架构与优化要点 | 🚧 TODO |
| [MNN OpenCL MatMul](/p/mnn-opencl-matmul-buf/) | 解释 `matmul_buf.cl` 的计算映射，以及 Host 侧 `.cpp` / `_mnn_cl.cpp` 的分工 | 🚧 TODO |

这个系列当前分成两类内容：

- 把 OpenCL Host 侧最核心的对象模型和执行链讲清楚。
- 用一份最小 `Vector Add` 代码，把 `context / program / kernel / buffer / NDRange` 串起来。
- 继续补 Qualcomm / Adreno 优化和真实工程 kernel 阅读，这两部分目前还是 draft。
