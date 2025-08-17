# Mario Builder 64 → SM64CoopDX Integration Guide

## Overview

This **library export** provides complete level loading and playback support for Mario Builder 64 levels in SM64CoopDX. **No struct definitions are required from the integrator** - everything is provided through the public API.

## Quick Start Integration

### 1. Build the Library Export

```bash
cd mario-builder-64/
make -f Makefile.lib
# Exports: 
# - build/lib/libmb64-levels.a (compiled library)
# - build/lib/include/mb64_api.h (complete public API header)

# Verify the library export worked:
ls build/lib/
# Should show: include/ libmb64-levels.a src/

ls build/lib/include/
# Should show: mb64_api.h
```

**Important: Always test the build after making changes!** If you see compilation errors, check the troubleshooting section below.

### 2. Integration in SM64CoopDX

**Makefile changes:**
```makefile
# Add Mario Builder 64 library
LIBS += -L./mario-builder-64/build/lib -lmb64-levels

# Include the complete API header path
INCLUDES += -I./mario-builder-64/build/lib/include
```

**C/C++ Integration:**
```c
// Single header includes ALL necessary structs and functions
#include "mb64_api.h"

// Now you have access to:
// - All struct definitions (mb64_obj, mb64_tile, mb64_level_save_header, etc.)
// - All API functions (mb64_load_level_data, mb64_get_object_count, etc.)
// - All constants and enums
// - No need to redefine ANY structures!
```

## Complete API Reference

### Core Functions

#### Level Loading
```c
// Load a .mb64 level file
int mb64_load_level_data(const char* filename);
// Returns: 0 on success, -1 on failure

// Generate level geometry and spawn objects  
int mb64_generate_level_objects(void);
// Returns: 0 on success, negative on failure
```

#### Data Access
```c
// Get level statistics
u16 mb64_get_object_count(void);    // Number of objects in level
u16 mb64_get_tile_count(void);      // Number of tiles in level  
u8  mb64_get_stars_max(void);       // Maximum stars available

// Get data arrays (ready to use, no copying needed)
struct mb64_obj* mb64_get_object_data(void);              // Object array
struct mb64_tile* mb64_get_tile_data(void);               // Tile array
struct mb64_grid_obj* mb64_get_grid_data(void);           // 64x64x64 grid
struct mb64_level_save_header* mb64_get_save_data(void);   // Level metadata
```

### Data Structures (ALL PROVIDED)

#### Object Data
```c
struct mb64_obj {
    u8 bparam;  // Behavior parameter
    u8 x, y, z; // Grid position
    u8 type;    // Object type ID (see enums)
    u8 rot;     // Rotation (0-3)
    u8 imbue;   // Power-up type
    u8 pad;
};
```

#### Tile Data  
```c
struct mb64_tile {
    u32 x:6, y:6, z:6,        // Grid position (0-63)
        type:5,               // Tile type (TILE_TYPE_*)
        mat:4,                // Material ID
        rot:2,                // Rotation
        waterlogged:1;        // Water flag
};
```

#### Level Metadata
```c
struct mb64_level_save_header {
    char file_header[10];           // "MB64LVL\0\0"
    u8 version;                     // Format version
    char author[31];                // Level author
    u16 piktcher[64][64];          // Thumbnail image
    
    // Level settings
    u8 costume, theme, bg, envfx;   // Visual settings
    u8 boundary, boundary_height;   // Level boundaries
    u8 waterlevel;                  // Water level
    u16 tile_count, object_count;   // Content counts
    
    struct mb64_custom_theme custom_theme; // Custom materials
    // ... (complete structure available in mb64_api.h)
};
```

## Recommended Lua Bindings

### Basic Level Operations
```lua
-- Load and setup level
local success = MB64.loadLevel("path/to/level.mb64")
if not success then
    error("Failed to load level")
end

-- Get level information
local info = MB64.getLevelInfo()
print("Level by:", info.author)
print("Objects:", info.objectCount, "Tiles:", info.tileCount)

-- Spawn level content
MB64.generateLevelGeometry()
MB64.spawnLevelObjects()
```

### Object Enumeration
```lua
-- Access all objects
local objects = MB64.getObjects()
for i = 1, #objects do
    local obj = objects[i]
    print(string.format("Object %d: type=%d pos=(%d,%d,%d) rot=%d", 
          i, obj.type, obj.x, obj.y, obj.z, obj.rot))
    
    -- Spawn the object in SM64CoopDX
    local worldX = MB64.gridToWorldX(obj.x)
    local worldY = MB64.gridToWorldY(obj.y) 
    local worldZ = MB64.gridToWorldZ(obj.z)
    spawnObjectAtPosition(obj.type, worldX, worldY, worldZ, obj.rot)
end
```

### Tile/Terrain Processing
```lua
-- Access all tiles for terrain generation
local tiles = MB64.getTiles()
for i = 1, #tiles do
    local tile = tiles[i]
    if tile.type ~= TILE_TYPE_EMPTY then
        local worldX = MB64.gridToWorldX(tile.x)
        local worldY = MB64.gridToWorldY(tile.y)
        local worldZ = MB64.gridToWorldZ(tile.z)
        
        -- Generate collision surfaces
        generateTileCollision(tile.type, tile.mat, worldX, worldY, worldZ, tile.rot)
        
        -- Generate visual geometry
        generateTileGeometry(tile.type, tile.mat, worldX, worldY, worldZ, tile.rot)
    end
end
```

## Example Integration Implementation

### C Integration Layer
```c
// mb64_coop_bridge.c - Bridge between MB64 API and SM64CoopDX

#include "mb64_api.h"
#include "sm64coop_internal.h"

static bool level_loaded = false;

bool load_mb64_level(const char* filename) {
    // Load level data
    if (mb64_load_level_data(filename) != 0) {
        return false;
    }
    
    // Generate level content
    if (mb64_generate_level_objects() != 0) {
        return false;
    }
    
    level_loaded = true;
    return true;
}

void spawn_mb64_objects(void) {
    if (!level_loaded) return;
    
    struct mb64_obj* objects = mb64_get_object_data();
    u16 count = mb64_get_object_count();
    
    for (u16 i = 0; i < count; i++) {
        struct mb64_obj* obj = &objects[i];
        
        // Convert grid to world coordinates
        f32 worldX = GRID_TO_POS(obj->x);
        f32 worldY = GRID_TO_POS(obj->y);
        f32 worldZ = GRID_TO_POS(obj->z);
        
        // Spawn in SM64CoopDX object system
        spawn_coop_object(obj->type, worldX, worldY, worldZ, obj->rot, obj->bparam);
    }
}

void generate_mb64_terrain(void) {
    if (!level_loaded) return;
    
    struct mb64_tile* tiles = mb64_get_tile_data();
    u16 count = mb64_get_tile_count();
    
    for (u16 i = 0; i < count; i++) {
        struct mb64_tile* tile = &tiles[i];
        if (tile->type == TILE_TYPE_EMPTY) continue;
        
        f32 worldX = GRID_TO_POS(tile->x);
        f32 worldY = GRID_TO_POS(tile->y);
        f32 worldZ = GRID_TO_POS(tile->z);
        
        // Generate collision surfaces for SM64CoopDX
        generate_tile_collision(tile->type, tile->mat, worldX, worldY, worldZ, tile->rot);
        
        // Generate visual geometry
        generate_tile_geometry(tile->type, tile->mat, worldX, worldY, worldZ, tile->rot);
    }
}
```

### Lua Binding Implementation
```c
// mb64_lua_bindings.c - Lua API for SM64CoopDX

#include "mb64_api.h"
#include "lua/lua.h"

static int lua_mb64_load_level(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    bool success = (mb64_load_level_data(filename) == 0) && 
                   (mb64_generate_level_objects() == 0);
    lua_pushboolean(L, success);
    return 1;
}

static int lua_mb64_get_level_info(lua_State* L) {
    struct mb64_level_save_header* save = mb64_get_save_data();
    
    lua_newtable(L);
    lua_pushstring(L, save->author);
    lua_setfield(L, -2, "author");
    lua_pushinteger(L, mb64_get_object_count());
    lua_setfield(L, -2, "objectCount");
    lua_pushinteger(L, mb64_get_tile_count());
    lua_setfield(L, -2, "tileCount");
    lua_pushinteger(L, mb64_get_stars_max());
    lua_setfield(L, -2, "maxStars");
    lua_pushinteger(L, save->theme);
    lua_setfield(L, -2, "theme");
    
    return 1;
}

static int lua_mb64_get_objects(lua_State* L) {
    struct mb64_obj* objects = mb64_get_object_data();
    u16 count = mb64_get_object_count();
    
    lua_newtable(L);
    for (u16 i = 0; i < count; i++) {
        lua_newtable(L);
        lua_pushinteger(L, objects[i].type);
        lua_setfield(L, -2, "type");
        lua_pushinteger(L, objects[i].x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, objects[i].y);
        lua_setfield(L, -2, "y");
        lua_pushinteger(L, objects[i].z);
        lua_setfield(L, -2, "z");
        lua_pushinteger(L, objects[i].rot);
        lua_setfield(L, -2, "rot");
        lua_pushinteger(L, objects[i].bparam);
        lua_setfield(L, -2, "bparam");
        lua_pushinteger(L, objects[i].imbue);
        lua_setfield(L, -2, "imbue");
        
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// Register Lua functions
static const luaL_Reg mb64_lua_funcs[] = {
    {"loadLevel", lua_mb64_load_level},
    {"getLevelInfo", lua_mb64_get_level_info},
    {"getObjects", lua_mb64_get_objects},
    // ... add more functions as needed
    {NULL, NULL}
};

void register_mb64_lua_api(lua_State* L) {
    luaL_newlib(L, mb64_lua_funcs);
    lua_setglobal(L, "MB64");
}
```

## Build Configuration Details

### Library Export Design

**The library export uses a clean separation approach:**

```bash
make -f Makefile.lib                    # Build library export
make -f Makefile.lib show-sources       # Show what's included vs host-provided
```

- **Library contains**: MB64-specific code only (4 object files)
- **Host provides**: Heavy dependencies (math_util, collision, etc.) at link time  
- **Result**: Clean integration without conflicts

### Check What's Included
```bash
make -f Makefile.lib show-sources
```
Shows exactly what's exported in the library vs what the host must provide.

## Library Export Structure

```
mario-builder-64/
├── build/lib/
│   ├── libmb64-levels.a       # Compiled library export
│   └── include/
│       └── mb64_api.h         # Complete public API header
├── Makefile.lib               # Library export build system
└── COOP_INTEGRATION.md        # This integration guide
```

## Integration Checklist

- [ ] **Build library export:** `make -f Makefile.lib`
- [ ] **Add to Makefile:** `LIBS += -L./mario-builder-64/build/lib -lmb64-levels`
- [ ] **Add includes:** `INCLUDES += -I./mario-builder-64/build/lib/include`
- [ ] **Include header:** `#include "mb64_api.h"` (provides ALL structs)
- [ ] **Load level:** Call `mb64_load_level_data(filename)`
- [ ] **Generate content:** Call `mb64_generate_level_objects()`
- [ ] **Access data:** Use `mb64_get_*()` functions to access level data
- [ ] **Create Lua bindings:** Wrap C functions for Lua access

## Troubleshooting

### Common Issues

**Build Errors:**
- Ensure N64/SM64 compatibility headers are available
- Check that `ultra64.h`, `PR/gbi.h` paths are correct
- Verify math constants (M_PI, MAX, MIN) are defined

**Link Errors:**
- Verify library path: `-L./mario-builder-64/build/lib`
- Verify library name: `-lmb64-levels`
- Check for missing host-provided dependencies (use `make -f Makefile.lib show-sources`)

**Runtime Issues:**
- Call `mb64_generate_level_objects()` after `mb64_load_level_data()`
- Check return values - both functions return 0 on success
- Verify file paths and permissions for .mb64 files

### Advanced Configuration

**Custom Include Paths:**
```makefile
# If SM64CoopDX uses different include structure
INCLUDES := -I./mario-builder-64/build/lib/include -I./your/custom/includes
```

**Platform-Specific Builds:**
```bash
# For different target platforms
make -f Makefile.lib CC=your-cross-compiler CFLAGS="-O2 -DYOUR_PLATFORM=1"
```

## How the API System Works

### mb64_api.h & level_api.c Interaction

**The system solves the original problem through a two-layer approach:**

1. **Public API Layer (`mb64_api.h`)**:
   - Contains ALL struct definitions needed by integrators
   - Provides complete function declarations  
   - Includes all constants, enums, and macros
   - **Integrators only need this single header**

2. **Implementation Layer (`src/mb64/level_api.c`)**:
   - Implements the API functions declared in mb64_api.h
   - Bridges between external integrators and internal MB64 code
   - Handles the complexity of internal dependencies
   - **Built into libmb64-levels.a library**

### Key Design Benefits:

**For Integrators (SM64CoopDX):**
```c
#include "mb64_api.h"  // Gets everything needed!

// All structs are immediately available:
struct mb64_obj* objects = mb64_get_object_data();
struct mb64_level_save_header* info = mb64_get_save_data();

// No manual struct definitions required!
```

**For Mario Builder 64:**
```c
// Internal code continues to use existing headers
#include "main.h"       // Internal implementation
#include "structs.h"    // Internal structs

// level_api.c bridges the gap:
// - Uses internal headers for implementation  
// - Exports via functions declared in mb64_api.h
// - No duplication, no conflicts
```

### Build Process:
1. **Compilation**: level_api.c compiles against internal headers
2. **Library Creation**: All objects bundled into libmb64-levels.a  
3. **Header Export**: mb64_api.h copied to build/lib/include/
4. **Integration**: Integrators link library + include exported header

### Symbol Resolution:
- **At Build Time**: level_api.c sees internal struct definitions
- **At Link Time**: Integrators get compiled functions via library
- **At Runtime**: API functions access real MB64 data structures
- **Result**: Zero struct duplication, complete compatibility

## Complete Working Example

See the integration examples above and the `mb64_api.h` header for the complete interface. The key benefit is that **integrators no longer need to redefine ANY structures** - everything is provided through the single public API header.

This design ensures:
- ✅ **No struct duplication** - all definitions come from Mario Builder 64
- ✅ **Version compatibility** - API evolves with the project
- ✅ **Complete interface** - access to all necessary data and functions
- ✅ **Easy integration** - single header, single library
- ✅ **Clear documentation** - detailed examples and usage patterns
- ✅ **Build verification** - always test compilation after changes