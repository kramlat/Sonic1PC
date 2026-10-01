#include "ScrapEggman.h"

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

static void SEgg_Setup(Object *obj, uint8_t routine, uint8_t anim, uint8_t priority, const void *mappings,
                       uint16_t tile, uint8_t width) {
    obj->routine_sec = 0;
    obj->routine = routine;
    obj->anim = anim;
    obj->priority = priority;
    // Force AnimateSprite's reset guard on the first call (see BossSpringYard.c).
    obj->prev_anim = (uint8_t)(anim + 1);
    obj->anim_frame = 0;
    obj->frame_time.w = 0;
    obj->frame = 0;
    obj->mappings = mappings;
    obj->tile = tile;
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->render.f.on_screen = true;
    obj->width_pixels = width;
}

static void SEgg_Eggman(Object *obj, Scratch_ScrapEggman *scratch);

static void SEgg_Main(Object *obj, Scratch_ScrapEggman *scratch) {
    memset(&obj->scratch, 0, sizeof(obj->scratch));
    uint8_t self_index = (uint8_t)(obj - objects);

    obj->pos.l.x.f.u = SBZ2_X + 0x110;
    obj->pos.l.y.f.u = SBZ2_Y + 0x94;
    obj->col_type = 0x0F;    // col_48x48 | col_boss
    obj->col_property = 16;  // obBossHits -- set to 16 despite being unhittable
    obj->status.o.f.x_flip = false;
    SEgg_Setup(obj, 2, 0, 3, Mappings_ScrapEggman, TILE_MAP(0, 0, 0, 0, ArtTile_Eggman), 64 / 2);

    // The floor switch Eggman jumps on.
    Object *sw = FindNextFreeObj(obj);
    if (sw != NULL) {
        memset(&sw->scratch, 0, sizeof(sw->scratch));
        sw->type = ObjId_ScrapEggman;
        sw->pos.l.x.f.u = SBZ2_X + 0xE0;
        sw->pos.l.y.f.u = SBZ2_Y + 0xAC;
        SEgg_Setup(sw, 4, 0, 3, Mappings_Button, TILE_MAP(0, 0, 0, 0, ArtTile_Eggman_Button), 32 / 2);
        ((Scratch_ScrapEggman *)&sw->scratch)->parent_index = self_index;
    }

    SEgg_Eggman(obj, scratch);
}

// Sets the floor's block manager breaking: the first FalseFloor object that is still the manager (routine 2).
// The real ASM takes the first object with the false-floor ID, which only works because the manager happens to
// sit in the lowest slot; here the blocks can land in lower slots, so look for the manager explicitly.
static bool SEgg_FindFloor(void) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        Object *floor = &level_objects[i];
        if (floor->type == ObjId_FalseFloor && floor->routine == 2) {
            ((Scratch_FalseFloor *)&floor->scratch)->cmd = CMD_GO;
            return true;
        }
    }
    return false;
}

static void SEgg_Eggman(Object *obj, Scratch_ScrapEggman *scratch) {
    switch (obj->routine_sec) {
    case 0: // ChkSonic
        if ((uint16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u) < 128) { // Sonic within 128px
            obj->routine_sec += 2;
            scratch->timer = 180; // 3 seconds
            obj->anim = 1;        // laugh
        }
        break;
    case 2: // PreLeap
        if (--scratch->timer == 0) {
            obj->routine_sec += 2;
            obj->anim = 2; // crouch
            obj->pos.l.y.f.u += 4;
            scratch->timer = 15;
        }
        break;
    case 4: { // Leap
        if (--scratch->timer > 0)
            break;
        if (scratch->timer == 0) {
            obj->xsp = -0xFC; // leap
            obj->ysp = -0x3C0;
        }
        if (obj->pos.l.x.f.u <= SBZ2_X + 0xE2) // reached the switch
            obj->xsp = 0;                      // fall straight down
        obj->ysp = (int16_t)(obj->ysp + 0x24);
        if (obj->ysp >= 0 && (uint16_t)obj->pos.l.y.f.u >= SBZ2_Y + 0x85) {
            ((Scratch_ScrapEggman *)&obj->scratch)->cmd = CMD_SWITCH;
            if ((uint16_t)obj->pos.l.y.f.u >= SBZ2_Y + 0x8B) {
                obj->pos.l.y.f.u = SBZ2_Y + 0x8B; // snap to the floor
                obj->ysp = 0;
            }
        }
        if ((obj->xsp | obj->ysp) == 0 && SEgg_FindFloor()) { // landed: break the floor
            obj->routine_sec += 2;
            obj->anim = 1;
        }
        break;
    }
    case 6: // Move
        break;
    }
    SpeedToPos(obj);

    AnimateSprite(obj, Animation_ScrapEggman);
    DisplaySprite(obj);
}

static void SEgg_Switch(Object *obj, Scratch_ScrapEggman *scratch) {
    Object *parent = &objects[scratch->parent_index];
    if (obj->routine_sec == 0 && parent->type == obj->type &&
        ((Scratch_ScrapEggman *)&parent->scratch)->cmd == CMD_SWITCH) {
        obj->frame = 1; // pressed
        obj->routine_sec += 2;
    }
    DisplaySprite(obj);
}

void Obj_ScrapEggman(Object *obj) {
    Scratch_ScrapEggman *scratch = (Scratch_ScrapEggman *)&obj->scratch;

    switch (obj->routine) {
    case 0: SEgg_Main(obj, scratch); break; // spawned by DLE_SBZ2 with routine 0
    case 2: SEgg_Eggman(obj, scratch); break;
    case 4: SEgg_Switch(obj, scratch); break;
    }
}

// ---------------------------------------------------------------------------
// Object 83 - the false floor
// ---------------------------------------------------------------------------

static const int16_t FFloor_FragSpeed[4] = { 0x80, 0, 0x120, 0xC0 }; // Y speeds
static const int16_t FFloor_FragPos[4][2] = { { -8, -8 }, { 0x10, 0 }, { 0, 0x10 }, { 0x10, 0x10 } };

static void FFloor_Main(Object *obj, Scratch_FalseFloor *scratch) {
    memset(&obj->scratch, 0, sizeof(obj->scratch));
    obj->pos.l.x.f.u = SBZ2_X + 0x30;
    obj->pos.l.y.f.u = SBZ2_Y + 0xC0;
    obj->width_pixels = 256 / 2;
    obj->y_rad = 32 / 2;
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->render.f.on_screen = true;

    int16_t x = SBZ2_X - 0x40; // first block
    for (int i = 0; i < 8; i++, x += 0x20) {
        Object *block = FindFreeObj();
        if (block == NULL)
            break;
        scratch->blocks[i] = (uint8_t)(block - objects);
        block->type = ObjId_FalseFloor;
        block->mappings = Mappings_EggmanTrapFloor;
        block->tile = TILE_MAP(0, 2, 0, 0, ArtTile_Eggman_Trap_Floor); // | Tile_Pal3
        block->render.b = 0;
        block->render.f.align_fg = true;
        block->width_pixels = 32 / 2;
        block->y_rad = 32 / 2;
        block->priority = 3;
        block->pos.l.x.f.u = x;
        block->pos.l.y.f.u = SBZ2_Y + 0xC0;
        block->routine = 8; // FFloor_Block
    }
    obj->routine = 2; // FFloor_ChkBreak
}

// The whole floor is one solid surface that shrinks from the left as each block goes, rather than 8 separate
// collision boxes.
static void FFloor_Solid(Object *obj, Scratch_FalseFloor *scratch) {
    int16_t half = (int16_t)((8 - (int8_t)scratch->break_count) << 4); // half the width still standing
    obj->width_pixels = (uint8_t)half;
    obj->pos.l.x.f.u = (int16_t)(SBZ2_X + 0xB0 - half);
    SolidObject(obj, (uint16_t)(11 /* sonic_solid_width */ + half), 16, 17, obj->pos.l.x.f.u, NULL, NULL);
}

static void FFloor_AllGone(Object *obj) {
    obj->status.o.f.player_stand = false;
    player->status.p.f.object_stand = false;
    ObjectDelete(obj);
}

static void FFloor_Break(Object *obj, Scratch_FalseFloor *scratch) {
    uint8_t before = scratch->time_frame;
    scratch->time_frame = (uint8_t)(before - 0xE);
    if (before >= 0xE) { // no borrow: not time to break the next block yet
        FFloor_Solid(obj, scratch);
        return;
    }
    Object *block = &objects[scratch->blocks[scratch->break_count]];
    ((Scratch_FalseFloor *)&block->scratch)->cmd = CMD_GO;
    if (++scratch->break_count == 8) {
        obj->routine = 6;
        FFloor_AllGone(obj);
        return;
    }
    FFloor_Solid(obj, scratch);
}

static void FFloor_BlockBreak(Object *obj) {
    obj->routine += 2; // FFloor_Frag
    obj->width_pixels = 16 / 2;
    obj->y_rad = 16 / 2;

    // This block becomes the first fragment; 3 more are copies of it, each offset from the last.
    uint8_t frame = 1;
    Object *frag = obj;
    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            frag = FindNextFreeObj(obj);
            if (frag == NULL)
                break;
            *frag = *obj;
        }
        frag->ysp = FFloor_FragSpeed[i];
        frag->pos.l.x.f.u = (int16_t)(frag->pos.l.x.f.u + FFloor_FragPos[i][0]);
        frag->pos.l.y.f.u = (int16_t)(frag->pos.l.y.f.u + FFloor_FragPos[i][1]);
        frag->frame = frame++;
    }
    QueueSound2(sfx_WallSmash);
    DisplaySprite(obj);
}

void Obj_FalseFloor(Object *obj) {
    Scratch_FalseFloor *scratch = (Scratch_FalseFloor *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        FFloor_Main(obj, scratch);
        break;
    case 2: // ChkBreak
        if (scratch->cmd == CMD_GO) {
            scratch->break_count = 0;
            obj->routine += 2;
        }
        FFloor_Solid(obj, scratch);
        break;
    case 4: FFloor_Break(obj, scratch); break;
    case 6: FFloor_AllGone(obj); break;
    case 8: // Block
        if (scratch->cmd == CMD_GO)
            FFloor_BlockBreak(obj);
        else
            DisplaySprite(obj);
        break;
    case 0xA: // Frag
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        ObjectFall(obj);
        DisplaySprite(obj);
        break;
    }
}
