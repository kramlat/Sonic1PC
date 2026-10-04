#include "Level.h"
#include "HTZQuake.h"

#include "Constants.h"
#include "Enigma.h"
#include "Game.h"
#include "Kosinski.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "SplitScreen.h"
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

// Sonic 2's level code: Sonic 1's Level.c (a copy; Sonic 1's stays as it was) with Nick Arcade's dynamic level events.

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
    HTZQuake_Reset();

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
static uint8_t level_art_scratch[0x8000]; // (Sonic 1's is 0x7000: Nick Arcade's Emerald Hill and Hill Top tilesets are 912 tiles, 0x7200 bytes)

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
    int zone = LEVEL_ZONE(level_id);
    KosDec(level_coli[zone][0], coll_index[0]);
    KosDec(level_coli[zone][1], coll_index[1]);
    collision_path = 0;
}

// Dynamic level events: Nick Arcade's per-zone events (s2.asm, DynResize_*). Green Hill's are Sonic 1's own; Chemical Plant's, Emerald Hill's act 1 and Hidden Palace's act 1 have none
// (the Marble and Spring Yard code Sonic 1 puts in those slots is "leftover from Sonic 1" there and never runs), and the zones with a boss end in an arena whose boss Nick Arcade has
// but this port has not ported yet (the boss objects are empty, so the camera locks there and nothing else happens). The camera limits are named as in Sonic 1: limit_btm1 is
// Camera_Max_Y_pos_target, limit_btm2 Camera_Max_Y_pos, limit_top2 Camera_Min_Y_pos, limit_left2 Camera_Min_X_pos.
void DynamicLevelEvents(void) {
    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_EHZ:
        if (LEVEL_ACT(level_id) != 1)
            break; // (act 1 has none, and act 3 is a copy of it)
        switch (dle_routine) {
        case 0: // the boss arena
            if ((uint16_t)scrpos_x.f.u < (0x26E0 - SCREEN_WIDEADD2))
                break;
            limit_left2 = scrpos_x.f.u;
            limit_btm1 = 0x390 - SCREEN_TALLADD;
            limit_btm2 = 0x390 - SCREEN_TALLADD;
            dle_routine += 2;
            {
                Object *boss = FindFreeObj();
                if (boss != NULL) {
                    boss->type = 0x55; // the EHZ boss
                    boss->scratch.u8[0] = 0x81; // (the subtype)
                    boss->pos.l.x.f.u = 0x29D0;
                    boss->pos.l.y.f.u = 0x426;
                }
            }
            QueueSound1(bgm_Boss);
            lock_screen = true;
            AddPLC(PlcId_Boss);
            break;
        case 2:
            if ((uint16_t)scrpos_x.f.u < (0x2880 - SCREEN_WIDEADD2))
                break;
            limit_left2 = 0x2880 - SCREEN_WIDEADD2;
            dle_routine += 2;
            break;
        case 4: // when the boss is beaten, Nick Arcade goes back to the Sega screen
            if (boss_status)
                gamemode = GameMode_Sega;
            break;
        }
        break;
    case ZoneId_HPZ:
        switch (LEVEL_ACT(level_id)) {
        case 1: // act 2
            limit_btm1 = 0x520 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0x25A0 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x420 - SCREEN_TALLADD;
            if ((uint16_t)player->pos.l.y.f.u < 0x4D0)
                break;
            limit_btm1 = 0x520 - SCREEN_TALLADD;
            break;
        }
        break;
    case ZoneId_HTZ:
        HTZQuake_Events(); // (the prototype's: Hill Top's earthquake; Nick Arcade's boss arenas are not in it)
        break;
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
    // Check if signpost should load (the prototype's End_Level_Art_Load: not in Emerald Hill's act 2, the boss's, where its art load would clear the queue of the art being loaded; every other act, Chemical
    // Plant's and Neo Green Hill's second too, has a signpost and loads its art. Nick Arcade skipped every second act.)
    if (debug_use || level_id == LEVEL_ID(ZoneId_EHZ, 1))
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
static ObjectsManager2P objects_manager_2p; // (a split screen: one manager for each camera, on the same layout and marks)

void ObjPosLoad(void) {
    if (opl_routine == 0) {
        opl_routine = 2;

        // Sonic 1's own resets at the start of a level
        memset(obj63_loaded, 0, sizeof(obj63_loaded));
        f_lz1tunnel_open = false; // doors respawn shut along with the remembered object state

        static const ObjectsManagerConfig config = OBJECTS_MANAGER_DEFAULT_CONFIG;
        if (camera_split) {
            ObjectsManager2P_Init(&objects_manager_2p, &config, objstate, sizeof(objstate), level_obj[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)], scrpos_x.f.u, scrpos_x_p2.f.u);
            objects_manager = objects_manager_2p.view[0];
        } else {
            ObjectsManager_Init(&objects_manager, &config, objstate, sizeof(objstate), level_obj[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)], scrpos_x.f.u);
        }
        Rings_Init(level_ring[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)], scrpos_x.f.u);
    } else {
        if (camera_split) {
            ObjectsManager2P_Update(&objects_manager_2p, scrpos_x.f.u, scrpos_x_p2.f.u);
            objects_manager = objects_manager_2p.view[0]; // (what the rest of the game reads of it: the marks)
        } else {
            ObjectsManager_Update(&objects_manager, scrpos_x.f.u);
        }
        Rings_Update(scrpos_x.f.u); // (the rings only load around the first camera)
    }
}

// The level's water: Hidden Palace's (Nick Arcade: Water_flag is set for zone 4). Sonic 1's Labyrinth code does the rest in LZWaterFeatures.c's place, as Sonic 2's own copy of it.
#include "Object/WaterSurface.h"

bool Level_HasWater(void) {
    return LEVEL_ZONE(level_id) == ZoneId_HPZ || LEVEL_ZONE(level_id) == ZoneId_ARZ || (LEVEL_ZONE(level_id) == ZoneId_CPZ && LEVEL_ACT(level_id) == 1); // (Chemical Plant's second act: the first has none; Neo Green Hill's both do)
}

// WaterHeight: the first act's is $600 (the lower the number the higher the water; Tails' pad moves it)
int16_t Level_WaterStartHeight(void) {
    if (LEVEL_ZONE(level_id) == ZoneId_CPZ)
        return 0x710; // (the prototype's WaterHeight: Chemical Plant's second act)
    if (LEVEL_ZONE(level_id) == ZoneId_ARZ)
        return LEVEL_ACT(level_id) == 0 ? 0x410 : 0x510; // (Neo Green Hill's)
    static const int16_t WaterHeight[4] = { 0x600, 0x328, 0x900, 0x228 };
    return WaterHeight[LEVEL_ACT(level_id)];
}

void Level_LoadWaterPalettes(bool sonic) {
    // The prototype's CPZ and NGHZ underwater palettes cover all four lines, Sonic's first row too: that row is the one that is loaded live (PalLoad3_Water, as Sonic 1's Sonic palette is) and
    // the whole of it is the reference the fades go to (PalLoad4_Water); the live other three are reached by the fade-in. (Loading only the whole one left Sonic's row black underwater.)
    if (LEVEL_ZONE(level_id) == ZoneId_CPZ) {
        if (sonic)
            PalLoad3_Water(PalId_CPZWaterSonic);
        else
            PalLoad4_Water(PalId_CPZWater);
        return;
    }
    if (LEVEL_ZONE(level_id) == ZoneId_ARZ) {
        if (sonic)
            PalLoad3_Water(PalId_NGHZWaterSonic);
        else
            PalLoad4_Water(PalId_NGHZWater);
        return;
    }
    if (sonic)
        PalLoad3_Water((LEVEL_ACT(level_id) == 3) ? PalId_SonicSBZ : PalId_SonicLZ);
    else
        PalLoad4_Water(PalId_LZWater);
}

// Two surfaces (Nick Arcade's), each as wide as the screen was ($80 each side of its place), and a third for pictures wider than the original; their places follow the camera
void Level_MakeWaterSurfaces(void) {
    // (the extra one has a slot of its own here: Sonic 1's, 28, is Tails' in Sonic 2, and 27 is the splash's and the spin dash dust's (it was there, which kept the splash from ever showing))
    objects[WATERSURFACE_SLOT_LEFT].type = 0x04;
    objects[WATERSURFACE_SLOT_LEFT].scratch.u8[0] = 0;
    objects[WATERSURFACE_SLOT_RIGHT].type = 0x04;
    objects[WATERSURFACE_SLOT_RIGHT].scratch.u8[0] = 1;
    objects[22].type = 0x04;
    objects[22].scratch.u8[0] = 2;
    if (camera_split) { // the second view has its own three, along its own camera
        for (int i = 0; i < 3; i++) {
            objects[26 - i].type = 0x04;
            objects[26 - i].scratch.u8[0] = (uint8_t)(3 + i);
        }
    }
}

// The level's music, one song for each zone slot as the Simon Wai prototype's MusicList has it (its zone ids are these slots)
uint8_t Level_Music(uint16_t level) {
    static const uint8_t zone_music[ZoneId_Num] = {
        [0x00] = mus_GHZ, [0x01] = mus_GHZ, [0x02] = mus_MTZ, [0x03] = mus_SSZ, [0x04] = mus_MTZ, [0x05] = mus_MTZ, [0x06] = mus_BOZ, [0x07] = mus_HTZ,
        [0x08] = mus_HPZ, [0x09] = mus_RWZ, [0x0A] = mus_OOZ, [0x0B] = mus_DHZ, [0x0C] = mus_CNZ, [0x0D] = mus_CPZ, [0x0E] = mus_CPZ, [0x0F] = mus_NGHZ,
    };
    unsigned zone = LEVEL_ZONE(level);
    return zone < ZoneId_Num ? zone_music[zone] : 0;
}
