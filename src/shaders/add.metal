#include <metal_stdlib>
using namespace metal;

// Runs once per array element, in parallel; index says which element this run is responsible
// for — the GPU assigns it automatically, we never compute it ourselves.
kernel void add_arrays(device const float* a [[buffer(0)]],
                        device const float* b [[buffer(1)]],
                        device float* c [[buffer(2)]],
                        uint index [[thread_position_in_grid]]) {
  c[index] = a[index] + b[index];
}
