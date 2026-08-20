// Exactly one translation unit in the whole program must define these before including the
// metal-cpp headers — it's what generates the actual Objective-C bridging implementations.
#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <cstdio>

int main() {
  NS::SharedPtr<MTL::Device> device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
  if (!device) {
    std::fprintf(stderr, "No Metal device found\n");
    return 1;
  }
  std::printf("Metal device: %s\n", device->name()->utf8String());
  return 0;
}
