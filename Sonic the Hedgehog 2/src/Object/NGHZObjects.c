// Neo Green Hill's own objects, the Simon Wai prototype's: the arrow shooter (22, Obj_0x22_Arrow_Shooter at loc_19660) and the leaves (2C, Obj2C): an invisible box that makes leaves fly off
// when a fast character hits it, and the leaves themselves. Each is a port of its routine in the prototype's disassembly.
#include "Object/NGHZObjects.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "SolidSst.h"
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
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

void Obj_SwingingPlatform(Sst *self);

#define ARTTILE_NGHZ_LEAVES 0x410 // ($8200 in the zone's art list)
#define ARTTILE_NGHZ_ARROW  0x417 // ($82E0)

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 22: the arrow shooter, a wall tube that fires an arrow along the floor when a character is close, and draws back when he is not
// ---------------------------------------------------------------------------------------------------------------------------------------
enum { ArrowRoutine_Init = 0, ArrowRoutine_Wait = 2, ArrowRoutine_Fire = 4, ArrowRoutine_ArrowInit = 6, ArrowRoutine_Arrow = 8 };

// Within $40 pixels of the shooter along x
static bool ArrowShooter_Near(const Sst *self, const Sst *chr) {
    int16_t d0 = (int16_t)(self->x_pos.f.u - chr->x_pos.f.u);
    if (d0 < 0)
        d0 = (int16_t)-d0;
    return d0 < 0x40;
}

void Obj_ArrowShooter(Sst *self) {
    switch (self->routine) {
    case ArrowRoutine_Init:
        self->routine += 2;
        self->mappings = Mappings_ArrowShooter;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ARTTILE_NGHZ_ARROW);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        self->width_pixels = 0x10;
        self->mapping_frame = 1;
        self->subtype &= 0xF;
        // Fallthrough
    case ArrowRoutine_Wait:
        if (self->anim != 2) { // (not already firing)
            uint8_t d2 = ArrowShooter_Near(self, player);
            if (TAILS_OBJ->code != NULL && ArrowShooter_Near(self, TAILS_OBJ))
                d2 = 1;
            if (d2 == 0 && self->anim != 0)
                d2 = 2; // (draws back after he has left)
            self->anim = d2;
        }
        Sst_Animate(self, Animation_ArrowShooter);
        SstOnScreenTest(self);
        break;
    case ArrowRoutine_Fire: {
        Sst *arrow = Sst_Allocate();
        if (arrow != NULL) {
            arrow->code = self->code;
            arrow->routine = 6;
            arrow->mappings = self->mappings;
            arrow->art_tile = self->art_tile;
            arrow->x_pos.f.u = self->x_pos.f.u;
            arrow->y_pos.f.u = self->y_pos.f.u;
            arrow->render_flags = self->render_flags;
            arrow->status = self->status;
            PlaySound(sfx_PreArrowFiring); // (the alpha's $DB, as the arrow is made; the arrow has its own sound as well)
        }
        self->routine -= 2;
        Sst_Animate(self, Animation_ArrowShooter);
        SstOnScreenTest(self);
        break;
    }
    case ArrowRoutine_ArrowInit:
        self->routine += 2;
        self->y_radius = 8;
        self->x_radius = 0x10;
        self->priority = 4 * 0x80;
        self->collision_flags = 0x9B;
        self->width_pixels = 8;
        self->mapping_frame = 0;
        self->x_vel = ((self->status & SST_STATUS_X_FLIP) != 0) ? -0x400 : 0x400;
        PlaySound(sfx_Fireball);
        // Fallthrough
    default: // the arrow flies until it meets a wall
        Sst_MoveSprite2(self);
        if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
            if (SstHitWallLeft(self, -8) < 0) {
                Sst_Delete(self);
                return;
            }
        } else if (SstHitWallRight(self, 8) < 0) {
            Sst_Delete(self);
            return;
        }
        SstOnScreenTest(self);
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
} Own_Leaves;

// byte_1A0D8
static const uint8_t leaves_collision[3] = { 0xD6, 0xD4, 0xD5 };
// word_1A224: x and y speed of each leaf
static const int16_t leaves_speeds[4][2] = { { -0x80, -0x80 }, { 0xC0, -0x40 }, { -0xC0, 0x40 }, { 0x80, 0x80 } };

// (false when the character is too slow: the box then forgets when it was touched)
static bool Leaves_Create(Sst *self, const Sst *chr) {
    int16_t d0 = chr->x_vel < 0 ? (int16_t)-chr->x_vel : chr->x_vel;
    if (d0 < 0x200) {
        d0 = chr->y_vel < 0 ? (int16_t)-chr->y_vel : chr->y_vel;
        if (d0 < 0x200)
            return false;
    }
    for (int i = 0; i < 4; i++) {
        Sst *leaf = Sst_Allocate();
        if (leaf == NULL)
            break;
        Own_Leaves *ls = (Own_Leaves *)leaf->own.u8;
        leaf->code = self->code;
        leaf->routine = 4;
        const uint32_t random = RandomNumber();
        leaf->x_pos.f.u = (int16_t)(chr->x_pos.f.u + (int16_t)((random & 0xF) - 8));
        leaf->y_pos.f.u = (int16_t)(chr->y_pos.f.u + (int16_t)(((random >> 16) & 0xF) - 8));
        leaf->x_vel = leaves_speeds[i][0];
        leaf->y_vel = leaves_speeds[i][1];
        if (((chr->status & SST_PSTATUS_X_FLIP) != 0))
            leaf->x_vel = (int16_t)-leaf->x_vel;
        ls->x = (int32_t)leaf->x_pos.f.u << 16;
        ls->y = (int32_t)leaf->y_pos.f.u << 16;
        leaf->mapping_frame = (uint8_t)(((random >> 16) & 0xF) & 1);
        leaf->mappings = Mappings_NGHZLeaf;
        leaf->art_tile = TILE_MAP(1, 3, 0, 0, ARTTILE_NGHZ_LEAVES);
        leaf->render_flags = SST_RENDER_LEVEL;
        leaf->width_pixels = 8;
        leaf->priority = 1 * 0x80;
        ls->spin = 4;
    }
    PlaySound(sfx_NGHZLeaves);
    return true;
}

void Obj_NGHZLeaves(Sst *self) {
    Own_Leaves *own = (Own_Leaves *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->collision_flags = leaves_collision[self->subtype % 3];
        self->width_pixels = 0x80;
        self->priority = 4 * 0x80;
        self->mapping_frame = self->subtype;
        // Fallthrough
    case 2:
        if (SstOutOfRange(self->x_pos.f.u)) {
            SstLoad_Forget(self);
            Sst_Delete(self); // (the prototype does not forget the mark here: it stays gone)
            return;
        }
        self->render_flags |= SST_RENDER_ON_SCREEN; // (the box is never drawn: the touch response needs to find it "on screen" all the same)
        {
            int16_t box_w, box_h;
            if (DebugMarkers_TouchBox(self->collision_flags, &box_w, &box_h))
                DebugMarkersSst_Show(self, box_w, box_h); // (the debug cheat shows its corners)
        }
        if (self->collision_property == 0) {
            own->touched = 0;
            return;
        }
        {
            // Sonic's turn comes on the first touch and then every 16 frames from it, Tails's 8 frames after (the alpha's Timer_frames word is kept at $2E)
            uint16_t phase = 0;
            bool sonic_turn = true;
            if (own->touched != 0) {
                phase = (uint16_t)((own->touched + (uint8_t)frame_count) & 0xF);
                sonic_turn = phase == 0;
            }
            if (sonic_turn) {
                if (self->collision_property & 1) {
                    self->collision_property &= (uint8_t)~1;
                    if (!Leaves_Create(self, player))
                        own->touched = 0;
                    if (own->touched == 0)
                        own->touched = (uint16_t)frame_count;
                }
            } else if (((phase + 8) & 0xF) == 0) {
                if (self->collision_property & 2) {
                    self->collision_property &= (uint8_t)~2;
                    if (TAILS_OBJ->code != NULL && !Leaves_Create(self, TAILS_OBJ))
                        own->touched = 0;
                    if (own->touched == 0)
                        own->touched = (uint16_t)frame_count;
                }
            }
        }
        self->collision_property = 0;
        break;
    default: { // a leaf
        self->angle = (uint8_t)(self->angle + own->spin);
        uint8_t d0 = (uint8_t)(own->spin + frame_count);
        if ((d0 & 0x1F) == 0 && (((self - sst) ^ 0x7F) & 1))
            own->spin = (uint8_t)-own->spin;
        own->x += (int32_t)self->x_vel << 8;
        own->y += (int32_t)self->y_vel << 8;
        self->y_vel = (int16_t)(self->y_vel + (((uint16_t)own->y & 3) + 4)); // (the fraction of the height, as the prototype adds it)
        int16_t sin, cos;
        CalcSine(self->angle, &sin, &cos);
        self->x_pos.f.u = (int16_t)((sin >> 6) + (int16_t)(own->x >> 16));
        self->y_pos.f.u = (int16_t)((cos >> 6) + (int16_t)(own->y >> 16));
        if (--self->anim_frame_timer < 0) {
            self->anim_frame_timer = 0xB;
            self->mapping_frame ^= 2;
        }
        if (!(self->render_flags & SST_RENDER_ON_SCREEN))
            Sst_Delete(self);
        else
            Sst_Draw(self);
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
#include "SolidSst.h"
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
} Own_NGHZSwing;

enum { SwingRoutine_Init = 0, SwingRoutine_Swing = 2, SwingRoutine_SwingLoose = 6, SwingRoutine_Empty = 8, SwingRoutine_Fall = 0xA, SwingRoutine_Raft = 0xC };

#define SWING_OSC(i) ((uint8_t)(oscillatory.state[(i)][0] >> 8))

// loc_885E: the spiked platform's swing: it waits for Sonic to come within $20 pixels of where it hangs, then swings between the left ($80) and the bottom ($40) with a speed that grows and shrinks by 8 a frame, and waits
// $3C frames at each end. Its angle is a word (the object's own byte is the high one). Returns the angle
static uint8_t Swing_Spiked(Sst *self, Own_NGHZSwing *own) {
    if (own->wait != 0) {
        own->wait--;
        return self->angle;
    }
    if (!own->started) {
        if ((uint16_t)(player->x_pos.f.u - own->base_x + 0x20) >= 0x40 || debug_use)
            return self->angle;
        own->started = 1;
    }
    uint16_t angle = (uint16_t)((self->angle << 8) | own->angle_low);
    if (own->up) {
        own->accel += 8;
        angle = (uint16_t)(angle + own->accel);
        if (own->accel == 0x200) {
            own->accel = 0;
            angle = 0x8000;
            own->up = 0;
            own->wait = 0x3C;
        }
    } else {
        own->accel -= 8;
        angle = (uint16_t)(angle + own->accel);
        if (own->accel == -0x200) {
            own->accel = 0;
            angle = 0x4000;
            own->up = 1;
            own->wait = 0x3C;
        }
    }
    self->angle = (uint8_t)(angle >> 8);
    own->angle_low = (uint8_t)angle;
    return self->angle;
}

// loc_8784: where the platform and the chain are, from the swing's angle
static void Swing_Move(Sst *self, Own_NGHZSwing *own) {
    uint8_t d0 = SWING_OSC(6); // (Oscillating_Data+$18)
    switch (self->subtype & 0x70) {
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
        d0 = Swing_Spiked(self, own);
        break;
    default:
        break;
    }
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (uint8_t)(0x80 - d0);

    int16_t sin, cos;
    CalcSine(d0, &sin, &cos);
    self->y_pos.f.u = (int16_t)(own->base_y + ((sin * own->radius) >> 8));
    self->x_pos.f.u = (int16_t)(own->base_x + ((cos * own->radius) >> 8));

    Sst *chain = &sst[own->chain];
    if (own->chain == 0 || chain->code != self->code || chain == self)
        return;
    // each link 16 pixels further along (as the 16.16 steps of the prototype: sin * 16 / 256 pixels)
    const int32_t step_y = (int32_t)(int16_t)(sin << 4) << 8, step_x = (int32_t)(int16_t)(cos << 4) << 8;
    int32_t y = 0, x = 0;
    for (int i = 0; i < chain->child_count; i++) {
        chain->children[i].y = (int16_t)((y >> 16) + own->base_y);
        chain->children[i].x = (int16_t)((x >> 16) + own->base_x);
        y += step_y;
        x += step_x;
    }
    chain->y_pos.f.u = (int16_t)((y >> 16) + own->base_y);
    chain->x_pos.f.u = (int16_t)((x >> 16) + own->base_x);
}

static void Swing_Platform(Sst *self, int16_t old_x) {
    FOR_EACH_CHARACTER(chr, who)
        SolidSst_Platform(self, chr, who, self->width_pixels, (int16_t)(self->y_radius + 1), old_x);
}

static void Swing_Display(Sst *self, Own_NGHZSwing *own) {
    if (SstOutOfRange(own->base_x)) {
        Sst_Delete(&sst[own->chain]); // (and neither forgets that it was loaded: the prototype leaves its mark)
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

void Obj_NGHZSwing(Sst *self) {
    Own_NGHZSwing *own = (Own_NGHZSwing *)self->own.u8;

    if ((self->render_flags & SST_RENDER_MULTI_DRAW)) { // the chain
        Sst_Draw(self);
        return;
    }

    switch (self->routine) {
    case SwingRoutine_Init: {
        self->routine += 2;
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        if (LEVEL_ZONE(level_id) == ZoneId_ARZ) {
            self->mappings = Mappings_NGHZSwing;
            self->art_tile = TILE_MAP(0, 0, 0, 0, 0);
            self->width_pixels = 0x20;
            self->y_radius = 8;
        } else if (LEVEL_ZONE(level_id) == ZoneId_SSZ) { // Sand Shower (the staged objpatch-swing.asm: drawn from the level's own tiles, $20 wide, $8 high: the platform is tile $264, the links $260 and the fulcrum $25C, see the importer)
            self->mappings = Mappings_SSZSwing;
            self->art_tile = TILE_MAP(0, 0, 0, 0, 0);
            self->width_pixels = 0x20;
            self->y_radius = 8;
        } else if (LEVEL_ZONE(level_id) == ZoneId_MCZ) { // Dust Hill
            self->mappings = Mappings_DHZSwing;
            self->art_tile = TILE_MAP(0, 0, 0, 0, 0);
            self->width_pixels = 0x18;
            self->y_radius = 8;
        } else { // Oil Ocean
            self->mappings = Mappings_OOZSwing;
            self->art_tile = TILE_MAP(0, 2, 0, 0, 0x3E3);
            self->width_pixels = 0x20;
            self->y_radius = 0x10;
        }
        own->base_y = self->y_pos.f.u;
        own->base_x = self->x_pos.f.u;
        if (self->subtype & 0x80)
            self->routine += 4;
        const int links = self->subtype & 0xF;
        own->radius = (uint8_t)(links * 16 + 8);
        int16_t y = self->y_pos.f.u;
        Sst *chain = Sst_AllocateAfter(self + 1);
        if (chain != NULL) {
            chain->code = self->code;
            chain->mappings = self->mappings;
            chain->art_tile = self->art_tile;
            chain->render_flags = 4;
            chain->render_flags |= SST_RENDER_MULTI_DRAW;
            chain->height_pixels = chain->y_radius;
            chain->width_pixels = 0x48;
            chain->y_radius = 0x50;
            chain->priority = 2 * 0x80;
            chain->child_count = (uint8_t)(links < OBJECT_CHILDREN ? links : OBJECT_CHILDREN);
            for (int i = 0; i < chain->child_count; i++) {
                chain->children[i].x = self->x_pos.f.u;
                chain->children[i].y = y;
                chain->children[i].frame = (i == 0) ? 2 : 1; // (the hub, then the links)
                y += 0x10;
            }
            chain->mapping_frame = 1;
            chain->x_pos.f.u = self->x_pos.f.u;
            chain->y_pos.f.u = y;
            own->chain = (uint8_t)(chain - sst);
        }
        self->y_pos.f.u = (int16_t)(y + 8);
        self->angle = 0x80;
        self->subtype &= 0x70;
    }
        // Fallthrough
    case SwingRoutine_Swing: {
        const int16_t old_x = self->x_pos.f.u;
        Swing_Move(self, own);
        Swing_Platform(self, old_x);
        Swing_Display(self, own);
        break;
    }
    case SwingRoutine_SwingLoose: {
        const int16_t old_x = self->x_pos.f.u;
        Swing_Move(self, own);
        Swing_Platform(self, old_x);
        if ((self->status & 0x18) && SWING_OSC(6) == 0) { // stood on, at the end of the swing: it lets go of the chain
            Sst *raft = Sst_AllocateAfter(self + 1);
            if (raft != NULL) {
                *raft = *self;
                raft->routine = (uint8_t)(LEVEL_ZONE(level_id) == ZoneId_ARZ ? SwingRoutine_Raft : SwingRoutine_Fall);
                raft->x_vel = ((self->status & SST_STATUS_X_FLIP) != 0) ? -0x200 : 0x200;
                raft->status |= 2; // (falling)
                const uint8_t old_slot = (uint8_t)(self - sst), new_slot = (uint8_t)(raft - sst);
                Scratch_Sonic *sonic = (Scratch_Sonic *)player->own.u8;
                if (sonic->standing_obj == old_slot)
                    sonic->standing_obj = new_slot;
                if (TAILS_OBJ->code != NULL) {
                    Scratch_Sonic *tails = (Scratch_Sonic *)TAILS_OBJ->own.u8;
                    if (tails->standing_obj == old_slot)
                        tails->standing_obj = new_slot;
                }
            }
            self->mapping_frame = 3; // (the chain's end is all that is left)
            self->routine += 2;
            self->status &= 0xE7;
        }
        Swing_Display(self, own);
        break;
    }
    case SwingRoutine_Empty:
        Swing_Move(self, own);
        Swing_Display(self, own);
        break;
    case SwingRoutine_Fall: { // (a platform that has let go, in the other zones: it falls to $720 and bobs there)
        const int16_t old_x = self->x_pos.f.u;
        if (self->status & 2) {
            Sst_MoveSprite2(self);
            self->y_vel += 0x18;
            if ((uint16_t)self->y_pos.f.u >= 0x720) {
                self->y_pos.f.u = 0x720;
                self->status &= (uint8_t)~2;
                self->x_vel = 0;
                self->y_vel = 0;
                own->base_y = 0x720;
            }
        } else {
            self->y_pos.f.u = (int16_t)(own->base_y + (SWING_OSC(5) >> 1));
        }
        Swing_Platform(self, old_x);
        SstOnScreenTest(self);
        break;
    }
    default: { // the raft
        const int16_t old_x = self->x_pos.f.u;
        Sst_MoveSprite2(self);
        if (self->status & 2) { // falling to the water
            self->y_vel += 0x18;
            if ((uint16_t)wtr_pos2 <= (uint16_t)self->y_pos.f.u) {
                self->y_pos.f.u = wtr_pos2;
                own->base_y = wtr_pos2;
                self->status &= (uint8_t)~2;
                self->x_vel = 0x100;
                self->y_vel = 0;
            }
        } else {
            self->y_pos.f.u = (int16_t)(own->base_y + (SWING_OSC(5) >> 1));
            if (self->x_vel != 0) {
                const int16_t d1 = SstHitWallRight(self, self->width_pixels);
                if (d1 < 0) {
                    self->x_pos.f.u = (int16_t)(self->x_pos.f.u + d1);
                    self->x_vel = 0;
                }
            }
        }
        Swing_Platform(self, old_x);
        SstOnScreenTest(self);
        break;
    }
    }
}

void Obj_SwingDispatch(Sst *self) {
    const int zone = LEVEL_ZONE(level_id);
    if (zone == ZoneId_ARZ || zone == ZoneId_MCZ || zone == ZoneId_OOZ || zone == ZoneId_SSZ)
        Obj_NGHZSwing(self);
    else
        Obj_SwingingPlatform(self);
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
} Own_Rotator;

enum { RotatorRoutine_Init = 0, RotatorRoutine_Leader = 2, RotatorRoutine_Follower = 4 };

static void Rotator_Platform(Sst *self, int16_t old_x) {
    FOR_EACH_CHARACTER(chr, who)
        SolidSst_Platform(self, chr, who, (int16_t)(self->width_pixels + 0xB), 9, old_x);
}

// The alpha's loop of positions (Offset_0x01F4B6): `count` steps of the arm from `first`, stored as child sprites from `*child`
static void Rotator_Links(Sst *chain, int *child, int16_t centre_x, int16_t centre_y, int16_t sin, int16_t cos, int first, int count) {
    for (int k = first; k < first + count; k++, (*child)++) {
        chain->children[*child].x = (int16_t)(centre_x + ((k * cos) >> 4));
        chain->children[*child].y = (int16_t)(centre_y + ((k * sin) >> 4));
    }
}

void Obj_NGHZRotator(Sst *self) {
    Own_Rotator *own = (Own_Rotator *)self->own.u8;

    if ((self->render_flags & SST_RENDER_MULTI_DRAW)) { // the chain
        Sst_Draw(self);
        return;
    }

    switch (self->routine) {
    case RotatorRoutine_Init: {
        self->routine += 2;
        self->mappings = LEVEL_ZONE(level_id) == ZoneId_SSZ ? Mappings_SSZSwing : Mappings_NGHZSwing; // (Sand Shower's: the staged objpatch83.asm, the same mapping as its swing, from the level's own tiles)
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x20;
        own->centre_x = self->x_pos.f.u;
        own->centre_y = self->y_pos.f.u;
        own->speed = (int16_t)((int16_t)(int8_t)(self->subtype & 0xF0) << 3);
        own->angle = (uint16_t)((((self->status >> 2) | (self->status << 6)) & 0xC0) << 8); // (ror.b #2, and #$C0)
        Sst *chain = Sst_AllocateAfter(self + 1);
        if (chain != NULL) {
            chain->code = self->code;
            chain->mappings = self->mappings;
            chain->art_tile = self->art_tile;
            chain->render_flags = SST_RENDER_LEVEL;
            chain->render_flags |= SST_RENDER_MULTI_DRAW;
            chain->height_pixels = chain->y_radius;
            chain->y_radius = 0x40;
            chain->width_pixels = 0xFF;
            chain->child_count = 8;
            for (int i = 0; i < 8; i++)
                chain->children[i].frame = 1;
            chain->x_pos.f.u = self->x_pos.f.u;
            chain->y_pos.f.u = self->y_pos.f.u;
            own->chain = (uint8_t)(chain - sst);
            uint8_t *slots[2] = { &own->second, &own->third };
            for (int i = 0; i < 2; i++) {
                Sst *follower = Sst_AllocateAfter(self + 1);
                if (follower == NULL)
                    break;
                follower->code = self->code;
                follower->routine = RotatorRoutine_Follower;
                follower->mappings = self->mappings;
                follower->art_tile = self->art_tile;
                follower->render_flags = SST_RENDER_LEVEL;
                follower->priority = 4 * 0x80;
                follower->width_pixels = 0x20;
                Own_Rotator *fs = (Own_Rotator *)follower->own.u8;
                fs->centre_x = self->x_pos.f.u;
                fs->centre_y = self->y_pos.f.u;
                fs->old_x = self->x_pos.f.u;
                *slots[i] = (uint8_t)(follower - sst);
            }
        }
    }
        // Fallthrough
    case RotatorRoutine_Leader: {
        const int16_t old_x = self->x_pos.f.u;
        own->angle = (uint16_t)(own->angle + own->speed);
        Sst *chain = &sst[own->chain];
        int child = 0;
        int16_t sin, cos;

        // the first arm: the chain's own place is its first step, two links at steps 2 and 3, and the platform at 4
        CalcSine((uint8_t)(own->angle >> 8), &sin, &cos);
        chain->x_pos.f.u = (int16_t)(own->centre_x + (cos >> 4));
        chain->y_pos.f.u = (int16_t)(own->centre_y + (sin >> 4));
        Rotator_Links(chain, &child, own->centre_x, own->centre_y, sin, cos, 2, 2);
        self->x_pos.f.u = (int16_t)(own->centre_x + ((4 * cos) >> 4));
        self->y_pos.f.u = (int16_t)(own->centre_y + ((4 * sin) >> 4));

        // the second arm, $55 on, and the third, $55 back: three links at steps 1-3 each and a platform at 4
        const int8_t turns[2] = { 0x55, -0x55 };
        const uint8_t followers[2] = { own->second, own->third };
        for (int arm = 0; arm < 2; arm++) {
            CalcSine((uint8_t)((own->angle >> 8) + turns[arm]), &sin, &cos);
            Rotator_Links(chain, &child, own->centre_x, own->centre_y, sin, cos, 1, 3);
            Sst *follower = &sst[followers[arm]];
            follower->x_pos.f.u = (int16_t)(own->centre_x + ((4 * cos) >> 4));
            follower->y_pos.f.u = (int16_t)(own->centre_y + ((4 * sin) >> 4));
        }

        Rotator_Platform(self, old_x);
        if (SstOutOfRange(own->centre_x)) {
            Sst_Delete(chain);
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    }

    case RotatorRoutine_Follower:
        Rotator_Platform(self, own->old_x);
        own->old_x = self->x_pos.f.u;
        SstOnScreenTest2(self, own->centre_x);
        break;
    }
}
