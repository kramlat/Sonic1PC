#pragma once

#include "Object.h"

// Debug (object placement) mode's per-zone item list -- matches the real
// disassembly's DebugList/dbug macro table (s1disasm's DebugMode.asm).
// Each entry describes one placeable object: which type to spawn, what
// subtype/frame to give it, and what mappings/tile to show for its preview
// sprite while cycling through the list (before it's actually placed).
//
// type == ObjId_Null marks a slot that's a placeholder only -- either the
// real game's list has an object here that this project hasn't ported yet,
// or (rarely) real hardware's own entry doesn't map cleanly onto this
// project's structure. Cycling still counts these slots (list length/order
// matches the real per-zone counts exactly), but pressing C on one does
// nothing -- no sprite, no spawn.
// One layer of a subtype variant's preview: which of the object's OWN
// existing mapping frames to stack in, and how far to nudge it (in
// addition to that frame's own piece coordinates -- most stacked layers
// want 0,0, since Sonic 1 sprite pieces already carry their own
// object-relative offsets; the nudge exists for the rare case where a
// second layer needs to land somewhere other than dead center).
// x_off/y_off are int16_t (wider than a real hardware sprite piece's own
// int8_t offset bytes) because a couple of stacks -- Ring's row/column
// formation preview in particular -- legitimately need offsets beyond
// +/-127 (e.g. 6 extra rings at 0x20 apart = 192px). DebugMode_BuildStack
// clamps the final composed byte to int8_t range rather than wrapping, so
// a stack that runs off the representable range just visually clips at the
// edge instead of producing garbage.
typedef struct {
    uint8_t frame;
    int16_t x_off;
    int16_t y_off;
} DebugFramePiece;

// One preview variant: which frame(s) to stack (assembling the composed
// sprite -- e.g. a Monitor's plain box frame plus its content icon frame,
// drawn together, rather than picking just one or the other) plus flip/
// palette-line-offset, for a given value of
// (subtype >> DebugListEntry::variant_shift) & DebugListEntry::variant_mask.
// Lets the debug preview sprite update live as you adjust the selected
// item's subtype (JPAD_EXT_SUBTYPE_*), so e.g. Spikes shows its real
// orientation and Monitor shows its real box+icon instead of always the
// default. Objects whose subtype only changes size (not appearance) --
// like InvisibleBarrier -- don't need one of these tables at all; the same
// frame stays correct across their whole subtype range.
typedef struct {
    uint8_t subtype_key;
    const DebugFramePiece *stack;
    uint8_t stack_count;
    uint8_t flip;         // bit0 = x_flip, bit1 = y_flip
    uint8_t pal_add;      // added (mod 4) to the entry's own base palette line
    uint16_t tile_override; // 0 = keep the entry's own base tile; otherwise replaces
                            // it before flip/pal_add are applied -- needed when a
                            // variant's art lives at a different tile index entirely
                            // (e.g. Spring's horizontal art vs its default upright art),
                            // not just a different frame within the same tile.
} DebugSubtypeVariant;

typedef struct {
    ObjectId type;
    const void *mappings;
    uint16_t tile;
    uint8_t subtype;
    uint8_t frame;
    const DebugSubtypeVariant *variants; // NULL = no subtype-driven preview change (default: keep showing `frame` above)
    uint8_t variant_count;
    uint8_t variant_shift; // subtype_key = (debug_subtype >> variant_shift) & variant_mask
    uint8_t variant_mask;
} DebugListEntry;

// Returns the current zone's (or the Ending/Special Stage list's) debug
// item table and entry count. Matches Debug_Init/Debug_Action's own zone
// selection: Special Stage and the ending sequence share one list,
// everything else is indexed by LEVEL_ZONE(level_id).
const DebugListEntry *DebugList_Get(int *count_out);
