#include "Rings.h"

#include <string.h>

#include "EngineObject.h"
#include "Mappings.h"
#include "ScrollEngine.h"
#include "EngineConstants.h"
#include "Video.h"

#include "Backend/VDP.h"

#define ENTRY_SIZE 4

uint8_t rings_frame;

static const uint8_t *layout;       // Ring_start_addr_ROM's list, or NULL
static int ring_count;
static RingStatus status[RINGS_MAX];   // Ring_status_table
static uint16_t consuming[RINGS_CONSUMING_MAX]; // Ring_consumption_list: the rings sparkling now
static int consuming_count;

static const uint8_t *ring_mappings;
static uint16_t ring_art_tile;

static uint16_t Word(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint16_t RingX(int i) {
    return Word(layout + i * ENTRY_SIZE);
}

static uint16_t RingY(int i) {
    return Word(layout + i * ENTRY_SIZE + 2);
}

// The first ring whose X is not below `x` (the camera's X - 8 and the like): the layout is sorted
static int FirstFrom(uint16_t x) {
    int lo = 0, hi = ring_count;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (RingX(mid) < x)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

void Rings_SetLook(const uint8_t *mappings, uint16_t art_tile) {
    ring_mappings = mappings;
    ring_art_tile = art_tile;
}

void Rings_Init(const uint8_t *new_layout) {
    memset(status, 0, sizeof(status));
    consuming_count = 0;
    layout = new_layout;
    ring_count = 0;
    if (layout != NULL) {
        // sub_EB1A: the rings are counted up to the end mark (the first entry with its X's top bit set), and no more than the table holds
        while (ring_count < RINGS_MAX && RingX(ring_count) < 0x8000)
            ring_count++;
    }
}

// sub_E994: each sparkling ring counts down 6 frames for each of the sparkle's frames, and is gone after the last
void Rings_Update(void) {
    for (int i = 0; i < consuming_count;) {
        RingStatus *s = &status[consuming[i]];
        if (--s->timer == 0) {
            s->timer = 6;
            if (++s->frame == 8) {
                s->timer = s->frame = 0xFF; // gone
                consuming[i] = consuming[--consuming_count];
                continue;
            }
        }
        i++;
    }
}

// The tests of Test_Ring_Collisions, in the word arithmetic of the 68000: `d0 = ring - 6 - player`, which is inside when it carries into the range ($C below) or is at most `size` above
static bool Inside(uint16_t ring, uint16_t player, uint16_t size) {
    uint16_t d0 = (uint16_t)(ring - 6);
    const bool borrow = d0 < player;
    d0 = (uint16_t)(d0 - player);
    if (!borrow)
        return d0 <= size;
    return (uint32_t)d0 + 0xC >= 0x10000;
}

int Rings_Collide(int16_t x, int16_t y, uint8_t y_radius) {
    if (ring_count == 0)
        return 0;
    const uint16_t px = (uint16_t)(x - 8);
    const uint16_t r = (uint8_t)(y_radius - 3);
    const uint16_t py = (uint16_t)(y - r);
    const uint16_t height = (uint16_t)(r * 2);
    int collected = 0;

    // The rings that can be in the box have their X within $6 below to $16 above the player's left edge (the exact test is Inside's); look at a few more
    const int low = x - 8 - 0x10 > 0 ? x - 8 - 0x10 : 0, high = x - 8 + 0x20;
    for (int i = FirstFrom((uint16_t)low); i < ring_count && RingX(i) <= high; i++) {
        if (status[i].timer != 0 || status[i].frame != 0)
            continue;
        if (!Inside(RingX(i), px, 0x10) || !Inside(RingY(i), py, height))
            continue;

        status[i].timer = 6;
        status[i].frame = 4;
        if (consuming_count < RINGS_CONSUMING_MAX)
            consuming[consuming_count++] = (uint16_t)i;
        else
            status[i].timer = status[i].frame = 0xFF; // (the list is full: no sparkle)
        collected++;
    }
    return collected;
}

void Rings_Render(sprite_t **sprite, uint8_t *sprite_i, const SpriteView *view, int16_t top, int view_w) {
    if (ring_count == 0 || ring_mappings == NULL)
        return;
    const SpriteLayer *layer = &view->layer[1];
    const int16_t cam_x = layer->x ? *layer->x : 0, cam_y = layer->y ? *layer->y : 0;
    const int16_t y_base = (int16_t)(top - 0x80);

    // The rings from 8 pixels left of the view to a little past its right edge (S3&K: $150 for its 320 pixels)
    const int first = cam_x > 8 ? cam_x - 8 : 1;
    const int last = first + view_w + 0x10;
    for (int i = FirstFrom((uint16_t)first); i < ring_count && RingX(i) < last; i++) {
        if (status[i].timer == 0xFF && status[i].frame == 0xFF)
            continue; // gone

        // Inside the view's height (with the level's wrap), the ring's centre
        uint16_t y = (uint16_t)((RingY(i) - cam_y + 8) & level_screen_y_wrap);
        if (y >= SCREEN_HEIGHT + 0x10)
            continue;
        y = (uint16_t)(y + 0x78);
        const int16_t x = (int16_t)(RingX(i) - cam_x + 0x80);

        const unsigned frame = status[i].frame != 0 ? status[i].frame : rings_frame;
        const uint8_t *pieces;
        const uint16_t count = Mappings_FramePieces(ring_mappings, frame, &pieces);
        if (count == 0 || *sprite_i >= BUFFER_SPRITES)
            continue;
        BuildSpr_PiecesS3(sprite, sprite_i, (uint16_t)x, (uint16_t)(y + y_base), ring_art_tile, pieces, count, view_w);
    }
}

int Rings_Count(void) {
    return ring_count;
}

RingStatus Rings_StatusOf(int index) {
    RingStatus none = {0, 0};
    return index >= 0 && index < ring_count ? status[index] : none;
}

void Rings_Position(int index, int16_t *x, int16_t *y) {
    *x = *y = 0;
    if (index >= 0 && index < ring_count) {
        *x = (int16_t)RingX(index);
        *y = (int16_t)RingY(index);
    }
}

int Rings_ConsumingCount(void) {
    return consuming_count;
}
