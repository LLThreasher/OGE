# OneGame::Engine

[![Linux](https://github.com/LLThreasher/OGE/actions/workflows/ci-linux.yml/badge.svg)](https://github.com/LLThreasher/OGE/actions/workflows/ci-linux.yml)
[![macOS](https://github.com/LLThreasher/OGE/actions/workflows/ci-macos.yml/badge.svg)](https://github.com/LLThreasher/OGE/actions/workflows/ci-macos.yml)
[![Windows](https://github.com/LLThreasher/OGE/actions/workflows/ci-windows.yml/badge.svg)](https://github.com/LLThreasher/OGE/actions/workflows/ci-windows.yml)
[![Android](https://github.com/LLThreasher/OGE/actions/workflows/ci-android.yml/badge.svg)](https://github.com/LLThreasher/OGE/actions/workflows/ci-android.yml)

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

### Build with CMake preset (Windows)
```bash
cmake --preset x64-debug
cmake --build ./out/build/x64-debug
```
