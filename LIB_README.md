# Mario Builder 64 Library Collection

This directory provides a library-mode build system that extracts Mario Builder 64 functionality into focused, reusable static libraries.

## Primary Use Case: SM64CoopDX Integration

**Target**: Load and play Mario Builder 64 levels in SM64CoopDX with Lua bindings
- **Need**: Level loading, parsing, and runtime support
- **Don't need**: Level editor UI, creation tools, menu systems
- **Goal**: Minimal, optimized libraries for level playback only

## Quick Start

```bash
# Build main library for SM64CoopDX level loading
make -f Makefile.lib

# This creates: build/lib/libmb64-levels.a
# Contains everything needed for level loading/playback

# See all options and requirements  
make -f Makefile.lib help
```

## Available Libraries

| Library | Description | Key Components |
|---------|-------------|----------------|
| `libmb64-math.a` | Core mathematical utilities | Trigonometry, vector math, transformations |
| `libmb64-collision.a` | Surface collision detection | Ray casting, surface intersection, collision response |
| `libmb64-behavior.a` | Object behavior scripting | Behavior script execution engine |
| `libmb64-geo.a` | Geometry & layout | Scene graph, graph nodes, geometry layout |
| `libmb64-material.a` | Colors & materials | Color management, material properties |
| `libmb64-core.a` | MB64 core editor logic | Level data management, compatibility layer |
| `libmb64-menu.a` | MB64 menu system | Editor menus, settings, UI logic |
| `libmb64-display.a` | MB64 display & rendering | Display functions, painting frame |
| `libmb64-main.a` | MB64 main logic | Primary editor state machine |
| `libmb64-gameutil.a` | Game utilities | Mario utilities, debug functions |
| `libmb64-audio.a` | Audio system | Audio management and effects |

## Host Project Requirements

Your project must provide the complete N64/SM64 compatibility layer:

### Required Headers
- `ultra64.h` - Core N64 SDK types and functions
- `PR/gbi.h` - Graphics Binary Interface 
- `PR/ultratypes.h` - Ultra64 type definitions

### Required Types
```c
// Graphics types
typedef struct { /* ... */ } Gfx;
typedef struct { /* ... */ } Mtx;
typedef struct { /* ... */ } Vp;
typedef struct { /* ... */ } Vtx;

// Vector types
typedef float Vec3f[3];
typedef s16 Vec3s[3];

// OS types  
typedef struct { /* ... */ } OSThread;
typedef struct { /* ... */ } OSMesgQueue;
typedef struct { /* ... */ } OSTask;
// ... etc
```

### Required Constants & Macros
```c
#define M_PI 3.14159265359f
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
```

### Assembly Block Handling
The libraries contain N64 MIPS assembly blocks that will need host-equivalent implementations. Common patterns include:
- Floating point rounding instructions
- MIPS register operations  
- Hardware-specific optimizations

## Integration Example

### In SM64CoopDX Makefile:
```makefile
# Add Mario Builder 64 library path
LIBDIR += -L./mario-builder-64/build/lib

# Link the main library (contains everything needed)
LIBS += -lmb64-levels

# Ensure includes are available for integration code
INCLUDES += -I./mario-builder-64/include -I./mario-builder-64/src
```

### In your source code:
```c
#include <ultra64.h>        // Your N64 compatibility layer
#include "sm64_types.h"     // Your SM64 type definitions

// Mario Builder 64 libraries are now available
// Functions from libmb64-math.a, libmb64-core.a, etc.
```

## Library Dependencies

- `libmb64-math.a` - Minimal dependencies, good starting point
- `libmb64-collision.a` - Depends on math library
- `libmb64-core.a` - Core editor functionality, moderate dependencies
- Other libraries - May have complex graphics/audio dependencies

## Notes

- Libraries are compiled with `TARGET_N64=0` for host compatibility
- Some functions may require adaptation for non-N64 platforms
- MIPS assembly blocks will need platform-specific implementations
- Built libraries are placed in `build/lib/`

## Example Projects

These libraries are designed for projects that already have SM64/N64 infrastructure such as:
- SM64 decomp ports
- SM64 engine recreations  
- Tools that work with SM64 data formats
- Projects using SM64-compatible data structures