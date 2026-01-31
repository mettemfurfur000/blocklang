#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/definitions.h"
#include "../include/parser.h"
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
    if (argc != 2)
    {
        printf("Invalid usage, just pass the file path\n");
        return -1;
    }
    const char *input_file = argv[1];

    char *source = NULL;

    source = read_to_heap(input_file);
    if (!source)
    {
        fprintf(stderr, "Failed to read source file: %s\n", input_file);
        return 1;
    }

    node root = parse_program(source);

    print_ast(&root, 0);

    return 0;
}