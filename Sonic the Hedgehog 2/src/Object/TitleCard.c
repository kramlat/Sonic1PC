#include "Object/TitleCard.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Constants.h"
#include "Game.h"
#include "Level.h"
#include "PLC.h"
#include "Sound.h"

#include "Resource/Mappings/TitleCard.h"
#include "Resource/Mappings/TitleCardNames.h"
#include "Resource/Art/TitleCardZone.h"
#include "Resource/Art/TitleCardLetters.h"
#include "Nemesis.h"
#include "Backend/VDP.h"
#include "ScrollEngine.h"
#include "SplitScreen.h"
#include <string.h>
#include "Mappings.h"
#include "Resource/Mappings/GotThrough.h"
#include "Resource/Mappings/SpecialResult.h"

// Title card configuration. The positions depend on the width and height of the picture, which are chosen at run time, so
// the tables are built where they are used (see the title card's creation routine).
struct TitleCard_Item {
    int16_t y;
    uint8_t routine, frame;
};

struct TitleCard_Config {
    int16_t x0, x1;
};

// "Got through" (end of act) card item data, transcribed from Got_ItemData
static const struct GotCard_Item {
    int16_t x0, x1; // start / target x-position
    int16_t y;
    uint8_t routine, frame;
} gotcard_item[7] = {
    { 0x0004, 0x0124, 0x00BC, 2, 0 }, // "SONIC HAS"
    { -0x0120, 0x0120, 0x00D0, 2, 1 }, // "PASSED"
    { 0x040C, 0x014C, 0x00D6, 2, 6 }, // "ACT" 1/2/3 (dynamic frame, see below)
    { 0x0520, 0x0120, 0x00EC, 2, 2 }, // Score tally
    { 0x0540, 0x0120, 0x00FC, 2, 3 }, // Time Bonus tally
    { 0x0560, 0x0120, 0x010C, 2, 4 }, // Ring Bonus tally
    { 0x020C, 0x014C, 0x00CC, 2, 5 }, // Blue oval
};

// What each act leads to, by zone slot and act (the Simon Wai prototype's level order, word_BF9A): the first act of a zone to its second, the second to the first of the next zone. An entry of 0 is the first act of Emerald Hill,
// where the prototype sends the acts of zones that have none, as its own table does (it does not go to the Sega screen, as Sonic 1 does for such an entry).
static const uint16_t level_order[ZoneId_Num][2] = {
    [0x00] = { LEVEL_ID(0x00, 1), LEVEL_ID(0x02, 0) },
    [0x02] = { LEVEL_ID(0x02, 1), LEVEL_ID(0x04, 0) },
    [0x04] = { LEVEL_ID(0x04, 1), LEVEL_ID(0x07, 0) },
    [0x07] = { LEVEL_ID(0x07, 1), 0 },
    [0x08] = { LEVEL_ID(0x08, 1), LEVEL_ID(0x0A, 0) },
    [0x09] = { LEVEL_ID(0x09, 1), LEVEL_ID(0x0F, 0) },
    [0x0A] = { LEVEL_ID(0x0A, 1), LEVEL_ID(0x0B, 0) },
    [0x0B] = { LEVEL_ID(0x0B, 1), LEVEL_ID(0x0C, 0) },
    [0x0C] = { LEVEL_ID(0x0C, 1), LEVEL_ID(0x0D, 0) },
    [0x0D] = { LEVEL_ID(0x0D, 1), LEVEL_ID(0x07, 0) },
    [0x0F] = { LEVEL_ID(0x0F, 1), LEVEL_ID(0x0D, 0) },
    [0x10] = { LEVEL_ID(0x10, 1), 0 },
};

// SBZ2 post-level cutscene: has the Ring Bonus element (the one that drives the tally) reached its slide-out
// routine? (real: cmpi.b #$E,(v_endcardring+obRoutine))
static bool GotCard_SlideOutStarted(void) {
    for (int i = 0; i < SST_COUNT; i++) {
        const Sst *o = &sst[i];
        if (o->code != NULL && o->code == Obj_GotThroughCard && o->mapping_frame == 4)
            return o->routine == 0xE;
    }
    return false;
}

// Got_SBZ2_MoveOut: every element slides back to where it started, twice as fast as it came in. The Ring Bonus
// element then hands control back to the player and starts the Final Zone music; the rest delete themselves.
static void GotCard_SBZ2_MoveOut(Sst *self, Own_TitleCard *own) {
    int16_t speed = 0x20;
    if (self->x_pos.f.u == own->final_x) {
        if (self->mapping_frame != 4) {
            Sst_Delete(self);
            return;
        }
        self->routine += 2; // Got_SBZ2_Boundary
        lock_ctrl = false; // unlock controls
        QueueSound1(bgm_FZ);
        return;
    }
    if (self->x_pos.f.u > own->final_x)
        speed = -speed;
    self->x_pos.f.u += speed;
    if (self->x_pos.f.u >= 0 && self->x_pos.f.u < (0x200 + SCREEN_WIDEADD))
        Sst_Draw(self);
}

// "Got through" (end of act) card object
void Obj_GotThroughCard(Sst *self) {
    Own_TitleCard *own = (Own_TitleCard *)self->own.u8;

    switch (self->routine) {
    case 0: // Wait for title card patterns to finish decompressing
        if (plc_buffer[0].art != NULL)
            return;

        // Fallthrough
    {
        // Spawn the 7 card elements (this object plus the next 6 in sequence)
        Sst *a1 = self;
        const struct GotCard_Item *item = gotcard_item;
        for (int i = 0; i < 7; i++, item++) {
            if (i > 0) {
                a1 = Sst_AllocateAfter(a1);
                if (a1 == NULL)
                    break;
            }
            Own_TitleCard *scratch_a1 = (Own_TitleCard *)a1->own.u8;
            a1->code = Obj_GotThroughCard;
            a1->x_pos.f.u = item->x0;
            scratch_a1->final_x = item->x0;
            scratch_a1->main_x = item->x1;
            a1->y_pos.f.u = item->y;
            a1->routine = item->routine;

            uint8_t frame = item->frame;
            if (frame == 6) // "ACT" element: add the current act number
                frame += LEVEL_ACT(level_id);
            a1->mapping_frame = frame;
            a1->mappings = Mappings_GotThrough;
            a1->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Title_Card);
            a1->width_pixels = 0;
            a1->render_flags = 0;
            a1->priority = 0 * 0x80;
            a1->anim_frame_timer = 60;
        }
        return;
    }
    case 2: { // Moving to on-screen position
        int16_t speed = 0x10;
        if (self->x_pos.f.u == own->main_x) {
            // Reached target position. In SBZ2 the card doesn't lead to the next act: once the Ring Bonus element
            // (which runs the tally) gets to its slide-out routine, every element slides back out.
            if (GotCard_SlideOutStarted()) {
                self->routine = 0xE; // Got_SBZ2_MoveOut
                GotCard_SBZ2_MoveOut(self, own);
                return; // (it displays itself)
            }

            if (self->mapping_frame != 4) // Only the Ring Bonus element controls this
                break;

            self->routine += 2; // -> Got_Wait
            own->timer = 3 * 60; // 3 second delay before tally
            break;
        } else if (self->x_pos.f.u > own->main_x) {
            speed = -speed;
        }
        self->x_pos.f.u += speed;
        break;
    }
    case 4: // Wait routines: hold for a timer, then advance
    case 8:
    case 0xC: // (SBZ2: the post-tally wait, before sliding out)
        if (--own->timer == 0)
            self->routine += 2;
        break;
    case 6: { // Score tally
        uint16_t ticked = 0;

        if (time_bonus) {
            ticked += 10;
            time_bonus -= 10;
        }
        if (ring_bonus) {
            ticked += 10;
            ring_bonus -= 10;
        }

        if (ticked) {
            AddPoints(ticked);
            if ((frame_count & 3) == 0) // (the blip is every 4th frame, as in Nick Arcade)
                PlaySound(sfx_Switch);
        } else {
            PlaySound(sfx_Cash);
            self->routine += 2; // -> Got_Wait, before Got_NextLevel
            own->timer = 3 * 60; // 3 second post-tally delay
        }
        break;
    }
    case 0xA: { // Advance to the next act
        level_id = (LEVEL_ZONE(level_id) == ZoneId_MTZ && LEVEL_ACT(level_id) == 2) ? 0 : level_order[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id) & 1]; // (Metropolis' third act has no next one yet: the prototype's own entry for it was empty)
        last_lamp = 0;

        // Sonic jumped into a giant ring (Obj_RingFlash set big_ring): the next screen is a special stage
        if (big_ring)
            gamemode = GameMode_Special;
        else
            restart = 1;
        break;
    }
    case 0xE: // Got_SBZ2_MoveOut
        GotCard_SBZ2_MoveOut(self, own);
        return;
    case 0x10: // Got_SBZ2_Boundary: the Ring Bonus element opens the screen's right boundary for the cutscene room
        limit_right2 += 2;
        if (limit_right2 == 0x2050 + 0xB0) // boss_sbz2_x+$B0
            Sst_Delete(self);
        return;
    }

    // Draw (card stays fully on-screen throughout this whole sequence)
    if (self->x_pos.f.u >= 0 && self->x_pos.f.u < (0x200 + SCREEN_WIDEADD))
        Sst_Draw(self);
}

// The elements of the level's title card, in the order they were made: the name, ZONE and the act number (NULL where the card is not there)
static Sst *TitleCard_Element(int n) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == Obj_TitleCard && n-- == 0)
            return &sst[i];
    return NULL;
}

bool TitleCard_Arrived(void) {
    const Sst *zone = TitleCard_Element(0); // (the level waits for the name: the pre-beta's `cmp.w ($FFFFB0B0).w,D0`)
    return zone == NULL || zone->x_pos.f.u == ((const Own_TitleCard *)zone->own.u8)->main_x;
}

void TitleCard_Leave(void) {
    static const uint8_t add[3] = { 2, 4, 4 }; // (the name loads the animals' art as it goes: routine 4; the others 6)
    for (int i = 0; i < 3; i++) {
        Sst *e = TitleCard_Element(i);
        if (e != NULL)
            e->routine = (uint8_t)(e->routine + add[i]);
    }
}

// The title cards of the Sonic 2 pre-beta (September 14, 1992: its object $34, Obj_0x34_Title_Cards; art, mappings and letter tables from tools/import_s2prebeta_titlecards.pl). Three elements slide
// in: the name of the zone (from the right), ZONE and the act number (from the left); they wait a second after the level has been loaded and slide away, the name's end loading the explosion's and the
// animals' art. Its frames: the names (the pre-beta's own, and Wood's, Sand Shower's, Cyber City's and Hidden Palace's written from its alphabet: the user), ZONE, ACT 1 to 3
enum { TC_FRAME_ZONE = 15, TC_FRAME_ACT1 = 16 };
extern const uint8_t titlecard_letters[][20];
#define TC_ART_FIXED 0x580   // the fixed part (E, N, O, ZONE, the numbers)
#define TC_ART_LETTERS 0x5DE // where the letters a name needs are copied to

// The name's frame by zone slot (a zone that has no name of its own is Green Hill, as the pre-beta's placeholder slots are)
static uint8_t TitleCard_NameFrame(int zone) {
    switch (zone) {
    case ZoneId_MTZ: return 1;
    case ZoneId_HTZ: return 2;
    case ZoneId_OOZ: return 3;
    case ZoneId_MCZ: return 4;  // Dust Hill
    case ZoneId_CNZ: return 5;
    case ZoneId_CPZ: return 6;
    case ZoneId_ARZ: return 7;  // Neo Green Hill
    case ZoneId_SCZ: return 8;
    case ZoneId_WZ: return 11;
    case ZoneId_SSZ: return 12;
    case ZoneId_GCZ: return 13;
    case ZoneId_HPZ: return 14;
    default: return 0;          // Green Hill (Emerald Hill's card says so too: the pre-beta has no other name for its first slot)
    }
}

// The card's art goes into VRAM when the level's first pattern load cues have finished, not when the card is made: those lists (the water zones' bubbles at $5E8, Hidden Palace's bat at $5A8) have
// art in tiles the card uses, and the card would show what they write over it
static bool titlecard_art_loaded;

// Load_Title_Cards_Art: the fixed part at tile $580, then the letters the name uses, from the alphabet, one after the other from tile $5DE
static void TitleCard_LoadArt(uint8_t name) {
    static uint8_t alphabet[0x1000];
    VDP_SeekVRAM(ART_VRAM(TC_ART_FIXED));
    NemDec(Art_TitleCardZone);
    NemDecToRAM(Art_TitleCardLetters, alphabet);
    VDP_SeekVRAM(ART_VRAM(TC_ART_LETTERS));
    for (const uint8_t *l = titlecard_letters[name]; *l != 0xFF; l += 2)
        VDP_WriteVRAM(alphabet + l[0] * TILE_SIZE, (size_t)l[1] * TILE_SIZE);
}

// The card in the split screen, side by side: each view is half as wide as the picture and the card is drawn in both (an object that is not in the level is at the same place in each view), so it has
// to fit and sit in the middle of each. Its art is squeezed to half its width (the pieces of every element are made again from the unscaled ones: a letter is two cells wide, so it becomes one; where two
// pixels meet the brighter one stays, the white of the letter before its outline) and the elements are put so that the name, and ZONE with the number after it, are centred in the view.
#define TC_SCALED_TILES 0x600 // where the squeezed pieces' tiles go
static uint8_t titlecard_scaled_map[3][4 + 24 * SPRITE_PIECE_SIZE_S3];

static bool TitleCard_SplitWanted(void) {
    return SplitScreen_SideBySide();
}

// One element's pieces squeezed: its mapping in titlecard_scaled_map[index] and its extent (the pieces' leftmost and rightmost pixel); `next_tile` is where the next piece's tiles go
static void TitleCard_Squeeze(int index, uint8_t frame, unsigned *next_tile, int *left, int *right) {
    const uint8_t *pieces;
    const uint16_t count = Mappings_FramePieces(Mappings_TitleCardNames, frame, &pieces);
    uint8_t *map = titlecard_scaled_map[index];
    map[0] = 0;
    map[1] = 2;
    map[2] = 0;
    map[3] = (uint8_t)count;
    uint8_t *out = map + 4;
    const uint8_t *vram = VDP_PeekVRAM();
    *left = 0x7FFF;
    *right = -0x7FFF;
    for (unsigned p = 0; p < count && p < 24; p++, pieces += SPRITE_PIECE_SIZE_S3, out += SPRITE_PIECE_SIZE_S3) {
        const uint8_t size = Mappings_PieceSize(pieces);
        const int wc = ((size >> 2) & 3) + 1, hc = (size & 3) + 1, wc2 = (wc + 1) / 2;
        const unsigned tile_word = Mappings_PieceTile(pieces), tile = tile_word & 0x7FF; // (the word's flags and palette line stay: the number is on line 1)
        // the source, a pixel at a time (the cells go down a column and then across), and the squeezed cells
        static uint8_t cells[16 * TILE_SIZE];
        memset(cells, 0, sizeof(cells));
        int first = 8 * wc2, last = -1;
        for (int y = 0; y < hc * 8; y++) {
            for (int dx = 0; dx < wc2 * 8; dx++) {
                uint8_t best = 0;
                for (int k = 0; k < 2; k++) {
                    const int sx = dx * 2 + k;
                    if (sx >= wc * 8)
                        continue;
                    const uint8_t *row = vram + (size_t)(tile + (size_t)(sx >> 3) * hc + (size_t)(y >> 3)) * TILE_SIZE + (size_t)(y & 7) * 4;
                    const uint8_t b = row[(sx & 7) >> 1];
                    const uint8_t px = (sx & 1) ? (b & 0xF) : (b >> 4);
                    if (px > best)
                        best = px;
                }
                if (best) {
                    if (dx < first) first = dx;
                    if (dx > last) last = dx;
                }
                uint8_t *dst = cells + (size_t)((dx >> 3) * hc + (y >> 3)) * TILE_SIZE + (size_t)(y & 7) * 4 + (size_t)((dx & 7) >> 1);
                *dst = (dx & 1) ? (uint8_t)((*dst & 0xF0) | best) : (uint8_t)((*dst & 0x0F) | (best << 4));
            }
        }
        VDP_SeekVRAM(ART_VRAM(*next_tile));
        VDP_WriteVRAM(cells, (size_t)wc2 * hc * TILE_SIZE);
        const int x = (Mappings_PieceXS3(pieces) >> 1);
        out[0] = pieces[0];
        out[1] = (uint8_t)(((wc2 - 1) << 2) | (hc - 1));
        out[2] = (uint8_t)(((tile_word & 0xF800) | *next_tile) >> 8);
        out[3] = (uint8_t)*next_tile;
        out[4] = (uint8_t)((uint16_t)x >> 8);
        out[5] = (uint8_t)x;
        *next_tile += (unsigned)(wc2 * hc);
        if (last >= first) {
            if (x + first < *left) *left = x + first;
            if (x + last + 1 > *right) *right = x + last + 1;
        }
    }
    if (*left > *right)
        *left = *right = 0;
}

// Makes the card fit the views: all three elements, once the split screen is on (the card is made before it is: the level is loaded under it)
static void TitleCard_FitSplit(void) {
    Sst *e[3] = { TitleCard_Element(0), TitleCard_Element(1), TitleCard_Element(2) };
    if (e[0] == NULL || e[0]->routine_secondary)
        return;
    const int view_w = SCREEN_WIDTH / 2;
    const int centre = 128 + view_w / 2; // (an object's x counts from 128 to the view's left edge)
    unsigned next_tile = TC_SCALED_TILES;
    int left[3] = { 0, 0, 0 }, right[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++)
        if (e[i] != NULL)
            TitleCard_Squeeze(i, e[i]->status_secondary, &next_tile, &left[i], &right[i]);
    // where each stands when it has arrived: the name in the middle; ZONE and the number after it as one, in the middle
    int target[3];
    target[0] = centre - (left[0] + right[0]) / 2;
    const int zone_w = right[1] - left[1], number_w = e[2] != NULL ? right[2] - left[2] : 0, gap = e[2] != NULL ? 6 : 0;
    const int start = centre - (zone_w + gap + number_w) / 2;
    target[1] = start - left[1];
    target[2] = start + zone_w + gap - left[2];
    for (int i = 0; i < 3; i++) {
        if (e[i] == NULL)
            continue;
        Own_TitleCard *own = (Own_TitleCard *)e[i]->own.u8;
        const int dx = target[i] - own->main_x;       // (the element keeps how far it still has to go, and how far out it starts and goes)
        e[i]->x_pos.f.u = (int16_t)(e[i]->x_pos.f.u + dx);
        own->final_x = (int16_t)(own->final_x + dx);
        own->main_x = (int16_t)target[i];
        e[i]->mappings = titlecard_scaled_map[i];
        e[i]->mapping_frame = 0;
        e[i]->routine_secondary = 1;
    }
}

// Title card object
void Obj_TitleCard(Sst *self) {
    Own_TitleCard *own = (Own_TitleCard *)self->own.u8;

    switch (self->routine) {
    case 0: {
        const int zone = LEVEL_ZONE(level_id);
        const uint8_t name = TitleCard_NameFrame(zone);
        titlecard_art_loaded = false; // (the art goes in once the level's first pattern load cues are done: see routine 2)

        // Get configuration (built now: it depends on the size of the picture): where each element starts and where it stops
        const int16_t to_add = SCREEN_WIDEADD2;
        const int16_t from_add = to_add + ((SCREEN_WIDEADD2 + 0xF) & ~0xF);
        const int16_t from_sub = (0x10 - to_add) & 0xF;
        const struct { int16_t x0, x1; int16_t y; uint8_t frame; } card[3] = {
            { 0x0240 + from_add, 0x0120 + to_add, 0x00B8 + SCREEN_TALLADD2, name },
            { 0x0028 - from_sub, 0x0148 + to_add, 0x00D0 + SCREEN_TALLADD2, TC_FRAME_ZONE },
            { 0x0068 - from_sub, 0x0188 + to_add, 0x00D0 + SCREEN_TALLADD2, (uint8_t)(TC_FRAME_ACT1 + LEVEL_ACT(level_id)) },
        };
        // (Sky Chase, Sky Fortress and Death Egg are not played in acts: their card has no number)
        const int count = (zone == ZoneId_SCZ || zone == 6 || zone == 0x0E) ? 2 : 3;

        Sst *a1 = self;
        for (int i = 0; i < count; i++) {
            if (i > 0) {
                a1 = Sst_AllocateAfter(a1);
                if (a1 == NULL)
                    break;
            }
            Own_TitleCard *scratch_a1 = (Own_TitleCard *)a1->own.u8;
            a1->code = Obj_TitleCard;
            a1->x_pos.f.u = card[i].x0;
            scratch_a1->final_x = card[i].x0;
            scratch_a1->main_x = card[i].x1;
            a1->y_pos.f.u = card[i].y;
            a1->routine = 2;
            a1->mapping_frame = card[i].frame;
            a1->status_secondary = card[i].frame; // (the unscaled frame, for the split screen)
            a1->routine_secondary = 0;
            a1->mappings = Mappings_TitleCardNames;
            a1->art_tile = TILE_MAP(1, 0, 0, 0, 0); // (priority; the pieces hold their tiles: $580 on)
            a1->width_pixels = 0x60;
            a1->render_flags = 0;
            a1->priority = 0 * 0x80;
            a1->anim_frame_timer = 60;
        }
    }
        // Fallthrough
    case 2: // Moving to on-screen position, 16 pixels a frame
        if (TitleCard_SplitWanted())
            TitleCard_FitSplit();
        if (!titlecard_art_loaded) {
            if (plc_buffer[0].art != NULL)
                break; // (the queue is still being drained: nothing of the card shows yet)
            TitleCard_LoadArt(TitleCard_NameFrame(LEVEL_ZONE(level_id)));
            titlecard_art_loaded = true;
        }
        if (self->x_pos.f.u > own->main_x)
            self->x_pos.f.u -= 16;
        else if (self->x_pos.f.u < own->main_x)
            self->x_pos.f.u += 16;
        if (self->x_pos.f.u >= 0 && self->x_pos.f.u < (0x200 + SCREEN_WIDEADD))
            Sst_Draw(self);
        break;
    case 4: // Waits, then moves off-screen, 32 pixels a frame
    case 6:
        if (TitleCard_SplitWanted())
            TitleCard_FitSplit();
        if (self->anim_frame_timer) {
            self->anim_frame_timer--;
            Sst_Draw(self);
            break;
        }
        if (self->render_flags & SST_RENDER_ON_SCREEN) {
            if (self->x_pos.f.u != own->final_x) {
                if (self->x_pos.f.u > own->final_x)
                    self->x_pos.f.u -= 32;
                else
                    self->x_pos.f.u += 32;
                if (self->x_pos.f.u >= 0 && self->x_pos.f.u < (0x200 + SCREEN_WIDEADD))
                    Sst_Draw(self);
                break;
            }
        }
        // (gone, or back where it started) the name's end loads the explosions' and the animals' art
        if (self->routine == 4) {
            AddPLC(PlcId_Explode);
            AddPLC(Level_AnimalsPlc()); // (the zone's pair of the alpha's: Emerald Hill's is the pre-beta's too, Squirrel and BlueBird)
            if (LEVEL_LOAD_BLOCK(level_id)->plc2 != 0)
                AddPLC(LEVEL_LOAD_BLOCK(level_id)->plc2); // (the zone's second list, which LevelDataLoad leaves for here)
        }
        Sst_Delete(self);
        break;
    }
}
