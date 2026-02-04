#ifndef BLOCKLANG_OBJFILE_H
#define BLOCKLANG_OBJFILE_H 1

#include "definitions.h"
#include <stdio.h>

/*
    Block Object File API
    
    Centralizes reading and writing of compiled block programs with debug info.
    Supports both debug and release builds with static allocation.
    
    File Format (with debug info):
    [1 byte]  Magic: 0xDB (219)
    [2 bytes] Source length (big-endian u16)
    [N bytes] Original source code
    [1 byte]  Bytecode length (u8)
    [M bytes] Compiled bytecode instructions
    [2*M bytes] Line table (u16 per instruction, maps instruction index to source line)
    
    File Format (without debug info):
    [1 byte]  Magic: 0xBC (188) - raw bytecode
    [1 byte]  Bytecode length (u8)
    [M bytes] Compiled bytecode instructions
*/

#define OBJFILE_MAGIC_DEBUG 0xDB
#define OBJFILE_MAGIC_BYTECODE 0xBC
#define MAX_SOURCE_SIZE 4096
#define MAX_BYTECODE_SIZE 256
#define MAX_LINE_TABLE_SIZE 256

typedef struct
{
    char source[MAX_SOURCE_SIZE];
    u16 source_length;
    u8 bytecode[MAX_BYTECODE_SIZE];
    u8 bytecode_length;
    u16 line_table[MAX_LINE_TABLE_SIZE];  // Maps instruction index to source line
    bool has_debug_info;
} block_object_file;

/*
    Write a block object file with debug info.
    Embeds source code and bytecode into a single file.
    
    file: FILE* opened for writing in binary mode
    source: source code string (can be NULL for no debug info)
    bytecode: compiled bytecode instructions
    bytecode_len: number of bytecode instructions
    line_table: lookup table mapping instruction index to source line (can be NULL)
    
    returns: true on success, false on write error
*/
bool objfile_write_with_debug(FILE *file, const char *source, const u8 *bytecode, u8 bytecode_len, const u16 *line_table);

/*
    Read a block object file.
    Handles both debug and raw bytecode formats.
    Uses static allocation - no dynamic memory.
    
    file: FILE* opened for reading in binary mode
    out: pointer to block_object_file struct to fill (caller allocated)
    
    returns: true on success, false on read error
*/
bool objfile_read(FILE *file, block_object_file *out);

/*
    Read a block object file from a filename.
    Convenience wrapper around objfile_read.
    
    filename: path to object file
    out: pointer to block_object_file struct to fill
    
    returns: true on success, false on read error
*/
bool objfile_read_file(const char *filename, block_object_file *out);

/*
    Get source line number for a given bytecode instruction index.
    Estimates based on newline count in source.
    
    obj: pointer to block_object_file
    instruction_index: instruction number in bytecode
    
    returns: estimated source line number (1-based)
*/
u16 objfile_get_source_line(const block_object_file *obj, u8 instruction_index);

#endif
