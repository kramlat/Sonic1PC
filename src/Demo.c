#include "Demo.h"

#include "Game.h"
#include "Level.h"

#include <stddef.h>

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
    /* ZoneId_GHZ  */ Demo_IntroGHZ,
    /* ZoneId_LZ   */ Demo_IntroGHZ,
    /* ZoneId_MZ   */ Demo_IntroMZ,
    /* ZoneId_SLZ  */ Demo_IntroMZ,
    /* ZoneId_SYZ  */ Demo_IntroSYZ,
    /* ZoneId_SBZ  */ Demo_IntroSYZ,
    /* ZoneId_EndZ */ Demo_IntroSS,
    Demo_IntroSS,
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

// Demo playback
void MoveSonicInDemo()
{
    if (!demo)
        return;

    // Return to title screen if start is pressed
    if (demo >= 0 && (jpad1_hold1 & JPAD_START))
        gamemode = GameMode_Title;

    // Get demo data
    const uint8_t* demo_data;
    if (demo < 0)
        demo_data = ending_demo_ptr[credits_num - 1];
    else
        demo_data = intro_demo_ptr[(gamemode == GameMode_Special) ? 6 : LEVEL_ZONE(level_id)];

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
