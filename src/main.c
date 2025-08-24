#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "../include/definitions.h"

int main()
{
    grid g = initialize_grid(1, 2);

    #define LEN 4

    u8 test[LEN] = {1, 2, 3, 4};
    u8 out[LEN] = {};

    attach_input(&g, up, 0, test, sizeof(test));
    attach_output(&g, down, 0, out, sizeof(out));

    instruction code_sample[9] = {};

    u8 cur = 0;

    code_sample[cur++] = (instruction){.operation = GET, .target = UP};   // 0
    code_sample[cur++] = (instruction){.operation = JOF, .target = ADJ};  // 1
    ((u8 *)code_sample)[cur++] = 7;                                       // 2
    code_sample[cur++] = (instruction){.operation = ADD, .target = ADJ};  // 3
    ((u8 *)code_sample)[cur++] = 1;                                      // 4
    code_sample[cur++] = (instruction){.operation = PUT, .target = DWN};  // 5
    code_sample[cur++] = (instruction){.operation = JMP, .target = NIL};  // 6
    code_sample[cur++] = (instruction){.operation = HALT, .target = NIL}; // 7

    load_program(&g, 0, 0, code_sample, sizeof(code_sample));
    load_program(&g, 0, 1, code_sample, sizeof(code_sample));

    run_grid(&g, 32);

    free_grid(&g);

    for (u8 i = 0; i < sizeof(test); i++)
        printf("%d -> %d\n", test[i], out[i]);

    return 0;
}