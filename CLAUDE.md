# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a **VTK-m/Viskores visualization miniapp** written in C++17 that demonstrates volume rendering and isosurface extraction for AMR (Adaptive Mesh Refinement) datasets. The project showcases integration with the Viskores visualization toolkit and includes a comprehensive parallel rendering framework (miniGraphics).

## Build Commands

```bash
# Standard build process
cmake -B build -S .
cmake --build build -j

# Clean build
rm -rf build && cmake -B build -S . && cmake --build build -j

# Run demos
./build/volrender    # Generates volume.png and surface.png
./build/isosurface   # Generates isosurface_wireframer.png and isosurface_raytracer.png
```

## Dependencies

- **Viskores library** (filter, rendering, source modules) - must be installed and findable by CMake
- **OpenGL/OSMesa** for rendering
- **MPI** for parallel operations (via miniGraphics)
- **C++17 compiler**
- **CMake 3.15+**

## Core Architecture

### Main Executables
- `volrender.cpp` - Volume rendering demonstration using ray tracing
- `isosurface.cpp` - Isosurface extraction and rendering (both wireframe and ray traced)
- `data.cpp/.hpp` - AMR dataset generation with 3 levels and 8x8x8 base resolution

### miniGraphics Framework
Embedded parallel rendering framework in `miniGraphics/` directory providing:
- **Compositing algorithms**: BinarySwap, DirectSend, RadixK, 2-3-Swap, IceT
- **Rendering backends**: OpenGL and simple CPU-based rendering
- **Third-party dependencies**: IceT (parallel compositing), GLM (math), optionparser
- **Extensive test suite** for image and compositing components

### Key Rendering Pipeline
1. **Data generation** (`data.cpp`) creates 3D AMR datasets
2. **Camera setup** with configurable position, look-at, and field of view
3. **Color mapping** using built-in color tables (e.g., "inferno") with opacity control
4. **Scene composition** with actors representing data and rendering properties
5. **Output** to PNG files using OSMesa (off-screen rendering)

## Testing

Limited testing infrastructure exists:
- Tests for miniGraphics components in `miniGraphics/Common/Testing/`
- **No comprehensive tests for main Viskores functionality**
- Uses CMake's `create_test_sourcelist` for test management

## Development Notes

- The project generates 3D AMR datasets programmatically rather than loading external files
- All rendering is done off-screen using OSMesa
- Camera positioning and color table configuration are critical for meaningful visualizations
- The miniGraphics directory appears to be untracked in git - handle carefully when making changes
- Output images are generated in the working directory (volume.png, surface.png, etc.)

## Code Style

- C++17 standard with modern CMake practices
- Viskores namespace conventions (`viskores::cont`, `viskores::rendering`, etc.)
- Header-only interfaces where possible (`data.hpp`)
- Minimal external dependencies beyond Viskores itself