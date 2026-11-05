#include "ScrapEggman.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Resource/Animation/ScrapEggman.h"
#include "Resource/Mappings/EggmanTrapFloor.h"
#include "Resource/Mappings/ScrapEggman.h"
#include "Sound.h"

extern const uint8_t Mappings_Button[]; // From Object/Button.c

// Objects 82/83 - the SBZ2 Eggman cutscene and its false floor. Ported from s1disasm's
// "_incObj/82, 83 SBZ Eggman Cutscene and Crumbling Floor.asm".
// boss_sbz2_x = 0x2050, boss_sbz2_y = 0x510 (real s1disasm _Constants.asm).

#define SBZ2_X 0x2050
#define SBZ2_Y 0x510

#define CMD_SWITCH 0x5357 // "SW"
#define CMD_GO     0x474F // "GO"

// ---------------------------------------------------------------------------
// Object 82 - Eggman and his switch
// ---------------------------------------------------------------------------

static void SEgg_Setup(Sst *self, uint8_t routine, uint8_t anim, uint8_t priority, const void *mappings,
                       uint16_t tile, uint8_t width) {
    self->routine_secondary = 0;
    self->routine = routine;
    self->anim = anim;
    self->priority = (uint16_t)(priority * 0x80);
    // Force AnimateSprite's reset guard on the first call (see BossSpringYard.c).
    self->prev_anim = (uint8_t)(anim + 1);
    self->anim_frame = 0;
    self->anim_frame_timer = 0;
    self->mapping_frame = 0;
    self->mappings = mappings;
    self->art_tile = tile;
    self->render_flags = SST_RENDER_LEVEL;
    self->render_flags |= SST_RENDER_ON_SCREEN;
    self->width_pixels = width;
}

static void SEgg_Eggman(Sst *self, Own_ScrapEggman *own);

static void SEgg_Main(Sst *self, Own_ScrapEggman *own) {
    memset(self->own.u8, 0, sizeof(self->own));
    uint8_t self_index = (uint8_t)(self - sst);

    self->x_pos.f.u = SBZ2_X + 0x110;
    self->y_pos.f.u = SBZ2_Y + 0x94;
    self->collision_flags = 0x0F;    // col_48x48 | col_boss
    self->collision_property = 16;  // obBossHits -- set to 16 despite being unhittable
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
    SEgg_Setup(self, 2, 0, 3, Mappings_ScrapEggman, TILE_MAP(0, 0, 0, 0, ArtTile_Eggman), 64 / 2);

    // The floor switch Eggman jumps on.
    Sst *sw = Sst_AllocateAfter(self);
    if (sw != NULL) {
        memset(sw->own.u8, 0, sizeof(sw->own));
        sw->code = Obj_ScrapEggman;
        sw->x_pos.f.u = SBZ2_X + 0xE0;
        sw->y_pos.f.u = SBZ2_Y + 0xAC;
        SEgg_Setup(sw, 4, 0, 3, Mappings_Button, TILE_MAP(0, 0, 0, 0, ArtTile_Eggman_Button), 32 / 2);
        ((Own_ScrapEggman *)sw->own.u8)->parent_index = self_index;
    }

    SEgg_Eggman(self, own);
}

// Sets the floor's block manager breaking: the first FalseFloor object that is still the manager (routine 2).
// The real ASM takes the first object with the false-floor ID, which only works because the manager happens to
// sit in the lowest slot; here the blocks can land in lower slots, so look for the manager explicitly.
static bool SEgg_FindFloor(void) {
    for (int i = 0; i < SST_COUNT; i++) {
        Sst *floor = &sst[i];
        if (floor->code != NULL && floor->code == Obj_FalseFloor && floor->routine == 2) {
            ((Own_FalseFloor *)floor->own.u8)->cmd = CMD_GO;
            return true;
        }
    }
    return false;
}

static void SEgg_Eggman(Sst *self, Own_ScrapEggman *own) {
    switch (self->routine_secondary) {
    case 0: // ChkSonic
        if ((uint16_t)(self->x_pos.f.u - player->x_pos.f.u) < 128) { // Sonic within 128px
            self->routine_secondary += 2;
            own->timer = 180; // 3 seconds
            self->anim = 1;        // laugh
        }
        break;
    case 2: // PreLeap
        if (--own->timer == 0) {
            self->routine_secondary += 2;
            self->anim = 2; // crouch
            self->y_pos.f.u += 4;
            own->timer = 15;
        }
        break;
    case 4: { // Leap
        if (--own->timer > 0)
            break;
        if (own->timer == 0) {
            self->x_vel = -0xFC; // leap
            self->y_vel = -0x3C0;
        }
        if (self->x_pos.f.u <= SBZ2_X + 0xE2) // reached the switch
            self->x_vel = 0;                      // fall straight down
        self->y_vel = (int16_t)(self->y_vel + 0x24);
        if (self->y_vel >= 0 && (uint16_t)self->y_pos.f.u >= SBZ2_Y + 0x85) {
            ((Own_ScrapEggman *)self->own.u8)->cmd = CMD_SWITCH;
            if ((uint16_t)self->y_pos.f.u >= SBZ2_Y + 0x8B) {
                self->y_pos.f.u = SBZ2_Y + 0x8B; // snap to the floor
                self->y_vel = 0;
            }
        }
        if ((self->x_vel | self->y_vel) == 0 && SEgg_FindFloor()) { // landed: break the floor
            self->routine_secondary += 2;
            self->anim = 1;
        }
        break;
    }
    case 6: // Move
        break;
    }
    Sst_MoveSprite2(self);

    Sst_Animate(self, Animation_ScrapEggman);
    Sst_Draw(self);
}

static void SEgg_Switch(Sst *self, Own_ScrapEggman *own) {
    Sst *parent = &sst[own->parent_index];
    if (self->routine_secondary == 0 && parent->code == self->code &&
        ((Own_ScrapEggman *)parent->own.u8)->cmd == CMD_SWITCH) {
        self->mapping_frame = 1; // pressed
        self->routine_secondary += 2;
    }
    Sst_Draw(self);
}

void Obj_ScrapEggman(Sst *self) {
    Own_ScrapEggman *own = (Own_ScrapEggman *)self->own.u8;

    switch (self->routine) {
    case 0: SEgg_Main(self, own); break; // spawned by DLE_SBZ2 with routine 0
    case 2: SEgg_Eggman(self, own); break;
    case 4: SEgg_Switch(self, own); break;
    }
}

// ---------------------------------------------------------------------------
// Object 83 - the false floor
// ---------------------------------------------------------------------------

static const int16_t FFloor_FragSpeed[4] = { 0x80, 0, 0x120, 0xC0 }; // Y speeds
static const int16_t FFloor_FragPos[4][2] = { { -8, -8 }, { 0x10, 0 }, { 0, 0x10 }, { 0x10, 0x10 } };

static void FFloor_Main(Sst *self, Own_FalseFloor *own) {
    memset(self->own.u8, 0, sizeof(self->own));
    self->x_pos.f.u = SBZ2_X + 0x30;
    self->y_pos.f.u = SBZ2_Y + 0xC0;
    self->width_pixels = 256 / 2;
    self->y_radius = 32 / 2;
    self->render_flags = SST_RENDER_LEVEL;
    self->render_flags |= SST_RENDER_ON_SCREEN;

    int16_t x = SBZ2_X - 0x40; // first block
    for (int i = 0; i < 8; i++, x += 0x20) {
        Sst *block = Sst_Allocate();
        if (block == NULL)
            break;
        own->blocks[i] = (uint8_t)(block - sst);
        block->code = Obj_FalseFloor;
        block->mappings = Mappings_EggmanTrapFloor;
        block->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Eggman_Trap_Floor); // | Tile_Pal3
        block->render_flags = SST_RENDER_LEVEL;
        block->width_pixels = 32 / 2;
        block->y_radius = 32 / 2;
        block->priority = 3 * 0x80;
        block->x_pos.f.u = x;
        block->y_pos.f.u = SBZ2_Y + 0xC0;
        block->routine = 8; // FFloor_Block
    }
    self->routine = 2; // FFloor_ChkBreak
}

// The whole floor is one solid surface that shrinks from the left as each block goes, rather than 8 separate
// collision boxes.
static void FFloor_Solid(Sst *self, Own_FalseFloor *own) {
    int16_t half = (int16_t)((8 - (int8_t)own->break_count) << 4); // half the width still standing
    self->width_pixels = (uint8_t)half;
    self->x_pos.f.u = (int16_t)(SBZ2_X + 0xB0 - half);
    SstSolidObject(self, (uint16_t)(11 /* sonic_solid_width */ + half), 16, 17, self->x_pos.f.u, NULL, NULL);
}

static void FFloor_AllGone(Sst *self) {
    self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
    player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    Sst_Delete(self);
}

static void FFloor_Break(Sst *self, Own_FalseFloor *own) {
    uint8_t before = own->time_frame;
    own->time_frame = (uint8_t)(before - 0xE);
    if (before >= 0xE) { // no borrow: not time to break the next block yet
        FFloor_Solid(self, own);
        return;
    }
    Sst *block = &sst[own->blocks[own->break_count]];
    ((Own_FalseFloor *)block->own.u8)->cmd = CMD_GO;
    if (++own->break_count == 8) {
        self->routine = 6;
        FFloor_AllGone(self);
        return;
    }
    FFloor_Solid(self, own);
}

static void FFloor_BlockBreak(Sst *self) {
    self->routine += 2; // FFloor_Frag
    self->width_pixels = 16 / 2;
    self->y_radius = 16 / 2;

    // This block becomes the first fragment; 3 more are copies of it, each offset from the last.
    uint8_t frame = 1;
    Sst *frag = self;
    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            frag = Sst_AllocateAfter(self);
            if (frag == NULL)
                break;
            *frag = *self;
        }
        frag->y_vel = FFloor_FragSpeed[i];
        frag->x_pos.f.u = (int16_t)(frag->x_pos.f.u + FFloor_FragPos[i][0]);
        frag->y_pos.f.u = (int16_t)(frag->y_pos.f.u + FFloor_FragPos[i][1]);
        frag->mapping_frame = frame++;
    }
    QueueSound2(sfx_WallSmash);
    Sst_Draw(self);
}

void Obj_FalseFloor(Sst *self) {
    Own_FalseFloor *own = (Own_FalseFloor *)self->own.u8;

    switch (self->routine) {
    case 0:
        FFloor_Main(self, own);
        break;
    case 2: // ChkBreak
        if (own->cmd == CMD_GO) {
            own->break_count = 0;
            self->routine += 2;
        }
        FFloor_Solid(self, own);
        break;
    case 4: FFloor_Break(self, own); break;
    case 6: FFloor_AllGone(self); break;
    case 8: // Block
        if (own->cmd == CMD_GO)
            FFloor_BlockBreak(self);
        else
            Sst_Draw(self);
        break;
    case 0xA: // Frag
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            Sst_Delete(self);
            return;
        }
        Sst_MoveSprite(self);
        Sst_Draw(self);
        break;
    }
}
