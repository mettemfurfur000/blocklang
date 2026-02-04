#include "../include/objfile.h"
#include "../include/utils.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>


bool objfile_write_with_debug(FILE *file, const char *source, const u8 *bytecode, u8 bytecode_len,
                              const u16 *line_table)
{
    if (!file || !bytecode || bytecode_len == 0)
        return false;

    if (source == NULL)
    {
        // Write raw bytecode format
        u8 magic = OBJFILE_MAGIC_BYTECODE;
        if (fwrite(&magic, 1, 1, file) != 1)
            return false;
        if (fwrite(&bytecode_len, 1, 1, file) != 1)
            return false;
        if (fwrite(bytecode, 1, bytecode_len, file) != bytecode_len)
            return false;
        return true;
    }

    // Write debug format with embedded source
    u16 source_len = strlen(source);

    if (source_len > MAX_SOURCE_SIZE)
        return false; // Source too long

    u8 magic = OBJFILE_MAGIC_DEBUG;
    if (fwrite(&magic, 1, 1, file) != 1)
        return false;

    // Source length (endianless)
    fwrite_endianless(&source_len, sizeof(source_len), 1, file);

    // Source code
    if (fwrite(source, 1, source_len, file) != source_len)
        return false;

    // Bytecode length
    if (fwrite(&bytecode_len, 1, 1, file) != 1)
        return false;

    // Bytecode
    if (fwrite(bytecode, 1, bytecode_len, file) != bytecode_len)
        return false;

    // Line table - store line number for each instruction (endianless)
    if (line_table)
    {
        fwrite_endianless(line_table, sizeof(line_table[0]), bytecode_len, file);
    }
    else
    {
        // Write zeros if no line table provided
        for (u8 i = 0; i < bytecode_len; i++)
        {
            u16 zero = 0;
            fwrite_endianless(&zero, sizeof(zero), 1, file);
        }
    }

    return true;
}

bool objfile_read(FILE *file, block_object_file *out)
{
    if (!file || !out)
        return false;

    memset(out, 0, sizeof(block_object_file));

    // Read magic byte
    u8 magic;
    if (fread(&magic, 1, 1, file) != 1)
        return false;

    if (magic == OBJFILE_MAGIC_BYTECODE)
    {
        // Raw bytecode format
        if (fread(&out->bytecode_length, 1, 1, file) != 1)
            return false;

        if (fread(out->bytecode, 1, out->bytecode_length, file) != out->bytecode_length)
            return false;

        out->source_length = 0;
        out->has_debug_info = false;
        return true;
    }
    else if (magic == OBJFILE_MAGIC_DEBUG)
    {
        // Debug format with embedded source
        fread_endianless(&out->source_length, sizeof(out->source_length), 1, file);

        if (out->source_length > MAX_SOURCE_SIZE)
            return false;

        if (fread(out->source, 1, out->source_length, file) != out->source_length)
            return false;

        if (fread(&out->bytecode_length, 1, 1, file) != 1)
            return false;

        if (fread(out->bytecode, 1, out->bytecode_length, file) != out->bytecode_length)
            return false;

        // Read line table (endianless)
        fread_endianless(out->line_table, sizeof(out->line_table[0]), out->bytecode_length, file);

        out->has_debug_info = true;
        return true;
    }
    else
    {
        // Unknown format - try to treat as raw bytecode from start
        // Rewind and read length as first byte
        rewind(file);
        if (fread(&out->bytecode_length, 1, 1, file) != 1)
            return false;

        if (fread(out->bytecode, 1, out->bytecode_length, file) != out->bytecode_length)
            return false;

        out->source_length = 0;
        out->has_debug_info = false;
        return true;
    }
}

bool objfile_read_file(const char *filename, block_object_file *out)
{
    if (!filename || !out)
        return false;

    FILE *f = fopen(filename, "rb");
    if (!f)
        return false;

    bool result = objfile_read(f, out);
    fclose(f);
    return result;
}

u16 objfile_get_source_line(const block_object_file *obj, u8 instruction_index)
{
    if (!obj || !obj->has_debug_info || instruction_index >= obj->bytecode_length)
        return 0;

    // Direct lookup from line table
    return obj->line_table[instruction_index];
}
