#ifndef BLOCKLANG_DEBUG_H
#define BLOCKLANG_DEBUG_H 1

#include "definitions.h"

/*
    Debug info format embedded in bytecode files:
    
    [1 byte] magic header: 0xDB (219) - indicates debug info follows
    [2 bytes] source length: u16 (big-endian)
    [source_length bytes] original source code
    [1 byte] instruction count: u8
    [instruction_count bytes] bytecode instructions
    
    This allows the debug viewer to:
    - Display original source code
    - Map instructions back to source lines
    - Step through execution with source context
    
    Future expansion: instruction-to-line mapping metadata
*/

#define DEBUG_MAGIC 0xDB

typedef struct
{
    char *source_code;
    u16 source_length;
    u8 *bytecode;
    u8 bytecode_length;
    bool has_debug_info;
} debug_info;

/*
    Embed debug information into a bytecode buffer.
    Allocates new memory and returns it, original bytecode parameter is not modified.
    
    source: original source code string (can be NULL for no debug info)
    bytecode: compiled bytecode instructions
    bytecode_len: number of bytecode instructions
    out_len: pointer to store total output length (including debug header)
    
    returns: newly allocated buffer with debug info embedded, or NULL on failure
*/
debug_info create_debug_info(const char *source, const u8 *bytecode, u8 bytecode_len);

/*
    Parse debug information from a bytecode buffer.
    
    buffer: buffer containing bytecode (possibly with debug info)
    len: length of buffer
    
    returns: debug_info struct with pointers into buffer (does NOT allocate new memory)
*/
debug_info parse_debug_info(const u8 *buffer, u32 len);

/*
    Free debug info that was created with create_debug_info
*/
void free_debug_info(debug_info info);

/*
    Get the source line number for a given bytecode instruction index
    For now returns estimated line based on instruction count
    Future: will use instruction-to-line metadata
*/
u16 get_source_line_for_instruction(debug_info *info, u8 instruction_index);

/*
    Get source code lines around a given line number
    out_start and out_end are output parameters for the actual range returned
*/
typedef struct
{
    const char *line;
    u16 line_number;
} source_line;

source_line *get_source_context(debug_info *info, u16 center_line, u8 context_lines, u8 *out_count);

#endif
