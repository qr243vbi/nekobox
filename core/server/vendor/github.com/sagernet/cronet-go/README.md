# cronet-go

[![Reference](https://pkg.go.dev/badge/github.com/sagernet/cronet-go.svg)](https://pkg.go.dev/github.com/sagernet/cronet-go)

Go bindings for [naiveproxy](https://github.com/klzgrad/naiveproxy).

## Supported Platforms

| Target        | OS      | CPU   |
|---------------|---------|-------|
| android/386   | android | x86   |
| android/amd64 | android | x64   |
| android/arm   | android | arm   |
| android/arm64 | android | arm64 |
| darwin/amd64  | mac     | x64   |
| darwin/arm64  | mac     | arm64 |
| ios/arm64     | ios     | arm64 |
| ios/amd64     | ios     | amd64 |
| linux/386     | linux   | x86   |
| linux/amd64   | linux   | x64   |
| linux/arm     | linux   | arm   |
| linux/arm64   | linux   | arm64   |
| linux/loong64 | linux   | loong64 |
| windows/amd64 | win     | x64     |
| windows/arm64 | win     | arm64 |

## System Requirements

| Platform      | Minimum Version |
|---------------|-----------------|
| macOS         | 12.0 (Monterey) |
| iOS/tvOS      | 15.0            |
| Windows       | 10              |
| Android       | 5.0 (API 21)    |
| Linux (glibc)       | glibc 2.31 (loong64: 2.36) |
| Linux (musl)        | any (loong64: 1.2.5)       |

## Downstream Build Requirements

| Platform                             | Requirements                    | Go Build Flags                    |
|--------------------------------------|---------------------------------|-----------------------------------|
| Linux (glibc)                        | Chromium toolchain              | -                                 |
| Linux (musl)                         | Chromium toolchain              | `-tags with_musl`                 |
| macOS / iOS                          | macOS Xcode                     | -                                 |
| iOS simulator/ tvOS / tvOS simulator | macOS Xcode + SagerNet/gomobile | -                                 |
| Windows                              | -                               | `CGO_ENABLED=0 -tags with_purego` |
| Android                              | Android NDK                     | -                                 |

## Linux Build instructions

```bash
git clone --recursive --depth=1 https://github.com/sagernet/cronet-go.git
cd cronet-go
go run ./cmd/build-naive --target=linux/amd64 download-toolchain
#go run ./cmd/build-naive --target=linux/amd64 --libc=musl download-toolchain

# Outputs CC, CXX, and CGO_LDFLAGS=-fuse-ld=lld
export $(go run ./cmd/build-naive --target=linux/amd64 env)
#export $(go run ./cmd/build-naive --target=linux/amd64 --libc=musl env)

cd /path/to/your/project
go build
# go build -tags with_musl
```

### Directories to cache

```yaml
- cronet-go/naiveproxy/src/third_party/llvm-build/
- cronet-go/naiveproxy/src/gn/out/
- cronet-go/naiveproxy/src/chrome/build/pgo_profiles/
- cronet-go/naiveproxy/src/out/sysroot-build/
```

## Windows / purego Build Instructions

For Windows or pure Go builds (no CGO), you need to distribute the dynamic library alongside your binary.

### Download Library

Download `libcronet.dll` (Windows) or `libcronet.so` (Linux) from [GitHub Releases](https://github.com/sagernet/cronet-go/releases).

### Build with purego

```bash
# Windows (purego is required)
CGO_ENABLED=0 GOOS=windows GOARCH=amd64 go build -tags with_purego -o myapp.exe

# Linux with purego (optional, for dynamic linking)
CGO_ENABLED=0 go build -tags with_purego -o myapp
```

### Distribution

Place the library file in the same directory as your executable:
- Windows: `libcronet.dll`
- Linux: `libcronet.so`

### For Downstream Developers

If you need to programmatically extract libraries from Go module dependencies (e.g., for CI/CD pipelines):

```bash
go run github.com/sagernet/cronet-go/cmd/build-naive@latest extract-lib --target windows/amd64 -n libcronet_amd64.dll
go run github.com/sagernet/cronet-go/cmd/build-naive@latest extract-lib --target linux/amd64 -n libcronet_amd64.so
```
