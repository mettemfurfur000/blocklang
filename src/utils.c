#include <stdio.h>
#include <stdlib.h>

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