#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/config.h"
#include "../include/utils.h"
#include <cjson/cJSON.h>

static void init_config(vm_config *config)
{
    memset(config, 0, sizeof(vm_config));
    strcpy(config->program_dir, "./");
    config->debug = false;
    config->ticks_limit = 128;
    config->print_strings = true;
}

bool parse_config(const char *filename, vm_config *config)
{
    init_config(config);
    
    long file_len = 0;
    char *json_str = read_to_heap_bin(filename, &file_len);
    if (!json_str)
    {
        fprintf(stderr, "Failed to read config file: %s\n", filename);
        return false;
    }
    
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    
    if (!root)
    {
        fprintf(stderr, "Failed to parse JSON: %s\n", cJSON_GetErrorPtr());
        return false;
    }
    
    cJSON *program_dir = cJSON_GetObjectItem(root, "program_dir");
    if (cJSON_IsString(program_dir))
    {
        strncpy(config->program_dir, program_dir->valuestring, sizeof(config->program_dir) - 1);
    }
    
    cJSON *debug = cJSON_GetObjectItem(root, "debug");
    if (cJSON_IsBool(debug))
    {
        config->debug = cJSON_IsTrue(debug);
    }
    
    cJSON *ticks = cJSON_GetObjectItem(root, "ticks");
    if (cJSON_IsNumber(ticks))
    {
        config->ticks_limit = (u32)ticks->valueint;
    }
    
    cJSON *print_strings = cJSON_GetObjectItem(root, "print_strings");
    if (cJSON_IsBool(print_strings))
    {
        config->print_strings = cJSON_IsTrue(print_strings);
    }
    
    cJSON *programs = cJSON_GetObjectItem(root, "programs");
    if (cJSON_IsObject(programs))
    {
        cJSON *prog = programs->child;
        while (prog && config->program_count < MAX_DEFINITIONS)
        {
            if (cJSON_IsString(prog))
            {
                config->programs[config->program_count].key = prog->string[0];
                strncpy(config->programs[config->program_count].filename, prog->valuestring,
                        sizeof(config->programs[0].filename) - 1);
                config->program_count++;
            }
            prog = prog->next;
        }
    }
    
    cJSON *layout = cJSON_GetObjectItem(root, "layout");
    if (cJSON_IsArray(layout))
    {
        cJSON *row = layout->child;
        while (row && config->layout_height < MAX_LAYOUT_HEIGHT)
        {
            if (cJSON_IsString(row))
            {
                size_t len = strlen(row->valuestring);
                if (len > MAX_LAYOUT_WIDTH)
                    len = MAX_LAYOUT_WIDTH;
                memcpy(config->layout[config->layout_height], row->valuestring, len);
                config->layout[config->layout_height][len] = '\0';
                config->layout_height++;
                if (len > config->layout_width)
                    config->layout_width = (u8)len;
            }
            row = row->next;
        }
    }
    
    cJSON *io = cJSON_GetObjectItem(root, "io");
    if (cJSON_IsArray(io))
    {
        cJSON *item = io->child;
        while (item && config->io_spec_count < 32)
        {
            cJSON *block = cJSON_GetObjectItem(item, "block");
            cJSON *dir = cJSON_GetObjectItem(item, "direction");
            cJSON *side = cJSON_GetObjectItem(item, "side");
            cJSON *slot = cJSON_GetObjectItem(item, "slot");
            cJSON *values = cJSON_GetObjectItem(item, "values");
            
            io_spec *spec = &config->io_specs[config->io_spec_count];
            
            if (cJSON_IsString(block))
            {
                spec->block_key = block->valuestring[0];
            }
            else
            {
                spec->block_key = 0;
            }
            
            if (cJSON_IsString(dir))
            {
                strncpy(spec->direction, dir->valuestring, 3);
                spec->direction[3] = '\0';
            }
            
            if (cJSON_IsString(side))
            {
                strncpy(spec->side, side->valuestring, sizeof(spec->side) - 1);
                spec->side[sizeof(spec->side) - 1] = '\0';
            }
            else
            {
                fprintf(stderr, "IO spec missing 'side' field\n");
                item = item->next;
                continue;
            }
            
            if (cJSON_IsNumber(slot))
                spec->slot = (u8)slot->valueint;
            
            if (cJSON_IsString(values))
            {
                strncpy(spec->values, values->valuestring, sizeof(spec->values) - 1);
                spec->values[sizeof(spec->values) - 1] = '\0';
            }
            
            config->io_spec_count++;
            item = item->next;
        }
    }
    
    cJSON_Delete(root);
    
    if (config->layout_height == 0)
    {
        fprintf(stderr, "No layout defined in config\n");
        return false;
    }
    
    if (config->program_count == 0)
    {
        fprintf(stderr, "No programs defined in config\n");
        return false;
    }
    
    return true;
}

void free_config(vm_config *config)
{
    for (u8 i = 0; i < config->program_count; i++)
    {
        free(config->programs[i].bytecode);
    }
}
