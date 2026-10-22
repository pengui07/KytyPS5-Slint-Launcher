# Kyty Slint launcher

Standalone copy of `src/slint_launcher` from kytyPS5.

## Requirements
- CMake >= 3.21, Ninja (or a Visual Studio generator)
- A C++20 compiler: MSVC, clang-cl, GCC or Clang
- Rust >= 1.92 (Slint is built from source)
- Linux: libcurl and libfontconfig1 dev packages

## Build
```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
On Windows, run these from a "x64 Native Tools Command Prompt for VS".
The exe ends up in `build/`.
