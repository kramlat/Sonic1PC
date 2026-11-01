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

// The animals of a zone, until each has its own: those of the slot it had in Nick Arcade (and Emerald Hill's for the zones without any yet)
static uint8_t TitleCard_AnimalsPlc(int zone) {
    switch (zone) {
    case ZoneId_CPZ: return PlcId_MZAnimals;
    case ZoneId_HPZ: return PlcId_SYZAnimals;
    case ZoneId_HTZ: return PlcId_SBZAnimals;
    default: return PlcId_SLZAnimals;
    }
}

// The four elements of the level's title card, in the order they were made (NULL where the card is not there)
static Sst *TitleCard_Element(int n) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == Obj_TitleCard && n-- == 0)
            return &sst[i];
    return NULL;
}

bool TitleCard_Arrived(void) {
    const Sst *zone = TitleCard_Element(2);
    return zone == NULL || zone->x_pos.f.u == ((const Own_TitleCard *)zone->own.u8)->main_x;
}

void TitleCard_Leave(void) {
    static const uint8_t add[4] = { 2, 4, 4, 4 };
    for (int i = 0; i < 4; i++) {
        Sst *e = TitleCard_Element(i);
        if (e != NULL)
            e->routine = (uint8_t)(e->routine + add[i]);
    }
}

// Title card object
void Obj_TitleCard(Sst *self) {
    Own_TitleCard *own = (Own_TitleCard *)self->own.u8;

    switch (self->routine) {
    case 0: {
        Sst *a1 = self;

        // The card of the zone: the name on it is the mapping frame of the zone's number (the prototype has only the cards of Sonic 1's six zones and Final Zone, so a zone past them gets no name)
        // and the card is placed by the zone's row of the prototype's table
        int zone = LEVEL_ZONE(level_id);
        uint8_t d0;
        uint16_t d2 = (uint16_t)zone;

        // Get configuration (built now: it depends on the size of the picture)
        const int16_t to_add = SCREEN_WIDEADD2;
        const int16_t from_add = to_add + ((SCREEN_WIDEADD2 + 0xF) & ~0xF);
        const int16_t from_sub = (0x10 - to_add) & 0xF;
        (void)from_add; (void)from_sub;
        const struct TitleCard_Item titlecard_item[4] = {
    { 0x00D0 + SCREEN_TALLADD2, 0x02, 0x00 },
    { 0x00E4 + SCREEN_TALLADD2, 0x02, 0x06 },
    { 0x00EA + SCREEN_TALLADD2, 0x02, 0x07 },
    { 0x00E0 + SCREEN_TALLADD2, 0x02, 0x0A },
};
        const struct TitleCard_Config card_a[4] = { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x0104 - from_sub, 0x013C + to_add }, { 0x0414 + from_add, 0x0154 + to_add }, { 0x0214 + from_add, 0x0154 + to_add } };
        const struct TitleCard_Config card_b[4] = { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x010C - from_sub, 0x0134 + to_add }, { 0x040C + from_add, 0x014C + to_add }, { 0x020C + from_add, 0x014C + to_add } };
        const struct TitleCard_Config card_c[4] = { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x0120 - from_sub, 0x0120 + to_add }, { 0x03F8 + from_add, 0x0138 + to_add }, { 0x01F8 + from_add, 0x0138 + to_add } };
        const struct TitleCard_Config card_d[4] = { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x00FC - from_sub, 0x0144 + to_add }, { 0x041C + from_add, 0x015C + to_add }, { 0x021C + from_add, 0x015C + to_add } };
        const struct TitleCard_Config card_e[4] = { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x011C - from_sub, 0x0124 + to_add }, { 0x03EC + from_add, 0x03EC + to_add }, { 0x01EC + from_add, 0x012C + to_add } };
        const struct TitleCard_Config* config = zone == 1 ? card_b : zone == 2 ? card_c : zone == 4 || zone == 5 ? card_d : zone == 0 || zone == 3 ? card_a : card_e;
        const struct TitleCard_Item* item = titlecard_item;

        // Create card objects
        for (int i = 0; i < 4; i++, config++, item++) {
            if (i > 0) {
                a1 = Sst_AllocateAfter(a1);
                if (a1 == NULL)
                    break;
            }
            // Write object info from configuration
            Own_TitleCard *scratch_a1 = (Own_TitleCard *)a1->own.u8;
            a1->code = Obj_TitleCard;
            a1->x_pos.f.u = config->x0;
            scratch_a1->final_x = config->x0;
            scratch_a1->main_x = config->x1;
            a1->y_pos.f.u = item->y;
            a1->routine = item->routine;
            if ((d0 = item->frame) == 0)
                d0 = d2;

            // Initialize object graphics (a frame past the mapping's twelve is nothing to draw; the act is not added to the ACT frame, as Sonic 1 does)
            a1->mapping_frame = d0 < 12 ? d0 : 0xFF;
            a1->mappings = Mappings_TitleCard;
            a1->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Title_Card);
            a1->width_pixels = 0;
            a1->render_flags = 0;
            a1->priority = 0 * 0x80;
            a1->anim_frame_timer = 60;
        }
    }
        // Fallthrough
    case 2: // Moving to on-screen position
        // Move
        if (self->x_pos.f.u > own->main_x)
            self->x_pos.f.u -= 16;
        else if (self->x_pos.f.u < own->main_x)
            self->x_pos.f.u += 16;

        // Draw
        if (self->mapping_frame != 0xFF && self->x_pos.f.u >= 0 && self->x_pos.f.u < (0x200 + SCREEN_WIDEADD))
            Sst_Draw(self);
        break;
    case 4: // Moving off-screen
    case 6:
        // Wait for timer to expire
        if (self->anim_frame_timer) {
            self->anim_frame_timer--;
            if (self->mapping_frame != 0xFF)
                Sst_Draw(self);
            break;
        }

        // Delete and/or load level art once off-screen
        if (!(self->render_flags & SST_RENDER_ON_SCREEN) || self->x_pos.f.u == own->final_x) {
            if (self->routine == 4) {
                AddPLC(PlcId_Explode);
                AddPLC(TitleCard_AnimalsPlc(LEVEL_ZONE(level_id)));
            }
            Sst_Delete(self);
            break;
        }

        // Move
        if (self->x_pos.f.u >= own->final_x)
            self->x_pos.f.u -= 16;
        else
            self->x_pos.f.u += 16;

        // Draw
        if (self->mapping_frame != 0xFF && self->x_pos.f.u >= 0 && self->x_pos.f.u < (0x200 + SCREEN_WIDEADD))
            Sst_Draw(self);
        break;
    }
}
