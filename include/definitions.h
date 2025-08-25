#ifndef BLOCKLANG_DEF_H
#define BLOCKLANG_DEF_H 1

#include <assert.h>
#include <stdbool.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef char i8;
typedef short i16;
typedef int i32;
typedef long long i64;

typedef enum
{
    up,
    right,
    down,
    left
} side;

/*
    operations in block assembly are coupled as op code and its target: 4 bits for each, 1 byte in total

    all values in block assemply are unsigned bytes - u8
*/

// possible targets of an operation:
typedef enum
{
    // current stack up value, if nothing was pushed to it, equals to zero
    STK,
    // block accumulator
    ACC,
    // general registers
    RG0,
    RG1,
    RG2,
    RG3,
    // represents next byte in a program, read only, and program counter will skip next byte automaticaly
    ADJ,
    // values from/to neighboring blocks / slots
    UP,  // up block / slot
    RIG, // right block / slot
    DWN, // down block / slot
    LFT, // left block / slot
    ANY, // to anyone ready to perform an transfer
    // other
    NIL,    // always zero, if chosen as destination, consumes input
    SLN, // represents the number of elements on the stack
    LAST
} targets;

static_assert(LAST <= 15, "");

// only 1 target is supported at the moment

// operations that can be done with these targets:
typedef enum
{
    NOP,  // does nothing, but spends a tick
    WAIT, // from target - wait for ticks
    // arithmetic
    ADD, // from target - add to ACC
    SUB, // from target - sub from ACC
    MLT, // from target - mult ACC by target
    DIV, // from target - divide ACC by target
    MOD, // from target - get remainder of a division
    // memory
    GET,  // from target to ACC
    PUT,  // from ACC to target
    PUSH, // from target push to stack
    POP,  // from stack pop to target
    // control
    JMP, // from target - jump to said position in a program
    JEZ, // from target - jump if ACC equals zero
    JNZ, // from target - jump if ACC not zero
    JOF, // jump if last arithmetic operation caused an overflow or underflow

    HALT, // stops execution
} operations;

static_assert(HALT <= 15, "");

typedef struct
{
    u8 operation : 4;
    u8 target : 4;
} instruction;

static_assert(sizeof(instruction) == 1, "");

typedef struct
{
    const instruction *bytecode; // reference to a program in bytecode somewhere in teh code
    u8 length;                   // at this instruction or further program will instantly wrap back to 0
    u8 current_instruction;      // where are we?

    u8 registers[4]; // 4 registers
    u8 accumulator;  // ACC is stored here
    u8 stack[16];
    i8 stack_top;

    u8 waiting_ticks; // if not zero, will substract 1 and do nothing

    u8 transfer_value;     // temporary place for transfered values
    u8 transfer_side;      // 0-3 for valid sides, 4 for any side
    bool waiting_transfer; // false if accepts values
    bool waiting_for_io;   // will be set to true if transfer is needed
    bool transfered;
    bool state_halted;

    u8 last_caused_overflow; // for arithmetic overflows/underflows
} block;

typedef struct
{
    u8 *ptr;
    u8 len;
    u8 cur;
    bool read_only; // if set to true, can be only readed from - no pushing
} io_slot;

typedef struct
{
    block *blocks;
    io_slot *slots;
    u8 width, height, perimeter, total_blocks;
    bool any_ticked;
    bool debug;
    u32 ticks;
} grid;

grid initialize_grid(u8 w, u8 h);

u16 io_slot_offset(grid *g, u8 side, u8 slot);

void attach_input(grid *g, u8 side, u8 slot, const u8 *src, u8 len);
void attach_output(grid *g, u8 side, u8 slot, u8 *dst, u8 len);
void load_program(grid *g, u8 x, u8 y, const void *bytecode, u8 length);

void run_grid(grid *g, u32 max_ticks);
void free_grid(grid *g);

// Assembler

bool assemble_program(const char *source, void **dest, u8 *out_len);

#endif