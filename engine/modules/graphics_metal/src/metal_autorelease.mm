#import <Foundation/Foundation.h>

extern "C" {

void* MetalAutoreleasePoolPush()
{
    return [[NSAutoreleasePool alloc] init];
}

void MetalAutoreleasePoolPop(void* pool)
{
    [(NSAutoreleasePool*)pool drain];
}

}  // extern "C"
