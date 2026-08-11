#include "GM_Title.h"

#include "Sound.h"
#include "Game.h"
#include "Level.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Nemesis.h"
#include "PLC.h"
#include "Palette.h"
#include "PaletteCycle.h"
#include "SpecialStage.h"
#include "Video.h"

#include "Backend/VDP.h"

#include <string.h>

// Level select's own uncompressed font (also used for HUD digits, see
// HUD_WriteHex) -- shared with the rest of the game, not a level-select-only
// asset. Declared (not #included -- Resource/Art/Text.h defines the array,
// so including it here too would be a duplicate definition at link time)
// since Game.c is the only place that includes the real header.
extern const uint8_t Art_Text[];

// Title screen state
uint8_t demo_num;

// ===========================================================================
// Level select
// ===========================================================================
// Matches Tit_ChkLevSel/LevelSelect/LevSelTextLoad/LevSelControls in the
// disassembly: enter Up, Down, Left, Right on the D-pad (in release builds
// only -- see Tit_ChkLevSel below), then hold A and press Start.

#define LEVSEL_LINE_COUNT   21
#define LEVSEL_LINE_LENGTH  24
#define LEVSEL_SNDTEST_ROW  (LEVSEL_LINE_COUNT - 1) // "SOUND SELECT"
#define LEVSEL_SS_ROW        (LEVSEL_LINE_COUNT - 2) // "SPECIAL STAGE"
#define LEVSEL_START_ROW    4
#define LEVSEL_START_COL    8
#define LEVSEL_VRAM_MAIN    (VRAM_BG + (LEVSEL_START_ROW << 7) + (LEVSEL_START_COL << 1))
#define LEVSEL_FONT_VRAM    0xD000 // ArtTile_Level_Select_Font ($680) * 32 bytes/tile
#define LEVSEL_SNDTEST_COL  (LEVSEL_LINE_LENGTH - 8) // column offset for the 2-digit sound number
// Highest registered sound/SFX ID, 0-based offset from bgm_GHZ (see
// enum SoundID, Sound.h) -- real hardware derives this from the assembled
// SoundIndex table's own size instead of a fixed constant, but that same
// effect here is just "the last ID we actually have data for". Was
// hard-coded as the real hardware's own $80-$D0 ID range until the
// SoundID enum got renumbered to start at 1 (driver-version-3 work,
// "0 is reserved -- id==0 means silence/stop") -- this constant (and the
// display/dispatch below) went stale at the same time and was silently
// selecting IDs $80-$D0, none of which exist in the renumbered sound_table
// (always NULL), so the sound test played nothing at all until this fix.
#define LEVSEL_SNDTEST_MAX  (sfx_Waterfall - bgm_GHZ)

// Persists across visits to level select, same as the real v_levselsound
// RAM variable (never reset on entry -- picks up where you left off).
static int levsel_sound = 0;

static const char *const levsel_text[LEVSEL_LINE_COUNT] = {
    "GREEN HILL ZONE  STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "MARBLE ZONE      STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "SPRING YARD ZONE STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "LABYRINTH ZONE   STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "STAR LIGHT ZONE  STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "SCRAP BRAIN ZONE STAGE 1",
    "                 STAGE 2",
    "                 STAGE 3",
    "FINAL ZONE",
    "SPECIAL STAGE",
    "SOUND SELECT",
};

// Level for each selectable row, in the same order as levsel_text. The last
// two rows (Final Zone, Special Stage) are handled specially below instead.
// Scrap Brain Zone Stage 3 and Final Zone are the same level in this port
// (LEVEL_ID(ZoneId_LZ, 3) -- see the water-palette check in GM_Level.c),
// just as they share underlying data on real hardware.
static const uint16_t levsel_levels[LEVSEL_LINE_COUNT - 2] = {
    LEVEL_ID(ZoneId_GHZ, 0), LEVEL_ID(ZoneId_GHZ, 1), LEVEL_ID(ZoneId_GHZ, 2),
    LEVEL_ID(ZoneId_MZ,  0), LEVEL_ID(ZoneId_MZ,  1), LEVEL_ID(ZoneId_MZ,  2),
    LEVEL_ID(ZoneId_SYZ, 0), LEVEL_ID(ZoneId_SYZ, 1), LEVEL_ID(ZoneId_SYZ, 2),
    LEVEL_ID(ZoneId_LZ,  0), LEVEL_ID(ZoneId_LZ,  1), LEVEL_ID(ZoneId_LZ,  2),
    LEVEL_ID(ZoneId_SLZ, 0), LEVEL_ID(ZoneId_SLZ, 1), LEVEL_ID(ZoneId_SLZ, 2),
    LEVEL_ID(ZoneId_SBZ, 0), LEVEL_ID(ZoneId_SBZ, 1), LEVEL_ID(ZoneId_LZ,  3),
    LEVEL_ID(ZoneId_SBZ, 2), // Final Zone
};

// Matches the disassembly's LevelMenuText charset table: ' '=blank tile,
// '0'-'9' as-is, a handful of symbols, then 'Y'/'Z' (out of alphabetical
// order in the font sheet) followed by 'A'-'X'.
static uint8_t LevSelCharToTile(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c == '$') return 0x0A;
    if (c == '-') return 0x0B;
    if (c == '=') return 0x0C;
    if (c == '>') return 0x0D;
    if (c == 'Y') return 0x0F;
    if (c == 'Z') return 0x10;
    if (c >= 'A' && c <= 'X') return (uint8_t)(0x11 + (c - 'A'));
    return 0xFF; // ' ' and anything else -> blank tile
}

// Draws all 21 lines, with `selected` highlighted in yellow and everything
// else in white.
static void LevSelTextLoad(int selected) {
    static const char hex_digits[] = "0123456789ABCDEF";
    char sndtest_line[LEVSEL_LINE_LENGTH];

    for (int row = 0; row < LEVSEL_LINE_COUNT; row++) {
        VDP_SeekVRAM(LEVSEL_VRAM_MAIN + (row * PLANE_ROW_BYTES));
        uint8_t palette = (row == selected) ? 2 : 3; // yellow : white
        const char *text = levsel_text[row];
        size_t len = strlen(text);
        if (row == LEVSEL_SNDTEST_ROW) {
            // Overlay the current sound test number (0-based, 2 hex
            // digits -- matches the renumbered SoundID enum's own
            // bgm_GHZ=1 start, not the real hardware's $80-based IDs
            // anymore) at its own fixed column, same spot the real
            // driver's LevSel_DrawSnd used -- rebuilt into a scratch
            // buffer each draw rather than mutating the static
            // levsel_text entry.
            for (int col = 0; col < LEVSEL_LINE_LENGTH; col++)
                sndtest_line[col] = (col < (int)len) ? text[col] : ' ';
            int id = levsel_sound;
            sndtest_line[LEVSEL_SNDTEST_COL + 0] = hex_digits[(id >> 4) & 0xF];
            sndtest_line[LEVSEL_SNDTEST_COL + 1] = hex_digits[id & 0xF];
            text = sndtest_line;
            len = LEVSEL_LINE_LENGTH;
        }
        for (int col = 0; col < LEVSEL_LINE_LENGTH; col++) {
            char c = (col < (int)len) ? text[col] : ' ';
            uint8_t tile = LevSelCharToTile(c);
            uint16_t word = (tile == 0xFF) ? 0 : TILE_MAP(1, palette, 0, 0, (0x680 + tile));
            VDP_WriteVRAM((const uint8_t*)&word, 2);
        }
    }
}

static void PlayLevel(void);

// Matches LevelSelect/LevSelControls: navigate with Up/Down (12-frame repeat
// delay while held), confirm with A/B/C/Start.
static void LevelSelect(void) {
    PalLoad2(PalId_LevelSel);

    memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
    VDP_SeekVRAM(VRAM_HSCROLL);
    VDP_FillVRAM(0, sizeof(hscroll_buffer));

    VDP_SeekVRAM(VRAM_BG);
    VDP_FillVRAM(0, (PLANE_WIDTH * PLANE_HEIGHT) << 1);
    VDP_SeekVRAM(LEVSEL_FONT_VRAM);
    // 41 tiles * 32 bytes/tile -- can't sizeof() an extern array with no
    // declared size, and Text.h itself is only ever #included once, from
    // Game.c (re-including it here would double-define Art_Text at link
    // time). The resource pipeline appends 8 bytes of padding after the
    // real data (see CMakeLists.txt's PAD_BYTES), which is fine to also
    // write here -- harmless past-the-end tile data, never referenced by
    // any nametable entry.
    VDP_WriteVRAM(Art_Text, 41 * 32);

    int item = 0;
    int delay = 0;
    LevSelTextLoad(item);

    while (1) {
        vbla_routine = 0x04;
        WaitForVBla();
        RunPLC();
        if (plc_buffer[0].art != NULL)
            continue; // block input while art is still loading

        uint8_t dir = jpad1_hold1 & (JPAD_UP | JPAD_DOWN);
        bool move = (jpad1_press1 & (JPAD_UP | JPAD_DOWN)) != 0;
        if (dir && !move)
            move = (--delay < 0);
        if (dir && move) {
            delay = 11;
            if (dir & JPAD_UP)
                item = item ? item - 1 : LEVSEL_LINE_COUNT - 1;
            else
                item = (item + 1) % LEVSEL_LINE_COUNT;
            LevSelTextLoad(item);
        }

        // Left/Right only ever does anything on the sound test row -- cycles
        // the selected sound/music ID, single-step per press (no hold-repeat,
        // matching the real LevSel_SndTest reading jpad1_press1 not _hold1).
        if (item == LEVSEL_SNDTEST_ROW) {
            uint8_t lr = jpad1_press1 & (JPAD_LEFT | JPAD_RIGHT);
            if (lr) {
                if (lr & JPAD_LEFT)
                    levsel_sound = levsel_sound ? levsel_sound - 1 : LEVSEL_SNDTEST_MAX;
                else
                    levsel_sound = (levsel_sound < LEVSEL_SNDTEST_MAX) ? levsel_sound + 1 : 0;
                LevSelTextLoad(item);
            }
        }

        if (jpad1_press1 & (JPAD_A | JPAD_B | JPAD_C | JPAD_START)) {
            if (item == LEVSEL_SNDTEST_ROW)
                // Plays through the new JSON tree-walking engine (verified
                // byte-identical to the byte-VM across the whole real
                // content set) rather than QueueSound2's byte-VM route --
                // first real (non-debug-tool) place this engine runs in
                // actual gameplay. levsel_sound is the 0-based on-screen
                // number; bgm_GHZ (1) is the enum's own first real ID (0
                // is reserved as the silence/stop sentinel -- see its own
                // comment in Sound.h). Stays in the loop -- doesn't exit
                // level select.
                Sound_PlayFromJSON((uint8_t)(bgm_GHZ + levsel_sound));
            else
                break;
        }
    }

    if (item == LEVSEL_SS_ROW) {
        gamemode = GameMode_Special;
        level_id = 0;
        last_special = 0;
        lives = 3;
        rings = 0;
        level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
        score = 0;
#ifndef SCP_REV00
        score_life = 5000;
#endif
    } else {
        level_id = levsel_levels[item];
        PlayLevel();
    }
}

// Matches LevSelCode_US (Up, Down, Left, Right) for level select. The debug
// mode cheat (C, C, C, C, Up, Down, Left, Right) is this project's own
// entry gesture, not the original game's (which instead counts total C
// presses during the level select cheat -- see Tit_ActivateCheat in the
// disassembly -- to layer slow motion/debug mode on top of it; slow motion
// isn't implemented in this port, so that scheme wasn't worth replicating).
//
// In both cases, presses of unrelated buttons in between are allowed/
// ignored, and entering the sequence's first button again after a mismatch
// restarts the count from there rather than requiring a clean restart.
static const uint8_t levsel_cheat_sequence[4] = {JPAD_UP, JPAD_DOWN, JPAD_LEFT, JPAD_RIGHT};
#ifdef NDEBUG
// Only used in release builds -- debug builds skip entry entirely (see
// GM_Title() below).
static const uint8_t debug_cheat_sequence[8] = {
    JPAD_C, JPAD_C, JPAD_C, JPAD_C, JPAD_UP, JPAD_DOWN, JPAD_LEFT, JPAD_RIGHT,
};
#endif

static bool TitleCheatStep(uint8_t *progress, const uint8_t *sequence, uint8_t length) {
    uint8_t input = jpad1_press1 & (JPAD_UP | JPAD_DOWN | JPAD_LEFT | JPAD_RIGHT | JPAD_C);
    if (input == 0)
        return false;
    if (input == sequence[*progress]) {
        if (++*progress >= length) {
            *progress = 0;
            return true;
        }
    } else {
        *progress = (input == sequence[0]) ? 1 : 0;
    }
    return false;
}

// Title screen demo list
static const uint16_t title_demos[] = {
    LEVEL_ID(ZoneId_GHZ, 0),
    LEVEL_ID(ZoneId_MZ, 0),
    LEVEL_ID(ZoneId_SYZ, 0),
    LEVEL_ID(7, 0), // Special stage
};

// Japanese credits
#include "Resource/Art/JapaneseCredits.h"
#include "Resource/Tilemap/JapaneseCredits.h"

// Credits font
#include "Resource/Art/CreditsFont.h"

// Title
#include "Resource/Art/TitleFG.h"
#include "Resource/Art/TitleSonic.h"
#include "Resource/Art/TitleTM.h"
#include "Resource/Tilemap/TitleFG.h"

// Level stuff
static void PlayLevel(void) {
    // Matches the disassembly's PlayLevel exactly -- always a normal level,
    // no special-casing here. (An earlier version of this port used
    // "A held -> Special Stage" as a stand-in shortcut before level select
    // existed; now that A is required just to *enter* level select, that
    // check would misfire on every level picked from the menu, since A is
    // already held by the time PlayLevel() runs. Special Stage is now its
    // own row in the menu, same as on real hardware.)
    gamemode = GameMode_Level;
    lives = 3;
    rings = 0;
    level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
    score = 0;
    last_special = 0;
    emeralds = 0;
    memset(emerald_list, 0, sizeof(emerald_list));
    continues = 0;
#ifndef SCP_REV00
    score_life = 5000;
#endif
   FadeOutMusic();
}

// Matches Tit_ChkLevSel: the cheat must be active (level_select_cheat) and A
// held when Start is pressed, or level select isn't entered.
//
// #ifndef NDEBUG: skips the D-pad requirement in debug builds -- just hold A
// and press Start -- so it doesn't have to be re-entered on every run while
// testing. Release builds require the real cheat (Up, Down, Left, Right,
// then hold A + Start), matching the original game.
static void Tit_ChkLevSel(bool level_select_cheat) {
#ifndef NDEBUG
    (void)level_select_cheat;
    bool ready = true;
#else
    bool ready = level_select_cheat;
#endif
    if (ready && (jpad1_hold1 & JPAD_A))
        LevelSelect();
    else
        PlayLevel();
}

// Title gamemode
void GM_Title(void) {
    // Stop music
    StopAllSound();

    // Clear the pattern load queue and fade out
    ClearPLC();
    PaletteFadeOut();

    // Set VDP state
    VDP_SetPlaneALocation(VRAM_FG);
    VDP_SetPlaneBLocation(VRAM_BG);
    VDP_SetBackgroundColour(0x20); // Line 2, entry 0

    wtr_state = 0;

    // Clear screen
    ClearScreen();

    // Clear object memory
    memset(objects, 0, sizeof(objects));

    // Load Japanese credits
    VDP_SeekVRAM(0x0000);
    NemDec(Art_JapaneseCredits);
    VDP_SeekVRAM(0x14C0);
    NemDec(Art_CreditsFont);

    CopyTilemap(Tilemap_JapaneseCredits, VRAM_FG + PLANE_WIDEADD + PLANE_TALLADD, 40, 24);

    // Clear palette
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    PalLoad1(PalId_Sonic);

    // Load "SONIC TEAM PRESENTS" object
    objects[2].type = ObjId_Credits;

    ExecuteObjects();
    BuildSprites(NULL);

    // Fade in
    PaletteFadeIn();

    // Load title art
    VDP_SeekVRAM(0x4000);
    NemDec(Art_TitleFG);
    VDP_SeekVRAM(0x6000);
    NemDec(Art_TitleSonic);
    VDP_SeekVRAM(0xA200);
    NemDec(Art_TitleTM);

    // Reset game state
    last_lamp = 0;
    debug_use = false;
    demo = 0;

    // Load GHZ
    level_id = LEVEL_ID(ZoneId_GHZ, 0);
    pcyc_time = 0;

    LevelSizeLoad();
    DeformLayers();
    LoadLevelMaps();
    LoadLevelLayout();
    player->pos.l.x.f.u += SCREEN_WIDEADD2; // For widescreen so the title starts scrolling at the correct time

    // Fade out
    PaletteFadeOut();

    // Draw background
    ClearScreen();
    DrawChunks(bg_scrpos_x.f.u, bg_scrpos_y.f.u, LEVEL_LAYOUT_BG(0), VRAM_BG);

    // Load title mappings
    CopyTilemap(&Tilemap_TitleFG[0x0000], MAP_PLANE(VRAM_FG, 3, 4) + PLANE_WIDEADD + PLANE_TALLADD, 34, 22);

    // Load GHZ art and title palette
    VDP_SeekVRAM(0x0000);
    NemDec(Art_GHZ1);
    PalLoad1(PalId_Title);

    // Run title screen for 376 frames
    demo_length = 376;
    memset(&objects[2], 0, sizeof(Object));

    // Load title objects
    objects[1].type = ObjId_TitleSonic;
    objects[2].type = ObjId_PSB;
#ifndef SCP_JP
    objects[3].type = ObjId_PSB;
    objects[3].frame = 3;
#endif
    objects[4].type = ObjId_PSB;
    objects[4].frame = 2;

    ExecuteObjects();
    DeformLayers();
    BuildSprites(NULL);

    NewPLC(PlcId_Main);

    // Fade in
    PaletteFadeIn();

    PlayMusic(bgm_Title);

    // Level select cheat entry state (see TitleCheatStep/Tit_ChkLevSel).
    uint8_t cheat_progress = 0;
    bool level_select_cheat = false;

    // Debug/object-placement mode cheat entry state. debug_cheat itself is
    // the real, global flag (Game.h) that GM_Level.c/GM_Special.c already
    // check when a level starts. In debug builds, both the cheat entry and
    // the "hold A" requirement below are skipped entirely, so debug mode is
    // always one level-start away while testing.
#ifndef NDEBUG
    debug_cheat = true;
#else
    uint8_t debug_progress = 0;
#endif

    // Loop
    do {
        // Render frame
        vbla_routine = 0x04;
        WaitForVBla();

        // Run game and load PLCs
        ExecuteObjects();
        DeformLayers();
        BuildSprites(NULL);
        RunPLC();

        // Run palette cycle
        PCycle_Title();

        // Move Sonic object (yep, this is how they scroll the camera)
        //...and return to the Sega screen after a minute?
        if ((player->pos.l.x.f.u += 2) >= 0x1C00) {
            gamemode = GameMode_Sega;
            return;
        }

        // Check for level select cheat entry (Up, Down, Left, Right)
        if (TitleCheatStep(&cheat_progress, levsel_cheat_sequence, 4)) {
            level_select_cheat = true;
            PlaySound(sfx_Ring);
        }
#ifdef NDEBUG
        // Check for debug mode cheat entry (C, C, C, C, Up, Down, Left, Right)
        if (TitleCheatStep(&debug_progress, debug_cheat_sequence, 8)) {
            debug_cheat = true;
            PlaySound(sfx_Ring);
        }
#endif

        // Check if the title's over
        if (!demo_length) {
            // Run the title screen but with reduced code for 30 frames
            demo_length = 30;

            do {
                // Render frame
                vbla_routine = 0x04;
                WaitForVBla();

                // Run game and load PLCs
                DeformLayers();
                PaletteCycle(); // Wrong palette cycle
                RunPLC();

                // Move Sonic object
                if ((player->pos.l.x.f.u += 2) >= 0x1C00) {
                    gamemode = GameMode_Sega;
                    return;
                }

                // Check for level select cheat entry (Up, Down, Left, Right)
                if (TitleCheatStep(&cheat_progress, levsel_cheat_sequence, 4)) {
                    level_select_cheat = true;
                    PlaySound(sfx_Ring);
                }
#ifdef NDEBUG
                // Check for debug mode cheat entry (C, C, C, C, Up, Down, Left, Right)
                if (TitleCheatStep(&debug_progress, debug_cheat_sequence, 8)) {
                    debug_cheat = true;
                    PlaySound(sfx_Ring);
                }
#endif

                // Check if start is pressed
                if (jpad1_press1 & JPAD_START) {
                    Tit_ChkLevSel(level_select_cheat);
                    return;
                }
            } while (demo_length);

            // Load demo
            FadeOutMusic();

            level_id = title_demos[demo_num & 7];
            if (++demo_num >= 4)
                demo_num = 0;

            // Enter demo gamemode
            demo = 1;
            if (level_id != 0x700) {
                // Regular level
                gamemode = GameMode_Demo;
            } else {
                // Special stage
                gamemode = GameMode_Special;
                level_id = 0;
                last_special = 0;
            }

            // Set game state
            lives = 3;
            rings = 0;
            level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
            score = 0;
#ifndef SCP_REV00
            score_life = 5000;
#endif
            return;
        }
    } while (!(jpad1_press1 & JPAD_START));

    Tit_ChkLevSel(level_select_cheat);
}
