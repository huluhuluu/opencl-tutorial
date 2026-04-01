---
title: "Hello OpenCL"
date: 2026-04-01T15:30:00+08:00
lastmod: 2026-04-01T15:30:00+08:00
draft: false
description: "第一个 OpenCL 程序"
slug: "hello-opencl"
tags: ["OpenCL", "GPU", "编程"]
categories: ["OpenCL教程"]
comments: true
math: true
---

# Hello OpenCL

本文通过一个简单的向量加法示例，介绍 OpenCL 程序的基本结构。

## 1. OpenCL 程序流程

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  获取平台设备  │ -> │  创建上下文队列  │ -> │  编译 Kernel   │
└─────────────┘    └─────────────┘    └─────────────┘
                                              │
                                              v
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   读取结果    │ <- │   执行 Kernel   │ <- │  创建缓冲区    │
└─────────────┘    └─────────────┘    └─────────────┘
```

## 2. Kernel 代码

```c
// vector_add.cl
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

## 3. Host 代码

```c
// main.c
#include <stdio.h>
#include <stdlib.h>
#include <CL/cl.h>

#define MAX_SOURCE_SIZE (0x100000)

int main() {
    // 1. 获取平台和设备
    cl_platform_id platform;
    clGetPlatformIDs(1, &platform, NULL);
    
    cl_device_id device;
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    
    // 2. 创建上下文和命令队列
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, NULL);
    cl_command_queue queue = clCreateCommandQueue(context, device, 0, NULL);
    
    // 3. 加载和编译 Kernel
    FILE *fp = fopen("vector_add.cl", "r");
    char *source_str = (char*)malloc(MAX_SOURCE_SIZE);
    size_t source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);
    
    cl_program program = clCreateProgramWithSource(context, 1, 
        (const char**)&source_str, &source_size, NULL);
    clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    cl_kernel kernel = clCreateKernel(program, "vector_add", NULL);
    
    // 4. 准备数据
    const int N = 1024;
    float *a = (float*)malloc(N * sizeof(float));
    float *b = (float*)malloc(N * sizeof(float));
    float *c = (float*)malloc(N * sizeof(float));
    
    for (int i = 0; i < N; i++) {
        a[i] = i;
        b[i] = i * 2;
    }
    
    // 5. 创建缓冲区
    cl_mem buf_a = clCreateBuffer(context, CL_MEM_READ_ONLY, N * sizeof(float), NULL, NULL);
    cl_mem buf_b = clCreateBuffer(context, CL_MEM_READ_ONLY, N * sizeof(float), NULL, NULL);
    cl_mem buf_c = clCreateBuffer(context, CL_MEM_WRITE_ONLY, N * sizeof(float), NULL, NULL);
    
    // 6. 写入数据
    clEnqueueWriteBuffer(queue, buf_a, CL_TRUE, 0, N * sizeof(float), a, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, buf_b, CL_TRUE, 0, N * sizeof(float), b, 0, NULL, NULL);
    
    // 7. 设置 Kernel 参数
    clSetKernelArg(kernel, 0, sizeof(cl_mem), &buf_a);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &buf_b);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &buf_c);
    clSetKernelArg(kernel, 3, sizeof(unsigned int), &N);
    
    // 8. 执行 Kernel
    size_t global_work_size = N;
    clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);
    
    // 9. 读取结果
    clEnqueueReadBuffer(queue, buf_c, CL_TRUE, 0, N * sizeof(float), c, 0, NULL, NULL);
    
    // 10. 验证结果
    for (int i = 0; i < 10; i++) {
        printf("c[%d] = %f (expected %f)\n", i, c[i], a[i] + b[i]);
    }
    
    // 11. 清理资源
    clReleaseMemObject(buf_a);
    clReleaseMemObject(buf_b);
    clReleaseMemObject(buf_c);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    free(a); free(b); free(c); free(source_str);
    
    return 0;
}
```

## 4. 编译运行

```bash
# Linux
gcc main.c -o vector_add -lOpenCL
./vector_add

# 输出
# c[0] = 0.000000 (expected 0.000000)
# c[1] = 3.000000 (expected 3.000000)
# c[2] = 6.000000 (expected 6.000000)
# ...
```

## 5. 代码解析

### 5.1 平台和设备

| 函数 | 说明 |
|------|------|
| `clGetPlatformIDs` | 获取可用平台 |
| `clGetDeviceIDs` | 获取设备 |
| `clGetDeviceInfo` | 获取设备信息 |

### 5.2 上下文和队列

| 函数 | 说明 |
|------|------|
| `clCreateContext` | 创建上下文 |
| `clCreateCommandQueue` | 创建命令队列 |
| `clReleaseContext` | 释放上下文 |
| `clReleaseCommandQueue` | 释放命令队列 |

### 5.3 程序和 Kernel

| 函数 | 说明 |
|------|------|
| `clCreateProgramWithSource` | 从源码创建程序 |
| `clBuildProgram` | 编译程序 |
| `clCreateKernel` | 创建 Kernel |
| `clSetKernelArg` | 设置 Kernel 参数 |

### 5.4 内存对象

| 函数 | 说明 |
|------|------|
| `clCreateBuffer` | 创建缓冲区 |
| `clEnqueueWriteBuffer` | 写入数据 |
| `clEnqueueReadBuffer` | 读取数据 |
| `clReleaseMemObject` | 释放内存 |

### 5.5 执行

| 函数 | 说明 |
|------|------|
| `clEnqueueNDRangeKernel` | 执行 Kernel |
| `clFinish` | 等待队列完成 |

---

## 参考链接

- [OpenCL API 参考](https://www.khronos.org/registry/OpenCL/sdk/3.0/docs/man/html/)
- [OpenCL 规范](https://www.khronos.org/registry/OpenCL/specs/3.0-unified/html/OpenCL_API.html)
