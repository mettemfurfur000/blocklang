#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/definitions.h"
#include "../include/parser.h"
#include "../include/codegen.h"
#include "../include/utils.h"
#include "../include/objfile.h"

void test_codegen(const char *source, const char *test_name)
{
    printf("\n=== Test: %s ===\n", test_name);
    printf("Input:\n%s\n", source);
    
    node ast = parse_program(source);
    
    if (ast.child_count == 0)
    {
        printf("Parse failed or empty\n");
        free_nodes();
        return;
    }
    
    const char *output = codegen(ast);
    
    printf("\nGenerated assembly:\n%s\n", output);

    u8 *bytecode = NULL;
    u8 bytecode_len = 0;
    u16 line_table[MAX_BYTECODE_SIZE] = {};

    bool ok = assemble_program(output, (void **)&bytecode , &bytecode_len, line_table);

    // if(!ok)
    // {
    //     // fprintf(stderr, "Assembly failed, all recognized tokens:\n");
    //     // debug_tokenize(output);
    //     return;
    // }
    
    free_nodes();
}

int main(int argc, char *argv[])
{
    test_codegen("x = 1 + 2;", "Simple addition");
    
    test_codegen("x = 5 * 3;", "Simple multiplication");
    
    test_codegen("x = 10 - 4;", "Simple subtraction");
    
    test_codegen("x = 10 / 2;", "Simple division");
    
    test_codegen("x = 10 % 3;", "Simple modulo");
    
    test_codegen("x = *ptr;", "Pointer dereference");
    
    test_codegen("x = y;", "Simple assignment");
    
    test_codegen("x = 1 + 2 + 3;", "Chained addition");
    
    test_codegen("x = 2 * 3 + 4;", "Mixed operators");
    
    return 0;
}
