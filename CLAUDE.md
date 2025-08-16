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

### Development Notes

- The project includes both vanilla SM64 code and extensive modifications
- Level editor code can be found by searching for `mb64_` prefixed functions
- Configuration toggles are extensively used - check `include/config/` before modifying behavior
- The codebase supports both console and emulator builds with automatic detection
- Asset extraction is automatic but requires valid base ROMs