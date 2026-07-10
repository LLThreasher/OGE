# OneGame::Engine

You build Minecraft when you want to find a reason to write C++. 

## Building
### Requirements
- Cmake >= 3.28
- Clang++ >= 17
- VulkanSDK >= 1.1
- MoltenVK (MacOS)
- AndroidSDK API ver 29 (Android)
- AndroidNDK ver 28.2.13676358 (Android)
- JDK >= 21 (Android)

### Fetch Submodules
```bash
git submodule update --init --recursive
```

### Build with CMake preset (Windows)
```bash
cmake --preset x64-debug
cmake --build ./out/build/x64-debug
```
