/**
 * Vector Addition Kernel
 * 每个工作项处理一个元素
 */
__kernel void vector_add(
    __global const float *a,    // 输入向量 A
    __global const float *b,    // 输入向量 B
    __global float *c,          // 输出向量 C
    const unsigned int n        // 向量长度
) {
    // 获取全局工作项 ID
    int id = get_global_id(0);
    
    // 边界检查
    if (id < n) {
        c[id] = a[id] + b[id];
    }
}

/**
 * Vector Multiplication Kernel
 */
__kernel void vector_mul(
    __global const float *a,
    __global const float *b,
    __global float *c,
    const unsigned int n
) {
    int id = get_global_id(0);
    if (id < n) {
        c[id] = a[id] * b[id];
    }
}

/**
 * SAXPY: y = a * x + y
 */
__kernel void saxpy(
    const float a,
    __global const float *x,
    __global float *y,
    const unsigned int n
) {
    int id = get_global_id(0);
    if (id < n) {
        y[id] = a * x[id] + y[id];
    }
}
