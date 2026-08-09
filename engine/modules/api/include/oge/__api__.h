#ifndef OGE_PLATFORM_API_H
#define OGE_PLATFORM_API_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

// Opaque pointer to mask the C++ class instantiation
typedef struct Window Window_t;
typedef struct WindowApp WindowApp_t;

// Lifecycle: Constructor & Destructor
Window_t* OGE_Window_Create(const char* backend, const char* name,
                            size_t width, size_t height);
void OGE_Window_Run(Window_t* instance, WindowApp_t* app);
void OGE_Window_Destroy(Window_t* instance);

WindowApp_t* OGE_App_Create(const char* name);
void OGE_App_Destroy(WindowApp_t* instance);

void OGE_App_SwitchToScene(const char* name, const char* args);

void OGE_Init(void);

#ifdef __cplusplus
}
#endif

#endif  // OGE_PLATFORM_API_H
