// Single translation unit that instantiates the private selector symbols
// for the metal-cpp header-only library (bkaradzic/metal-cpp fork).
// Must be compiled exactly once per binary.
#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
