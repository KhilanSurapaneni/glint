#include <metal_stdlib>
using namespace metal;

kernel void add_arrays(device const float* a [[buffer(0)]],
                        device const float* b [[buffer(1)]],
                        device float* c [[buffer(2)]],
                        uint index [[thread_position_in_grid]]) {
  c[index] = a[index] + b[index];
}
