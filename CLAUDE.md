# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project Overview

Mario Builder 64 is a Super Mario 64 ROM hack that allows players to create
custom levels in-game. This project is built on top of the **SM64: Beyond the
Cursed Mirror** source code and uses the **HackerSM64** repo as a base. The
level editor-specific code can be found in `src/mb64/` and its counterparts.

This is a complete decompilation of Super Mario 64 with extensive modifications
and enhancements for ROM hacking capabilities.

## Build Commands

### Basic Build

```bash
make                    # Build the ROM (default: us version, sram save, f3dzex microcode)
make clean             # Clean build artifacts
make distclean         # Clean everything including extracted assets
```

### Build Options

Key build variables (use `make OPTION=value`):

- `VERSION=us|jp|eu|sh` - Game version (default: us)
- `CONSOLE=n64|bb` - Target console (n64 or iQue Player)
- `SAVETYPE=sram|eep4k|eep16k` - Save type (default: sram)
- `GRUCODE=f3dzex|f3dex2|f3dex|l3dex2|super3d` - Graphics microcode (default:
  f3dzex)
- `COMPRESS=rnc1|rnc2|gzip|mio0|yay0|uncomp` - Compression method (default:
  rnc1)
- `COMPILER=gcc|clang` - C compiler (default: gcc)
- `UNF=1` - Enable UNFLoader for flashcart debugging
- `VERBOSE=1` - Show detailed build output

### Testing

```bash
make test              # Run ROM in mupen64plus emulator
make load              # Load ROM via UNFLoader (requires UNF=1 build)
make unf               # Load ROM with UNFLoader in debug mode
```

### Development Tools

```bash
make rebuildtools      # Rebuild all build tools
make patch             # Create BPS patch file from baserom
```

## Architecture Overview

### Core Directories

- `src/mb64/` - **Mario Builder 64 specific code** - level editor functionality
- `src/game/` - Core game engine, Mario behaviors, object interactions
- `src/engine/` - Low-level engine systems (math, collision, rendering)
- `src/audio/` - Audio system and sound processing
- `src/menu/` - Menu systems (file select, title screen)
- `actors/` - 3D models and actor definitions
- `levels/` - Level data, geometry, and scripts
- `include/config/` - **Configuration headers** for toggling features

### Key Source Files

- `src/mb64/main.c` - Main level editor logic
- `src/mb64/menu.c` - Level editor menu system
- `src/mb64/data.c` - Level editor data structures
- `src/game/mario.c` - Mario's core behavior system
- `src/engine/surface_collision.c` - Collision detection system
- `src/game/camera.c` - Camera system

### Configuration System

The `include/config/` directory contains toggleable features:

- `config_game.h` - Game mechanics settings
- `config_graphics.h` - Graphics and rendering options
- `config_camera.h` - Camera behavior (including Puppycam)
- `config_collision.h` - Collision system modifications
- `config_debug.h` - Debug features and test level settings
- `config_movement.h` - Mario movement modifications
- `config_audio.h` - Audio system enhancements

## Prerequisites

### Required ROMs

This repo requires **both** US and JP ROMs:

- Place `baserom.us.z64` in the repository root
- Place `baserom.jp.z64` in the repository root

### Required Dependencies

- `gcc-mips-linux-gnu` - MIPS cross-compiler
- `python3` with `pypng` and `bitstring` packages
- Standard build tools (make, etc.)

Install on Ubuntu/Debian:

```bash
sudo apt install gcc-mips-linux-gnu
pip install pypng bitstring
```

## Important Notes

### Asset Extraction

When building from a freshly cloned repo, the baserom extractor will overwrite
assets. Discard the extra changes that appear in your source control.

### Code Quality Warning

Some code (particularly older sections) may be "unsightly and abominable" as
noted in the README. Exercise caution when working with legacy code sections.

### HackerSM64 Features

This repo includes many ROM hacking enhancements:

- Extended boundaries and collision improvements
- Debug mode with coordinate display
- Configurable movement physics
- Custom camera systems (Puppycam, Reonucam)
- Enhanced lighting system
- Widescreen support
- Console/emulator detection
- Visual debug overlays for collision surfaces

### Build System

- Uses custom MIPS toolchain detection
- Supports multiple compression formats
- Includes automatic asset extraction
- Has extensive configuration options via Makefile variables
- Includes tools for texture conversion, sound assembly, and ROM patching
