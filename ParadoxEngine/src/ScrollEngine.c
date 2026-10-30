#include "ScrollEngine.h"

static ScrollZone zones[SCROLL_MAX_ZONES][SCROLL_MAX_ACTS];
static bool zone_set[SCROLL_MAX_ZONES][SCROLL_MAX_ACTS];

unsigned level_layout_col_mask = 0x7F, level_layout_row_mask = 0xF;
uint16_t level_camera_y_mask = 0x7F0, level_screen_y_wrap = 0x7FF;

void ScrollEngine_SetLevelMasks(unsigned col_mask, unsigned row_mask) {
    level_layout_col_mask = col_mask;
    level_layout_row_mask = row_mask;
    level_screen_y_wrap = (uint16_t)(((row_mask + 1) << 7) - 1); // (a chunk is 128 pixels tall)
    level_camera_y_mask = (uint16_t)(level_screen_y_wrap & ~0xF);
}

void ScrollEngine_SetZone(int zone, int act, const ScrollZone *routines) {
    if (zone < 0 || zone >= SCROLL_MAX_ZONES)
        return;
    for (int a = 0; a < SCROLL_MAX_ACTS; a++)
        if (act < 0 || act == a) {
            zones[zone][a] = *routines;
            zone_set[zone][a] = true;
        }
}

const ScrollZone *ScrollEngine_GetZone(int zone, int act) {
    if (zone < 0 || zone >= SCROLL_MAX_ZONES || act < 0 || act >= SCROLL_MAX_ACTS || !zone_set[zone][act])
        return 0;
    return &zones[zone][act];
}

void ScrollEngine_Init(int zone, int act) {
    const ScrollZone *z = ScrollEngine_GetZone(zone, act);
    if (z == 0)
        return;
    if (z->screen_init)
        z->screen_init();
    if (z->background_init)
        z->background_init();
}

void ScrollEngine_Events(int zone, int act) {
    const ScrollZone *z = ScrollEngine_GetZone(zone, act);
    if (z == 0)
        return;
    if (z->screen_event)
        z->screen_event();
    if (z->background_event)
        z->background_event();
}
