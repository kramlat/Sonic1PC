#include "Level.h"

#include "Constants.h"
#include "Enigma.h"
#include "Game.h"
#include "Kosinski.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "PLC.h"
#include "Palette.h"
#include "Sound.h"

#include "Backend/VDP.h"

#include <string.h>

// Sets a 16.16 fixed-point position from a saved pixel value.
static void LampSetPos(dword_s *pos, uint16_t pixels) {
    pos->f.u = (int16_t)pixels;
    pos->f.l = 0;
}

void Obj_Checkpoint_LoadInfo(void) {
    last_lamp = prev_lamp;
    // The saved values are pixel positions (the high word of the 16.16 fixed point
    // fields); writing them into .v put them in the fractional word, i.e. position 0.
    LampSetPos(&player->pos.l.x, lamp_state.spawn.x);
    LampSetPos(&player->pos.l.y, lamp_state.spawn.y);
    rings = 0;
    life_count = 0; // the original reloads the saved value and then immediately clears it
    level_time.pad = lamp_state.time.pad;
    level_time.min = lamp_state.time.min;
    level_time.sec = lamp_state.time.sec;
    level_time.frame = lamp_state.time.frame;
    level_time.frame = 59;
    level_time.sec--;
    dle_routine = lamp_state.dle;
    wtr_routine = lamp_state.water_level.routine;
    limit_btm2 = lamp_state.limitbtm;
    limit_btm1 = lamp_state.limitbtm;
    LampSetPos(&scrpos_x, lamp_state.foreground.x);
    LampSetPos(&scrpos_y, lamp_state.foreground.y);
    LampSetPos(&bg_scrpos_x, lamp_state.background.x);
    LampSetPos(&bg_scrpos_y, lamp_state.background.y);
    LampSetPos(&bg2_scrpos_x, lamp_state.background2.x);
    LampSetPos(&bg2_scrpos_y, lamp_state.background2.y);
    LampSetPos(&bg3_scrpos_x, lamp_state.background3.x);
    LampSetPos(&bg3_scrpos_y, lamp_state.background3.y);

    if (LEVEL_ZONE(level_id) == ZoneId_LZ) {  // Is this Labyrinth Zone?
        wtr_pos2 = lamp_state.water_level.pos;
        wtr_routine = lamp_state.water_level.routine;
        wtr_state = lamp_state.water_level.state;
    }

    if ((int8_t)last_lamp >= 0) {
        return;
    }

    uint16_t ds = lamp_state.spawn.x - 0xA0;
    limit_left2 = ds;
}

// Level state
uint16_t level_id;

uint8_t dle_routine;
// GHZ2-only: debounce timer for DynamicLevelEvents' own tube-exit
// boundary shrink (see its own comment). Not part of real hardware --
// added so the shrink doesn't apply the instant Sonic's X crosses the
// threshold while he may still be deep in the tube.
static uint16_t ghz2_tube_exit_timer;

uint16_t limit_left1, limit_right1, limit_top1, limit_btm1;
uint16_t limit_left2, limit_right2, limit_top2, limit_btm2;
uint16_t limit_left3;
uint16_t limit_top_db, limit_btm_db;

LevelAnim level_anim[6];

uint8_t last_lamp;
uint8_t prev_lamp;
CheckpointState lamp_state;

uint16_t restart;
uint16_t pause_state;
uint8_t time_over;

uint16_t frame_count;

// Player state
uint32_t score;
LevelTime level_time;
uint16_t rings;
uint8_t lives;
uint8_t continues;

uint32_t score_life;

uint16_t air;
uint8_t last_special;

uint8_t life_num;
uint8_t life_count;
uint8_t ring_count;
uint8_t time_count;
uint8_t score_count;

uint8_t shield;
uint8_t invincibility;
uint8_t shoes;
uint8_t debug_use;
uint8_t debug_item;
uint8_t debug_speed;
uint8_t debug_speed_timer;
uint8_t debug_subtype;

// Water state
int16_t wtr_pos1, wtr_pos2, wtr_pos3;
uint8_t water;
uint8_t wtr_routine;


// Loaded level data

// Object state

uint16_t opl_routine;


int16_t obj31_ypos;
uint8_t boss_status;
uint8_t lock_screen;
uint16_t gfx_big_ring;
uint8_t convey_rev;
uint8_t obj63[6];
uint8_t tunnel_mode;
uint8_t lock_multi;
uint8_t tunnel_allow;
uint8_t jump_only;
uint8_t obj6B;
uint8_t lock_ctrl;
uint8_t big_ring;
uint8_t ending_eggmobile_exploding;
uint16_t item_bonus;
uint16_t time_bonus;
uint16_t ring_bonus;
uint8_t endact_bonus;
uint8_t sonicend;
uint16_t lz_deform;
uint8_t f_switch[0x10];
bool f_wtunneldisallow = false;
bool f_lz1tunnel_open = false;
uint8_t obj63_loaded[0x80];
bool f_slidemode = false;

Oscillatory oscillatory;

LevelAnim sprite_anim[4];
uint16_t sprite_anim_3buf;

// Game functions
void AddPoints(uint16_t points) {
    // Update HUD
    score_count = 1;

    // Increase score
    if ((score += points) >= 999999)
        score = 999999;

    // Check if we should be rewarded an extra life every 50000 points (the score counts in tens, so 5000).
    // Real REV01 hardware has this same mechanic (repurposed from an unused
    // REV00 high-score-copy value), but hard-gates the actual award to the
    // Japanese region only -- overseas carts silently update the
    // requirement and never get the life. Sonic 2 onward removed that
    // region gate, awarding it everywhere; matching that here instead.
    if (score >= score_life) {
        score_life += 5000;
        lives++;
        life_count++;
        PlayMusic(bgm_ExtraLife);
    }
}

// Level loading
void LoadLevelMaps(void) {
    // Get header
    const LevelHeader* header = &level_header[LEVEL_ZONE(level_id)];

    // Load chunk maps and tile map
    KosDec(header->map128, level_map128);
    KosDec(header->map16, level_map16);
}

void LoadLevelLayout(void) {
    // Load the single interleaved FG+BG layout blob (Kosinski-compressed,
    // matches level_layout's flat LEVEL_LAYOUT_ROWS*LEVEL_LAYOUT_ROW_STRIDE
    // shape exactly, one KosDec call decodes both planes at once)
    memset(level_layout, 0, sizeof(level_layout));
    KosDec(level_layouts[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)].layout, level_layout);
}

void LevelSizeLoad(void) {
    // Reset level state
    dle_routine = 0;
    ghz2_tube_exit_timer = 0;

    // Get sizes to load
    const int16_t* sizes = LevelSizes(LEVEL_ZONE(level_id), LEVEL_ACT(level_id));

    // Load sizes and other stuff
    /* FFFFF730 = */ sizes++;
    limit_left2 = *sizes;
    limit_left1 = *sizes++;
    limit_right2 = *sizes;
    limit_right1 = *sizes++;
    limit_top2 = *sizes;
    limit_top1 = *sizes++;
    limit_btm2 = *sizes;
    limit_btm1 = *sizes++;
    limit_left3 = limit_left2 + 0x240;
    look_shift = *sizes++;

    // Load player start
    int16_t x, y;
    if (last_lamp) {
        Obj_Checkpoint_LoadInfo();
        x = player->pos.l.x.f.u;
        y = player->pos.l.y.f.u;
    } else {
        if (demo < 0) {
            // In an ending (credits) demo: where each of them starts, indexed by the credits page after it
            static const uint16_t EndingStartLocArray[8][2] = {
                { 0x0050, 0x03B0 }, { 0x0EA0, 0x046C }, { 0x1750, 0x00BD }, { 0x0A00, 0x062C },
                { 0x0BB0, 0x004C }, { 0x1570, 0x016C }, { 0x01B0, 0x072C }, { 0x1400, 0x02AC },
            };
            int demo_index = (credits_num - 1) & 7;
            x = (int16_t)EndingStartLocArray[demo_index][0];
            y = (int16_t)EndingStartLocArray[demo_index][1];
        } else {
            // Level
            x = StartLocArray[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)][0];
            y = StartLocArray[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)][1];
        }

        player->pos.l.x.f.u = x;
        player->pos.l.y.f.u = y;
    }

    // CLI test hook: override Sonic's start position (e.g. to drop straight
    // into a boss fight instead of walking there).
    if (cli_start_x >= 0)
        x = (int16_t)cli_start_x;
    if (cli_start_y >= 0)
        y = (int16_t)cli_start_y;
    player->pos.l.x.f.u = x;
    player->pos.l.y.f.u = y;

    // Clip camera position against left and right
    if ((x -= (SCREEN_WIDTH / 2)) < 0) // 0 instead of limit_left
        x = 0;
    if (x >= limit_right2)
        x = limit_right2;
    scrpos_x.f.u = x;

    // Clip camera position against top and bottom
    if ((y -= (96 + SCREEN_TALLADD2)) < 0) // 0 instead of limit_top
        y = 0;
    if (y >= limit_btm2)
        y = limit_btm2;
    scrpos_y.f.u = y;

    // Load other level stuff
    BgScrollSpeed(x, y);

    const int16_t* scroll_size = BGScrollBlockSizes[LEVEL_ZONE(level_id)];
    scroll_block1_size = *scroll_size++;
    scroll_block2_size = *scroll_size++;
    scroll_block3_size = *scroll_size++;
    scroll_block4_size = *scroll_size++;
}

// Scratch RAM for decoding the main terrain tileset before it's copied to
// VRAM -- Kosinski has no VRAM-aware streaming decoder here (unlike NemDec),
// so it decodes to RAM first, same as every other Kosinski resource. Sized
// for the largest real zone (SYZ, 882 tiles / 0x6E40 bytes) with headroom;
// this buffer is purely transient, not read again once VDP_WriteVRAM runs.
static uint8_t level_art_scratch[0x7000];

void LevelDataLoad(void) {
    // Get header
    const LevelHeader* header = &level_header[LEVEL_ZONE(level_id)];

    // Load chunk maps and tile map
    KosDec(header->map128, level_map128);
    KosDec(header->map16, level_map16);

    // Load the main terrain tileset at VRAM tile 0 -- the block/chunk tile
    // indices read directly from here. Each zone's PLC decoration list
    // starts well clear of the real tile counts involved (checked against
    // SonLVL's real per-zone tile limits when this was wired up), so this
    // never collides with PLC_GHZ/PLC_LZ/etc's own art.
    uint8_t *art_end = KosDec(header->art, level_art_scratch);
    VDP_SeekVRAM(0x0000);
    VDP_WriteVRAM(level_art_scratch, (size_t)(art_end - level_art_scratch));

    // Load level layout
    LoadLevelLayout();

    // Load level palette
    PaletteId pal = header->pal;
    if (level_id == LEVEL_ID(ZoneId_LZ, 3))
        pal = PalId_SBZ3;
    if (level_id == LEVEL_ID(ZoneId_SBZ, 1) || level_id == LEVEL_ID(ZoneId_SBZ, 2))
        pal = PalId_SBZ2;
    PalLoad1(pal);

    // Load level art
    if (header->plc2 != 0)
        AddPLC(header->plc2);
}

void ColIndexLoad(void) {
    // Decode the zone's two independent Kosinski-compressed collision
    // indices, one per path. Both currently hold identical data (no real
    // content path-swaps exist yet -- Obj03 isn't ported to C), but they're
    // separate resources, not a single blob decoded twice.
    // The ending sequence (zone EndZ) uses Green Hill's collision
    int zone = LEVEL_ZONE(level_id) < ZoneId_EndZ ? LEVEL_ZONE(level_id) : ZoneId_GHZ;
    KosDec(level_coli[zone][0], coll_index[0]);
    KosDec(level_coli[zone][1], coll_index[1]);
    collision_path = 0;
}

// Dynamic level events
void DynamicLevelEvents(void) {
    // Every transition in this function is camera-based (scrpos_x/y),
    // matching real hardware exactly -- MZ1's own case 0 used to special-
    // case a fabricated Sonic-position check here (mistakenly borrowed from
    // PushBlock's own, unrelated stomper-button trigger zone), which made
    // that transition permanently unreachable once Sonic walked past the
    // made-up X window. Real hardware's own DLE_MZ1_0 has no X-gate at all,
    // just camera Y crossing $340 -- see that case's own comment.

    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_GHZ:
        switch (LEVEL_ACT(level_id)) {
        case 0: // Act 1
            if ((uint16_t)scrpos_x.f.u >= (0x1780 - SCREEN_WIDEADD2))
                limit_btm1 = 0x400 - SCREEN_TALLADD;
            else
                limit_btm1 = 0x300 - SCREEN_TALLADD;
            break;
        case 1: // Act 2
            limit_btm1 = 0x300 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0xED0 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x200 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0x1600 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x400 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0x1D60 - SCREEN_WIDEADD2)) {
                ghz2_tube_exit_timer = 0;
                break;
            }
            // Real hardware shrinks the boundary back down the instant X
            // crosses 0x1D60, with nothing accounting for Sonic still
            // being deep in the tube at that point -- added a 20-second
            // (1200 frame) debounce here so the shrink only applies once
            // he's plausibly actually clear of it, resetting above if he
            // drifts back before the delay elapses.
            if (++ghz2_tube_exit_timer < 1200)
                break;
            limit_btm1 = 0x300 - SCREEN_TALLADD;
            break;
        case 2: // Act 3
            switch (dle_routine) {
            case 0:
                limit_btm1 = 0x300 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_x.f.u < (0x380 - SCREEN_WIDEADD2))
                    break;
                limit_btm1 = 0x310 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_x.f.u < (0x960 - SCREEN_WIDEADD2))
                    break;
                if (scrpos_y.f.u >= 0x280 + SCREEN_TALLADD) {
                    limit_btm1 = 0x400 - SCREEN_TALLADD;
                    if ((uint16_t)scrpos_x.f.u < (0x1380 - SCREEN_WIDEADD2)) {
                        limit_btm1 = 0x4C0 - SCREEN_TALLADD;
                        limit_btm2 = 0x4C0 - SCREEN_TALLADD;
                    } else if ((uint16_t)scrpos_x.f.u >= (0x1700 - SCREEN_WIDEADD2)) {
                        limit_btm1 = 0x300 - SCREEN_TALLADD;
                        dle_routine += 2;
                    }
                } else {
                    limit_btm1 = 0x300 - SCREEN_TALLADD;
                    dle_routine += 2;
                }
                break;
            case 2:
                if ((uint16_t)scrpos_x.f.u < (0x960 - SCREEN_WIDEADD2))
                    dle_routine -= 2;
                if ((uint16_t)scrpos_x.f.u < (0x2960 - SCREEN_WIDEADD2))
                    break;
                {
                    Object *boss = FindFreeObj();
                    if (boss != NULL) {
                        boss->type = ObjId_BossGreenHill;
                        boss->pos.l.x.f.u = 0x2960 + 0x100;
                        boss->pos.l.y.f.u = 0x300 - 0x80;
                    }
                }
                QueueSound1(bgm_Boss);
                lock_screen = true;
                dle_routine += 2;
                AddPLC(PlcId_Boss);
                break;
            case 4:
                // Continuously pin the left boundary to the camera's
                // current position so Sonic can't scroll back out of the
                // boss arena for the rest of the fight.
                limit_left2 = (uint16_t)scrpos_x.f.u;
                break;
            }
            break;
        }
        break;
    case ZoneId_LZ:
        switch (LEVEL_ACT(level_id)) {
        case 0: case 1: // Acts 1 & 2 -- no events
            break;
        case 2: // Act 3
            if (f_switch[0xF] &&
                !(LEVEL_LAYOUT_FG(5)[12] == 0x17 && LEVEL_LAYOUT_FG(5)[13] == 0x18)) {
                // MJ: modify level layout (P128 overwrites two chunks instead
                // of one) -- opens an exit from the water slide section by
                // replacing the slide-through chunks DynWater_LZ3 opened
                // earlier with a slide+landing pair.
                LEVEL_LAYOUT_FG(5)[12] = 0x17;
                LEVEL_LAYOUT_FG(5)[13] = 0x18;
                QueueSound2(sfx_Rumbling);
            }
            if (dle_routine == 0) {
#ifdef SCP_FIX_BUGS
                // Load the boss earlier so Eggman's art has finished
                // decompressing before he's on screen.
                uint16_t boss_cam_x = 0x1DE0 - 0x1C0; // boss_lz_x-$1C0
#else
                uint16_t boss_cam_x = 0x1DE0 - 0x140; // boss_lz_x-$140
#endif
                if ((uint16_t)scrpos_x.f.u >= (uint16_t)(boss_cam_x - SCREEN_WIDEADD2) &&
                    (uint16_t)scrpos_y.f.u < 0xC0 + 0x540) { // boss_lz_y+$540
                    Object *boss = FindFreeObj();
                    if (boss != NULL) {
                        memset(boss, 0, sizeof(Object)); // don't inherit a stale slot
                        boss->mappings = NULL;
                        boss->type = ObjId_BossLabyrinth;
                    }
                    QueueSound1(bgm_Boss);
                    lock_screen = true;
                    dle_routine += 2;
                    AddPLC(PlcId_Boss);
                }
            }
            break;
        case 3: // Act 4 (SBZ3): reaching the top of the level at the far right drops into Final Zone (DLE_SBZ3)
            if ((uint16_t)scrpos_x.f.u < (0xD00 - SCREEN_WIDEADD2))
                break;
            if (player->pos.l.y.f.u >= 0x18) // not at the top yet
                break;
            last_lamp = 0;
            restart = true;
            level_id = LEVEL_ID(ZoneId_SBZ, 2); // id_FZ
            lock_multi = 1;                     // f_playerctrl: lock controls, position and animation
            break;
        }
        break;
    case ZoneId_MZ:
        switch (LEVEL_ACT(level_id)) {
        case 0: // Act 1 -- reversible 4-state boundary chain (can step forward OR backward as the camera moves)
            switch (dle_routine) {
            case 0:
                limit_btm1 = 0x1D0 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_x.f.u < (0x700 - SCREEN_WIDEADD2))
                    break;
                limit_btm1 = 0x220 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_x.f.u < (0xD00 - SCREEN_WIDEADD2))
                    break;
                limit_btm1 = 0x340 - SCREEN_TALLADD;
                // Matches the real DLE_MZ1_0 exactly: the only gate here is
                // the camera's own Y position crossing $340 -- no X check of
                // any kind. An earlier version of this added a fabricated
                // sonic_x window (mistakenly borrowed from PushBlock's own,
                // unrelated stomper-button trigger zone) plus a switch to
                // Sonic's own Y instead of the camera's -- neither exists in
                // real hardware's own DLE_MZ1_0, and the X-gate specifically
                // made this transition permanently unreachable once Sonic
                // walked past that window, even though the real (camera-Y-
                // only) condition had already been satisfied.
                if ((uint16_t)scrpos_y.f.u < (0x340 + SCREEN_TALLADD))
                    break;
                dle_routine += 2;
                break;
            case 2:
                if ((uint16_t)scrpos_y.f.u < (0x340 + SCREEN_TALLADD)) {
                    dle_routine -= 2;
                    break;
                }
                limit_top2 = 0;
                if ((uint16_t)scrpos_x.f.u >= (0xE00 - SCREEN_WIDEADD2))
                    break;
                limit_top2 = 0x340 - SCREEN_TALLADD;
                limit_btm1 = 0x340 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_x.f.u >= (0xA90 - SCREEN_WIDEADD2))
                    break;
                limit_btm1 = 0x500 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_y.f.u < (0x370 + SCREEN_TALLADD))
                    break;
                dle_routine += 2;
                break;
            case 4:
                if ((uint16_t)scrpos_y.f.u < (0x370 + SCREEN_TALLADD)) {
                    dle_routine -= 2;
                    break;
                }
                if ((uint16_t)scrpos_y.f.u < (0x500 + SCREEN_TALLADD))
                    break;
                if ((uint16_t)scrpos_x.f.u < (0xB80 - SCREEN_WIDEADD2))
                    break;
                limit_top2 = 0x500 - SCREEN_TALLADD;
                dle_routine += 2;
                break;
            case 6:
                if ((uint16_t)scrpos_x.f.u < (0xB80 - SCREEN_WIDEADD2)) {
                    if (limit_top2 != (uint16_t)(0x340 - SCREEN_TALLADD))
                        limit_top2 -= 2;
                    break;
                }
                if (limit_top2 != (uint16_t)(0x500 - SCREEN_TALLADD)) {
                    if ((uint16_t)scrpos_y.f.u < (0x500 + SCREEN_TALLADD))
                        break; // real ASM's blo.s .exit here returns immediately, skipping the $E70 check below entirely
                    limit_top2 = 0x500 - SCREEN_TALLADD;
                }
                if ((uint16_t)scrpos_x.f.u < (0xE70 - SCREEN_WIDEADD2))
                    break;
                limit_top2 = 0;
                limit_btm1 = 0x500 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_x.f.u < (0x1430 - SCREEN_WIDEADD2))
                    break;
                limit_btm1 = 0x210 - SCREEN_TALLADD;
                break;
            }
            break;
        case 1: // Act 2
            limit_btm1 = 0x520 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0x1700 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x200 - SCREEN_TALLADD;
            break;
        case 2: // Act 3
            switch (dle_routine) {
            case 0: // DLE_MZ3_Boss
                limit_btm1 = 0x720 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_x.f.u < (0x1560 - SCREEN_WIDEADD2)) // boss_mz_x-0x2A0
                    break;
                limit_btm1 = 0x210 - SCREEN_TALLADD; // boss_mz_y
                if ((uint16_t)scrpos_x.f.u < (0x17F0 - SCREEN_WIDEADD2)) // boss_mz_x-0x10
                    break;
                {
                    Object *boss = FindFreeObj();
                    if (boss != NULL) {
                        boss->type = ObjId_BossMarble;
                        boss->pos.l.x.f.u = 0x1800 + 0x1F0; // boss_mz_x+0x1F0
                        boss->pos.l.y.f.u = 0x210 + 0x1C;   // boss_mz_y+0x1C
                    }
                }
                QueueSound1(bgm_Boss);
                lock_screen = true;
                dle_routine += 2;
                AddPLC(PlcId_Boss);
                break;
            case 2: // DLE_MZ3_End
                limit_left2 = scrpos_x.f.u; // camera-freeze at the level's end, deliberately still camera-based
                break;
            }
            break;
        }
        break;
    case ZoneId_SLZ:
        if (LEVEL_ACT(level_id) != 2)
            break; // acts 1 and 2 have no events
        switch (dle_routine) {
        case 0: // DLE_SLZ3_Main
            if ((uint16_t)scrpos_x.f.u < (0x1E70 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x210 - SCREEN_TALLADD; // boss_slz_y
            dle_routine += 2;
            break;
        case 2: // DLE_SLZ3_Boss
            if ((uint16_t)scrpos_x.f.u < (0x2000 - SCREEN_WIDEADD2)) // boss_slz_x
                break;
            {
                Object *boss = FindFreeObj();
                if (boss != NULL)
                    boss->type = ObjId_BossStarLight;
            }
            QueueSound1(bgm_Boss);
            lock_screen = true;
            dle_routine += 2;
            AddPLC(PlcId_Boss);
            break;
        case 4: // DLE_SLZ3_End
            limit_left2 = scrpos_x.f.u;
            break;
        }
        break;
    case ZoneId_SYZ:
        switch (LEVEL_ACT(level_id)) {
        case 0: // Act 1 -- no events
            break;
        case 1: // Act 2
            limit_btm1 = 0x520 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0x25A0 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x420 - SCREEN_TALLADD;
            if ((uint16_t)player->pos.l.y.f.u < 0x4D0)
                break;
            limit_btm1 = 0x520 - SCREEN_TALLADD;
            break;
        case 2: // Act 3
            switch (dle_routine) {
            case 0: // DLE_SYZ3_Main
                if ((uint16_t)scrpos_x.f.u < (0x2AC0 - SCREEN_WIDEADD2)) // boss_syz_x-0x140
                    break;
                {
                    Object *blocks = FindFreeObj();
                    if (blocks != NULL)
                        blocks->type = ObjId_BossBlock;
                }
                dle_routine += 2;
                break;
            case 2: // DLE_SYZ3_Boss
                if ((uint16_t)scrpos_x.f.u < (0x2C00 - SCREEN_WIDEADD2)) // boss_syz_x
                    break;
                limit_btm1 = 0x4CC - SCREEN_TALLADD; // boss_syz_y
                {
                    Object *boss = FindFreeObj();
                    if (boss != NULL)
                        boss->type = ObjId_BossSpringYard;
                }
                QueueSound1(bgm_Boss);
                lock_screen = true;
                dle_routine += 2;
                AddPLC(PlcId_Boss);
                break;
            case 4: // DLE_SYZ3_End
                limit_left2 = scrpos_x.f.u; // camera-freeze at the level's end, deliberately still camera-based
                break;
            }
            break;
        }
        break;
    case ZoneId_SBZ:
        switch (LEVEL_ACT(level_id)) {
        case 0: // Act 1
            limit_btm1 = 0x720 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0x1880 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x620 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0x2000 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x2A0 - SCREEN_TALLADD;
            break;
        case 1: // Act 2 (ends with Eggman's false-floor cutscene)
            switch (dle_routine) {
            case 0: // DLE_SBZ2_Main
                limit_btm1 = 0x800 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_x.f.u < (0x1800 - SCREEN_WIDEADD2))
                    break;
                limit_btm1 = 0x510 - SCREEN_TALLADD; // boss_sbz2_y
                if ((uint16_t)scrpos_x.f.u < (0x1E00 - SCREEN_WIDEADD2))
                    break;
                dle_routine += 2;
                break;
            case 2: // DLE_SBZ2_Blocks
                if ((uint16_t)scrpos_x.f.u < (0x2050 - 0x1A0 - SCREEN_WIDEADD2)) // boss_sbz2_x-$1A0
                    break;
                {
                    Object *floor = FindFreeObj();
                    if (floor == NULL)
                        break;
                    floor->type = ObjId_FalseFloor;
                }
                dle_routine += 2;
                AddPLC(PlcId_EggmanSBZ2);
                break;
            case 4: // DLE_SBZ2_Eggman
                if ((uint16_t)scrpos_x.f.u >= (0x2050 - 0xF0 - SCREEN_WIDEADD2)) { // boss_sbz2_x-$F0
                    Object *eggman = FindFreeObj();
                    if (eggman != NULL) {
                        eggman->type = ObjId_ScrapEggman;
                        dle_routine += 2;
                    }
                    lock_screen = true;
                }
                limit_left2 = scrpos_x.f.u; // DLE_SBZ2_SetBoundary
                break;
            case 6: // DLE_SBZ2_End
                if ((uint16_t)scrpos_x.f.u < (0x2050 - SCREEN_WIDEADD2)) // boss_sbz2_x
                    limit_left2 = scrpos_x.f.u;
                break;
            }
            // The cutscene's trigger for SBZ3 (stage 0x0103, Labyrinth Zone act 4): once Eggman's false floor has
            // dropped Sonic below the cutscene room, load it. Replaces the original's special case in Sonic's
            // bottom-boundary check, which let SBZ2 fall out past x $2000 instead of dying.
            // Sonic must be past the end-of-act sign (the signpost object is at x $1EE0 in SBZ2), as well as in
            // the cutscene room.
            if (dle_routine >= 6 && player->pos.l.x.f.u > 0x1EE0 && player->pos.l.x.f.u >= 0x2000 &&
                (uint16_t)player->pos.l.y.f.u > 0x510 + 0xE0) { // boss_sbz2_y+$E0, just under the false floor
                last_lamp = 0;
                restart = true;
                level_id = LEVEL_ID(ZoneId_LZ, 3); // 0x0103
            }
            break;
        case 2: // Final Zone
            switch (dle_routine) {
            case 0: // DLE_FZ_Main
                if ((uint16_t)scrpos_x.f.u >= (0x2450 - 0x308 - SCREEN_WIDEADD2)) { // boss_fz_x-$308
                    dle_routine += 2;
                    AddPLC(PlcId_FZBoss);
                }
                limit_left2 = scrpos_x.f.u;
                break;
            case 2: // DLE_FZ_Boss
                if ((uint16_t)scrpos_x.f.u >= (0x2450 - 0x150 - SCREEN_WIDEADD2)) { // boss_fz_x-$150
                    Object *boss = FindFreeObj();
                    if (boss != NULL) {
                        boss->type = ObjId_BossFinal;
                        dle_routine += 2;
                        lock_screen = true;
                    }
                }
                limit_left2 = scrpos_x.f.u;
                break;
            case 4: // DLE_FZ_Arena
                if ((uint16_t)scrpos_x.f.u >= (0x2450 - SCREEN_WIDEADD2)) // boss_fz_x: the boss arena
                    dle_routine += 2;
                limit_left2 = scrpos_x.f.u;
                break;
            case 6: // DLE_FZ_Wait: wait until the boss is beaten
                break;
            case 8: // DLE_FZ_End: allow scrolling right
                limit_left2 = scrpos_x.f.u;
                break;
            }
            break;
        }
        break;
    default:
        break;
    }

    // Update scroll limits
    int16_t scroll_diff = limit_btm1 - limit_btm2;
    int16_t spd = 2;

    if (scroll_diff < 0) {
        if ((uint16_t)scrpos_y.f.u > limit_btm1)
            limit_btm2 = scrpos_y.f.u & ~1;
        limit_btm2 -= spd;
        bgscrollvert = true;
    } else if (scroll_diff > 0) {
        if (((uint16_t)scrpos_y.f.u + 8) >= limit_btm2 && player->status.p.f.in_air)
            spd *= 4;
        limit_btm2 += spd;
        bgscrollvert = true;
    }
}

// Object animation
void SynchroAnimate(void) {
    // Spiked log
    if (--sprite_anim[0].time < 0) {
        sprite_anim[0].time = 11;
        sprite_anim[0].frame = (sprite_anim[0].frame - 1) & 7;
    }

    // Rings
    if (--sprite_anim[1].time < 0) {
        sprite_anim[1].time = 7;
        sprite_anim[1].frame = (sprite_anim[1].frame + 1) & 3;
    }

    // Unused
    if (--sprite_anim[2].time < 0) {
        sprite_anim[2].time = 7;
        if (++sprite_anim[2].frame >= 6)
            sprite_anim[2].frame = 0;
    }

    // Bouncing rings
    if (sprite_anim[3].time != 0) {
        // WACKY!!
        sprite_anim_3buf += (uint8_t)sprite_anim[3].time;
        sprite_anim[3].frame = (sprite_anim_3buf >> 9) & 3;
        sprite_anim[3].time--;
    }
}

// Signpost loading
void SignpostArtLoad(void) {
    // Check if signpost should load
    if (debug_use || (level_id & 0xFF) == 2)
        return;

    // Check if we've reached the end of the level
    int16_t end_x = limit_right2 - 0x100 - SCREEN_WIDEADD2;
    if (scrpos_x.f.u >= end_x && time_count && limit_left2 != end_x) {
        limit_left2 = end_x;
        NewPLC(PlcId_Signpost);
    }
}

// Level object loading: the engine's objects manager (ObjectsManager.h) on Sonic 1's layouts. Its load range (0x280 pixels ahead of the camera, 0x80 behind) is the
// original 320-pixel picture's, whatever the picture size: see IS_OFFSCREEN.
ObjectsManager objects_manager;

void ObjPosLoad(void) {
    if (opl_routine == 0) {
        opl_routine = 2;

        // Sonic 1's own resets at the start of a level
        memset(obj63_loaded, 0, sizeof(obj63_loaded));
        f_lz1tunnel_open = false; // doors respawn shut along with the remembered object state

        static const ObjectsManagerConfig config = OBJECTS_MANAGER_DEFAULT_CONFIG;
        ObjectsManager_Init(&objects_manager, &config, objstate, sizeof(objstate), level_obj[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)], scrpos_x.f.u);
        Rings_Init(level_ring[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)], scrpos_x.f.u);
    } else {
        ObjectsManager_Update(&objects_manager, scrpos_x.f.u);
        Rings_Update(scrpos_x.f.u);
    }
}

// The level's water: Sonic 1 has it in Labyrinth Zone only (see Level.h)
#include "Object/WaterSurface.h"

bool Level_HasWater(void) {
    return LEVEL_ZONE(level_id) == ZoneId_LZ;
}

// All three water heights start at the act's WaterHeight entry (LZ act 4 is SBZ3, see [[project_lz_act4_sbz3]])
int16_t Level_WaterStartHeight(void) {
    static const int16_t WaterHeight[4] = { 0xB8, 0x328, 0x900, 0x228 };
    return WaterHeight[LEVEL_ACT(level_id)];
}

// (Act 3 is the SBZ3-under-LZ slot: purple water, not LZ's usual green)
void Level_LoadWaterPalettes(bool sonic) {
    if (sonic)
        PalLoad3_Water((LEVEL_ACT(level_id) == 3) ? PalId_SonicSBZ : PalId_SonicLZ);
    else
        PalLoad4_Water((LEVEL_ACT(level_id) == 3) ? PalId_SBZ3Water : PalId_LZWater);
}

void Level_MakeWaterSurfaces(void) {
    objects[WATERSURFACE_SLOT_LEFT].type = ObjId_WaterSurface;
    objects[WATERSURFACE_SLOT_LEFT].pos.l.x.f.u = 0x60;
    objects[WATERSURFACE_SLOT_RIGHT].type = ObjId_WaterSurface;
    objects[WATERSURFACE_SLOT_RIGHT].pos.l.x.f.u = 0x120;
    objects[WATERSURFACE_SLOT_EXTRA].type = ObjId_WaterSurface;
    objects[WATERSURFACE_SLOT_EXTRA].pos.l.x.f.u = 0x1E0;
}

// The level's music: one song per zone in Sonic 1's order, except Scrap Brain act 3 (stored under Labyrinth's slot, 0103) and the Final Zone (under Scrap Brain's, 0502)
uint8_t Level_Music(uint16_t level) {
    static const uint8_t zone_music[] = {bgm_GHZ, bgm_LZ, bgm_MZ, bgm_SLZ, bgm_SYZ, bgm_SBZ};
    if (level == 0x0103)
        return bgm_SBZ;
    if (level == 0x0502)
        return bgm_FZ;
    uint8_t zone = LEVEL_ZONE(level);
    return zone < sizeof(zone_music) ? zone_music[zone] : 0;
}
