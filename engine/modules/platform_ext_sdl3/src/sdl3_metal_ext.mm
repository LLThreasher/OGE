#ifdef PLATFORM_DARWIN

#include <SDL3/SDL.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include "oge/log.hpp"

namespace oge::platform::sdl3
{

    const void* GetMetalLayer(SDL_Window* sdlWindow) {
        SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
        NSWindow* nsWindow = (__bridge NSWindow*)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        
        if (!nsWindow) {
            LOG_ERROR("Failed to extract native NSWindow from SDL3!");
            return nullptr;
        }

        // 2. Fetch or create a CAMetalLayer on the NSWindow content view
        NSView* contentView = [nsWindow contentView];
        [contentView setWantsLayer:YES];
        
        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        [contentView setLayer:metalLayer];

        return (__bridge const void*)metalLayer;
    }

}
#endif
