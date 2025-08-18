# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

Mario Builder 64 is built using a Makefile system with various configuration options:

```bash
# Build the ROM (default target)
make

# Clean build artifacts
make clean

# Complete clean including extracted assets and tools
make distclean

# Rebuild tools
make rebuildtools

# Test the ROM in an emulator
make test

# Load ROM to UNFLoader (flashcart debugging)
make load

# Build with debug mode
make UNF=1

# Build with different save types
make SAVETYPE=sram     # Default for Mario Builder 64
make SAVETYPE=eep4k
make SAVETYPE=eep16k

# Build with different compression
make COMPRESS=rnc1     # Default - best all-around compression
make COMPRESS=rnc2     # Faster decompression
make COMPRESS=gzip     # Better compression ratio
make COMPRESS=uncomp   # No compression (faster load times)

# Build for different consoles
make CONSOLE=n64       # Default - Nintendo 64
make CONSOLE=bb        # iQue Player

# Build with different graphics microcode
make GRUCODE=f3dzex    # Default - Fast3DZEX (Animal Crossing microcode)
make GRUCODE=f3dex2    # Fast3DEX2
make GRUCODE=l3dex2    # Line3DEX2 (wireframe only)

# Build with different compilers
make COMPILER=gcc      # Default - GNU C Compiler
make COMPILER=clang    # Clang C/C++ frontend

# Test specific configurations
make VERSION=us        # Default US version
make VERSION=jp        # Japanese version
make VERSION=eu        # PAL version
make VERSION=sh        # Shindou version with rumble support
```

## Project Architecture

### Core Structure

This is a **Super Mario 64 ROM hack** called Mario Builder 64, built on top of the HackerSM64 decomp. The project allows creating custom levels in-game through a level editor.

**Key directories:**
- `src/mb64/` - Mario Builder 64 specific code (level editor, menus, data management)
- `src/game/` - Core game logic, Mario behaviors, interactions, rendering
- `src/engine/` - Low-level engine systems (collision, math, graphics)
- `src/audio/` - Audio system and synthesis
- `actors/` - 3D models and actor definitions (400+ actors/objects)
- `levels/` - Level data and scripts
- `include/config/` - Comprehensive configuration system

### Mario Builder 64 Specific Components

The level editor functionality is primarily contained in:
- `src/mb64/main.c` - Main level editor logic and state management
- `src/mb64/menu.c` - Menu system for the level editor
- `src/mb64/menu_engine.c` - Menu rendering and interaction engine
- `src/mb64/display_funcs.c` - Display and rendering utilities
- `src/mb64/data.c` - Data management for custom levels

### Configuration System

The project uses an extensive configuration system in `include/config/`:
- `config_game.h` - Core game settings
- `config_graphics.h` - Graphics and rendering options
- `config_audio.h` - Audio system configuration
- `config_camera.h` - Camera behavior settings
- `config_debug.h` - Debug features and test level settings
- Many other specialized config files

### Key Features

**HackerSM64 Features:**
- Extended boundaries for large levels
- Puppycam (custom camera system)
- Advanced collision fixes (slope fix, ledgegrab fix, hanging improvements)
- Platform Displacement 2
- Silhouette rendering when Mario is behind surfaces
- Configurable world scaling for console compatibility
- Expanded audio heap and microcode options
- Visual debug for collision surfaces and hitboxes

**Mario Builder 64 Features:**
- In-game level editor with object placement
- Custom level saving/loading system (uses libcart for cart filesystem)
- Comprehensive object library (400+ placeable objects)
- Real-time level testing and building

### Build System

The Makefile supports:
- Multiple console targets (N64, iQue Player)
- Various compression methods (RNC1/2, GZIP, MIO0, YAY0, uncompressed)
- Different graphics microcodes (F3DZEX, F3DEX2, L3DEX2, etc.)
- Automatic baserom extraction and asset management
- Comprehensive optimization flags for different code sections
- Debug builds with UNFLoader support for flashcart development

### Dependencies

**Required:**
- US and JP Super Mario 64 ROMs (`baserom.us.z64`, `baserom.jp.z64`)
- MIPS cross-compiler (`gcc-mips-linux-gnu` on Debian/Ubuntu)
- Python 3 with `pypng` and `bitstring` packages

**Build artifacts are placed in:**
- `build/$(VERSION)_$(CONSOLE)/` - All build outputs
- Target ROM: `build/$(VERSION)_$(CONSOLE)/mb64.z64`

## Mario Builder 64 Level Loading System

This section documents the internal level loading mechanism within Mario Builder 64, focusing on how the game loads levels from the cartridge filesystem into memory and spawns them into the game world.

### Level Loading Entry Point

**Game initialization sequence:**
1. `src/game/level_update.c:1295` - `init_level()` checks if `mb64_mode == MB64_MODE_UNINITIALIZED`
2. If uninitialized, calls `mb64_init()` at `src/mb64/main.c:3381`
3. `mb64_init()` immediately calls `load_level()` at line 3382

**Prerequisites for level loading:**
- `mb64_file_name[MAX_FILE_NAME_SIZE]` must be set (global variable at `main.c:3066`)
- Level filename comes from user input in menu system (`src/mb64/menu.c:1773`)
- Menu copies keyboard input to `mb64_file_name` and appends ".mb64" extension

### Core Level Loading Function: `load_level()`

**Function location:** `src/mb64/main.c:3197-3379`

**Local variables:**
- `s32 i, j` - Loop iterators
- `u8 fresh = FALSE` - Flag indicating new vs existing level
- `TCHAR path[256]` - Full file path buffer
- `FRESULT code` - FatFS operation result
- `UINT bytes_read` - FatFS read byte count

**Global data structures accessed:**
- `mb64_save` - Level header structure containing all metadata
- `mb64_grid_data[64][64][64]` - 3D spatial grid for collision detection
- `mb64_tile_data[MB64_TILE_POOL_SIZE]` - Array of terrain tiles (max 16384)
- `mb64_object_data[MB64_MAX_OBJS]` - Array of game objects (max 512)
- `mb64_file` - FatFS file handle
- `mb64_file_info` - FatFS file information

### Level Loading Process (Step by Step)

#### 1. Memory Initialization (lines 3202-3203)
```c
bzero(&mb64_save, sizeof(mb64_save));
bzero(&mb64_grid_data, sizeof(mb64_grid_data));
```
Clears level header and 3D grid to ensure clean slate.

#### 2. File Path Construction and Check (lines 3205-3207)
```c
TCHAR path[256];
create_level_file_path(path, mb64_file_name, NULL);
FRESULT code = f_stat(path, &mb64_file_info);
```
Creates full cartridge path from filename and checks if file exists using FatFS.

#### 3A. Existing Level Loading (lines 3208-3219)
**If file exists (`code == FR_OK`):**
```c
f_open(&mb64_file, path, FA_READ | FA_WRITE);
f_read(&mb64_file, &mb64_save, sizeof(mb64_save), &bytes_read);
f_read(&mb64_file, &mb64_tile_data, sizeof(mb64_tile_data[0]) * mb64_save.tile_count, &bytes_read);
f_read(&mb64_file, &mb64_object_data, sizeof(mb64_object_data[0]) * mb64_save.object_count, &bytes_read);
f_close(&mb64_file);
```
**File format structure:**
1. Level header (`mb64_save`) - metadata, author, settings, counts
2. Tile array - `tile_count` entries of `struct mb64_tile`
3. Object array - `object_count` entries of `struct mb64_obj`

#### 3B. Fresh Level Creation (lines 3221-3271)
**If file doesn't exist:**
- Sets `fresh = TRUE` flag
- Copies filename to `mb64_file_info.fname`
- Sets `mb64_save.version = MB64_VERSION`
- **Creates default spawn point:**
  ```c
  mb64_save.object_count = 1;
  mb64_object_data[0].x = 32;  // Grid center
  mb64_object_data[0].z = 32;  // Grid center  
  mb64_object_data[0].y = mb64_templates[mb64_lopt_template].spawnHeight;
  mb64_object_data[0].type = OBJECT_TYPE_MARIO_SPAWN;
  ```
- Loads template settings from `mb64_templates[]` array
- Creates starting platform if template requires it (3x3 block platform under spawn)

#### 4. Version Compatibility (lines 3273-3276)
```c
if (mb64_save.version < MB64_VERSION) {
    append_puppyprint_log("Performing upgrade from version %d", mb64_save.version);
    mb64_perform_file_upgrade(&mb64_save, &mb64_tile_data, &mb64_object_data);
}
```
Handles backwards compatibility for older level formats.

#### 5. Global State Synchronization (lines 3279-3328)
**Updates count variables:**
```c
mb64_tile_count = mb64_save.tile_count;
mb64_object_count = mb64_save.object_count;
```

**Copies settings to live options:**
- `mb64_lopt_costume = mb64_save.costume`
- `mb64_lopt_seq[0-2] = mb64_save.seq[0-2]` (music)
- `mb64_lopt_theme = mb64_save.theme`
- All boundary, water, and visual settings

**Copies toolbar and custom theme data:**
```c
bcopy(&mb64_save.toolbar, &mb64_toolbar, sizeof(mb64_toolbar));
bcopy(&mb64_save.custom_theme, &mb64_curr_custom_theme, sizeof(struct mb64_custom_theme));
```

#### 6. Grid Size Configuration (lines 3299-3312)
Based on `mb64_lopt_size`, sets grid boundaries:
- Size 0: 32x32 usable area (16-48 range)
- Size 1: 48x48 usable area (8-56 range)  
- Size 2: 64x64 full area (0-64 range)

#### 7. Tile Grid Population (lines 3331-3355)
**For each tile in `mb64_tile_data[]`:**
1. Gets tile type index for organizing by material
2. Places tile into 3D grid using `place_terrain_data()`
3. Sets waterlogged flag: `get_grid_tile(pos)->waterlogged = mb64_tile_data[i].waterlogged`
4. Builds index arrays for fast material-based lookup

#### 8. Trajectory Processing (lines 3357-3371)
- Counts trajectory objects for path planning
- Converts saved trajectory data to world coordinates:
  ```c
  mb64_trajectory_list[i][j][1] = GRID_TO_POS(mb64_save.trajectories[i][j].x);
  mb64_trajectory_list[i][j][2] = GRID_TO_POS(mb64_save.trajectories[i][j].y);
  mb64_trajectory_list[i][j][3] = GRID_TO_POS(mb64_save.trajectories[i][j].z);
  ```

#### 9. Finalization (lines 3374-3378)
- Calls `mb64_set_data_overrides()` for game-specific setup
- Updates painting data if loading existing level (`!fresh`)

### Object Spawning: `generate_objects_to_level()`

**Function location:** `src/mb64/main.c:2524-2551`
**Called from:** Mode switching logic when entering play mode (`main.c:3440`)

**Process:**
1. Resets `mb64_play_stars_max = 0`
2. **For each object in `mb64_object_data[]`:**
   ```c
   struct mb64_object_info *info = &mb64_object_type_list[mb64_object_data[i].type];
   obj = spawn_object(gMarioObject, info->model_id, info->behavior);
   obj->oPosX = GRID_TO_POS(mb64_object_data[i].x);
   obj->oPosY = GRID_TO_POS(mb64_object_data[i].y) - TILE_SIZE/2 + info->y_offset;
   obj->oPosZ = GRID_TO_POS(mb64_object_data[i].z);
   obj->oFaceAngleYaw = mb64_object_data[i].rot * 0x4000;  // 90° increments
   ```
3. **Star ID assignment:**
   - Objects with star flags get sequential star IDs (0-62)
   - Updates `mb64_play_stars_max` counter
4. Sets behavior parameters and imbue (power-up) properties

### Mode Transition and Timing

**Level loading occurs during mode transitions:**
1. **Initialization:** `MB64_MODE_UNINITIALIZED` → calls `mb64_init()` → `load_level()`
2. **Play mode entry:** `MB64_MODE_PLAY` → calls `generate_objects_to_level()`
3. **Editor mode:** Level data already loaded, objects spawned on demand

**Key global variables:**
- `mb64_mode` - Current operating mode
- `mb64_target_mode` - Desired mode for transitions
- `mb64_level_action` - Specific action (build, play, test)

### File Format Details

**Level files (`.mb64`) contain:**
1. **Header** (`struct mb64_level_save_header`) - Fixed size metadata
2. **Tile data** - Variable array based on `tile_count`
3. **Object data** - Variable array based on `object_count`

**Coordinate system:**
- Grid coordinates: 0-63 in each dimension
- World coordinates: `GRID_TO_POS(grid) = (grid * TILE_SIZE - (32 * TILE_SIZE) + TILE_SIZE/2)` where `TILE_SIZE = 256`
- Grid center offset: World coordinates are centered around grid position 32, with proper tile alignment
- Rotation: 0-3 representing 0°, 90°, 180°, 270°

## Library Integration Level Loading

The Mario Builder 64 library export (via `mb64_api.h`) provides external access to the same internal level loading mechanisms documented above. The library functions enable integrators to trigger and control the same loading process that occurs within the game.

### API Function Mapping to Internal Process

**Direct function forwarding** (`src/mb64/level_api.c:25-36`):
```c
// Library API directly calls internal functions
int mb64_load_level_data(const char* filename) {
    strncpy(mb64_file_name, filename, MAX_FILE_NAME_SIZE - 1);
    load_level();  // Same function as internal game calls
    return (mb64_object_count > 0 || mb64_tile_count > 0) ? 0 : -1;
}

int mb64_generate_level_objects(void) {
    generate_objects_to_level();  // Same function as mode switching calls
    return 0;
}
```

**Data accessor forwarding** (`src/mb64/level_api.c:40-46`):
```c
// Library API provides direct access to same global variables
struct mb64_obj* mb64_get_object_data(void) { return mb64_object_data; }
struct mb64_tile* mb64_get_tile_data(void) { return mb64_tile_data; }
struct mb64_level_save_header* mb64_get_save_data(void) { return &mb64_save; }
```

### Alternative Loading Pathways

The library also provides alternative I/O methods that achieve the same internal state:

#### Disk Loading (`src/mb64/level_disk_api.c`)
**Replicates the same data population as internal `load_level()`:**
1. **File reading** - Uses standard C I/O instead of FatFS
2. **Memory clearing** - `bzero()` calls on same global structures  
3. **Header parsing** - Reads into same `mb64_save` structure
4. **Array population** - Fills same `mb64_tile_data[]` and `mb64_object_data[]` arrays
5. **Count updates** - Sets same `mb64_tile_count` and `mb64_object_count` variables
6. **Compatibility upgrades** - Calls same `mb64_perform_file_upgrade()` function

```c
// Disk loading achieves identical result to cartridge loading
mb64_load_level_from_disk("/path/to/level.mb64");
// Same global variables populated as load_level() does
```

#### Memory Buffer Loading
**Processes in-memory data through the same validation and upgrade pipeline:**
- Uses `memcpy()` instead of file I/O operations
- Populates identical global data structures
- Applies same compatibility upgrades and bounds checking

### Level Loading Process Equivalence

**All library loading methods result in identical internal state:**

| Internal Process Step | Library API Equivalent |
|---|---|
| 1. `mb64_init()` → `load_level()` | `mb64_load_level_data()` or `mb64_load_level_from_disk()` |
| 2. FatFS file operations | Standard C I/O operations (disk API) |
| 3. Global data population | Same global variables populated |
| 4. Version compatibility | Same upgrade functions called |
| 5. Mode switch → `generate_objects_to_level()` | `mb64_generate_level_objects()` |
| 6. Object spawning | Same `spawn_object()` calls |
| 7. Coordinate conversion | Same `GRID_TO_POS()` macro |

### Timing and Control Differences

**Internal game timing:**
```
Game startup → init_level() → mb64_init() → load_level() → [wait for mode switch] → generate_objects_to_level()
```

**Library timing options:**
```
// Option 1: Immediate loading + spawning
mb64_load_level_complete("/path/to/level.mb64");  // load_level() + generate_objects_to_level() combined

// Option 2: Separated control (matches internal timing)
mb64_load_level_from_disk("/path/to/level.mb64"); // load_level() equivalent
// ... custom processing/validation ...
mb64_generate_level_objects();                   // generate_objects_to_level() equivalent

// Option 3: Original internal behavior
mb64_load_level_data("LEVEL.mb64");              // calls load_level() directly
mb64_generate_level_objects();                   // calls generate_objects_to_level() directly
```

### Integration Benefits

**The library preserves all internal functionality while adding flexibility:**

1. **Same data structures** - Integrators access the exact same `mb64_save`, `mb64_tile_data[]`, `mb64_object_data[]` that the internal game uses
2. **Same object spawning** - `mb64_generate_level_objects()` calls the same `generate_objects_to_level()` function with identical star ID assignment and world coordinate conversion
3. **Same file format** - All loading methods read the same `.mb64` format with automatic version upgrades
4. **Multiple I/O backends** - Cartridge (FatFS), disk (stdio), or memory (memcpy) all achieve identical results
5. **Timing control** - Integrators can separate loading from object spawning or combine them as needed

### Validation of API Adequacy

**The library API adequately supports level loading because:**

✅ **Complete internal access** - All core functions (`load_level`, `generate_objects_to_level`) accessible  
✅ **Identical data flow** - Same global variables populated with same data  
✅ **Multiple I/O options** - Supports cartridge, disk, and memory loading  
✅ **Same object spawning** - Uses identical object creation and coordinate conversion  
✅ **Preserved timing** - Can replicate internal loading sequence or provide immediate loading  
✅ **Format compatibility** - Handles same file format with same upgrade mechanisms

The library successfully enables external projects to trigger and control the same level loading process that occurs within Mario Builder 64, while providing additional I/O flexibility for cross-platform integration.

### Development Notes

- The project includes both vanilla SM64 code and extensive modifications
- Level editor code can be found by searching for `mb64_` prefixed functions
- Configuration toggles are extensively used - check `include/config/` before modifying behavior
- The codebase supports both console and emulator builds with automatic detection
- Asset extraction is automatic but requires valid base ROMs