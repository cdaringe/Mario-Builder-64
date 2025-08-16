# Mario Builder 64 → SM64CoopDX Integration Guide

## Overview

This integration provides **level loading and playback support only** for Mario Builder 64 levels in SM64CoopDX. No editor functionality is included.

## Quick Integration

### 1. Build the Library

**External Mode (Recommended for SM64CoopDX):**
```bash
cd mario-builder-64/
make -f Makefile.lib
# Creates: build/lib/libmb64-levels.a (7 object files)
# SM64CoopDX must provide: math_util, surface_collision, graph_node, materials
```

**Bundle Mode (Include everything):**
```bash
cd mario-builder-64/
make -f Makefile.lib EXTERNAL_MODE=0
# Creates: build/lib/libmb64-levels.a (13 object files)
# Self-contained but may conflict with SM64CoopDX's versions
```

**Check what's external:**
```bash
make -f Makefile.lib show-external
```

### 2. Integrate in SM64CoopDX Makefile
```makefile
# Add Mario Builder 64 library
LIBS += -L./mario-builder-64/build/lib -lmb64-levels

# Ensure includes are available for integration code
INCLUDES += -I./mario-builder-64/include -I./mario-builder-64/src
```

### 3. Lua Bindings Setup
The library provides C functions that can be exposed to Lua for:
- Loading MB64 level files
- Spawning level geometry and objects
- Managing level state

## Key Functions for Lua Bindings

Based on the included sources, key functions likely include:

### Level Loading (`src/mb64/data.c`)
```c
// Level data management functions
// (exact signatures need verification from source)
void mb64_load_level_data(/* level file path */);
void mb64_parse_level_objects(/* level data */);
void mb64_cleanup_level_data(void);
```

### Runtime Support (`src/engine/`)
```c
// Collision and behavior support
void load_surface_collision(/* surfaces */);
void execute_behavior_script(/* object, behavior */);
// Math utilities for level positioning
```

## Recommended Lua API Design

```lua
-- Load a Mario Builder 64 level
MB64.loadLevel(levelPath)

-- Get level information
local info = MB64.getLevelInfo()
-- Returns: { name, author, objectCount, surfaceCount, ... }

-- Spawn level geometry and objects
MB64.spawnLevelGeometry()
MB64.spawnLevelObjects()

-- Cleanup when leaving level
MB64.cleanupLevel()

-- Query level objects
local objects = MB64.getLevelObjects()
for i, obj in ipairs(objects) do
    print("Object:", obj.type, obj.x, obj.y, obj.z)
end
```

## Library Contents

**Included (11 source files):**
- Level data loading and parsing
- Collision detection and surface loading
- Object behavior script execution
- Geometry layout and scene graph management
- Math utilities for 3D positioning
- Material and color management

**Excluded:**
- Level editor UI/menus
- Level creation tools
- Editor display functions
- Menu systems

## Technical Notes

### Dependencies
- SM64CoopDX must provide N64/SM64 compatibility types
- Standard math constants (M_PI, MAX, MIN)
- Graphics types (Gfx, Mtx, Vp, Vtx, Vec3f, Vec3s)
- OS types (OSThread, OSMesgQueue, etc.)

### Assembly Blocks
Some functions contain N64 MIPS assembly that will need host equivalents in SM64CoopDX.

### File Format Support
The library supports Mario Builder 64's custom level format. File loading typically requires:
- Cart filesystem support (`libcart`)
- Level data parsing
- Object placement and configuration

## Next Steps

1. **Build the library** with `make -f Makefile.coop`
2. **Examine key functions** in the source files to determine exact API signatures
3. **Create Lua wrapper functions** around the C API
4. **Test level loading** with existing MB64 level files
5. **Handle platform differences** (assembly blocks, file paths, etc.)

## File Structure
```
mario-builder-64/
├── Makefile.coop           # SM64CoopDX build system
├── COOP_INTEGRATION.md     # This file
├── build/coop/
│   └── libmb64-levels.a    # Final library
└── src/
    ├── mb64/data.c         # Core level data functions
    ├── engine/             # Runtime support
    └── ...
```

This setup provides the minimal, focused functionality needed to load and play Mario Builder 64 levels in SM64CoopDX through Lua bindings.