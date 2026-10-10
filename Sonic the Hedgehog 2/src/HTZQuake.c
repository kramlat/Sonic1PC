// Hill Top's earthquake for Sonic 2: the prototype's events (DynResize_HTz, loc_7AE4: act 1's three states and act 2's five), the shaking branch of its background scroll (Bg_Scroll_HTz's loc_6236) and
// object 30 (Obj_0x30, the solid pieces of the ground that moves). The events move the background by a diff of their own while the camera is in a quake's stretch of the level (Screen_Shaking_Flag_HTZ),
// and every now and then they move the ground up or down by a quarter pixel a frame, between two limits, with the screen shaking (Screen_Shaking_Flag) while it does.
#include "HTZQuake.h"
#include "LevelCollision.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Object/CPZObjects.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Video.h"

#include "Backend/VDP.h"
#include "Sprites.h"
#include "Object/DebugMarkers.h"
#include "Macros.h"

// (LevelScroll.c's, Sonic 2's own scroll helpers)
void UpdateBGScroll(dword_s *pos, int32_t delta, uint8_t *block, uint16_t *flags, uint16_t neg_bit, uint16_t pos_bit);
void BGScroll_YRelative(int32_t y_off);

uint8_t htz_quake, htz_shaking;
int16_t htz_bg_y_offset;
static int16_t htz_bg_x_offset;
static int16_t htz_bg_x_diff, htz_bg_y_diff; // (Camera_BG_X_pos_diff and _Y_: a movement in 1/256 pixels, as the camera's own diffs are)
static uint8_t htz_direction;                // HTZ_Terrain_Direction
static int16_t htz_delay;                    // HTZ_Terrain_Delay

// The sprites follow the camera's copy (Camera_X_pos_copy, _Y_), which the shake moves with the screen: its own variables while a quake's branch runs, the camera itself otherwise
static int16_t sprite_cam_x, sprite_cam_y;
static void SpritesFollow(bool copy) {
    sprite_view.layer[1].x = copy ? &sprite_cam_x : &scrpos_x.f.u;
    sprite_view.layer[1].y = copy ? &sprite_cam_y : &scrpos_y.f.u;
}

void HTZQuake_Reset(void) {
    SpritesFollow(false);
    for (int layer = 1; layer < 4; layer++) { // (the second view's sprites follow its own camera again)
        sprite_view_p2.layer[layer].x = &scrpos_x_p2.f.u;
        sprite_view_p2.layer[layer].y = &scrpos_y_p2.f.u;
    }
    htz_p2_shake_y = 0;
    htz_quake = htz_shaking = 0;
    htz_direction = 0;
    htz_delay = 0;
    htz_bg_x_offset = htz_bg_y_offset = 0;
    htz_bg_x_diff = htz_bg_y_diff = 0;
}

// loc_7C84: where the background should move to follow (x, y): the difference to its place, less its offsets, held to 16 pixels a frame. The prototype writes the byte of each diff that is the pixels
// (the word's high byte), so the low byte stays what it was. Returns the two moves.
static int16_t FollowTarget(int16_t x, int16_t y, int16_t *move_y) {
    int16_t d0 = (int16_t)(x - bg_scrpos_x.f.u - htz_bg_x_offset);
    if (d0 >= 0) {
        if ((uint16_t)d0 >= 0x10)
            d0 = 0x10;
    } else if (d0 <= -0x10) {
        d0 = -0x10;
    }
    htz_bg_x_diff = (int16_t)((htz_bg_x_diff & 0x00FF) | (int16_t)((uint8_t)d0 << 8));
    int16_t d1 = (int16_t)(y - bg_scrpos_y.f.u - htz_bg_y_offset);
    if (d1 >= 0) {
        if ((uint16_t)d1 >= 0x10)
            d1 = 0x10;
    } else if (d1 <= -0x10) {
        d1 = -0x10;
    }
    htz_bg_y_diff = (int16_t)((htz_bg_y_diff & 0x00FF) | (int16_t)((uint8_t)d1 << 8));
    *move_y = d1;
    return d0;
}

// The quake has left the camera's stretch: the background eases to its resting place (x $200, y 0), and the flag goes when it has got there
static void EaseOut(void) {
    if (!htz_quake)
        return;
    htz_bg_x_diff = htz_bg_y_diff = 0;
    int16_t d1;
    const int16_t d0 = FollowTarget(0x200, 0, &d1);
    if ((d0 | d1) == 0)
        htz_quake = 0;
}

// The quake starts: the background begins at the camera's place (less $100 down) with the ground at its offset, and waits
static void Start(int16_t y_offset) {
    htz_quake = 1;
    bg_scrpos_x.v = scrpos_x.v;
    bg_scrpos_y.v = scrpos_y.v;
    htz_bg_x_diff = htz_bg_y_diff = 0;
    htz_bg_x_offset = 0;
    htz_bg_y_offset = y_offset;
    bg_scrpos_y.f.u = (int16_t)(bg_scrpos_y.f.u - 0x100);
    htz_delay = 0;
}

// The quake's stretch is left: the background goes back to its own resting place
static void Leave(void) {
    bg_scrpos_x.v = 0x04000000;
    bg_scrpos_y.v = 0;
    htz_bg_x_offset = htz_bg_y_offset = 0;
    htz_direction = 0;
}

// The ground between its two offsets: a quarter pixel every fourth frame, and at each end it rests for $78 frames with the screen still before it turns and the shaking begins again
static void Terrain(int16_t low, int16_t high) {
    if (!htz_direction) {
        if (htz_bg_y_offset != high) {
            if ((frame_count & 3) == 0)
                htz_bg_y_offset++;
            return;
        }
    } else if (htz_bg_y_offset != low) {
        if ((frame_count & 3) == 0)
            htz_bg_y_offset--;
        return;
    }
    htz_shaking = 0;
    if (--htz_delay >= 0)
        return;
    htz_delay = 0x78;
    htz_direction ^= 1;
    htz_shaking = 1;
}

// The background follows the camera in the quake's stretch
static void Follow(void) {
    htz_bg_x_diff = scrshift_x;
    htz_bg_y_diff = scrshift_y;
    int16_t d1;
    FollowTarget(scrpos_x.f.u, scrpos_y.f.u, &d1);
}

// DynResize_HTz: Hill Top's act 1 (loc_7B00, loc_7B6C, loc_7C20) and act 2 (loc_7D0E ... loc_7F5C)
void HTZQuake_SpritesNormal(void) {
    SpritesFollow(false);
}

void HTZQuake_Events(void) {
    const uint16_t camx = (uint16_t)scrpos_x.f.u, camy = (uint16_t)scrpos_y.f.u;
    if (LEVEL_ACT(level_id) == 0) {
        switch (dle_routine) {
        case 0:
            if (camy >= 0x400 && camx >= 0x1800) {
                Start(0x140);
                dle_routine += 2;
            } else {
                EaseOut();
            }
            break;
        case 2:
            Terrain(0xE0, 0x140);
            if (camx < 0x1800) {
                Leave();
                dle_routine -= 2;
            } else if (camx >= 0x1F00) {
                Leave();
                dle_routine += 2;
            } else {
                Follow();
            }
            break;
        case 4:
            if (camx >= 0x1F00) {
                EaseOut();
            } else {
                Start(0x140);
                dle_routine -= 2;
            }
            break;
        }
        return;
    }
    switch (dle_routine) {
    case 0:
        if (camx >= 0x14C0) {
            Start(0x2C0);
            dle_routine += 2;
            if (camy >= 0x380) {
                htz_bg_x_offset = (int16_t)0xF980;
                bg_scrpos_x.f.u = (int16_t)(bg_scrpos_x.f.u + 0x480);
                htz_bg_y_offset = 0x300;
                dle_routine += 6;
            }
        } else {
            EaseOut();
        }
        break;
    case 2:
        Terrain(0, 0x2C0);
        if (camx < 0x14C0) {
            Leave();
            dle_routine -= 2;
        } else if (camx >= 0x1B00) {
            Leave();
            dle_routine += 2;
        } else {
            Follow();
        }
        break;
    case 4:
        if (camx >= 0x1B00) {
            EaseOut();
        } else {
            Start(0x2C0);
            dle_routine -= 2;
        }
        break;
    case 6:
        Terrain(0, 0x300);
        if (camx < 0x14C0) {
            Leave();
            dle_routine -= 6;
        } else if (camx >= 0x1B00) {
            Leave();
            dle_routine += 2;
        } else {
            Follow();
        }
        break;
    case 8:
        if (camx >= 0x1B00) {
            EaseOut();
        } else {
            Start(0x300);
            htz_bg_x_offset = (int16_t)0xF980;
            bg_scrpos_x.f.u = (int16_t)(bg_scrpos_x.f.u + 0x480);
            dle_routine -= 2;
        }
        break;
    }
}

// loc_5F60: how far the screen shakes (y, then x) by the frame
static const uint8_t shake_table[0x42] = {
    1, 2, 1, 3, 1, 2, 2, 1, 2, 3, 1, 2, 1, 2, 0, 0, 2, 0, 3, 2, 2, 3, 2, 2, 1, 3, 0, 0, 1, 0, 1, 3,
    1, 2, 1, 3, 1, 2, 2, 1, 2, 3, 1, 2, 1, 2, 0, 0, 2, 0, 3, 2, 2, 3, 2, 2, 1, 3, 0, 0, 1, 0, 1, 3, 1, 2,
};

int16_t htz_camera_shake_x, htz_camera_shake_y; // what the sprites are moved by (Camera_X_pos_copy and _Y_)

// loc_6236, the shaking branch of Bg_Scroll_HTz: the background moves by the events' diffs, the screen shakes while the ground moves
void HTZQuake_Deform(void) {
    UpdateBGScroll(&bg_scrpos_x, (int32_t)htz_bg_x_diff << 8, &bg1_xblock, &bg1_scroll_flags, SCROLL_FLAG_LEFT, SCROLL_FLAG_RIGHT); // (Scroll_Block2)
    BGScroll_YRelative((int32_t)htz_bg_y_diff << 8);                                                                                    // (Scroll_Block3)
    vid_scrpos_y_dup = scrpos_y.f.u;
    vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
    int16_t shake_x = 0;
    htz_camera_shake_x = htz_camera_shake_y = 0;
    if (htz_shaking) {
        const uint8_t *shake = &shake_table[frame_count & 0x3F];
        vid_scrpos_y_dup = (int16_t)(vid_scrpos_y_dup + shake[0]);
        vid_bg_scrpos_y_dup = (int16_t)(vid_bg_scrpos_y_dup + shake[0]);
        htz_camera_shake_y = shake[0];
        shake_x = shake[1];
        htz_camera_shake_x = shake_x;
    }
    sprite_cam_x = (int16_t)(scrpos_x.f.u + shake_x);
    sprite_cam_y = (int16_t)(scrpos_y.f.u + htz_camera_shake_y);
    SpritesFollow(true);
    int16_t *bufp = &hscroll_buffer[0][0];
    for (int i = 0; i < SCREEN_HEIGHT; i++) {
        *bufp++ = (int16_t)-(scrpos_x.f.u + shake_x);
        *bufp++ = (int16_t)-(bg_scrpos_x.f.u + shake_x);
    }
}

// The second view's shake: the same as the first's (the ground is the same ground), on the second camera: its background and scroll lines, its foreground (SplitScreen.c adds htz_p2_shake_y) and its sprites
int16_t htz_p2_shake_y;
static int16_t sprite_cam_x_p2, sprite_cam_y_p2;

void HTZQuake_ShakeP2(void) {
    int16_t shake_x = 0;
    htz_p2_shake_y = 0;
    if (htz_shaking) {
        const uint8_t *shake = &shake_table[frame_count & 0x3F];
        htz_p2_shake_y = shake[0];
        shake_x = shake[1];
        vid_bg_scrpos_y_dup = (int16_t)(vid_bg_scrpos_y_dup + shake[0]);
        for (int i = 0; i < SCREEN_HEIGHT; i++) {
            hscroll_buffer[i][0] = (int16_t)(hscroll_buffer[i][0] - shake_x);
            hscroll_buffer[i][1] = (int16_t)(hscroll_buffer[i][1] - shake_x);
        }
    }
    sprite_cam_x_p2 = (int16_t)(scrpos_x.f.u + shake_x); // (scrpos is the second camera while its deformation runs)
    sprite_cam_y_p2 = (int16_t)(scrpos_y.f.u + htz_p2_shake_y);
    for (int layer = 1; layer < 4; layer++) {
        sprite_view_p2.layer[layer].x = &sprite_cam_x_p2;
        sprite_view_p2.layer[layer].y = &sprite_cam_y_p2;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 30: the solid pieces of the ground that moves (Obj_0x30): invisible, solid on every side (the lava's top, kind 4, hurts whoever stands on it, and kind 8 has a sloped top), at their own
// place plus the ground's offset. While a quake's stretch is on they stay (they are only forgotten when it is not).
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype; // 0x28: 0, 2, 4, 6 or 8
    uint8_t pad0[7];
    int16_t base_x;  // 0x30
    int16_t base_y;  // 0x32
} Scratch_QuakeBlock;

// loc_17B74: the sloped top of kind 8, one value for each pixel
static const uint8_t quake_slope[268] = {
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x3F, 0x3F, 0x3E, 0x3E, 0x3D, 0x3D, 0x3C, 0x3C,
    0x3B, 0x3B, 0x3A, 0x3A, 0x39, 0x39, 0x38, 0x38, 0x37, 0x37, 0x36, 0x36, 0x35, 0x35, 0x34, 0x34,
    0x33, 0x33, 0x32, 0x32, 0x31, 0x31, 0x30, 0x30, 0x2F, 0x2F, 0x2E, 0x2E, 0x2D, 0x2D, 0x2C, 0x2C,
    0x2B, 0x2B, 0x2A, 0x2A, 0x29, 0x29, 0x28, 0x28, 0x27, 0x27, 0x26, 0x26, 0x25, 0x25, 0x24, 0x24,
    0x23, 0x23, 0x22, 0x22, 0x21, 0x21, 0x20, 0x20, 0x1F, 0x1F, 0x1E, 0x1E, 0x1D, 0x1D, 0x1C, 0x1C,
    0x1B, 0x1B, 0x1A, 0x1A, 0x19, 0x19, 0x18, 0x18, 0x17, 0x17, 0x16, 0x16, 0x15, 0x15, 0x14, 0x14,
    0x13, 0x13, 0x12, 0x12, 0x11, 0x11, 0x10, 0x10, 0x0F, 0x0F, 0x0E, 0x0E, 0x0D, 0x0D, 0x0C, 0x0C,
    0x0B, 0x0B, 0x0A, 0x0A, 0x09, 0x09, 0x08, 0x08, 0x07, 0x07, 0x06, 0x06, 0x05, 0x05, 0x04, 0x04,
    0x03, 0x03, 0x02, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0xFF, 0xFE, 0xFE, 0xFD, 0xFD, 0xFC, 0xFC,
    0xFB, 0xFB, 0xFA, 0xFA, 0xF9, 0xF9, 0xF8, 0xF8, 0xF7, 0xF7, 0xF6, 0xF6, 0xF5, 0xF5, 0xF4, 0xF4,
    0xF3, 0xF3, 0xF2, 0xF2, 0xF1, 0xF1, 0xF0, 0xF0, 0xEF, 0xEF, 0xEE, 0xEE, 0xED, 0xED, 0xEC, 0xEC,
    0xEB, 0xEB, 0xEA, 0xEA, 0xE9, 0xE9, 0xE8, 0xE8, 0xE7, 0xE7, 0xE6, 0xE6, 0xE5, 0xE5, 0xE4, 0xE4,
    0xE3, 0xE3, 0xE2, 0xE2, 0xE1, 0xE1, 0xE0, 0xE0, 0xDF, 0xDF, 0xDE, 0xDE, 0xDD, 0xDD, 0xDC, 0xDC,
    0xDB, 0xDB, 0xDA, 0xDA, 0xD9, 0xD9, 0xD8, 0xD8, 0xD7, 0xD7, 0xD6, 0xD6, 0xD5, 0xD5, 0xD4, 0xD4,
    0xD3, 0xD3, 0xD2, 0xD2, 0xD1, 0xD1, 0xD0, 0xD0, 0xCF, 0xCF, 0xCE, 0xCE, 0xCD, 0xCD, 0xCC, 0xCC,
    0xCB, 0xCB, 0xCA, 0xCA, 0xC9, 0xC9, 0xC8, 0xC8, 0xC7, 0xC7, 0xC6, 0xC6, 0xC5, 0xC5, 0xC4, 0xC4,
    0xC3, 0xC3, 0xC2, 0xC2, 0xC1, 0xC1, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
};

static Object *Char(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
}

void Obj_HTZQuakeBlock(Object *obj) {
    Scratch_QuakeBlock *scratch = (Scratch_QuakeBlock *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        scratch->base_y = obj->pos.l.y.f.u;
        scratch->base_x = obj->pos.l.x.f.u;
        static const uint8_t widths[5] = { 0xC0, 0xC0, 0xC0, 0xE0, 0xFF };
        obj->width_pixels = widths[(scratch->subtype >> 1) % 5];
        if (scratch->subtype >= 6) { // the camera decides whether they are there (loc_17A68)
            if (scratch->subtype == 6 ? scrpos_y.f.u >= 0x380 : scrpos_y.f.u < 0x380) {
                if (obj->respawn_index)
                    objstate[obj->respawn_index] &= 0x7F;
                ObjectDelete(obj);
                return;
            }
        }
    }

    obj->pos.l.y.f.u = (int16_t)(scratch->base_y + htz_bg_y_offset);
    const int16_t x = obj->pos.l.x.f.u;
    int16_t x_rad, y_rad;
    switch ((scratch->subtype >> 1) % 5) {
    case 0:
    case 1:
        x_rad = 0xCB;
        y_rad = 0x80;
        break;
    case 2:
        x_rad = 0xCB;
        y_rad = 0x78;
        break;
    case 3:
        x_rad = 0xEB;
        y_rad = 0x78;
        break;
    default:
        x_rad = 0x10A;
        y_rad = 0x3E;
        break;
    }
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = Char(who);
        if (chr != NULL)
            Solid_Character(obj, chr, who, x_rad, y_rad, (int16_t)(y_rad + 1), x, ((scratch->subtype >> 1) % 5) == 4 ? (const int8_t *)quake_slope : NULL);
    }
    // loc_FBF4: whoever stands on it is let go when the level's own floor is at or under his feet (the ground moves up through him)
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = Char(who);
        if (chr == NULL || !(obj->status.b & (1 << (3 + who))))
            continue;
        if (ObjFloorDist(chr, chr->pos.l.x.f.u) <= 0) {
            chr->status.p.f.object_stand = false;
            obj->status.b &= (uint8_t)~(1 << (3 + who));
        }
    }
    DebugMarkers_Show(obj, (int16_t)(x_rad - 0xB), y_rad); // (the debug cheat shows the corners of its box)
    const int kind = (scratch->subtype >> 1) % 5;
    if (kind == 2 || kind == 3) { // the lava's top (kinds 4 and 6) hurts whoever stands on it (Touch_ChkHurt: not while invincible or flashing)
        for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
            Object *chr = Char(who);
            if (chr == NULL || !(obj->status.b & (1 << (3 + who))) || invincibility || ((Scratch_Sonic *)&chr->scratch)->flash_time)
                continue;
            if (who == SolidChar_Sonic)
                HurtSonic(chr, obj);
            else
                Tails_Hurt(chr, obj);
        }
    }
    if (!htz_quake && IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= 0x7F;
        ObjectDelete(obj);
    }
}
