#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/definitions.h"
#include "../include/utils.h"

/*
    A minimal assembler program that accepts block assembly and spits out bytecode \

    -f <filename> for an input source file
    -o <filename> for out bytecode

    No input from stdin for now

    Minimal error checking
*/

int main(int argc, char *argv[])
{
    int c;
    const char *input_file = NULL;
    const char *output_file = NULL;

    while ((c = getopt(argc, argv, ":o:f:")) != -1)
        switch (c)
        {
        case 'f':
            input_file = optarg;
            break;
        case 'o':
            output_file = optarg;
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
            fprintf(stderr, "Usage: -f <filename> -o <output_file>");
            return 1;
        }

    char *source = NULL;

    if (input_file == NULL || output_file == NULL)  
        goto usage;

    if (input_file)
    {
        source = read_to_heap(input_file);
        if (!source)
        {
            fprintf(stderr, "Failed to read source file: %s\n", input_file);
            return 1;
        }
    }

    void *bytecode = NULL;
    u8 bytecode_len = 0;

    if (!assemble_program(source, &bytecode, &bytecode_len))
    {
        fprintf(stderr, "Assembly failed, all recognized tokens:\n");
        debug_tokenize(source);
        free(source);
        return 1;
    }

    FILE *f = fopen(output_file, "wb");
    if (!f)
    {
        fprintf(stderr, "Failed to open the file %s\n", output_file);
        return -1;
    }

    fwrite(bytecode, 1, bytecode_len, f);
    fclose(f);
    fprintf(stdout, "Successfull assembly\n");

    return 0;
}