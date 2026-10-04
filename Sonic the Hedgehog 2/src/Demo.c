#include "Demo.h"

#include "Game.h"
#include "SplitScreen.h"
#include "Level.h"
#include "SpecialStage.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Demo state
uint16_t btn_pushtime1;
uint8_t btn_pushtime2;

// Demos
#include "Resource/Demo/NACPZ.h"
#include "Resource/Demo/NAEHZ.h"
#include "Resource/Demo/NAGHZ.h"
#include "Resource/Demo/NAHPZ.h"
#include "Resource/Demo/NAHTZ.h"
#include "Resource/Demo/NASS.h"

#include "Resource/Demo/SWEHZTails.h"
#include "Resource/Demo/EndingGHZ1.h"
#include "Resource/Demo/EndingGHZ2.h"
#include "Resource/Demo/EndingLZ.h"
#include "Resource/Demo/EndingMZ.h"
#include "Resource/Demo/EndingSBZ1.h"
#include "Resource/Demo/EndingSBZ2.h"
#include "Resource/Demo/EndingSLZ.h"
#include "Resource/Demo/EndingSYZ.h"

// (every zone slot has one, since the level loader reads the demo's first bytes even when no demo plays: the slots without a demo of their own of the prototypes have Emerald Hill's)
// Nick Arcade's attract-mode demos, by zone slot (the same inputs it plays back in the same levels; its Emerald Hill demo was a two-player one, whose second pad moved Tails: here only Sonic's side)
const uint8_t* intro_demo_ptr[ZoneId_Num] = {
    [0x00] = Demo_NAEHZ, [0x01] = Demo_NAEHZ, [0x02] = Demo_NAEHZ, [0x03] = Demo_NAEHZ, [0x04] = Demo_NAEHZ, [0x05] = Demo_NAEHZ, [0x06] = Demo_NAEHZ, [0x07] = Demo_NAHTZ,
    [0x08] = Demo_NAHPZ, [0x09] = Demo_NAEHZ, [0x0A] = Demo_NAEHZ, [0x0B] = Demo_NAEHZ, [0x0C] = Demo_NAEHZ, [0x0D] = Demo_NACPZ, [0x0E] = Demo_NAEHZ, [0x0F] = Demo_NAEHZ,
    [ZoneId_SS] = Demo_NASS, // (the special stage's, where Sonic 1 has it at index 7: GM_Special.c names it by ZoneId_SS)
};

const uint8_t* ending_demo_ptr[] = {
    Demo_EndingGHZ1,
    Demo_EndingMZ,
    Demo_EndingSYZ,
    Demo_EndingLZ,
    Demo_EndingSLZ,
    Demo_EndingSBZ1,
    Demo_EndingSBZ2,
    Demo_EndingGHZ2,
};

const uint8_t *cli_demo_override = NULL;
int32_t cli_demo_length = -1;

bool cli_demo_record = false;
const char *cli_demo_record_path = NULL;
int32_t cli_demo_record_frames = -1;

// Demo recording. Mirrors the disassembly's unused DemoRecorder routine
// (MoveSonicInDemo.asm): records are [button_state, duration] byte pairs.
// While the held buttons stay the same as the current record's button byte,
// just bump its duration (capped at 255, at which point a new record
// starts even though the buttons didn't change, matching the original's
// 8-bit duration field); otherwise start a new record.
#define DEMO_RECORD_MAX 0x10000
static uint8_t demo_record_buffer[DEMO_RECORD_MAX];
static uint16_t demo_record_pos; // byte offset of the current record's button byte
static int32_t demo_record_frame_count;

static bool demo_request_pending = false;
static DemoRecordRequest demo_request;
static char demo_last_saved_path[512];
static int demo_last_saved_frames = 0;

static void ResetRecordingBuffer(void) {
    memset(demo_record_buffer, 0, sizeof(demo_record_buffer));
    demo_record_pos = 0;
    demo_record_frame_count = 0;
}

static void DumpDemoRecording(void) {
    const char *path = cli_demo_record_path ? cli_demo_record_path : "demo_record.bin";
    size_t length = (size_t)demo_record_pos + 4; // include the in-progress trailing record

    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(demo_record_buffer, 1, length, f);
        fclose(f);
    }
    fprintf(stderr, "Demo recording: wrote %zu bytes to '%s' (%d frames)\n",
            length, path, demo_record_frame_count);

    // Remember what was saved, stop recording and carry on playing.
    if (f) {
        snprintf(demo_last_saved_path, sizeof(demo_last_saved_path), "%s", path);
        demo_last_saved_frames = demo_record_frame_count;
    } else {
        demo_last_saved_path[0] = '\0';
    }
    cli_demo_record = false;
    cli_start_x = cli_start_y = -1;
}

void Demo_RequestRecording(const DemoRecordRequest *request) {
    demo_request = *request;
    demo_request_pending = true;
}

bool Demo_PlaybackActive(void) {
    return cli_demo_override != NULL && demo != 0;
}

bool Demo_RecordingActive(void) {
    return demo_request_pending || cli_demo_record;
}

int Demo_RecordedFrames(void) {
    return demo_record_frame_count;
}

void Demo_StopRecording(void) {
    demo_request_pending = false;
    if (cli_demo_record)
        DumpDemoRecording();
}

const char *Demo_LastSavedPath(void) {
    return demo_last_saved_path;
}

int Demo_LastSavedFrames(void) {
    return demo_last_saved_frames;
}

// Playback of a recorded demo file: the data is copied, and the request is applied by
// Demo_ServiceRequests like a recording request.
static uint8_t demo_play_buffer[0x10000 + 8];
static DemoPlayRequest demo_play_request;
static size_t demo_play_length = 0;
static bool demo_play_pending = false;

bool Demo_RequestPlayback(const DemoPlayRequest *request, const uint8_t *data, size_t length) {
    if (length < 4 || length > 0x10000)
        return false;
    memcpy(demo_play_buffer, data, length);
    memset(demo_play_buffer + length, 0, 8);
    demo_play_length = length;
    demo_play_request = *request;
    demo_play_pending = true;
    return true;
}

// Total frames of a recorded demo: the sum of every complete [buttons, duration - 1] pair.
static int32_t DemoFrameCount(const uint8_t *data, size_t length) {
    int32_t frames = 0;
    for (size_t i = 0; i + 4 <= length; i += 2)
        frames += data[i + 1] + 1;
    return frames;
}

// Applies a pending request: the same start state as the CLI hook in Game.c's EntryPoint, then the
// level is restarted (or entered, from the title screen and the like).
void Demo_ServiceRequests(void) {
    if (!demo_play_pending && !demo_request_pending)
        return;

    // Only the screens whose loops notice an outside mode change (Sega, title) or restart on a flag (level),
    // plus the special stage, can be left right now. Anywhere else (continue screen, ending, credits, the
    // splash screens) the request simply waits until the game reaches one of them.
    uint8_t mode = gamemode & 0x7F;
    if (mode != GameMode_Sega && mode != GameMode_Title && mode != GameMode_Demo &&
        mode != GameMode_Level && mode != GameMode_Special)
        return;

    if (demo_play_pending) {
        demo_play_pending = false;
        demo_request_pending = false;
        cli_demo_record = false; // playing and recording are exclusive
        size_t length = demo_play_length;
        cli_demo_override = demo_play_buffer;
        cli_demo_length = DemoFrameCount(demo_play_buffer, length) + 30; // run to the end, then the usual fade
        cli_start_x = demo_play_request.start_x;
        cli_start_y = demo_play_request.start_y;
        level_id = (uint16_t)LEVEL_ID(demo_play_request.zone, demo_play_request.act);
        two_player_mode = demo_play_request.split_screen; // (a demo recorded in the split screen plays in it)
        lives = 3;
        rings = 0;
        level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
        score = 0;
        continues = 0;
        last_lamp = 0;
        prev_lamp = 0;
        demo = 1; // MoveSonicInDemo drives Sonic from cli_demo_override
        gamemode = GameMode_Demo; // other screens leave their loop on seeing this
        restart = true;           // restarts a running level loop into the demo (cleared again when a level starts)
        return;
    }
    if (!demo_request_pending)
        return;
    demo_request_pending = false;
    const DemoRecordRequest *r = &demo_request;

    ResetRecordingBuffer();
    cli_demo_record = true;
    cli_demo_record_path = r->path;
    cli_demo_record_frames = r->frames;
    cli_start_x = r->start_x;
    cli_start_y = r->start_y;

    level_id = r->special ? 0 : (uint16_t)LEVEL_ID(r->zone, r->act);
    two_player_mode = !r->special && r->split_screen;
    last_special = r->special ? (uint8_t)r->special_stage : 0;
    lives = 3;
    rings = 0;
    level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
    score = 0;
    emeralds = 0;
    memset(emerald_list, 0, sizeof(emerald_list));
    continues = 0;
    last_lamp = 0;
    prev_lamp = 0;
    demo = 0;
    score_life = 5000;

    if (r->special) {
        gamemode = GameMode_Special;
    } else if ((gamemode & 0x7F) == GameMode_Level) {
        restart = true; // the level loop restarts itself, picking up level_id
    } else {
        gamemode = GameMode_Level; // other screens leave their loop when they see this change
    }
}

void RecordDemoFrame(void) {
    if (!cli_demo_record)
        return;

    uint8_t *record = &demo_record_buffer[demo_record_pos];
    uint8_t buttons = jpad1_hold1;

    if (buttons == record[0] && record[1] != 0xFF) {
        record[1]++;
    } else {
        record[2] = buttons;
        record[3] = 0;
        demo_record_pos += 2;
        if (demo_record_pos + 4 > DEMO_RECORD_MAX)
            demo_record_pos = 0; // wrap, same as the original's andi.w #$3FF
    }
    demo_record_frame_count++;

    // Start stays the game's pause button: only the Tools menu (Demo_StopRecording) or the frame limit
    // ends a recording.
    if (cli_demo_record_frames >= 0 && demo_record_frame_count >= cli_demo_record_frames)
        DumpDemoRecording();
}

// Demo playback
static uint16_t tails_demo_pos;
static uint8_t tails_demo_time, tails_demo_last;

void MoveSonicInDemo(void) {
    if (!demo)
        return;

    // Return to title screen if start is pressed
    if (demo >= 0 && (jpad1_hold1 & JPAD_START))
        gamemode = GameMode_Title;

    // Get demo data
    const uint8_t* demo_data;
    if (cli_demo_override)
        demo_data = cli_demo_override;
    else if (demo < 0)
        demo_data = ending_demo_ptr[credits_num - 1];
    else
        demo_data = intro_demo_ptr[(gamemode == GameMode_Special) ? ZoneId_SS : LEVEL_ZONE(level_id)];

    // The Simon Wai prototype's Emerald Hill demo is a two-player one: Tails plays pad 2 from a demo of his own (Demo_Tails_Ghz) -- a recorded demo of the split screen has no second pad
    if (demo > 0 && two_player_mode && !cli_demo_override && LEVEL_ZONE(level_id) == ZoneId_EHZ) {
        if (btn_pushtime1 == 0 && btn_pushtime2 == demo_data[1] - 1) { // (the start of the demo: Sonic's has not moved on yet)
            tails_demo_pos = 0;
            tails_demo_time = Demo_SWEHZTails[1] - 1;
        }
        const uint8_t *tails = Demo_SWEHZTails + tails_demo_pos;
        jpad2_hold = tails[0];
        jpad2_press = tails[0] & (uint8_t)~tails_demo_last;
        tails_demo_last = tails[0];
        if (--tails_demo_time == 0xFF) {
            tails_demo_time = tails[3];
            tails_demo_pos += 2;
        }
    }

    // Offset demo address
    demo_data += btn_pushtime1;

    // Apply input onto joypad state
    uint8_t d0 = demo_data[0];
    uint8_t d1 = d0;
    uint8_t d2 = 0; // Fix the infamous demo playback bug
    d0 ^= d2;
    jpad1_hold1 = d1;
    d0 &= d1;
    jpad1_press1 = d0;

    // Handle demo timer
    if (--btn_pushtime2 == 0xFF) {
        btn_pushtime2 = demo_data[3];
        btn_pushtime1 += 2;
    }
}
