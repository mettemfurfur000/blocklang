#include "../include/definitions.h"
#include <stdbool.h>
#include <stdio.h>

block *grid_step_block(grid *g, u8 x, u8 y, u8 side)
{
    x += side == right && x != g->width - 1 ? 1 : 0;
    x += side == left && x != 0 ? -1 : 0;
    y += side == down && y != g->height - 1 ? 1 : 0;
    y += side == up && y != 0 ? -1 : 0;

    return &g->blocks[y * g->width + x];
}

void block_iter_pre_move(grid *g, block *b, u8 x, u8 y)
{
    if (!b->bytecode || b->state_halted)
        return;

    if (b->current_instruction >= b->length)
        b->current_instruction = 0;

    instruction i = b->bytecode[b->current_instruction];

    if (i.operation == HALT)
    {
        b->state_halted = true;
        return;
    }

    g->any_ticked = true;

    b->transfered = false;

    // prepare block interactions

    switch (i.target)
    {
    case UP:  // up block / slot
    case RIG: // right block / slot
    case DWN: // down block / slot
    case LFT: // left block / slot
        b->transfer_side = i.target - UP;
        break;
    // case ANY: // to anyone ready to perform an transfer
    //     b->transfer_side = 4;
    //     break;
    default:
        return; // no block interactions needed
    }

    switch (i.operation) // if operation requires reading from target:
    {
    case WAIT:
    case ADD:
    case SUB:
    case MLT:
    case DIV:
    case MOD:
    case GET:
    case PUSH:
    case JMP:
    case JEZ:
    case JNZ:
        b->waiting_transfer = false; // will accept bytes
        b->waiting_for_io = true;
        return;
    }

    switch (i.operation) // if operation requires writing to target:
    {
    case PUT:
        b->transfer_value = b->accumulator;
        b->waiting_transfer = true;
        b->waiting_for_io = true;
        return;
    case POP:
        b->transfer_value = b->stack[b->stack_top];
        b->stack_top--;
        b->waiting_transfer = true;
        b->waiting_for_io = true;
        return;
    }
}

void block_iter_write_to(grid *g, block *b, u8 x, u8 y)
{
    if (!b->bytecode || b->state_halted)
        return;
    if (!b->waiting_for_io)
        return;
    if (!b->waiting_transfer)
        return;

    side side = b->transfer_side;
    bool to_edge = false;
    u16 io_index = 0;

    // checks for attempts to push to the edge
    if ((side == up && y == 0) || (side == down && y == g->height - 1))
    {
        to_edge = true;
        io_index = io_slot_offset(g, side, x);
    }

    if ((side == left && x == 0) || (side == right && x == g->width - 1))
    {
        to_edge = true;
        io_index = io_slot_offset(g, side, y);
    }

    if (to_edge)
    {
        io_slot *s = &g->slots[io_index];

        if (s->cur == s->len || !s->ptr || s->read_only) // cannot write
        {
            b->waiting_for_io = false;
            b->transfered = false;
            b->last_caused_overflow = true;
            return;
        }

        s->ptr[s->cur] = b->transfer_value;

        s->cur++;

        b->waiting_for_io = false;
        b->transfered = true;
        b->last_caused_overflow = false;
        return;
    }

    // transfer is to another block, they will read the value later
}

void block_iter_read_from(grid *g, block *b, u8 x, u8 y)
{
    if (!b->bytecode || b->state_halted)
        return;
    if (!b->waiting_for_io)
        return;
    if (b->waiting_transfer)
        return;

    side side = b->transfer_side;
    bool from_edge = false;
    u16 io_index = 0;

    // checks for attempts to read from edges
    if ((side == up && y == 0) || (side == down && y == g->height - 1))
    {
        from_edge = true;
        io_index = io_slot_offset(g, side, x);
    }

    if ((side == left && x == 0) || (side == right && x == g->width - 1))
    {
        from_edge = true;
        io_index = io_slot_offset(g, side, y);
    }

    if (from_edge)
    {
        io_slot *s = &g->slots[io_index];

        // inputs ar being read from 0 to len -1

        if (s->cur == s->len - 1 || !s->ptr) // cannot read
        {
            b->waiting_for_io = false;      // will no longer be blocked
            b->transfered = false;          // wasnt transfered
            b->last_caused_overflow = true; // error-ish flag
            return;
        }

        b->transfer_value = s->ptr[s->cur];
        s->cur++;

        b->waiting_for_io = false;       // no moreweait
        b->transfered = true;            // readfrom transfer value
        b->last_caused_overflow = false; // no error
        return;
    }

    // from other blocks

    block *src = grid_step_block(g, x, y, side);

    if (src->state_halted) // if block cannot respond we go offline
    {
        b->waiting_for_io = false;      // will no longer be blocked
        b->transfered = false;          // wasnt transfered
        b->last_caused_overflow = true; // error-ish flag
        return;
    }

    if (src->waiting_for_io && src->waiting_transfer)
    {
        b->transfer_value = src->transfer_value; // accept his value
        b->waiting_for_io = false;
        b->transfered = true;
        b->last_caused_overflow = false;

        src->waiting_for_io = false;
        src->transfered = true;
        src->last_caused_overflow = false;
        return;
    }
}

void block_iter_exec_op(grid *g, block *b, u8 x, u8 y)
{
    if (!b->bytecode || b->state_halted)
        return;
    instruction i = b->bytecode[b->current_instruction];

    if (b->waiting_for_io) // still waiting for IO, cant do the thing
        return;

    u8 operand_value = 0;
    u8 advance_to = b->current_instruction + 1;

    if (b->transfered)
        operand_value = b->transfer_value;

    switch (i.target)
    {
    case STK:
        operand_value = b->stack[b->stack_top];
        break;
    case ACC:
        operand_value = b->accumulator;
    case RG0:
    case RG1:
    case RG2:
    case RG3:
        operand_value = b->registers[i.target - RG0];
        break;
    case ADJ:
        operand_value = ((u8 *)(b->bytecode))[b->current_instruction + 1];
        advance_to++;
        break;
    case NIL:
        operand_value = 0;
        break;
    }

    switch (i.operation)
    {
    case WAIT:
        b->waiting_ticks = operand_value;
        break;
    case ADD:
        b->last_caused_overflow = b->accumulator + operand_value > 255;
        b->accumulator += operand_value;
        break;
    case SUB:
        b->last_caused_overflow = b->accumulator - operand_value < 0;
        b->accumulator -= operand_value;
        break;
    case MLT:
        b->last_caused_overflow = b->accumulator * operand_value > 255;
        b->accumulator -= operand_value;
        break;
    case DIV:
        b->last_caused_overflow = operand_value == 0;
        if (operand_value != 0)
            b->accumulator /= operand_value;
        break;
    case MOD:
        b->last_caused_overflow = operand_value == 0;
        if (operand_value != 0)
            b->accumulator %= operand_value;
        break;
    case GET:
        b->accumulator = operand_value;
        break;
        // handled in pre-transfer
        // case PUT:
        // break;
    case PUSH:
        b->stack_top++;
        b->stack[b->stack_top] = operand_value;
        break;
        // handled in pre-transfer
        // case POP:
        // break;
    case JMP:
        advance_to = operand_value;
        break;
    case JEZ:
        if (b->accumulator == 0)
            advance_to = operand_value;
        break;
    case JNZ:
        if (b->accumulator != 0)
            advance_to = operand_value;
        break;
    case JOF:
        if (b->last_caused_overflow)
            advance_to = operand_value;
        break;
    }

    b->current_instruction = advance_to >= b->length ? 0 : advance_to;
}

void print_block_state(block *b)
{
    if (!b->bytecode)
    {
        printf("\t[ \t---\t ]\n");
        return;
    }

    instruction i = b->bytecode ? b->bytecode[b->current_instruction] : (instruction){};
    printf("\t[%d :\t %x-%x ] { %d }\n", b->current_instruction, i.operation, i.target, b->accumulator);
}

/*
block iteration process:
1) pre-transfer - check if current operation requires input or output to other blocks
2) transfer stage - move bytes to io_slots
3) accept stage - accept inputs from io_slots or blocks that have sent some values
4) operation execution - do arithmetic, set overflow flag, jump if needed
5) advance instruction counter, if transfer was completed
*/

void grid_iterate(grid *g)
{
    for (u8 y = 0; y < g->height; y++)
        for (u8 x = 0; x < g->width; x++)
            print_block_state(&g->blocks[y * g->width + x]);

    for (u8 y = 0; y < g->height; y++)
        for (u8 x = 0; x < g->width; x++)
            block_iter_pre_move(g, &g->blocks[y * g->width + x], x, y);
    for (u8 y = 0; y < g->height; y++)
        for (u8 x = 0; x < g->width; x++)
            block_iter_write_to(g, &g->blocks[y * g->width + x], x, y);
    for (u8 y = 0; y < g->height; y++)
        for (u8 x = 0; x < g->width; x++)
            block_iter_read_from(g, &g->blocks[y * g->width + x], x, y);
    for (u8 y = 0; y < g->height; y++)
        for (u8 x = 0; x < g->width; x++)
            block_iter_exec_op(g, &g->blocks[y * g->width + x], x, y);
}

void run_grid(grid *g, u32 max_ticks)
{
    while (true)
    {
        printf("tick %d\n", g->ticks);
        g->any_ticked = false;
        grid_iterate(g);

        if (g->any_ticked == false)
            return;

        g->ticks++;
        if (g->ticks >= max_ticks)
            return;
    }
}
