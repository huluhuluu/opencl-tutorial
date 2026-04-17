---
title: "OpenCL 教程"
date: 2026-04-01T15:00:00+08:00
lastmod: 2026-04-15T21:20:00+08:00
draft: false
description: "围绕 Android GPU 的 OpenCL 初版教程：执行模型、环境配置和第一个内核程序"
slug: "opencl-tutorial"
tags: ["opencl"]
categories: ["opencl"]
build:
  list: never
comments: true
math: true
---

# OpenCL 教程

这组内容先写成初版，不假设当前环境能真的把代码编起来。目标是先把 OpenCL 在 Android 端最核心的几件事讲清楚：

- Host 和 Device 在分什么工。
- NDRange 和 Work-group 到底是什么。
- Android 工程里要怎么准备头文件、动态库和最小 demo。

## 系列目录

| 文章 | 说明 |
|------|------|
| [核心概念与执行流程](/p/opencl-intro/) | 平台模型、执行模型、内存层次，以及 Android 端的典型调用顺序 |
| [环境配置](/p/opencl-setup/) | 头文件、NDK、Vendor OpenCL 库和最小编译链路 |
| [从 Vector Add 到 GEMM](/p/hello-opencl/) | 用最小 kernel 理解 OpenCL 的运行方式，并引出后续矩阵计算优化 |

## 这个系列的定位

- 不是 OpenCL 规范逐章翻译。
- 也不是只面向桌面 GPU 的通用教程。
- 它更偏 Android / Mobile GPU 语境，先把能跑、能理解、能继续扩展的最小路线搭起来。

## 建议阅读顺序

1. 先看 `opencl-intro`，把概念和调用顺序弄明白。
2. 再看 `opencl-setup`，理解 Android 端真正要准备什么。
3. 最后看 `hello-opencl`，把最小 kernel 和 Host 侧执行链跑通。
