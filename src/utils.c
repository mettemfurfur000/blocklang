#include "../include/utils.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static endian_type system_endianness = ENDIAN_NONE;

// Detect system endianness at runtime
endian_type get_system_endianness(void)
{
    static endian_type cached = ENDIAN_UNKNOWN;

    if (cached != ENDIAN_UNKNOWN)
        return cached;

    uint32_t test_value = 0x01020304;
    unsigned char *bytes = (unsigned char *)&test_value;

    if (bytes[0] == 0x01)
        cached = ENDIAN_BIG;
    else if (bytes[0] == 0x04)
        cached = ENDIAN_LITTLE;
    else
        cached = ENDIAN_UNKNOWN;

    return cached;
}

static void swap_bytes(unsigned char *bytes, size_t size)
{
    for (size_t i = 0; i < size / 2; i++)
    {
        unsigned char temp = bytes[i];
        bytes[i] = bytes[size - 1 - i];
        bytes[size - 1 - i] = temp;
    }
}

char *read_to_heap(const char *filename)
{
    if (!filename)
        return NULL;
    FILE *file = fopen(filename, "r");
    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = calloc(filesize + 1, 1);
    if (!buffer)
    {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, filesize, file);
    buffer[filesize] = '\0';

    fclose(file);
    return buffer;
}

char *read_to_heap_bin(const char *filename, long *out_len)
{
    if (!out_len || !filename)
        return NULL;
    FILE *file = fopen(filename, "rb");
    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = calloc(filesize + 1, 1);
    if (!buffer)
    {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, filesize, file);

    *out_len = filesize;

    fclose(file);
    return buffer;
}

void fwrite_endianless(const void *ptr, size_t size, size_t count, FILE *stream)
{
    if (system_endianness == ENDIAN_NONE)
        system_endianness = get_system_endianness();

    // Always write in big-endian format for portability
    // If system is little-endian, swap bytes before writing
    if (system_endianness == ENDIAN_LITTLE && size > 1 && size % 2 == 0 && size <= 8)
    {
        for (size_t i = 0; i < count; i++)
        {
            unsigned char buffer[8]; // Assume max size fits in buffer
            assert(size <= sizeof(buffer));

            const unsigned char *src = (const unsigned char *)ptr + i * size;
            memcpy(buffer, src, size);
            swap_bytes(buffer, size);

            if (fwrite(buffer, 1, size, stream) != size)
                return;
        }
    }
    else
    {
        // Big-endian or unknown - write as-is
        if (fwrite(ptr, size, count, stream) != count)
            return;
    }
}

void fread_endianless(void *ptr, size_t size, size_t count, FILE *stream)
{
    if (system_endianness == ENDIAN_NONE)
        system_endianness = get_system_endianness();

    // Always reading from big-endian format
    if (system_endianness == ENDIAN_LITTLE && size > 1 && size % 2 == 0 && size <= 8)
    {
        for (size_t i = 0; i < count; i++)
        {
            unsigned char buffer[8]; // Assume max size fits in buffer
            assert(size <= sizeof(buffer));

            if (fread(buffer, 1, size, stream) != size)
                return;

            swap_bytes(buffer, size);

            unsigned char *dst = (unsigned char *)ptr + i * size;
            memcpy(dst, buffer, size);
        }
    }
    else
    {
        // Big-endian or unknown - read as-is
        if (fread(ptr, size, count, stream) != count)
            return;
    }
}