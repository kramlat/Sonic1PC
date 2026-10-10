#include "SplitScreen.h"
#include "HTZQuake.h"
#include "HTZBackground.h"

#include <string.h>

#include "Backend/VDP.h"
#include "Camera.h"
#include "EngineConstants.h"
#include "EnginePalette.h"
#include "Level.h"
#include "LevelDrawCore.h"
#include "LevelPlane.h"
#include "LevelScroll.h"
#include "Object.h"
extern bool hud_lives_lower_left; // (Object/HUD.c)
#include "Object/Tails.h"
#include "Sprites.h"
#include "Video.h"

uint8_t two_player_mode;

// (LevelScroll.c: the zone's background deformation run for the second camera)
void DeformLayersP2_Init(void);
void DeformLayersP2(int16_t (*lines)[2], int16_t *bg_y);
void DeformLayersP2_Draw(void);

static bool side_by_side;  // the views are next to each other (a wide picture)
static int16_t view_width; // how wide each view is
static int16_t follow_x;   // where its camera keeps the player from the view's left edge
static uint16_t left_limit_p2; // the second camera's left limit: the level's, until something locks it

bool SplitScreen_Active(void) {
    return camera_split;
}

int16_t SplitScreen_FollowX(void) {
    return (camera_split && side_by_side) ? follow_x : 144;
}

static void SplitOff(void) {
    hud_lives_lower_left = false;
    camera_split = false;
    sprite_split_screen = SPRITE_SPLIT_NONE;
    VDP_SetSplitScreen(VDP_SPLIT_NONE, NULL);
    VDP_SetSplitWater(NULL, NULL, 0, 0);
    DrawTileRemap_Clear();
}

void SplitScreen_LoadLevel(void) {
    SplitOff();
    if (!two_player_mode)
        return;

    side_by_side = (SCREEN_WIDTH * 5 >= SCREEN_HEIGHT * 8);
    view_width = side_by_side ? SCREEN_WIDTH / 2 : SCREEN_WIDTH;
    follow_x = (int16_t)(view_width / 2 - 16);
    camera_split = true;
    left_limit_p2 = limit_left1;
    hud_lives_lower_left = true;
    sprite_split_screen = side_by_side ? SPRITE_SPLIT_SIDE : SPRITE_SPLIT_STACKED;
    VDP_SetSplitScreen(side_by_side ? VDP_SPLIT_SIDE : VDP_SPLIT_STACKED, &video_second_view);

    // Both start from where Sonic is (Tails is put beside him), their cameras centred on them; the views are as wide as the narrower of the two
    Object *tails = TAILS_OBJ;
    tails->pos.l.x.f.u = player->pos.l.x.f.u;
    tails->pos.l.y.f.u = player->pos.l.y.f.u;
    if (side_by_side) {
        int16_t x = (int16_t)(player->pos.l.x.f.u - follow_x);
        if (x < 0)
            x = 0;
        if (x >= (int16_t)limit_right2)
            x = (int16_t)limit_right2;
        scrpos_x.f.u = x;
    }
    scrpos_x_p2 = scrpos_x;
    scrpos_y_p2 = scrpos_y;
    if (LEVEL_ZONE(level_id) == ZoneId_HTZ) // (the second view's background has mountains of its own: the tiles of the first's set are drawn as the second's)
        DrawTileRemap_Set(VRAM_BG_P2, 0x2000, 0x500, 0x20, HTZ_P2_TILES);
    LevelPlane_Init(&fg_plane, VRAM_FG, &scrpos_x, &scrpos_y, view_width);
    LevelPlane_Init(&level_plane_p2, VRAM_FG_P2, &scrpos_x_p2, &scrpos_y_p2, view_width);
    level_plane_p2.xblock = fg_plane.xblock;
    level_plane_p2.yblock = fg_plane.yblock;
    LevelPlane_DrawAll(&fg_plane, LEVEL_LAYOUT_FG(0));
    LevelPlane_DrawAll(&level_plane_p2, LEVEL_LAYOUT_FG(0));
    DrawChunks(bg_scrpos_x.f.u, bg_scrpos_y.f.u, LEVEL_LAYOUT_BG(0), VRAM_BG_P2); // (its background starts where the first's is)
    DeformLayersP2_Init();
    video_second_view.vscroll_a = scrpos_y.f.u;
    video_second_view.vscroll_b = vid_bg_scrpos_y_dup;
}

void SplitScreen_LockCameras(int16_t x, int16_t half_width) {
    if (!camera_split) {
        limit_left2 = limit_right2;
        return;
    }
    // Each view locks (its camera's left limit goes up to the right limit, so the screen runs on to the end of the level and stays) only if the object is in sight there
    if ((uint16_t)(x + half_width - scrpos_x.f.u) < (uint16_t)(view_width + 2 * half_width))
        limit_left2 = limit_right2;
    if ((uint16_t)(x + half_width - scrpos_x_p2.f.u) < (uint16_t)(view_width + 2 * half_width))
        left_limit_p2 = limit_right2;
}

// The level's end, found by the cameras (SignpostArtLoad): a camera that has got to the sign's stretch (end_x) can no longer go back from it. Each view's left limit is its own, so the first camera getting there
// holds the first view only. True when the end is newly reached by a camera and no other has been held there before (the sign's art is loaded then, once).
bool SplitScreen_ReachEnd(int16_t end_x) {
    const bool held = limit_left2 == (uint16_t)end_x || (camera_split && left_limit_p2 == (uint16_t)end_x);
    bool reached = false;
    if (scrpos_x.f.u >= end_x && limit_left2 != (uint16_t)end_x) {
        limit_left2 = (uint16_t)end_x;
        reached = true;
    }
    if (camera_split && scrpos_x_p2.f.u >= end_x && left_limit_p2 != (uint16_t)end_x) {
        left_limit_p2 = (uint16_t)end_x;
        reached = true;
    }
    return reached && !held;
}

// The second camera: Sonic's own rules (a dead zone of 16 pixels at the following place, at most 16 pixels a frame, the level's limits), with a simpler vertical, which is Sonic's without the look
// up and down
static void MoveCameraP2(void) {
    const Object *tails = TAILS_OBJ;

    LevelPlane_ClearFlags(&level_plane_p2);

    int16_t old_x = scrpos_x_p2.f.u;
    int16_t push = (int16_t)(tails->pos.l.x.f.u - old_x - follow_x);
    int16_t step = 0;
    if (push < 0)
        step = push < -16 ? -16 : push;
    else if (push >= 16)
        step = (push - 16) > 16 ? 16 : (int16_t)(push - 16);
    int16_t x = (int16_t)(old_x + step);
    if (x < (int16_t)left_limit_p2)
        x = (int16_t)left_limit_p2;
    if (x > (int16_t)limit_right2)
        x = (int16_t)limit_right2;
    scrshift_x_p2 = (int16_t)((x - old_x) << 8);
    scrpos_x_p2.f.u = x;
    LevelPlane_CameraMovedX(&level_plane_p2, old_x);

    int16_t old_y = scrpos_y_p2.f.u;
    int16_t look = (int16_t)(96 + SCREEN_TALLADD2);
    int16_t y = (int16_t)(tails->pos.l.y.f.u - old_y);
    if (tails->status.p.f.in_ball)
        y -= 5;
    uint16_t speed;
    if (tails->status.p.f.in_air) {
        y += 32 - look;
        if (y < 0 || (y - 64) >= 0)
            speed = 0x1000;
        else
            speed = 0; // inside the window: stays
    } else {
        y -= look;
        uint16_t inertia_abs = (tails->inertia < 0) ? -tails->inertia : tails->inertia;
        speed = (inertia_abs < 0x800) ? 0x600 : 0x1000;
    }
    int16_t ny = old_y;
    if (speed != 0 && y != 0) {
        int16_t px = (int16_t)(speed >> 8);
        if (y > -px && y < px)
            ny = (int16_t)(old_y + y);
        else
            ny = (int16_t)(old_y + (y < 0 ? -px : px));
    }
    if (ny < (int16_t)limit_top2)
        ny = (int16_t)limit_top2;
    if (ny > (int16_t)limit_btm2)
        ny = (int16_t)limit_btm2;
    scrshift_y_p2 = (int16_t)((ny - old_y) << 8);
    scrpos_y_p2.f.u = ny;
    LevelPlane_CameraMovedY(&level_plane_p2, old_y);
}

void SplitScreen_Scroll(void) {
    if (!camera_split)
        return;
    MoveCameraP2();

    // The second view's scroll: its foreground is the first's (with whatever waves the zone gave it) moved by what the cameras are apart; its background (a plane of its own) is the zone's deformation run
    // for the second camera
    static int16_t lines[SCREEN_MAX_HEIGHT][2];
    int16_t bg_y2;
    DeformLayersP2(lines, &bg_y2);
    for (int i = 0; i < SCREEN_HEIGHT; i++) {
        hscroll_buffer_p2[i][0] = lines[i][0];
        hscroll_buffer_p2[i][1] = lines[i][1];
    }
    video_second_view.vscroll_a = (int16_t)(scrpos_y_p2.f.u + htz_p2_shake_y);
    video_second_view.vscroll_b = bg_y2;

    // The sea: each view has its own water line against its own camera
    if (Level_HasWater())
        VDP_SetSplitWater(&dry_palette[0][0], &wet_palette[0][0], (int16_t)(wtr_pos1 - scrpos_y.f.u), (int16_t)(wtr_pos1 - scrpos_y_p2.f.u));
}

void SplitScreen_VBlank(void) {
    if (!camera_split)
        return;
    LevelPlane_Snapshot(&level_plane_p2);
    Video_UploadHScrollP2();
    LevelPlane_DrawPending(&level_plane_p2, LEVEL_LAYOUT_FG(0));
    DeformLayersP2_Draw();
}
