#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../include/definitions.h"

grid initialize_grid(u8 w, u8 h)
{
    grid g = {};

    u16 total_blocks = w * h;
    u16 edge_length = (w + h) * 2; // amount of possible imputs and outputs

    g.width = w;
    g.height = h;
    g.total_blocks = total_blocks;
    g.perimeter = edge_length;

    g.blocks = calloc(total_blocks, sizeof(block));
    assert(g.blocks);

    g.slots = calloc(edge_length, sizeof(io_slot));
    assert(g.slots);

    return g;
}

void free_grid(grid *g)
{
    free(g->blocks);
    free(g->slots);

    memset(g, 0, sizeof(grid));
}

u16 io_slot_offset(grid *g, u8 side, u8 slot)
{
    // only 4
    assert(side < 4);

    if (side == up || side == down)
        assert(slot < g->width);
    else
        assert(slot < g->height);

    // limit to availabl amount of slots on said side
    slot = (side == up || side == down) ? slot % g->width : slot % g->height;

    u16 offset = 0;

    if (side >= right)
        offset += g->width;
    if (side >= down)
        offset += g->height;
    if (side == left)
        offset += g->width;

    offset += slot;

    return offset;
}

void attach_input(grid *g, u8 side, u8 slot, const u8 *src, u8 len)
{
    u16 offset = io_slot_offset(g, side, slot);

    io_slot *s = &g->slots[offset];

    s->len = len;
    s->ptr = (void *)src;
    s->read_only = true;
    s->cur = 0;
}

void attach_output(grid *g, u8 side, u8 slot, u8 *dst, u8 len)
{
    u16 offset = io_slot_offset(g, side, slot);

    io_slot *s = &g->slots[offset];

    s->len = len;
    s->ptr = (void *)dst;
    s->read_only = false;
    s->cur = 0;
}

void load_program(grid *g, u8 x, u8 y, const void *bytecode, u8 length)
{
    block *b = &g->blocks[y * g->width + x];

    memset(b, 0, sizeof(block));

    b->bytecode = (void *)bytecode;
    b->length = length;
}