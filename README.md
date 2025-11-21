# NK Engine

A C++ game engine built with Vulkan.

## Platform Support

**Note:** Currently only Windows is supported. Linux support is planned for the future.

## Getting Started

### Prerequisites

- CMake 3.24 or higher
- A C++20 compatible compiler (GCC recommended on Windows)
- Ninja build system
- Git with submodule support

### Cloning the Repository

This repository uses Git submodules for dependencies. When cloning, make sure to include the submodules:

```bash
git clone --recurse-submodules <repository-url>
```

If you've already cloned the repository without submodules, you can initialize them with:

```bash
git submodule update --init --recursive
```

### Building the Project

The project uses CMake with preset configurations. To build the editor (main project):

1. Configure the project using a CMake preset:
```bash
cmake --preset Win32-Debug
```

2. Build the project:
```bash
cmake --build out/build/Win32-Debug
```

Alternatively, you can use the build preset directly:
```bash
cmake --build --preset Win32-Debug
```

Available presets:
- `Win32-Debug` - Debug build with shared libraries
- `Win32-RelWithDebInfo` - Release build with debug information
- `Win32-Release` - Release build

### Running the Editor

After building, the editor executable will be located at:
```
bin/Win32-Debug/editor.exe
```

**Important:** The application takes approximately 10 seconds to load before displaying the lights and models. This is expected behavior during initialization.

## TODO:

- [x] Memory System
- [x] Allocator Types
- [x] Assertions
- [ ] Array-Like types (Map, Array, Dynamic Array, String)
- [ ] Logging System
- [ ] App creation and entry point
- [ ] Window Agnostic (Linux and Windows for now)
- [ ] Event System
- [ ] Input System
- [ ] Renderer with Vulkan
