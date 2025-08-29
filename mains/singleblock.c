#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/definitions.h"
#include "../include/utils.h"

/*
    Single block vm, with stdin attached at the top and stdout at the bottom
*/

#define TICK_LIMIT 4096
#define LOOPS_CONSIDERED_INFINITE 8

int main(int argc, char *argv[])
{
    int c;
    const char *input_file = NULL;

    while ((c = getopt(argc, argv, ":f:")) != -1)
        switch (c)
        {
        case 'f':
            input_file = optarg;
            break;
        case ':':
            fprintf(stderr, "Option needs a value\n");
            break;
        case '?':
            if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        default:
        usage:
            fprintf(stderr, "Usage: -f <bytecode file>");
            return 1;
        }

    if (!input_file)
        goto usage;

    u32 len = 0;
    static_assert(sizeof(u32) == sizeof(long), ""); // just in case your long is not my long, ykwim
    u8 *bytecode = (void *)read_to_heap_bin(input_file, (void *)&len);

    if (len > BYTECODE_LIMIT)
    {
        fprintf(stderr, "Bytecode cannot be longer than %d bytes\n", BYTECODE_LIMIT);
        return 1;
    }

    if (!bytecode)
    {
        fprintf(stderr, "Failed to read file: %s\n", input_file);
        return 1;
    }

    grid *g = initialize_grid(1, 1);

    u8 in_buffer[256] = {};
    u8 out_buffer[256] = {};

    attach_input(g, up, 0, in_buffer, 255);
    attach_output(g, down, 0, out_buffer, 255);

    load_program(g, 0, 0, bytecode, len);

    printf("/quit to exit\n");

    for (;;)
    {
        printf("> ");
        fgets((void *)in_buffer, 255, stdin);

        if (strncmp((void *)in_buffer, "/quit", 5) == 0)
            break;

        for (u32 i = 0; i < LOOPS_CONSIDERED_INFINITE; i++)
        {
            run_grid(g, TICK_LIMIT);

            if (g->ticks >= TICK_LIMIT)
                printf("Ticking for %d ticks, loop %d...\n", TICK_LIMIT, i);
            else
                break;
        }

        if (g->ticks >= TICK_LIMIT)
        {
            printf("Grid seems to be stuck, aborting\n");
            printf("Current output: %.255s\n", out_buffer);
            abort();
        }

        printf("%.255s\n(took %d ticks)\n", out_buffer, g->ticks);

        if (g->any_ticked == false)
        {
            printf("Execution completed\n");
            break;
        }

        g->ticks = 0;
    }

    free_grid(g);

    return 0;
}