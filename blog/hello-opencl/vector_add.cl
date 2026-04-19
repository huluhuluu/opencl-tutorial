__kernel void vector_add(__global const float* a,
                         __global const float* b,
                         __global float* c,
                         const unsigned int n) {
  const size_t id = get_global_id(0);
  if (id < n) {
    c[id] = a[id] + b[id];
  }
}
