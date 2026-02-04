#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/debug.h"

debug_info create_debug_info(const char *source, const u8 *bytecode, u8 bytecode_len)
{
    debug_info info = {0};

    if (source == NULL)
    {
        // No debug info, just bytecode
        info.bytecode = (u8 *)bytecode;
        info.bytecode_length = bytecode_len;
        info.has_debug_info = false;
        return info;
    }

    // Create temporary storage for our format (will be written to file immediately)
    info.source_code = (char *)source;
    info.source_length = strlen(source);
    info.bytecode = (u8 *)bytecode;
    info.bytecode_length = bytecode_len;
    info.has_debug_info = true;

    return info;
}

debug_info parse_debug_info(const u8 *buffer, u32 len)
{
    debug_info info = {0};

    if (len < 1)
        return info;

    // Check for debug magic
    if (buffer[0] != DEBUG_MAGIC)
    {
        // No debug info, entire buffer is bytecode
        info.bytecode = (u8 *)buffer;
        info.bytecode_length = len;
        info.has_debug_info = false;
        return info;
    }

    // Parse debug header
    if (len < 4)
        return info; // Incomplete header

    u16 source_len = ((u16)buffer[1] << 8) | buffer[2];
    u32 expected_total = 1 + 2 + source_len + 1 + buffer[4 + source_len];

    if (len < expected_total)
        return info; // Incomplete data

    info.source_code = (char *)(buffer + 3);
    info.source_length = source_len;
    info.bytecode = (u8 *)(buffer + 4 + source_len);
    info.bytecode_length = buffer[3 + source_len];
    info.has_debug_info = true;

    return info;
}

void free_debug_info(debug_info info)
{
    // For now, create_debug_info doesn't allocate, so nothing to free
    // This is kept for future expansion
    (void)info;
}

u16 get_source_line_for_instruction(debug_info *info, u8 instruction_index)
{
    if (!info->has_debug_info || info->source_length == 0)
        return 0;

    // Rough heuristic: count newlines in source up to estimated position
    // Future: use actual instruction-to-line metadata
    u16 estimated_chars = (info->source_length * instruction_index) / info->bytecode_length;
    estimated_chars = estimated_chars > info->source_length ? info->source_length : estimated_chars;

    u16 line = 1;
    for (u16 i = 0; i < estimated_chars; i++)
    {
        if (info->source_code[i] == '\n')
            line++;
    }

    return line;
}

source_line *get_source_context(debug_info *info, u16 center_line, u8 context_lines, u8 *out_count)
{
    if (!info->has_debug_info || info->source_length == 0)
    {
        *out_count = 0;
        return NULL;
    }

    // Count total lines
    u16 total_lines = 1;
    for (u16 i = 0; i < info->source_length; i++)
        if (info->source_code[i] == '\n')
            total_lines++;

    // Determine range
    u16 start_line = center_line > context_lines ? center_line - context_lines : 1;
    u16 end_line = center_line + context_lines;
    if (end_line > total_lines)
        end_line = total_lines;

    u8 range = end_line - start_line + 1;
    source_line *lines = (source_line *)malloc(sizeof(source_line) * range);
    if (!lines)
    {
        *out_count = 0;
        return NULL;
    }

    // Extract lines
    u16 current_line = 1;
    const char *line_start = info->source_code;
    u8 index = 0;

    for (u16 i = 0; i <= info->source_length && current_line <= end_line; i++)
    {
        if (current_line >= start_line && current_line <= end_line)
        {
            lines[index].line_number = current_line;
            lines[index].line = line_start;
            index++;
        }

        if (i < info->source_length && info->source_code[i] == '\n')
        {
            current_line++;
            line_start = info->source_code + i + 1;
        }
    }

    *out_count = index;
    return lines;
}
