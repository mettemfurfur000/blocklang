#include "../include/definitions.h"

#include <stdbool.h>
#include <stdio.h>

block *grid_step_block(grid *g, const u8 x, const u8 y, const u8 side)
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

io_slot *grid_step_edge(grid *g, const u8 x, const u8 y, const u8 side)
{
    if ((side == up && y == 0) ||               //
        (side == down && y == g->height - 1) || //
        (side == left && x == 0) ||             //
        (side == right && x == g->width - 1)    //
    )
        return &g->slots[io_slot_offset(g, side, side == up || side == down ? x : y)];
    return NULL;
}

bool can_read(const io_slot *slot)
{
    return slot->read_only && slot->cur < slot->len;
}

bool can_write(const io_slot *slot)
{
    return !slot->read_only && slot->cur < slot->len;
}

u8 read_byte(io_slot *slot)
{
    assert(slot->cur < slot->len);
    return slot->bytes[slot->cur++];
}

void write_byte(io_slot *slot, const u8 val)
{
    assert(slot->cur < slot->len);
    slot->bytes[slot->cur++] = val;
}

u8 block_get_transfer_side(const instruction i)
{
    switch (i.target)
    {
    case UP:    // up block / slot
    case RIGHT: // right block / slot
    case DOWN:  // down block / slot
    case LEFT:  // left block / slot
    case ANY:   // to/from anyone ready to perform n transfer
        return i.target - UP;
        break;
    default:
        // no block interactions needed
        return invalid;
    }
}

const char *op_code_str(u8 opcode)
{
    switch (opcode)
    {
        CASE(EXT)
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
        CASE(RIGHT)
        CASE(DOWN)
        CASE(LEFT)
        CASE(ANY)
        CASE(NIL)
        CASE(SLN)
        CASE(CUR)
        CASE(REF)
    default:
        return "???";
    }
}

side get_opposite_side(side val)
{
    switch (val)
    {
    case up:
        return down;
    case right:
        return left;
    case down:
        return up;
    case left:
        return right;
    default:
        assert(0 && "Unreachable");
        return 0;
    }
}

target_t to_target(side val)
{
    switch (val)
    {
    case up:
        return UP;
    case right:
        return RIGHT;
    case down:
        return DOWN;
    case left:
        return LEFT;
    case any:
        return ANY;
    case invalid:
        assert(0 && "invalid side");
    }
    return 0;
}

bool is_target_used(instruction i)
{
    switch (i.operation)
    {
    case HALT:
        return false;
    }
    return true;
}

bool is_writing(instruction i)
{
    switch (i.operation) // if operation requires writing to target:
    {
    case PUT:
    case POP:
        return true;
    }
    return false;
}

u8 block_pop_stack(block *b)
{
    if (b->stack_top < 0)
    {
        b->last_caused_overflow = true;
        return 0;
    }
    return b->stack[(u8)b->stack_top--];
}

void block_push_stack(block *b, u8 value)
{
    if (b->stack_top >= 15)
    {
        b->last_caused_overflow = true;
        return;
    }

    b->stack[(u8)b->stack_top++] = value;
}

void block_write_to_any(grid *g, block *b, u8 x, u8 y, u8 value)
{
    io_slot *slot = NULL;
    for (u8 s = up; s <= left; s++)
    {
        slot = grid_step_edge(g, x, y, s);
        if (slot && can_write(slot))
        {
            write_byte(slot, b->transfer_value);
            return;
        }
    }
}

void block_write_to_target(grid *g, block *b, instruction i, u8 value, u8 *advance_to)
{
    switch (i.target)
    {
    case STK:
        if (b->stack_top < 0)
            b->last_caused_overflow = true;
        else
            b->stack[(u8)b->stack_top] = value;
        break;
    case ACC:
        b->accumulator = value;
        break;
    case RG0:
    case RG1:
    case RG2:
    case RG3:
        b->registers[i.target - RG0] = value;
        break;
    case ADJ:
        ((u8 *)(b->bytecode))[b->current_instruction + 1] = value;
        (*advance_to)++;
        break;
    case REF:;
        const u8 addr = value;
        const bool toofar = addr > b->length;
        b->last_caused_overflow = toofar;
        if (!toofar)
            ((u8 *)(b->bytecode))[addr] = b->registers[3];
        break;
    case NIL:
    case SLN:
        break;
    default:
        break;
    }
}

static bool block_write_to_block_direct(grid *g, block *src, u8 x, u8 y, side side, u8 value)
{
    block *dst = grid_step_block(g, x, y, side);
    if (!dst)
        return false;

    if (!dst->bytecode || dst->state_halted)
        return false;

    if (dst->waiting_ticks)
        return false;

    if (dst->current_instruction >= dst->length)
        return false;

    instruction dst_i = dst->bytecode[dst->current_instruction];

    target_t needed_side = to_target(get_opposite_side(side));
    if (dst_i.target != needed_side) // block io until dest block has the side opposite to ours in its target bits
    {
        src->io_blocked = true;
        return false;
    }

    if (is_writing(dst_i)) // if its writing to us, stay blocked. Causes deadlocks!
    {
        src->io_blocked = true;
        return false;
    }

    // is reading from us, fine. Give the value

    src->io_blocked = false;
    dst->io_blocked = false;

    dst->transfer_value = value;
    return true;
}

void block_write_to_side(grid *g, block *b, u8 x, u8 y, side side, u8 value)
{
    if (side == any)
    {
        block_write_to_any(g, b, x, y, value);
        return;
    }

    io_slot *slot = grid_step_edge(g, x, y, side);

    if (slot && can_write(slot))
    {
        write_byte(slot, value);
        return;
    }

    if (block_write_to_block_direct(g, b, x, y, side, value))
        return;

    if (!slot && !b->io_blocked)
        b->last_caused_overflow = true;
}

u8 block_get_instruction_write_operand(block *b, instruction i)
{
    switch (i.operation)
    {
    case PUT:
        return b->accumulator;
    case POP:
        return block_pop_stack(b);
    }
    assert(0);
    return 0;
}

u8 block_get_operand_value(block *b, instruction i, u8 *advance_to)
{
    switch (i.target)
    {
    case STK:
        if (b->stack_top < 0)
            return 0;
        return b->stack[(u8)b->stack_top];
    case ACC:
        return b->accumulator;
    case RG0:
    case RG1:
    case RG2:
    case RG3:
        return b->registers[i.target - RG0];
    case ADJ:
        (*advance_to)++;
        // since EXT reads from the next byte, ADJ is actualy next after ext opcode
        return ((u8 *)(b->bytecode))[b->current_instruction + (i.operation == EXT ? 2 : 1)];
    case REF:;
        const u8 addr = b->accumulator;
        const bool toofar = addr > b->length;
        b->last_caused_overflow = toofar;
        return toofar ? 0 : ((u8 *)(b->bytecode))[addr];
    case NIL:
        return 0;
    case SLN:
        return b->stack_top >= 0 ? b->stack_top + 1 : 0;
    case CUR:
        return b->current_instruction;
    default:
        return 0;
    }
}

void block_execute_operation(block *b, u8 operation, u8 operand_value, u8 *advance_to)
{
    switch (operation)
    {
    case WAIT:
        b->waiting_ticks = operand_value;
        break;
    case ADD:
        b->last_caused_overflow = b->accumulator + operand_value > 255;
        b->accumulator = b->accumulator + operand_value;
        break;
    case SUB:
        b->last_caused_overflow = b->accumulator < operand_value;
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
        *advance_to = operand_value;
        break;
    case JEZ:
        if (b->accumulator == 0)
            *advance_to = operand_value;
        break;
    case JNZ:
        if (b->accumulator != 0)
            *advance_to = operand_value;
        break;
    case JOF:
        if (b->last_caused_overflow)
        {
            *advance_to = operand_value;
            b->last_caused_overflow = false;
        }
        break;
    default:
        break;
    }
}

void block_exec_extended_op(block *b, u8 ext_opcode, u8 target_value, u8 *advance_to)
{
    u8 acc_value = b->accumulator;
    u8 shift;

    switch (ext_opcode)
    {
    case EXT_XOR:
        b->accumulator = acc_value ^ target_value;
        break;
    case EXT_AND:
        b->accumulator = acc_value & target_value;
        break;
    case EXT_OR:
        b->accumulator = acc_value | target_value;
        break;
    case EXT_NOT:
        b->accumulator = ~target_value;
        break;
    case EXT_SHL:
        shift = acc_value & 0x07;
        b->accumulator = target_value << shift;
        break;
    case EXT_SHR:
        shift = acc_value & 0x07;
        b->accumulator = target_value >> shift;
        break;
    case EXT_ROL:
        shift = acc_value & 0x07;
        b->accumulator = (target_value << shift) | (target_value >> (8 - shift));
        break;
    case EXT_ROR:
        shift = acc_value & 0x07;
        b->accumulator = (target_value >> shift) | (target_value << (8 - shift));
        break;
    default:
        break;
    }
}

bool block_try_read_from_neighbor(grid *g, block *b, u8 x, u8 y, side s, u8 *out_value)
{
    block *src = grid_step_block(g, x, y, s);
    if (src && src->state_halted)
    {
        b->io_blocked = false;
        b->last_caused_overflow = true;
    }

    if (!src || !src->bytecode || src->state_halted || src->waiting_ticks || src->current_instruction >= src->length)
        return false;

    instruction src_i = src->bytecode[src->current_instruction];
    target_t needed_side = to_target(get_opposite_side(s));

    if (src_i.target != needed_side || !is_writing(src_i))
    {
        b->io_blocked = true;
        return false;
    }

    b->io_blocked = false;
    src->io_blocked = false;
    *out_value = block_get_instruction_write_operand(src, src_i);
    return true;
}

bool block_try_read_from_slot(grid *g, block *b, u8 x, u8 y, side s, u8 *out_value)
{
    io_slot *slot = grid_step_edge(g, x, y, s);
    if (!slot || !can_read(slot))
    {
        if (!b->io_blocked)
            b->last_caused_overflow = true;
        return false;
    }

    *out_value = read_byte(slot);
    return true;
}

bool block_read_from_io(grid *g, block *b, u8 x, u8 y, side read_side, u8 *out_value)
{
    if (read_side == any)
    {
        for (side s = up; s <= left; s++)
        {
            if (block_try_read_from_neighbor(g, b, x, y, s, out_value))
                return true;
        }
        for (side s = up; s <= left; s++)
        {
            if (block_try_read_from_slot(g, b, x, y, s, out_value))
                return true;
        }
    }
    else
    {
        if (block_try_read_from_neighbor(g, b, x, y, read_side, out_value))
            return true;
        if (block_try_read_from_slot(g, b, x, y, read_side, out_value))
            return true;
    }
    return false;
}

void block_exec_instruction_mono(grid *g, block *b, u8 x, u8 y)
{
    if (!b->bytecode || b->state_halted)
        return;

    g->any_ticked = true;

    if (b->waiting_ticks)
    {
        b->waiting_ticks--;
        return;
    }

    if (b->current_instruction >= b->length)
        b->current_instruction = 0;

    const instruction i = b->bytecode[b->current_instruction];

    // printf("%d:%d : %d\t%s\t%s\t%s\t%s\n", x, y, b->current_instruction, op_code_str(i.operation),
    // target_str(i.target),
    //        b->io_blocked ? "BK" : "", b->last_caused_overflow ? "OF" : "");

    if (i.operation == HALT)
    {
        b->state_halted = true;
        return;
    }

    u8 advance_to = b->current_instruction + 1;
    u8 operand_value = 0;

    u8 transfer_side = block_get_transfer_side(i);
    bool target_needed = is_target_used(i);
    bool io_needed = target_needed && (transfer_side != invalid);
    bool is_writing_op = is_writing(i);

    if (is_writing_op)
    {
        u8 value = block_get_instruction_write_operand(b, i);
        if (io_needed)
            block_write_to_side(g, b, x, y, transfer_side, value);
        else
            block_write_to_target(g, b, i, value, &advance_to);
    }
    else
    {
        if (io_needed)
        {
            if (block_read_from_io(g, b, x, y, transfer_side, &operand_value))
            {
                b->transfer_value = operand_value;
            }
        }
        else if (target_needed)
        {
            operand_value = block_get_operand_value(b, i, &advance_to);
        }

        if (i.operation == EXT) // special case for the extended ops, since none of them "write" at the moment, its in
                                // the "read" branch
        {
            u8 ext_opcode = ((u8 *)b->bytecode)[b->current_instruction + 1];
            advance_to++; // jump over ext opcode byte

            block_exec_extended_op(b, ext_opcode, operand_value, &advance_to); // exec
        }
        else
            block_execute_operation(b, i.operation, operand_value, &advance_to); // exec normally
    }

    if (!b->io_blocked) // advance if not blocked from doing so
        b->current_instruction = advance_to >= b->length ? b->length - 1 : advance_to;
}

void run_grid(grid *g, u32 max_ticks)
{
    while (true)
    {
        g->any_ticked = false;

        for (u8 y = 0; y < g->height; y++)
            for (u8 x = 0; x < g->width; x++)
                block_exec_instruction_mono(g, &g->blocks[y * g->width + x], x, y);

        if (g->any_ticked == false)
            return;

        if (g->ticks++ >= max_ticks)
            return;
    }
}
