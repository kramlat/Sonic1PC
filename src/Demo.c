#include "Demo.h"

#include "Game.h"
#include "Level.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// Demo state
uint16_t btn_pushtime1;
uint8_t btn_pushtime2;

// Demos
#include "Resource/Demo/IntroGHZ.h"
#include "Resource/Demo/IntroMZ.h"
#include "Resource/Demo/IntroSS.h"
#include "Resource/Demo/IntroSYZ.h"

#include "Resource/Demo/EndingGHZ1.h"
#include "Resource/Demo/EndingGHZ2.h"
#include "Resource/Demo/EndingLZ.h"
#include "Resource/Demo/EndingMZ.h"
#include "Resource/Demo/EndingSBZ1.h"
#include "Resource/Demo/EndingSBZ2.h"
#include "Resource/Demo/EndingSLZ.h"
#include "Resource/Demo/EndingSYZ.h"

const uint8_t* intro_demo_ptr[] = {
    /* ZoneId_GHZ  */   Demo_IntroGHZ,
    /* ZoneId_LZ   */   Demo_IntroGHZ,
    /* ZoneId_MZ   */   Demo_IntroMZ,
    /* ZoneId_SLZ  */   Demo_IntroGHZ,
    /* ZoneId_SYZ  */   Demo_IntroSYZ,
    /* ZoneId_SBZ  */   Demo_IntroGHZ,
    /* ZoneId_EndZ */   Demo_IntroGHZ,
    /* Special Stage */ Demo_IntroSS,
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

static void DumpDemoRecording(void) {
    const char *path = cli_demo_record_path ? cli_demo_record_path : "demo_record.bin";
    size_t length = (size_t)demo_record_pos + 4; // include the in-progress trailing record

    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(demo_record_buffer, 1, length, f);
        fclose(f);
    }
    fprintf(stderr, "SonicDemoRecord: wrote %zu bytes to '%s' (%d frames)\n",
            length, path, demo_record_frame_count);
    exit(f ? 0 : 1);
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

    if ((jpad1_press1 & JPAD_START) ||
        (cli_demo_record_frames >= 0 && demo_record_frame_count >= cli_demo_record_frames))
        DumpDemoRecording();
}

// Demo playback
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
        demo_data = intro_demo_ptr[(gamemode == GameMode_Special) ? 7 : LEVEL_ZONE(level_id)];

    // Offset demo address
    demo_data += btn_pushtime1;

    // Apply input onto joypad state
    uint8_t d0 = demo_data[0];
    uint8_t d1 = d0;
#ifdef SCP_REV00
    uint8_t d2 = jpad1_hold1;
#else
    uint8_t d2 = 0; // Fix the infamous demo playback bug
#endif
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
