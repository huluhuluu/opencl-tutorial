# OpenCL Vector Add Example

简单的向量加法示例，演示 OpenCL 基本使用流程。

## 文件说明

```
hello_opencl/
├── vector_add.cl     # Kernel 源码
├── main.c            # Host 代码
├── CMakeLists.txt    # 构建配置
└── README.md         # 说明文档
```

## 编译运行

```bash
mkdir build && cd build
cmake ..
make
./vector_add
```

## 预期输出

```
Vector addition result:
c[0] = 0.000000 + 0.000000 = 0.000000
c[1] = 1.000000 + 2.000000 = 3.000000
c[2] = 2.000000 + 4.000000 = 6.000000
...
OpenCL vector add completed successfully!
```

