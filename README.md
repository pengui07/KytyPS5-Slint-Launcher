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

A local build is a development build: it never updates itself.

## Releases
Push a version tag to publish a release:
```
git tag v1.2.0
git push origin v1.2.0
```
`.github/workflows/release.yml` builds Windows and Linux, stamps the tag into the launcher
(`-DKYTY_LAUNCHER_VERSION=v1.2.0`) and attaches `KytyPS5-Slint-Launcher-<tag>-Windows-x64.zip` and
`-Linux-x86_64.tar.gz` to the release. Released launchers look for a newer release at startup
(Settings > Launcher) and from Help > Check for updates, then download it, swap the executable and restart.
