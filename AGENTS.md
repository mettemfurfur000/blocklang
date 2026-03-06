# AGENTS.md - BlockLang Development Guide

This document provides guidelines for AI agents working on the BlockLang codebase.

## Project Overview

BlockLang is a tiny bytecode language with an 8-bit word size. It's a C/C++ project that implements a grid-based VM where each cell runs a bytecode program independently.

## Build Commands

### Full Build
```bash
make          # Builds all targets: assembler, singleblock, blocklang
make all      # Same as make
```

### Individual Targets
```bash
make test          # Builds test_app (integration test runner)
make assembler     # Builds 'basm' (blocklang assembler)
make singleblock   # Builds 'block' (single block VM)
make blocklang     # Builds 'blocklang' (full grid VM)
```

### Clean
```bash
make clean         # Removes build/* and obj/*
```

### Running Tests
There is no formal unit test framework. Tests are run as integration tests using the test_app:
```bash
./build/test_app <input_file> <width> <height> <debug> <ticks_limit> <print_strings> [io_specifications...]
```
Example:
```bash
./build/test_app test.bl 2 2 true 128 true "in:up:0:1,2,3" "out:up:="
```

To run a single test:
1. Create a `.bl` assembly file in `asm_programs/` or `example_blocklang/`
2. Build with `make test`
3. Run with appropriate arguments

### Test I/O Specification Format
The test_app uses this format for I/O slots:
```
{in|out}:{up|left|right|down}:slot:{[value1,value2,value3]|=}
```
- `in` / `out`: Input or output slot
- `up|left|right|down`: Which side of the grid
- `slot`: Slot number (0-based)
- `[values]` or `=`: Either a comma-separated list of byte values, or `=` for output buffers

Example I/O specifications:
```bash
"in:up:0:1,2,3"      # Input on up side, slot 0, values 1,2,3
"out:down:0:="       # Output on down side, slot 0, 255-byte buffer
"in:right:1:65,66"   # Input on right side, slot 1, values 'A','B'
```

## Code Style Guidelines

### Formatting (from .clang-format)

- **Base Style**: Microsoft
- **Indent Width**: 4 spaces
- **Pointer Alignment**: Right (e.g., `u8 *ptr`)
- **Brace Wrapping**: After control statements, case labels, classes, enums, functions, namespaces, structs, unions

### Code Conventions

#### Types
- Use custom integer types defined in `include/definitions.h`:
  - `u8`, `u16`, `u32`, `u64` (unsigned)
  - `i8`, `i16`, `i32`, `i64` (signed)
- Always use fixed-width types for VM state (8-bit values throughout)

#### Naming Conventions
- **Functions/variables**: snake_case (e.g., `initialize_grid`, `current_instruction`)
- **Constants/macros**: SCREAMING_SNAKE_CASE (e.g., `BYTECODE_LIMIT`, `CASE(x)`)
- **Enums**: lowercase (e.g., `up`, `right`, `down`, `left`)
- **Structs**: snake_case (e.g., `block`, `grid`, `io_slot`)

#### File Organization
- Header files in `include/`
- Source files in `src/`
- Entry points in `mains/`
- Use include guards: `#ifndef BLOCKLANG_DEF_H`

#### Include Order
1. Corresponding header (e.g., `#include "../include/definitions.h"`)
2. System headers (`<stdbool.h>`, `<stdio.h>`, etc.)
3. Other local headers

#### Error Handling
- Use `assert()` for debugging invariants
- Use `ERROR(format, ...)` macro for runtime errors (prints file:line)
- Use `ERROR_ABORT(format, ...)` for fatal errors that should terminate
- Return `bool` for functions that can fail
- Check all allocations with `if (!ptr) { ... }`

#### Documentation
- Minimize comments (follow existing code style)
- Document complex VM behavior in header files
- Keep the README.md updated with language specification

## Architecture

### Core Components
- **VM** (`src/vm.c`): Grid execution, block state, I/O handling
- **Assembler** (`src/asm.c`): Assembly to bytecode conversion
- **Parser** (`src/parser.c`): Syntax parsing
- **Tokenizer** (`src/tokenizer.c`): Lexical analysis
- **Codegen** (`src/codegen.c`): Code generation utilities
- **Debug** (`src/debug.c`): Debugging utilities
- **Objfile** (`src/objfile.c`): Object file handling
- **Utils** (`src/utils.c`): Common utilities

### Key Data Structures
- `instruction`: 1-byte packed struct {operation:4, target:4}
- `block`: Per-block VM state (registers, stack, bytecode)
- `grid`: Collection of blocks with I/O slots

### Compilation
- C files compiled with: `gcc -O0 -Wall -Wpedantic -fanalyzer -g -pg -no-pie`
- C++ files compiled with: `g++ -std=c++17` (same flags)
- Links with: `-lm -lmingw32 -lws2_32` (Windows)

### Key Source Files

- **Header files** (`include/`): definitions.h, utils.h, parser.h, tokenizer.h, objfile.h, debug.h, codegen.h
- **Source files** (`src/`): vm.c, asm.c, parser.c, tokenizer.c, codegen.c, objfile.c, debug.c, utils.c, base.c
- **Entry points** (`mains/`): assembler.c, singleblock.c, blocklang.c, test.c

## Common Development Tasks

### Adding a New Operation
1. Add to `operations` enum in `include/definitions.h`
2. Implement in VM switch statement (`src/vm.c`)
3. Add to assembler grammar (if applicable)
4. Test with sample `.bl` program

### Adding a New Target
1. Add to `targets` enum in `include/definitions.h`
2. Ensure `TARGET_GUARD_LAST <= 16` (4-bit limit)
3. Handle in VM target resolution

### Debugging
- Enable debug mode: pass `true` as 4th argument to test_app
- Use `gdb` or `lldb` with debug symbols (`-g` flag enabled)
- Check `asm_programs/` and `example_blocklang/` for sample programs
