#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/definitions.h"

char *read_asm_source_file(const char *filename)
{
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

u8 string_to_side(const char *str)
{
    if (strcmp(str, "up") == 0)
        return up;
    if (strcmp(str, "right") == 0)
        return right;
    if (strcmp(str, "down") == 0)
        return down;
    if (strcmp(str, "left") == 0)
        return left;

    return 0xFF; // Invalid side
}

void prepare_slot(grid *g, const char *specification, void **out_data, u8 *out_size)
{
    if (!g || !specification || !out_data || !out_size)
        return;
    // specification format: {in|out}:{up|left|right|down}:slot:[value1,value2,value3]
    char direction[4];
    char side[5];
    int slot;
    char values[256];

    if (sscanf(specification, "%3[^:]:%4[^:]:%d:%255[^]]", direction, side, &slot, values) != 4)
    {
        fprintf(stderr, "Invalid IO specification: %s\n", specification);
        return;
    }

    if (slot < 0)
    {
        fprintf(stderr, "Invalid slot number in IO specification: \"%d\"\n", slot);
        return;
    }

    u8 *data = calloc(1, 1);
    size_t data_size = 0;

    // Parse values
    char *token = strtok(values, ",");
    while (token)
    {
        u8 *new = realloc(data, data_size + 1);
        if (!new)
        {
            fprintf(stderr, "Memory allocation failed while preparing IO slot\n");
            free(data);
            return;
        }
        data = new;

        data[data_size++] = (u8)atoi(token);
        token = strtok(NULL, ",");
    }

    u8 side_num = (u8)string_to_side(side);

    if (side_num == 0xFF)
    {
        fprintf(stderr, "Invalid side in IO specification: \"%s\"\n", side);
        free(data);
        return;
    }

    if (strcmp(direction, "in") == 0)
    {
        attach_input(g, side_num, (u8)slot, data, (u8)data_size);
    }
    else if (strcmp(direction, "out") == 0)
    {
        attach_output(g, side_num, (u8)slot, data, (u8)data_size);
    }
    else
    {
        fprintf(stderr, "Invalid direction in IO specification: \"%s\"\n", direction);
        free(data);
        return;
    }

    *out_data = data;
    *out_size = (u32)data_size;
}

#define MINIMAL_ARGS 6

// Sample program: loads blocklang assembly from a file, assembles it, loads it into a grid for each block and runs it
int main(int argc, char *argv[])
{
    if (argc < MINIMAL_ARGS)
    {
        printf(
            "Usage: %s <input file> <width> <height> <debug> <ticks_limit> <{in|out}:{up|left|right|down}:slot:[value1,value2,value3]> ...\n",
            argv[0]);
        printf("example args: test.bl 2 2 \"in:up:0:1,2,3\" \"out:up:1:4,5,6\"\n");
        return 1;
    }

    const char *input_file = argv[1];
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    bool debug = strcmp(argv[4], "true") == 0;
    int ticks_limit = atoi(argv[5]);

    char *source = read_asm_source_file(input_file);
    if (!source)
    {
        fprintf(stderr, "Failed to read source file: %s\n", input_file);
        return 1;
    }

    void *bytecode = NULL;
    u8 bytecode_len = 0;

    if (!assemble_program(source, &bytecode, &bytecode_len))
    {
        fprintf(stderr, "Assembly failed\n");
        free(source);
        return 1;
    }

    free(source);

    grid g = initialize_grid(width, height);

    g.debug = debug;

    // Prepare IO slots based on command line arguments
    void **data_ptrs = NULL;
    u8 *sizes = NULL;
    if (argc > MINIMAL_ARGS)
    {
        data_ptrs = malloc((argc - MINIMAL_ARGS) * sizeof(void *));
        sizes = malloc((argc - MINIMAL_ARGS) * sizeof(u8));

        if (!data_ptrs || !sizes)
        {
            fprintf(stderr, "Memory allocation failed for IO data pointers\n");
            free(bytecode);
            free_grid(&g);
            if (data_ptrs)
                free(data_ptrs);
            if (sizes)
                free(sizes);
            return 1;
        }

        for (int i = MINIMAL_ARGS; i < argc; i++)
        {
            void *data = NULL;
            u8 size = 0;
            prepare_slot(&g, argv[i], &data, &size);
            // data is owned by the grid now
            data_ptrs[i - MINIMAL_ARGS] = data;
            sizes[i - MINIMAL_ARGS] = size;
            // Note: In a real application, you might want to keep track of these pointers to free them later if needed
        }
    }

    for (u8 y = 0; y < g.height; y++)
        for (u8 x = 0; x < g.width; x++)
        {
            load_program(&g, x, y, bytecode, bytecode_len);
        }

    run_grid(&g, ticks_limit);

    free(bytecode);
    free_grid(&g);

    if (data_ptrs)
    {
        for (int i = 0; i < (argc - MINIMAL_ARGS); i++)
        {
            // Print all the results
            if (sizes[i] > 0)
            {
                printf("Results for \"%s\" \t-> ", argv[i + MINIMAL_ARGS]);
                u8 *data = (u8 *)data_ptrs[i];
                for (u8 j = 0; j < sizes[i]; j++)
                {
                    printf("%d ", data[j]);
                }
                printf("\n");
            }
            free(data_ptrs[i]);
        }
        free(data_ptrs);
        free(sizes);
    }

    return 0;
}