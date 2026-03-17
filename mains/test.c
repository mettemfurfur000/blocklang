#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/config.h"
#include "../include/definitions.h"
#include "../include/objfile.h"
#include "../include/utils.h"

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

    return 0xFF;
}

static void attach_io_from_spec(grid *g, const io_spec *spec)
{
    u8 side_num = (u8)string_to_side(spec->side);
    if (side_num == 0xFF)
    {
        fprintf(stderr, "Invalid side in IO spec: \"%s\"\n", spec->side);
        return;
    }

    u8 *data = NULL;
    size_t data_size = 0;

    if (strcmp(spec->direction, "in") == 0)
    {
        data = attach_input(g, side_num, spec->slot);
    }
    else if (strcmp(spec->direction, "out") == 0)
    {
        data = attach_output(g, side_num, spec->slot);
    }
    else
    {
        fprintf(stderr, "Invalid direction in IO spec: \"%s\"\n", spec->direction);
        exit(1);
    }

    if (spec->values[0] != '=')
    {
        char values_copy[256];
        strncpy(values_copy, spec->values, sizeof(values_copy) - 1);

        char *token = strtok(values_copy, ",");
        while (token)
        {
            data[data_size++] = (u8)atoi(token);
            token = strtok(NULL, ",");
        }
    }
    else
        data_size = 255;

    slot_set_length(g, side_num, spec->slot, data_size);
}

static bool run_with_config(vm_config *config)
{
    grid *g = initialize_grid(config->layout_width, config->layout_height);
    if (!g)
    {
        fprintf(stderr, "Failed to initialize grid\n");
        return false;
    }

    g->debug = config->debug;

    for (u8 i = 0; i < config->program_count; i++)
    {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s%s", config->program_dir, config->programs[i].filename);

        char *source = read_to_heap(filepath);
        if (!source)
        {
            fprintf(stderr, "Failed to read source file: %s\n", filepath);
            free_grid(g);
            return false;
        }

        void *bytecode = NULL;
        u8 bytecode_len = 0;
        u16 line_table[MAX_BYTECODE_SIZE] = {};

        if (!assemble_program(source, &bytecode, &bytecode_len, line_table))
        {
            fprintf(stderr, "Assembly failed for %s\n", filepath);
            free(source);
            free_grid(g);
            return false;
        }

        free(source);
        config->programs[i].bytecode = bytecode;
        config->programs[i].bytecode_len = bytecode_len;
    }

    for (u8 y = 0; y < config->layout_height; y++)
    {
        for (u8 x = 0; x < config->layout_width; x++)
        {
            char block_char = config->layout[y][x];

            for (u8 i = 0; i < config->program_count; i++)
            {
                if (config->programs[i].key == block_char)
                {
                    load_program(g, x, y, config->programs[i].bytecode, config->programs[i].bytecode_len);
                    break;
                }
            }
        }
    }

    for (u8 i = 0; i < config->io_spec_count; i++)
    {
        io_spec *spec = &config->io_specs[i];
        if (spec->block_key == 0)
        {
            attach_io_from_spec(g, spec);
        }
    }

    for (u8 y = 0; y < config->layout_height; y++)
    {
        for (u8 x = 0; x < config->layout_width; x++)
        {
            char block_char = config->layout[y][x];
            for (u8 i = 0; i < config->io_spec_count; i++)
            {
                io_spec *spec = &config->io_specs[i];
                if (spec->block_key != 0 && spec->block_key == block_char)
                {
                    attach_io_from_spec(g, spec);
                }
            }
        }
    }

    run_grid(g, config->ticks_limit);

    if(g->ticks >= config->ticks_limit)
    {
        printf("Ran out of ticks\n");
    }

    for (u8 i = 0; i < config->program_count; i++)
    {
        free(config->programs[i].bytecode);
    }

    for (u8 i = 0; i < config->io_spec_count; i++)
    {
        io_spec *spec = &config->io_specs[i];
        if (spec->direction[0] != 'o')
            continue;
        
        u8 side_num = string_to_side(spec->side);
        u8 offset = io_slot_offset(g, side_num, spec->slot);
        u8 *slot_ptr = g->slots[offset].bytes;
        
        printf("Output from %s side slot %d: ", spec->side, spec->slot);
        
        if (!config->print_strings)
        {
            for (u8 j = 0; j < g->slots[offset].len; j++)
            {
                if (slot_ptr[j] != 0)
                    printf("%d ", slot_ptr[j]);
            }
        }
        else
        {
            for (u8 j = 0; j < g->slots[offset].len; j++)
            {
                if (slot_ptr[j] != 0)
                    printf("%c", slot_ptr[j]);
            }
        }
        printf("\n");
    }

    free_grid(g);
    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage:\n");
        printf("  Config mode: %s <config.cfg>\n", argv[0]);
        printf("\nConfig file format:\n");
        printf("  # Comment\n");
        printf("  program_dir: programs/\n");
        printf("  A: adder.bl\n");
        printf("  B: multiplier.bl\n");
        printf("  layout:\n");
        printf("  AB\n");
        printf("  BA\n");
        printf("  io_in:up:0:1,2,3\n");
        printf("  io_out:down:0:=\n");
        printf("  A:out:right:0:=\n");
        printf("  debug:true\n");
        printf("  ticks:128\n");
        printf("  print_strings:true\n");
        return 1;
    }

    if (argc == 2)
    {
        vm_config config;
        if (!parse_config(argv[1], &config))
        {
            fprintf(stderr, "Failed to parse config file\n");
            return 1;
        }

        bool result = run_with_config(&config);
        free_config(&config);
        return result ? 0 : 1;
    }
}
