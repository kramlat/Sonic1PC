#include "GM_Title.h"
#include "Constants.h"

#include "Sound.h"
#include "Game.h"
#include "Level.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Enigma.h"
#include "Nemesis.h"
#include "PLC.h"
#include "Palette.h"
#include "PaletteCycle.h"
#include "SpecialStage.h"
#include "Video.h"

#include "Backend/VDP.h"

#include <stdio.h>
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
#define LEVSEL_FONT_VRAM    ART_VRAM(ArtTile_Level_Select_Font)
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

// Likewise the highlighted row: coming back to level select (after a level, a special stage, the title screen...)
// leaves the cursor where it was. It only starts from the top again when the program is restarted.
static int levsel_item = 0;

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

// The sound test's last two values, 9E and 9F, open the Easter Eggs menu (the original's sound test started the credits at 9E and the
// ending at 9F once a cheat was on; the menu holds those and more). Every value in between the last real sound and 9E is an empty
// slot that plays nothing but can be stepped onto, so the two are reached the same way as any other value, by pressing Right (or
// Left from 00, which wraps round to 9F).
#define LEVSEL_EGG_FIRST 0x9E
#define LEVSEL_EGG_LAST  0x9F

static bool LevSelIsEggValue(int value) {
    return value == LEVSEL_EGG_FIRST || value == LEVSEL_EGG_LAST;
}

// The sound test number after a Left/Right press: all values in turn from 00 to 9F, wrapping round.
static int LevSelStepSound(int value, bool right) {
    if (right)
        return value < LEVSEL_EGG_LAST ? value + 1 : 0;
    return value > 0 ? value - 1 : LEVSEL_EGG_LAST;
}

// Draws one line of the level select's text area (blank-padded to its width).
static void LevSelDrawLine(int row, const char *text, int palette) {
    VDP_SeekVRAM(LEVSEL_VRAM_MAIN + (row * PLANE_ROW_BYTES));
    size_t len = strlen(text);
    for (int col = 0; col < LEVSEL_LINE_LENGTH; col++) {
        uint8_t tile = LevSelCharToTile(col < (int)len ? text[col] : ' ');
        uint16_t word = (tile == 0xFF) ? 0 : TILE_MAP(1, palette, 0, 0, (0x680 + tile));
        VDP_WriteVRAM((const uint8_t *)&word, 2);
    }
}

// The Easter Eggs menu (what the sound test's hidden 9E/9F values open): the credits and the ending, in either of their
// forms, and whether Eggman's wrecked Eggmobile falls in the background of the ending. Returns true when a game mode was
// picked (the level select is over), false to go back to the sound test.
enum { EGG_CREDITS, EGG_GOOD_ENDING, EGG_BAD_ENDING, EGG_SHIP, EGG_BACK, EGG_COUNT };

static void EasterEggsDraw(int selected, bool ship) {
    for (int row = 0; row < LEVSEL_LINE_COUNT; row++)
        LevSelDrawLine(row, "", 3);
    LevSelDrawLine(0, "EASTER EGGS", 3);
    static const char *const names[EGG_COUNT] = {"CREDITS", "GOOD ENDING", "BAD ENDING", "WRECKED SHIP", "BACK"};
    for (int i = 0; i < EGG_COUNT; i++) {
        char line[LEVSEL_LINE_LENGTH + 1];
        snprintf(line, sizeof(line), "%-17s%s", names[i], i == EGG_SHIP ? (ship ? "ON" : "OFF") : "");
        LevSelDrawLine(2 + i, line, i == selected ? 2 : 3);
    }
}

static bool EasterEggs(void) {
    int item = 0, delay = 0;
    bool ship = ending_eggmobile_exploding != 0;
    EasterEggsDraw(item, ship);

    for (;;) {
        vbla_routine = 0x04;
        WaitForVBla();
        RunPLC();

        uint8_t dir = jpad1_hold1 & (JPAD_UP | JPAD_DOWN);
        bool move = (jpad1_press1 & (JPAD_UP | JPAD_DOWN)) != 0;
        if (dir && !move)
            move = (--delay < 0);
        if (dir && move) {
            delay = 11;
            item = (dir & JPAD_UP) ? (item ? item - 1 : EGG_COUNT - 1) : (item + 1) % EGG_COUNT;
            EasterEggsDraw(item, ship);
        }

        if (jpad1_press1 & JPAD_B)
            return false;
        if (!(jpad1_press1 & (JPAD_A | JPAD_C | JPAD_START)))
            continue;

        switch (item) {
        case EGG_SHIP:
            ship = !ship;
            EasterEggsDraw(item, ship);
            break;
        case EGG_BACK:
            return false;
        case EGG_CREDITS:
            gamemode = GameMode_Credits;
            credits_num = 0;
            PlayMusic(bgm_Credits);
            return true;
        default: // the ending: with every emerald it is the good one, without them the bad one
            emeralds = (item == EGG_GOOD_ENDING) ? 6 : 0;
            for (int i = 0; i < 6; i++)
                emerald_list[i] = (uint8_t)(i < emeralds ? i : 0);
            ending_eggmobile_exploding = ship;
            gamemode = GameMode_Ending;
            return true;
        }
    }
}

static void PlayLevel(bool new_game);

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

    int item = levsel_item;
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
                levsel_sound = LevSelStepSound(levsel_sound, (lr & JPAD_RIGHT) != 0);
                LevSelTextLoad(item);
            }
        }

        if (jpad1_press1 & (JPAD_A | JPAD_B | JPAD_C | JPAD_START)) {
            if (item == LEVSEL_SNDTEST_ROW && LevSelIsEggValue(levsel_sound)) {
                if (EasterEggs()) {
                    levsel_item = item;
                    return; // a game mode was picked
                }
                LevSelTextLoad(item);
            } else if (item == LEVSEL_SNDTEST_ROW) {
                // Plays through the new JSON tree-walking engine (verified
                // byte-identical to the byte-VM across the whole real
                // content set) rather than QueueSound2's byte-VM route --
                // first real (non-debug-tool) place this engine runs in
                // actual gameplay. levsel_sound is fed straight through as
                // the sound ID, matching real Sonic 1's own sound test
                // numbering directly (this project's own core underneath,
                // same on-screen behavior). Stays in the loop -- doesn't
                // exit level select.
                if (levsel_sound <= LEVSEL_SNDTEST_MAX) // (the empty slots up to 9D play nothing)
                    Sound_PlayFromJSON((uint8_t)(levsel_sound));
            } else
                break;
        }
    }

    levsel_item = item; // remembered for the next visit

    if (item == LEVSEL_SS_ROW) {
        gamemode = GameMode_Special;
        level_id = 0;
        // (last_special and the emeralds are left alone, like the original: the stage number moves on with each
        // visit and skips emeralds you have. Only starting a level, or the attract-mode demo, clears them.)
        lives = 3;
        rings = 0;
        level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
        score = 0;
        score_life = 5000;
    } else {
        level_id = levsel_levels[item];
        PlayLevel(false);
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
// (Nick Arcade's Demo_Levels: Chemical Plant, Emerald Hill, Hidden Palace, Hill Top)
static const uint16_t title_demos[] = {
    LEVEL_ID(ZoneId_MZ, 0),
    LEVEL_ID(ZoneId_SLZ, 0),
    LEVEL_ID(ZoneId_SYZ, 0),
    LEVEL_ID(ZoneId_SBZ, 0),
};

// Japanese credits
#include "Resource/Art/JapaneseCredits.h"
#include "Resource/Tilemap/JapaneseCredits.h"

// Credits font
#include "Resource/Art/CreditsFont.h"

// Sonic 2's title (the Simon Wai prototype's, which is also Nick Arcade's): wings and background art, their maps, the palette
#include "Resource/S2Title/Art.h"
#include "Resource/S2Title/SonicTails.h"
#include "Resource/S2Title/MapWings.h"
#include "Resource/S2Title/MapBG.h"
#include "Resource/S2Title/MapBG2.h"
#include "Resource/S2Title/Palette.h"

// Title
#include "Resource/Art/TitleFG.h"
#include "Resource/Art/TitleSonic.h"
#include "Resource/Art/TitleTM.h"
#include "Resource/Tilemap/TitleFG.h"

// Level stuff
// new_game: starting from the title screen. A level picked from the level select behaves like arriving through a
// giant ring does: the special stage counter and the emeralds you have are left alone (the original resets them
// here too, but only the attract-mode demo should force the first stage).
static void PlayLevel(bool new_game) {
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
    if (new_game) {
        last_special = 0;
        emeralds = 0;
        memset(emerald_list, 0, sizeof(emerald_list));
    }
    continues = 0;
    score_life = 5000;
   FadeOutMusic();
}

// Sonic 2's title Start: the level select cheat is on from the start (hold A and press Start, no code to enter), and a plain Start begins in the Emerald Hill slot (zone slot 3: EHZ in
// this game's zone order, GHZ / (empty) / CPZ / EHZ / HPZ / HTZ).
#define ZONE_SLOT_EHZ 3
static void Tit_ChkLevSel(bool level_select_cheat) {
    (void)level_select_cheat;
    if (jpad1_hold1 & JPAD_A) {
        LevelSelect();
    } else {
        level_id = LEVEL_ID(ZONE_SLOT_EHZ, 0);
        PlayLevel(true);
    }
}

void Deform_TitleScreen(void); // Sonic 2's LevelScroll.c

// Copies a part of the background into plane B, row by row, wrapping at the plane's edge: the two 32-cell parts fill the whole 64-cell plane, so with the widescreen
// margin the right part runs off the end of each row and has to come back round to its start (not spill into the next row)
static void CopyTilemapWrapped(const uint8_t *map, int col, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int c = (PLANE_WIDEADD / 2 + col + x) % PLANE_WIDTH;
            VDP_SeekVRAM(VRAM_BG + PLANE_TALLADD + ((y * PLANE_WIDTH + c) << 1));
            const uint8_t *m = map + ((y * width + x) << 1);
            uint16_t v = (m[0] << 8) | m[1];
            VDP_WriteVRAM((const uint8_t *)&v, 2);
        }
    }
}

// Title gamemode
void GM_Title(void) {
    // Stop music
    StopAllSound();

    // The hidden Japanese credits (hold A+B+C+Down on the "Sonic Team Presents" screen) and the sound test's Easter Eggs
    // menu come with the debug cheat: always in debug builds, once its code has been entered in release builds (the
    // original only had them through a build option).
#ifndef NDEBUG
    credits_cheat = true;
#endif

    // Clear the pattern load queue and fade out
    ClearPLC();
    PaletteFadeOut();

    // Set VDP state
    VDP_SetPlaneALocation(VRAM_FG);
    VDP_SetPlaneBLocation(VRAM_BG);
    VDP_SetBackgroundColour(0x20); // Line 2, entry 0

    VDPDisableWaterSplit();

    // Clear screen
    ClearScreen();

    // Clear object memory
    memset(objects, 0, sizeof(objects));

    // (Sonic 1 shows its "SONIC TEAM PRESENTS" screen here, with the hidden Japanese credits. Sonic 2 skips it; "MILES TAILS PROWER IN" is to take its place later.)
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    PalLoad1(PalId_Sonic);
    ExecuteObjects();
    BuildSprites(NULL);

    // Load title art: the prototype's wings and background at tile 0. Sonic 1's Sonic and "press start" text stay for the objects below until Sonic 2's own
    // (Sonic and Tails) come.
    VDP_SeekVRAM(0x0000);
    NemDec(S2Title_Art);
    VDP_SeekVRAM(0x4000);
    NemDec(S2Title_SonicTails);
    VDP_SeekVRAM(0xA200);
    NemDec(Art_TitleTM);

    // Reset game state
    last_lamp = 0;
    debug_use = false;
    demo = 0;
    level_id = LEVEL_ID(ZoneId_GHZ, 0);
    pcyc_time = 0;

    // The title has a camera of its own (Deform_TitleScreen runs it on from 0). What ends it (below) counts from where Sonic 1's level start put the player.
    scrpos_x.f.u = 0;
    bg_scrpos_y.v = 0;
    player->pos.l.x.f.u = 0x50 + SCREEN_WIDEADD2;

    // Fade out
    PaletteFadeOut();
    ClearScreen();

    // Plane A: the wings (and the emblem); plane B: the background, in two 32-cell parts side by side
    {
        static uint8_t map[40 * 28 * 2];
        EniDec(S2Title_MapWings, map, 0);
        CopyTilemap(map, VRAM_FG + PLANE_WIDEADD + PLANE_TALLADD, 40, 28);
        EniDec(S2Title_MapBG, map, 0);
        CopyTilemapWrapped(map, 0, 32, 28);
        EniDec(S2Title_MapBG2, map, 0);
        CopyTilemapWrapped(map, 32, 32, 28);
    }

    // The title's palette: the prototype's four lines
    for (int i = 0; i < 4 * 16; i++)
        dry_palette_dup[i / 16][i % 16] = LESWAP_16(((const uint16_t *)S2Title_Palette)[i]);

    // Run title screen for 376 frames
    demo_length = 376;
    memset(&objects[2], 0, sizeof(Object));

    // Load title objects
    objects[1].type = ObjId_TitleSonic; // Sonic (frame 0) and Tails (frame 1): the prototypes' title characters
    objects[5].type = ObjId_TitleSonic;
    objects[5].frame = 1;
    // (Sonic 1's "PRESS START BUTTON" object, its trademark sign and its sprite mask are not loaded: Sonic 2 disables the PSB object.)

    ExecuteObjects();
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
    debug_cheat = !cli_no_debug;
#else
    uint8_t debug_progress = 0;
#endif

    // Loop
    do {
        // Render frame
        vbla_routine = 0x04;
        WaitForVBla();

        // The mode was changed from outside (e.g. an in-app demo recording request): leave.
        if ((gamemode & 0x7F) != GameMode_Title)
            return;

        // Run game and load PLCs
        ExecuteObjects();
        BuildSprites(NULL);
        RunPLC();

        // Run palette cycle, and scroll the background (the title's own camera runs on by 8 a frame: Nick Arcade's Deform_TitleScreen)
        PCycle_Title();
        Deform_TitleScreen();

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
            credits_cheat = true;
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
                Deform_TitleScreen();
                PCycle_Title();
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
                    credits_cheat = true;
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
            score_life = 5000;
            return;
        }
    } while (!(jpad1_press1 & JPAD_START));

    Tit_ChkLevSel(level_select_cheat);
}
