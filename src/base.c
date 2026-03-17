#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../include/definitions.h"

grid *initialize_grid(u8 w, u8 h)
{
    grid *g = calloc(1, sizeof(grid));

    assert(g != 0);
    assert(w != 0);
    assert(h != 0);

    u16 total_blocks = w * h;
    u16 edge_length = (w + h) * 2;

    assert(total_blocks <= 256);
    assert(edge_length <= 256);

    g->width = w;
    g->height = h;
    g->total_blocks = total_blocks;
    g->perimeter = edge_length;

    return g;
}

void free_grid(grid *g)
{
    free(g);
}

u16 io_slot_offset(const grid *g, const u8 side, const u8 slot)
{
    assert(side < 4);

    if (side == up || side == down)
        assert(slot < g->width);
    else
        assert(slot < g->height);

    const u8 actual_slot = (side == up || side == down) ? slot % g->width : slot % g->height;

    u16 offset = 0;

    if (side >= right)
        offset += g->width;
    if (side >= down)
        offset += g->height;
    if (side == left)
        offset += g->width;

    offset += actual_slot;

    return offset;
}

void slot_set_length(grid*g, u8 side, u8 slot, u8 len)
{
    u16 offset = io_slot_offset(g, side, slot);

    io_slot *s = &g->slots[offset];

    s->len = len;
}

u8* attach_input(grid *g, u8 side, u8 slot)
{
    u16 offset = io_slot_offset(g, side, slot);

    io_slot *s = &g->slots[offset];

    s->read_only = true;
    s->cur = 0;

    return s->bytes;
}

u8* attach_output(grid *g, u8 side, u8 slot)
{
    u16 offset = io_slot_offset(g, side, slot);

    io_slot *s = &g->slots[offset];

    s->read_only = false;
    s->cur = 0;

    return s->bytes;
}

void load_program(grid *g, u8 x, u8 y, const void *bytecode, u8 length)
{
    block *b = &g->blocks[y * g->width + x];

    memset(b, 0, sizeof(block));

    b->bytecode = (void *)bytecode;
    b->length = length;
    b->stack_top = -1;
    b->current_instruction = 0;
}