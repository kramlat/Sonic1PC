// Neo Green Hill's own objects, the Simon Wai prototype's: the arrow shooter (22, Obj_0x22_Arrow_Shooter at loc_19660) and the leaves (2C, Obj2C): an invisible box that makes leaves fly off
// when a fast character hits it, and the leaves themselves. Each is a port of its routine in the prototype's disassembly.
#include "Object/NGHZObjects.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "MathUtil.h"
#include "Object.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Object/DebugMarkers.h"

#include "Macros.h"

#include "Resource/Animation/ArrowShooter.h"
#include "Resource/Mappings/ArrowShooter.h"
#include "Resource/Mappings/NGHZLeaf.h"
#include "Resource/Mappings/SSZSwing.h"

// What the object does for each of the two characters: `chr` is NULL for a Tails who is not there
#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Object *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

void Obj_SwingingPlatform(Object *obj);

#define ARTTILE_NGHZ_LEAVES 0x410 // ($8200 in the zone's art list)
#define ARTTILE_NGHZ_ARROW  0x417 // ($82E0)

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 22: the arrow shooter, a wall tube that fires an arrow along the floor when a character is close, and draws back when he is not
// ---------------------------------------------------------------------------------------------------------------------------------------
enum { ArrowRoutine_Init = 0, ArrowRoutine_Wait = 2, ArrowRoutine_Fire = 4, ArrowRoutine_ArrowInit = 6, ArrowRoutine_Arrow = 8 };

// Within $40 pixels of the shooter along x
static bool ArrowShooter_Near(const Object *obj, const Object *chr) {
    int16_t d0 = (int16_t)(obj->pos.l.x.f.u - chr->pos.l.x.f.u);
    if (d0 < 0)
        d0 = (int16_t)-d0;
    return d0 < 0x40;
}

void Obj_ArrowShooter(Object *obj) {
    switch (obj->routine) {
    case ArrowRoutine_Init:
        obj->routine += 2;
        obj->mappings = Mappings_ArrowShooter;
        obj->tile = TILE_MAP(0, 0, 0, 0, ARTTILE_NGHZ_ARROW);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 3;
        obj->width_pixels = 0x10;
        obj->frame = 1;
        obj->scratch.u8[0] &= 0xF;
        // Fallthrough
    case ArrowRoutine_Wait:
        if (obj->anim != 2) { // (not already firing)
            uint8_t d2 = ArrowShooter_Near(obj, player);
            if (TAILS_OBJ->type != 0 && ArrowShooter_Near(obj, TAILS_OBJ))
                d2 = 1;
            if (d2 == 0 && obj->anim != 0)
                d2 = 2; // (draws back after he has left)
            obj->anim = d2;
        }
        AnimateSprite(obj, Animation_ArrowShooter);
        RememberState(obj);
        break;
    case ArrowRoutine_Fire: {
        Object *arrow = FindFreeObj();
        if (arrow != NULL) {
            arrow->type = obj->type;
            arrow->routine = 6;
            arrow->mappings = obj->mappings;
            arrow->tile = obj->tile;
            arrow->pos.l.x.f.u = obj->pos.l.x.f.u;
            arrow->pos.l.y.f.u = obj->pos.l.y.f.u;
            arrow->render.b = obj->render.b;
            arrow->status.b = obj->status.b;
            PlaySound(sfx_PreArrowFiring); // (the alpha's $DB, as the arrow is made; the arrow has its own sound as well)
        }
        obj->routine -= 2;
        AnimateSprite(obj, Animation_ArrowShooter);
        RememberState(obj);
        break;
    }
    case ArrowRoutine_ArrowInit:
        obj->routine += 2;
        obj->y_rad = 8;
        obj->x_rad = 0x10;
        obj->priority = 4;
        obj->col_type = 0x9B;
        obj->width_pixels = 8;
        obj->frame = 0;
        obj->xsp = obj->status.o.f.x_flip ? -0x400 : 0x400;
        PlaySound(sfx_Fireball);
        // Fallthrough
    default: // the arrow flies until it meets a wall
        SpeedToPos(obj);
        if (!obj->status.o.f.x_flip) {
            if (ObjHitWallLeft(obj, -8) < 0) {
                ObjectDelete(obj);
                return;
            }
        } else if (ObjHitWallRight(obj, 8) < 0) {
            ObjectDelete(obj);
            return;
        }
        RememberState(obj);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 2C: the leaves. The box (routines 0 and 2) is a touch box (col $D6, $D4, $D5 by subtype) that the touch response marks for each character that is in it (bit 0 Sonic, bit 1 Tails in
// col_property); the leaves (routine 4) are four of the same id that drift away
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: which box
    uint8_t pad0[5];
    uint16_t touched; // 0x2E: (a box's) when it was first touched: the characters take their turns, 16 frames apart, from then on
    int32_t x;        // 0x30: the position of a leaf, 16.16
    int32_t y;        // 0x34
    uint8_t spin;     // 0x38: how fast a leaf turns round its centre
    uint8_t pad1[3];
} Scratch_Leaves;

// byte_1A0D8
static const uint8_t leaves_collision[3] = { 0xD6, 0xD4, 0xD5 };
// word_1A224: x and y speed of each leaf
static const int16_t leaves_speeds[4][2] = { { -0x80, -0x80 }, { 0xC0, -0x40 }, { -0xC0, 0x40 }, { 0x80, 0x80 } };

// (false when the character is too slow: the box then forgets when it was touched)
static bool Leaves_Create(Object *obj, const Object *chr) {
    int16_t d0 = chr->xsp < 0 ? (int16_t)-chr->xsp : chr->xsp;
    if (d0 < 0x200) {
        d0 = chr->ysp < 0 ? (int16_t)-chr->ysp : chr->ysp;
        if (d0 < 0x200)
            return false;
    }
    for (int i = 0; i < 4; i++) {
        Object *leaf = FindFreeObj();
        if (leaf == NULL)
            break;
        Scratch_Leaves *ls = (Scratch_Leaves *)&leaf->scratch;
        leaf->type = obj->type;
        leaf->routine = 4;
        const uint32_t random = RandomNumber();
        leaf->pos.l.x.f.u = (int16_t)(chr->pos.l.x.f.u + (int16_t)((random & 0xF) - 8));
        leaf->pos.l.y.f.u = (int16_t)(chr->pos.l.y.f.u + (int16_t)(((random >> 16) & 0xF) - 8));
        leaf->xsp = leaves_speeds[i][0];
        leaf->ysp = leaves_speeds[i][1];
        if (chr->status.p.f.x_flip)
            leaf->xsp = (int16_t)-leaf->xsp;
        ls->x = (int32_t)leaf->pos.l.x.f.u << 16;
        ls->y = (int32_t)leaf->pos.l.y.f.u << 16;
        leaf->frame = (uint8_t)(((random >> 16) & 0xF) & 1);
        leaf->mappings = Mappings_NGHZLeaf;
        leaf->tile = TILE_MAP(1, 3, 0, 0, ARTTILE_NGHZ_LEAVES);
        leaf->render.b = 0;
        leaf->render.f.level_fg = true;
        leaf->width_pixels = 8;
        leaf->priority = 1;
        ls->spin = 4;
    }
    PlaySound(sfx_NGHZLeaves);
    return true;
}

void Obj_NGHZLeaves(Object *obj) {
    Scratch_Leaves *scratch = (Scratch_Leaves *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->col_type = leaves_collision[scratch->subtype % 3];
        obj->width_pixels = 0x80;
        obj->priority = 4;
        obj->frame = scratch->subtype;
        // Fallthrough
    case 2:
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj); // (the prototype does not forget the mark here: it stays gone)
            return;
        }
        obj->render.f.on_screen = true; // (the box is never drawn: the touch response needs to find it "on screen" all the same)
        {
            int16_t box_w, box_h;
            if (DebugMarkers_TouchBox(obj->col_type, &box_w, &box_h))
                DebugMarkers_Show(obj, box_w, box_h); // (the debug cheat shows its corners)
        }
        if (obj->col_property == 0) {
            scratch->touched = 0;
            return;
        }
        {
            // Sonic's turn comes on the first touch and then every 16 frames from it, Tails's 8 frames after (the alpha's Timer_frames word is kept at $2E)
            uint16_t phase = 0;
            bool sonic_turn = true;
            if (scratch->touched != 0) {
                phase = (uint16_t)((scratch->touched + (uint8_t)frame_count) & 0xF);
                sonic_turn = phase == 0;
            }
            if (sonic_turn) {
                if (obj->col_property & 1) {
                    obj->col_property &= (uint8_t)~1;
                    if (!Leaves_Create(obj, player))
                        scratch->touched = 0;
                    if (scratch->touched == 0)
                        scratch->touched = (uint16_t)frame_count;
                }
            } else if (((phase + 8) & 0xF) == 0) {
                if (obj->col_property & 2) {
                    obj->col_property &= (uint8_t)~2;
                    if (TAILS_OBJ->type != 0 && !Leaves_Create(obj, TAILS_OBJ))
                        scratch->touched = 0;
                    if (scratch->touched == 0)
                        scratch->touched = (uint16_t)frame_count;
                }
            }
        }
        obj->col_property = 0;
        break;
    default: { // a leaf
        obj->angle = (uint8_t)(obj->angle + scratch->spin);
        uint8_t d0 = (uint8_t)(scratch->spin + frame_count);
        if ((d0 & 0x1F) == 0 && (((obj - objects) ^ 0x7F) & 1))
            scratch->spin = (uint8_t)-scratch->spin;
        scratch->x += (int32_t)obj->xsp << 8;
        scratch->y += (int32_t)obj->ysp << 8;
        obj->ysp = (int16_t)(obj->ysp + (((uint16_t)scratch->y & 3) + 4)); // (the fraction of the height, as the prototype adds it)
        int16_t sin, cos;
        CalcSine(obj->angle, &sin, &cos);
        obj->pos.l.x.f.u = (int16_t)((sin >> 6) + (int16_t)(scratch->x >> 16));
        obj->pos.l.y.f.u = (int16_t)((cos >> 6) + (int16_t)(scratch->y >> 16));
        if (--obj->frame_time.b < 0) {
            obj->frame_time.b = 0xB;
            obj->frame ^= 2;
        }
        if (!obj->render.f.on_screen)
            ObjectDelete(obj);
        else
            DisplaySprite(obj);
        break;
    }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 15: the swinging platform of Neo Green Hill (Obj_0x15_Swing_Platform, loc_85F8): a platform on a chain that swings on the first oscillator. The chain is an object of its own, drawn
// as sub-sprites (a hub, then a link every 16 pixels). With subtype bit 7 the platform lets go of the chain when it is stood on at the bottom of its swing, and becomes a raft (routine $C) that
// falls to the water, floats on it and drifts to the right until it meets a wall. Only the plain swing of the subtype's bits 4-6 (0), the two that hold one side (1 and 3) and the one that
// hangs still (2) are here; the prototype's 4 (a spiked ball that falls at Sonic) is not used by Neo Green Hill.
// ---------------------------------------------------------------------------------------------------------------------------------------
#include "Game.h"
#include "Object/Sonic.h"
#include "Solid.h"
#include "Oscillatory Routines.h"

#include "Resource/Mappings/DHZSwing.h"
#include "Resource/Mappings/NGHZSwing.h"
#include "Resource/Mappings/OOZSwing.h"

const void *DHZSwingMappings(void) {
    return Mappings_DHZSwing;
}

extern Oscillatory oscillatory;

typedef struct {
    uint8_t subtype;  // 0x28: bits 4-6 how it swings (the length, bits 0-3, is only used at the start)
    uint8_t pad0[7];
    uint8_t chain;    // 0x30: the slot of the chain's object
    uint8_t pad1[3];
    uint8_t started;  // 0x34: the spiked kind has been set off
    uint8_t angle_low;// 0x35: the low byte of its angle (the high byte is the object's own)
    int16_t wait;     // 0x36: frames to wait at the end of a swing (the spiked kind)
    int16_t base_y;   // 0x38: where it hangs from (and, once a raft, where it floats about)
    int16_t base_x;   // 0x3A
    uint8_t radius;   // 0x3C: from the hub to the platform
    uint8_t up;       // 0x3D: the spiked kind is swinging up
    int16_t accel;    // 0x3E: its angle's speed
} Scratch_NGHZSwing;

enum { SwingRoutine_Init = 0, SwingRoutine_Swing = 2, SwingRoutine_SwingLoose = 6, SwingRoutine_Empty = 8, SwingRoutine_Fall = 0xA, SwingRoutine_Raft = 0xC };

#define SWING_OSC(i) ((uint8_t)(oscillatory.state[(i)][0] >> 8))

// loc_885E: the spiked platform's swing: it waits for Sonic to come within $20 pixels of where it hangs, then swings between the left ($80) and the bottom ($40) with a speed that grows and shrinks by 8 a frame, and waits
// $3C frames at each end. Its angle is a word (the object's own byte is the high one). Returns the angle
static uint8_t Swing_Spiked(Object *obj, Scratch_NGHZSwing *scratch) {
    if (scratch->wait != 0) {
        scratch->wait--;
        return obj->angle;
    }
    if (!scratch->started) {
        if ((uint16_t)(player->pos.l.x.f.u - scratch->base_x + 0x20) >= 0x40 || debug_use)
            return obj->angle;
        scratch->started = 1;
    }
    uint16_t angle = (uint16_t)((obj->angle << 8) | scratch->angle_low);
    if (scratch->up) {
        scratch->accel += 8;
        angle = (uint16_t)(angle + scratch->accel);
        if (scratch->accel == 0x200) {
            scratch->accel = 0;
            angle = 0x8000;
            scratch->up = 0;
            scratch->wait = 0x3C;
        }
    } else {
        scratch->accel -= 8;
        angle = (uint16_t)(angle + scratch->accel);
        if (scratch->accel == -0x200) {
            scratch->accel = 0;
            angle = 0x4000;
            scratch->up = 1;
            scratch->wait = 0x3C;
        }
    }
    obj->angle = (uint8_t)(angle >> 8);
    scratch->angle_low = (uint8_t)angle;
    return obj->angle;
}

// loc_8784: where the platform and the chain are, from the swing's angle
static void Swing_Move(Object *obj, Scratch_NGHZSwing *scratch) {
    uint8_t d0 = SWING_OSC(6); // (Oscillating_Data+$18)
    switch (scratch->subtype & 0x70) {
    case 0x10: // swings to one side only
        if (d0 == 0x3F)
            PlaySound(sfx_PlatformKnock); // (the alpha's $D7: the knock of a cradle's end when it comes to rest)
        if (d0 < 0x40)
            d0 = 0x40;
        break;
    case 0x20: // hangs straight down, and is not moved
        return;
    case 0x30:
        if (d0 == 0x41)
            PlaySound(sfx_PlatformKnock);
        if (d0 >= 0x40)
            d0 = 0x40;
        break;
    case 0x40: // (the spiked platform: its own angle)
        d0 = Swing_Spiked(obj, scratch);
        break;
    default:
        break;
    }
    if (obj->status.o.f.x_flip)
        d0 = (uint8_t)(0x80 - d0);

    int16_t sin, cos;
    CalcSine(d0, &sin, &cos);
    obj->pos.l.y.f.u = (int16_t)(scratch->base_y + ((sin * scratch->radius) >> 8));
    obj->pos.l.x.f.u = (int16_t)(scratch->base_x + ((cos * scratch->radius) >> 8));

    Object *chain = &objects[scratch->chain];
    if (scratch->chain == 0 || chain->type != obj->type || chain == obj)
        return;
    // each link 16 pixels further along (as the 16.16 steps of the prototype: sin * 16 / 256 pixels)
    const int32_t step_y = (int32_t)(int16_t)(sin << 4) << 8, step_x = (int32_t)(int16_t)(cos << 4) << 8;
    int32_t y = 0, x = 0;
    for (int i = 0; i < chain->child_count; i++) {
        chain->children[i].y = (int16_t)((y >> 16) + scratch->base_y);
        chain->children[i].x = (int16_t)((x >> 16) + scratch->base_x);
        y += step_y;
        x += step_x;
    }
    chain->pos.l.y.f.u = (int16_t)((y >> 16) + scratch->base_y);
    chain->pos.l.x.f.u = (int16_t)((x >> 16) + scratch->base_x);
}

static void Swing_Platform(Object *obj, int16_t old_x) {
    FOR_EACH_CHARACTER(chr, who)
        Solid_Platform(obj, chr, who, obj->width_pixels, (int16_t)(obj->y_rad + 1), old_x);
}

static void Swing_Display(Object *obj, Scratch_NGHZSwing *scratch) {
    if (IS_OFFSCREEN(scratch->base_x)) {
        ObjectDelete(&objects[scratch->chain]); // (and neither forgets that it was loaded: the prototype leaves its mark)
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

void Obj_NGHZSwing(Object *obj) {
    Scratch_NGHZSwing *scratch = (Scratch_NGHZSwing *)&obj->scratch;

    if (obj->render.f.multi_sprite) { // the chain
        DisplaySprite(obj);
        return;
    }

    switch (obj->routine) {
    case SwingRoutine_Init: {
        obj->routine += 2;
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->priority = 3;
        if (LEVEL_ZONE(level_id) == ZoneId_ARZ) {
            obj->mappings = Mappings_NGHZSwing;
            obj->tile = TILE_MAP(0, 0, 0, 0, 0);
            obj->width_pixels = 0x20;
            obj->y_rad = 8;
        } else if (LEVEL_ZONE(level_id) == ZoneId_SSZ) { // Sand Shower (the staged objpatch-swing.asm: drawn from the level's own tiles, $20 wide, $8 high: the platform is tile $264, the links $260 and the fulcrum $25C, see the importer)
            obj->mappings = Mappings_SSZSwing;
            obj->tile = TILE_MAP(0, 0, 0, 0, 0);
            obj->width_pixels = 0x20;
            obj->y_rad = 8;
        } else if (LEVEL_ZONE(level_id) == ZoneId_MCZ) { // Dust Hill
            obj->mappings = Mappings_DHZSwing;
            obj->tile = TILE_MAP(0, 0, 0, 0, 0);
            obj->width_pixels = 0x18;
            obj->y_rad = 8;
        } else { // Oil Ocean
            obj->mappings = Mappings_OOZSwing;
            obj->tile = TILE_MAP(0, 2, 0, 0, 0x3E3);
            obj->width_pixels = 0x20;
            obj->y_rad = 0x10;
        }
        scratch->base_y = obj->pos.l.y.f.u;
        scratch->base_x = obj->pos.l.x.f.u;
        if (scratch->subtype & 0x80)
            obj->routine += 4;
        const int links = scratch->subtype & 0xF;
        scratch->radius = (uint8_t)(links * 16 + 8);
        int16_t y = obj->pos.l.y.f.u;
        Object *chain = FindNextFreeObj(obj + 1);
        if (chain != NULL) {
            chain->type = obj->type;
            chain->mappings = obj->mappings;
            chain->tile = obj->tile;
            chain->render.b = 4;
            chain->render.f.multi_sprite = true;
            chain->render.f.explicit_height = true;
            chain->width_pixels = 0x48;
            chain->y_rad = 0x50;
            chain->priority = 2;
            chain->child_count = (uint8_t)(links < OBJECT_CHILDREN ? links : OBJECT_CHILDREN);
            for (int i = 0; i < chain->child_count; i++) {
                chain->children[i].x = obj->pos.l.x.f.u;
                chain->children[i].y = y;
                chain->children[i].frame = (i == 0) ? 2 : 1; // (the hub, then the links)
                y += 0x10;
            }
            chain->frame = 1;
            chain->pos.l.x.f.u = obj->pos.l.x.f.u;
            chain->pos.l.y.f.u = y;
            scratch->chain = (uint8_t)(chain - objects);
        }
        obj->pos.l.y.f.u = (int16_t)(y + 8);
        obj->angle = 0x80;
        scratch->subtype &= 0x70;
    }
        // Fallthrough
    case SwingRoutine_Swing: {
        const int16_t old_x = obj->pos.l.x.f.u;
        Swing_Move(obj, scratch);
        Swing_Platform(obj, old_x);
        Swing_Display(obj, scratch);
        break;
    }
    case SwingRoutine_SwingLoose: {
        const int16_t old_x = obj->pos.l.x.f.u;
        Swing_Move(obj, scratch);
        Swing_Platform(obj, old_x);
        if ((obj->status.b & 0x18) && SWING_OSC(6) == 0) { // stood on, at the end of the swing: it lets go of the chain
            Object *raft = FindNextFreeObj(obj + 1);
            if (raft != NULL) {
                *raft = *obj;
                raft->routine = (uint8_t)(LEVEL_ZONE(level_id) == ZoneId_ARZ ? SwingRoutine_Raft : SwingRoutine_Fall);
                raft->xsp = obj->status.o.f.x_flip ? -0x200 : 0x200;
                raft->status.b |= 2; // (falling)
                const uint8_t old_slot = (uint8_t)(obj - objects), new_slot = (uint8_t)(raft - objects);
                Scratch_Sonic *sonic = (Scratch_Sonic *)&player->scratch;
                if (sonic->standing_obj == old_slot)
                    sonic->standing_obj = new_slot;
                if (TAILS_OBJ->type != 0) {
                    Scratch_Sonic *tails = (Scratch_Sonic *)&TAILS_OBJ->scratch;
                    if (tails->standing_obj == old_slot)
                        tails->standing_obj = new_slot;
                }
            }
            obj->frame = 3; // (the chain's end is all that is left)
            obj->routine += 2;
            obj->status.b &= 0xE7;
        }
        Swing_Display(obj, scratch);
        break;
    }
    case SwingRoutine_Empty:
        Swing_Move(obj, scratch);
        Swing_Display(obj, scratch);
        break;
    case SwingRoutine_Fall: { // (a platform that has let go, in the other zones: it falls to $720 and bobs there)
        const int16_t old_x = obj->pos.l.x.f.u;
        if (obj->status.b & 2) {
            SpeedToPos(obj);
            obj->ysp += 0x18;
            if ((uint16_t)obj->pos.l.y.f.u >= 0x720) {
                obj->pos.l.y.f.u = 0x720;
                obj->status.b &= (uint8_t)~2;
                obj->xsp = 0;
                obj->ysp = 0;
                scratch->base_y = 0x720;
            }
        } else {
            obj->pos.l.y.f.u = (int16_t)(scratch->base_y + (SWING_OSC(5) >> 1));
        }
        Swing_Platform(obj, old_x);
        RememberState(obj);
        break;
    }
    default: { // the raft
        const int16_t old_x = obj->pos.l.x.f.u;
        SpeedToPos(obj);
        if (obj->status.b & 2) { // falling to the water
            obj->ysp += 0x18;
            if ((uint16_t)wtr_pos2 <= (uint16_t)obj->pos.l.y.f.u) {
                obj->pos.l.y.f.u = wtr_pos2;
                scratch->base_y = wtr_pos2;
                obj->status.b &= (uint8_t)~2;
                obj->xsp = 0x100;
                obj->ysp = 0;
            }
        } else {
            obj->pos.l.y.f.u = (int16_t)(scratch->base_y + (SWING_OSC(5) >> 1));
            if (obj->xsp != 0) {
                const int16_t d1 = ObjHitWallRight(obj, obj->width_pixels);
                if (d1 < 0) {
                    obj->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + d1);
                    obj->xsp = 0;
                }
            }
        }
        Swing_Platform(obj, old_x);
        RememberState(obj);
        break;
    }
    }
}

void Obj_SwingDispatch(Object *obj) {
    const int zone = LEVEL_ZONE(level_id);
    if (zone == ZoneId_ARZ || zone == ZoneId_MCZ || zone == ZoneId_OOZ || zone == ZoneId_SSZ)
        Obj_NGHZSwing(obj);
    else
        Obj_SwingingPlatform(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 83 (the alpha's): three platforms that turn round a middle, on arms of chain (Neo Green Hill's second act). The first object is the middle's leader and one of the platforms; it makes an object for the chain
// (the links of all three arms, eight of them, as the object's child sprites) and two more for the other platforms (routine 4), and moves all of them each frame by the sine of its angle: a platform is 4 steps of the
// angle's sine/cosine ($10 pixels at most) from the middle, the arms' links at steps 1-3, the second arm turned $55 (120 degrees) on from the first and the third as much back. The subtype's high nibble is
// how fast it turns (signed, times 8 in 1/256 of a turn a frame), and the object's flips are where it starts (0, a quarter, a half, three quarters).
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0;      // 0x29
    int16_t old_x;     // 0x2C: (a follower's) where it was
    int16_t speed;     // 0x2E: how fast it turns
    int16_t centre_x;  // 0x30
    int16_t centre_y;  // 0x32
    uint8_t chain;     // 0x34: the slot of the chain's object
    uint8_t second;    // 0x38: the slot of the second platform
    uint8_t third;     // 0x3C: the slot of the third
    uint8_t pad1;
    uint16_t angle;    // (the angle, in 1/256 of 1/256 of a turn: the leader's own 0x26 and 0x27)
} Scratch_Rotator;

enum { RotatorRoutine_Init = 0, RotatorRoutine_Leader = 2, RotatorRoutine_Follower = 4 };

static void Rotator_Platform(Object *obj, int16_t old_x) {
    FOR_EACH_CHARACTER(chr, who)
        Solid_Platform(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), 9, old_x);
}

// The alpha's loop of positions (Offset_0x01F4B6): `count` steps of the arm from `first`, stored as child sprites from `*child`
static void Rotator_Links(Object *chain, int *child, int16_t centre_x, int16_t centre_y, int16_t sin, int16_t cos, int first, int count) {
    for (int k = first; k < first + count; k++, (*child)++) {
        chain->children[*child].x = (int16_t)(centre_x + ((k * cos) >> 4));
        chain->children[*child].y = (int16_t)(centre_y + ((k * sin) >> 4));
    }
}

void Obj_NGHZRotator(Object *obj) {
    Scratch_Rotator *scratch = (Scratch_Rotator *)&obj->scratch;

    if (obj->render.f.multi_sprite) { // the chain
        DisplaySprite(obj);
        return;
    }

    switch (obj->routine) {
    case RotatorRoutine_Init: {
        obj->routine += 2;
        obj->mappings = LEVEL_ZONE(level_id) == ZoneId_SSZ ? Mappings_SSZSwing : Mappings_NGHZSwing; // (Sand Shower's: the staged objpatch83.asm, the same mapping as its swing, from the level's own tiles)
        obj->tile = TILE_MAP(0, 0, 0, 0, 0);
        obj->render.b = SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->width_pixels = 0x20;
        scratch->centre_x = obj->pos.l.x.f.u;
        scratch->centre_y = obj->pos.l.y.f.u;
        scratch->speed = (int16_t)((int16_t)(int8_t)(scratch->subtype & 0xF0) << 3);
        scratch->angle = (uint16_t)((((obj->status.b >> 2) | (obj->status.b << 6)) & 0xC0) << 8); // (ror.b #2, and #$C0)
        Object *chain = FindNextFreeObj(obj + 1);
        if (chain != NULL) {
            chain->type = obj->type;
            chain->mappings = obj->mappings;
            chain->tile = obj->tile;
            chain->render.b = SPRITE_CAM_FIELD;
            chain->render.f.multi_sprite = true;
            chain->render.f.explicit_height = true;
            chain->y_rad = 0x40;
            chain->width_pixels = 0xFF;
            chain->child_count = 8;
            for (int i = 0; i < 8; i++)
                chain->children[i].frame = 1;
            chain->pos.l.x.f.u = obj->pos.l.x.f.u;
            chain->pos.l.y.f.u = obj->pos.l.y.f.u;
            scratch->chain = (uint8_t)(chain - objects);
            uint8_t *slots[2] = { &scratch->second, &scratch->third };
            for (int i = 0; i < 2; i++) {
                Object *follower = FindNextFreeObj(obj + 1);
                if (follower == NULL)
                    break;
                follower->type = obj->type;
                follower->routine = RotatorRoutine_Follower;
                follower->mappings = obj->mappings;
                follower->tile = obj->tile;
                follower->render.b = SPRITE_CAM_FIELD;
                follower->priority = 4;
                follower->width_pixels = 0x20;
                Scratch_Rotator *fs = (Scratch_Rotator *)&follower->scratch;
                fs->centre_x = obj->pos.l.x.f.u;
                fs->centre_y = obj->pos.l.y.f.u;
                fs->old_x = obj->pos.l.x.f.u;
                *slots[i] = (uint8_t)(follower - objects);
            }
        }
    }
        // Fallthrough
    case RotatorRoutine_Leader: {
        const int16_t old_x = obj->pos.l.x.f.u;
        scratch->angle = (uint16_t)(scratch->angle + scratch->speed);
        Object *chain = &objects[scratch->chain];
        int child = 0;
        int16_t sin, cos;

        // the first arm: the chain's own place is its first step, two links at steps 2 and 3, and the platform at 4
        CalcSine((uint8_t)(scratch->angle >> 8), &sin, &cos);
        chain->pos.l.x.f.u = (int16_t)(scratch->centre_x + (cos >> 4));
        chain->pos.l.y.f.u = (int16_t)(scratch->centre_y + (sin >> 4));
        Rotator_Links(chain, &child, scratch->centre_x, scratch->centre_y, sin, cos, 2, 2);
        obj->pos.l.x.f.u = (int16_t)(scratch->centre_x + ((4 * cos) >> 4));
        obj->pos.l.y.f.u = (int16_t)(scratch->centre_y + ((4 * sin) >> 4));

        // the second arm, $55 on, and the third, $55 back: three links at steps 1-3 each and a platform at 4
        const int8_t turns[2] = { 0x55, -0x55 };
        const uint8_t followers[2] = { scratch->second, scratch->third };
        for (int arm = 0; arm < 2; arm++) {
            CalcSine((uint8_t)((scratch->angle >> 8) + turns[arm]), &sin, &cos);
            Rotator_Links(chain, &child, scratch->centre_x, scratch->centre_y, sin, cos, 1, 3);
            Object *follower = &objects[followers[arm]];
            follower->pos.l.x.f.u = (int16_t)(scratch->centre_x + ((4 * cos) >> 4));
            follower->pos.l.y.f.u = (int16_t)(scratch->centre_y + ((4 * sin) >> 4));
        }

        Rotator_Platform(obj, old_x);
        if (IS_OFFSCREEN(scratch->centre_x)) {
            ObjectDelete(chain);
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }

    case RotatorRoutine_Follower:
        Rotator_Platform(obj, scratch->old_x);
        scratch->old_x = obj->pos.l.x.f.u;
        if (IS_OFFSCREEN(scratch->centre_x)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
}
