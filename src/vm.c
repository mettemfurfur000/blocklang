#include "../include/definitions.h"
#include <stdbool.h>
#include <stdio.h>

block *grid_step_block(const grid *g, const u8 x, const u8 y, const u8 side)
{
    u8 offset_x = x;
    u8 offset_y = y;

    offset_x += side == right && x != g->width - 1 ? 1 : 0;
    offset_x += side == left && x != 0 ? -1 : 0;
    offset_y += side == down && y != g->height - 1 ? 1 : 0;
    offset_y += side == up && y != 0 ? -1 : 0;

    if (offset_x == x && offset_y == y)
        return NULL;

    return &g->blocks[offset_y * g->width + offset_x];
}

io_slot *grid_step_edge(const grid *g, const u8 x, const u8 y, const u8 side)
{
    if ((side == up && y == 0) ||               //
        (side == down && y == g->height - 1) || //
        (side == left && x == 0) ||             //
        (side == right && x == g->width - 1)    //
    )
        return &g->slots[io_slot_offset(g, side, side == up || side == down ? x : y)];
    return NULL;
}

void block_io_unlock(block *b)
{
    assert(b != NULL);

    b->transfer_side = invalid;
    b->waiting_for_io = false;
    b->waiting_transfer = false;
    b->transfered = true;
    b->last_caused_overflow = false;
}

void block_io_unlock_error(block *b)
{
    assert(b != NULL);

    b->transfer_side = invalid;
    b->waiting_for_io = false;
    b->waiting_transfer = false;
    b->transfered = false;
    b->last_caused_overflow = true;
}

bool can_read(const io_slot *slot)
{
    return slot->ptr && slot->read_only && slot->cur < slot->len;
}

bool can_write(const io_slot *slot)
{
    return slot->ptr && !slot->read_only && slot->cur < slot->len;
}

u8 read_byte(io_slot *slot)
{
    assert(slot->cur < slot->len);
    return slot->ptr[slot->cur++];
}

void write_byte(io_slot *slot, const u8 val)
{
    assert(slot->cur < slot->len);
    slot->ptr[slot->cur++] = val;
}

void block_iter_pre_move(grid *g, block *b, const u8 x, const u8 y)
{
    if (!b->bytecode || b->state_halted || b->waiting_for_io)
        return;

    g->any_ticked = true;

    if (b->waiting_ticks)
        return;

    if (b->current_instruction >= b->length) // wrap around to not read garbage
        b->current_instruction = 0;

    const instruction i = b->bytecode[b->current_instruction];

    if (i.operation == HALT)
    {
        b->state_halted = true;
        return;
    }

    b->transfered = false;

    // prepare block interactions, if target is a block

    switch (i.target)
    {
    case UP:  // up block / slot
    case RIG: // right block / slot
    case DWN: // down block / slot
    case LFT: // left block / slot
    case ANY: // to/from anyone ready to perform n transfer
        b->transfer_side = i.target - UP;
        break;
    default:
        // no block interactions needed
        b->transfer_side = invalid;
    }

    if (b->transfer_side != invalid)
    {
        b->waiting_for_io = true;

        switch (i.operation)
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
            b->waiting_transfer = false; // will read values from target
            return;
        }

        b->waiting_transfer = true;
    }

    switch (i.operation) // if operation requires writing to target:
    {
    case PUT:
        b->transfered = !b->waiting_for_io;
        b->transfer_value = b->accumulator;
        break;
    case POP:
        b->transfered = !b->waiting_for_io;
        b->transfer_value = b->stack_top < 0 ? 0 : b->stack[(u8)b->stack_top--];
        break;
    }
}

// write values to target
void block_iter_write_to(const grid *g, block *b, const u8 x, const u8 y)
{
    if (!b->bytecode || b->state_halted)
        return;
    if (!b->waiting_for_io)
        return;
    if (!b->waiting_transfer)
        return;
    if (b->waiting_ticks)
        return;

    const side side = b->transfer_side;
    io_slot *slot = NULL;

    if (side == invalid)
        return;

    if (side == 4)
    {
        for (int s = up; s <= left; s++)
        {
            slot = grid_step_edge(g, x, y, s);
            if (slot && can_write(slot))
            {
                write_byte(slot, b->transfer_value);
                block_io_unlock(b);
                return;
            }
        }

        // no edges detected, someone will get that byte surely
        return;
    }

    slot = grid_step_edge(g, x, y, side); // checks for attempts to push to the edge

    // transfer is to another block, they will read the value later
    if (!slot)
        return;

    if (can_write(slot))
    {
        write_byte(slot, b->transfer_value);
        block_io_unlock(b);
        return;
    }

    block_io_unlock_error(b);
}

// read values from neighboink blocks
void block_iter_read_from(const grid *g, block *b, u8 x, u8 y)
{
    if (!b->bytecode || b->state_halted)
        return;
    if (!b->waiting_for_io) // not waiting for io
        return;
    if (b->waiting_transfer) // block is actually supposed to write data - not reading anything
        return;
    if (b->waiting_ticks)
        return;

    side trans_side = b->transfer_side;
    io_slot *slot = NULL;

    if (trans_side == invalid)
        return;

    for (u8 look_counter = 0;;look_counter++)
    {
        if (trans_side != 4) // for specific sides:
        {
            slot = grid_step_edge(g, x, y, trans_side); // check for edge

            if (!slot) // has to be a block
            {
                block *src = grid_step_block(g, x, y, trans_side);
                assert(src != NULL);

                if (src->state_halted) // if block cannot respond we go offline
                {
                    block_io_unlock_error(b);
                    return;
                }

                if (src->waiting_for_io && src->waiting_transfer) // block is waiting for io and waiting to push data
                {
                    b->transfer_value = src->transfer_value; // accept his value
                    block_io_unlock(b);
                    block_io_unlock(src);
                    return;
                }

                return; // wait for more mayb he will send data soon
            }

            if (can_read(slot))
            {
                b->transfer_value = read_byte(slot);
                block_io_unlock(b);
                return;
            }

            block_io_unlock_error(b); // cannot read - ran out of bytes it seems
            return;
        } // side is ANY

        if (look_counter > 4) // didn find anything! must be a 1x1 world with no outputs
        {
            assert(0 && "Grid 1x1 with no outputs");
            return;
        }

        // look for anything that can get us bytes, start with up and loop to left
        trans_side = trans_side == 4 ? up : trans_side + 1;
    }
}

void block_iter_exec_op(const grid *g, block *b, u8 x, u8 y)
{
    if (!b->bytecode || b->state_halted)
        return;
    if (b->waiting_ticks)
    {
        b->waiting_ticks--;
        return;
    }

    if (b->waiting_for_io) // still waiting for IO, cant do the thing
        return;

    u8 operand_value = 0;
    u8 advance_to = b->current_instruction + 1;

    if (b->transfered)
        operand_value = b->transfer_value;

    const instruction i = b->bytecode[b->current_instruction];

    // handle write to targets that doesn require inter-block transfers
    if (i.operation == PUT || i.operation == POP)
    { // these will read from ACC and write into targets that are not blocks
        switch (i.target)
        {
        case STK:
            if (b->stack_top < 0)
                b->last_caused_overflow = true;
            else
                b->stack[(u8)b->stack_top] = operand_value;
            break;
        case ACC:
            b->accumulator = operand_value; // from acc to acc?
            break;
        case RG0:
        case RG1:
        case RG2:
        case RG3:
            b->registers[i.target - RG0] = operand_value;
            break;
        case ADJ:
            // should be illegal
            // ((u8 *)(b->bytecode))[b->current_instruction + 1] = operand_value;
            // advance_to++;
            break;
        // case REF:
        //     // illegal to write to bytecode
        //     break;
        case NIL:
            // nothing
            break;
        case SLN:
            // nothing
            break;
        }
    }
    else
    {
        // check for local targets to read values from them

        switch (i.target)
        {
        case STK:
            if (b->stack_top < 0)
                operand_value = 0;
            else
                operand_value = b->stack[(u8)b->stack_top];
            break;
        case ACC:
            operand_value = b->accumulator;
            break;
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
        case REF:;
            u8 ptr = b->accumulator;

            const bool toofar = ptr > b->length;

            b->last_caused_overflow = toofar ? true : false;
            operand_value = toofar ? 0 : ((u8 *)(b->bytecode))[ptr];
            break;
        case NIL:
            operand_value = 0;
            break;
        case SLN:
            operand_value = b->stack_top >= 0 ? b->stack_top + 1 : 0;
            break;
        case CUR:
            operand_value = b->current_instruction;
            break;
        }

        switch (i.operation) // these only write to acc
        {
        case WAIT:
            b->waiting_ticks = operand_value;
            break;
        case ADD:
            b->last_caused_overflow = b->accumulator + operand_value > 255;
            b->accumulator = b->accumulator + operand_value;
            break;
        case SUB:
            b->last_caused_overflow = b->accumulator - operand_value < 0;
            b->accumulator = b->accumulator - operand_value;
            break;
        case MLT:
            b->last_caused_overflow = b->accumulator * operand_value > 255;
            b->accumulator = b->accumulator * operand_value;
            break;
        case DIV:
            b->last_caused_overflow = operand_value == 0;
            if (operand_value != 0)
                b->accumulator = b->accumulator / operand_value;
            break;
        case MOD:
            b->last_caused_overflow = operand_value == 0;
            if (operand_value != 0)
                b->accumulator = b->accumulator % operand_value;
            break;
        case GET:
            b->accumulator = operand_value;
            break;
        case PUSH:
            if (b->stack_top >= 15)
            {
                b->last_caused_overflow = true;
                break;
            }
            b->stack_top++;
            b->stack[(u8)b->stack_top] = operand_value;
            break;
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
    }

    b->current_instruction = advance_to >= b->length ? b->length - 1 : advance_to;
}

const char *op_code_str(u8 opcode)
{
    switch (opcode)
    {
        CASE(NOP)
        CASE(WAIT)
        CASE(ADD)
        CASE(SUB)
        CASE(MLT)
        CASE(DIV)
        CASE(MOD)
        CASE(GET)
        CASE(PUT)
        CASE(PUSH)
        CASE(POP)
        CASE(JMP)
        CASE(JEZ)
        CASE(JNZ)
        CASE(JOF)
        CASE(HALT)
    default:
        return "???";
    }
}

const char *target_str(u8 target)
{
    switch (target)
    {
        CASE(STK)
        CASE(ACC)
        CASE(RG0)
        CASE(RG1)
        CASE(RG2)
        CASE(RG3)
        CASE(ADJ)
        CASE(UP)
        CASE(RIG)
        CASE(DWN)
        CASE(LFT)
        CASE(ANY)
        CASE(NIL)
        CASE(SLN)
        CASE(CUR)
        CASE(REF)
    default:
        return "???";
    }
}

void print_block_state(u8 x, u8 y, block *b)
{
    if (!b->bytecode)
    {
        printf("%2d :%2d - [ empty_block ] \n", x, y);
        return;
    }

    instruction i = b->bytecode ? b->bytecode[b->current_instruction] : (instruction){};

    printf("x:%2d y:%2d ln:%3d [%4s %4s] ", x, y, b->current_instruction, op_code_str(i.operation),
           target_str(i.target));
    printf("a:%3d r0:%3d r1:%3d r2:%3d r3:%3d ", b->accumulator, b->registers[0], b->registers[1], b->registers[2],
           b->registers[3]);
    printf("wait:%3d\n", b->waiting_ticks);
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
    if (g->debug)
        for (u8 y = 0; y < g->height; y++)
            for (u8 x = 0; x < g->width; x++)
                print_block_state(x, y, &g->blocks[y * g->width + x]);

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
    if (g->debug)
    {
        printf("raw bytecode:\n");
        for (u8 y = 0; y < g->height; y++)
            for (u8 x = 0; x < g->width; x++)
            {
                printf("  block %d-%d:\n", x, y);
                block *b = &g->blocks[y * g->width + x];
                if (!b->bytecode)
                    printf("(no code)");
                else
                    for (u32 i = 0; i < b->length; i++)
                        printf("%d : %.2X\n", i, ((u8 *)b->bytecode)[i]);
            };
    }

    while (true)
    {
        if (g->debug)
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
