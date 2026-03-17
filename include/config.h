#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "../include/definitions.h"

#define MAX_DEFINITIONS 16
#define MAX_LAYOUT_WIDTH 32
#define MAX_LAYOUT_HEIGHT 32

typedef struct
{
    char key;
    char filename[64];
    void *bytecode;
    u8 bytecode_len;
} program_def;

typedef struct
{
    char block_key;
    char direction[4];
    char side[6];
    u8 slot;
    char values[256];
} io_spec;

typedef struct
{
    char program_dir[128];
    
    program_def programs[MAX_DEFINITIONS];
    u8 program_count;
    
    char layout[MAX_LAYOUT_HEIGHT][MAX_LAYOUT_WIDTH];
    u8 layout_width;
    u8 layout_height;
    
    io_spec io_specs[32];
    u8 io_spec_count;
    
    bool debug;
    u32 ticks_limit;
    bool print_strings;
} vm_config;

bool parse_config(const char *filename, vm_config *config);
void free_config(vm_config *config);

#endif
