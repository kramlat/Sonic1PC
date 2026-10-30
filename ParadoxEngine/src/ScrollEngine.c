#include "ScrollEngine.h"

#include <string.h>

#include "LevelDrawCore.h"

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

int ScrollBands_BandAt(const uint16_t *heights, int count, int y, int *offset_in_band) {
    int top = 0;
    if (y < 0)
        y = 0;
    for (int b = 0; b < count; b++) {
        int h = heights[b] & ~SCROLL_BAND_PER_LINE;
        if (y < top + h || b == count - 1) {
            if (offset_in_band)
                *offset_in_band = y - top;
            return b;
        }
        top += h;
    }
    if (offset_in_band)
        *offset_in_band = 0;
    return 0;
}

void ScrollEngine_ApplyDeformation(const uint16_t *heights, int count, const int16_t *positions, int16_t layer_y, int16_t other_x, bool deform_background, int lines, int16_t (*out)[2]) {
    if (count <= 0)
        return;
    // Skip the bands above the view: a per-line band has an entry for every line, any other one entry
    int y = layer_y < 0 ? 0 : layer_y;
    int b = 0, entry = 0, first_off = 0;
    for (; b < count; b++) {
        int h = heights[b] & ~SCROLL_BAND_PER_LINE;
        if (y < h)
            break;
        y -= h;
        entry += (heights[b] & SCROLL_BAND_PER_LINE) ? h : 1;
    }
    if (b == count) { // below the table: the last band's last line
        b = count - 1;
        int h = heights[b] & ~SCROLL_BAND_PER_LINE;
        entry -= (heights[b] & SCROLL_BAND_PER_LINE) ? h : 1;
        y = h > 0 ? h - 1 : 0;
    }
    first_off = y;
    int line = 0;
    int off = first_off;
    int16_t last = positions[entry + (((heights[b] & SCROLL_BAND_PER_LINE) != 0) ? first_off : 0)];
    for (; b < count && line < lines; b++) {
        int h = heights[b] & ~SCROLL_BAND_PER_LINE;
        bool per_line = (heights[b] & SCROLL_BAND_PER_LINE) != 0;
        for (; off < h && line < lines; off++, line++) {
            last = per_line ? positions[entry + off] : positions[entry];
            out[line][deform_background ? 1 : 0] = (int16_t)-last;
            out[line][deform_background ? 0 : 1] = (int16_t)-other_x;
        }
        entry += per_line ? h : 1;
        off = 0;
    }
    for (; line < lines; line++) { // (the table ended: the last position holds)
        out[line][deform_background ? 1 : 0] = (int16_t)-last;
        out[line][deform_background ? 0 : 1] = (int16_t)-other_x;
    }
}

// ---- band layers in a plane ----

static int FirstRow(const ScrollBands *sb, int16_t y) {
    (void)sb;
    return (y >> 4) - 1; // the rows drawn start one block above the view
}

static int BandOfRow(const ScrollBands *sb, int row) {
    return ScrollBands_BandAt(sb->heights, sb->count, row * 16, 0);
}

// A whole row of the plane (all the blocks it holds), with its band's X
static void DrawRow(const ScrollBands *sb, const LevelLayoutPlane *layout, int row) {
    int16_t sx = sb->x[BandOfRow(sb, row)];
    int16_t y = (int16_t)(row * 16);
    int16_t x = (int16_t)(-16 - (sx & 15));
    DrawBlocks_LR_2(sb->plane, CalcVRAMPos(sx, 0, x, y), sx, 0, x, y, layout, PLANE_WIDTH / 2);
}

// One column of blocks (world column `col`) for the rows of one band
static void DrawBandColumn(const ScrollBands *sb, const LevelLayoutPlane *layout, int band, int col) {
    int first = FirstRow(sb, sb->y);
    int16_t wx = (int16_t)(col * 16);
    for (int r = first; r < first + SCROLL_ROWS; r++)
        if (BandOfRow(sb, r) == band) {
            int16_t y = (int16_t)(r * 16);
            DrawBlocks_LR_2(sb->plane, CalcVRAMPos(wx, 0, 0, y), wx, 0, 0, y, layout, 1);
        }
}

void ScrollBands_Init(ScrollBands *sb, plane_t *plane, const uint16_t *heights, int count) {
    memset(sb, 0, sizeof(*sb));
    sb->plane = plane;
    sb->heights = heights;
    sb->count = count > SCROLL_MAX_BANDS ? SCROLL_MAX_BANDS : count;
}

void ScrollBands_DrawAll(ScrollBands *sb, const LevelLayoutPlane *layout) {
    int first = FirstRow(sb, sb->y);
    for (int r = first; r < first + SCROLL_ROWS; r++)
        DrawRow(sb, layout, r);
    memcpy(sb->drawn_x, sb->x, sizeof(sb->x));
    sb->drawn_y = sb->y;
}

void ScrollBands_Draw(ScrollBands *sb, const LevelLayoutPlane *layout) {
    int old_first = FirstRow(sb, sb->drawn_y), first = FirstRow(sb, sb->y);
    int dl = first - old_first;
    if (dl >= SCROLL_ROWS || -dl >= SCROLL_ROWS) {
        ScrollBands_DrawAll(sb, layout);
        return;
    }
    // Rows that came into the view, drawn whole with the positions now
    for (int r = old_first + SCROLL_ROWS; r < first + SCROLL_ROWS; r++)
        DrawRow(sb, layout, r);
    for (int r = first; r < old_first; r++)
        DrawRow(sb, layout, r);
    // Each band that crossed 16-pixel lines in X: the columns that came into the view, for its rows
    for (int b = 0; b < sb->count; b++) {
        int l0 = sb->drawn_x[b] >> 4, l1 = sb->x[b] >> 4, k = l1 - l0;
        if (k == 0)
            continue;
        if (k > 8 || k < -8) { // too far for columns: the band's rows again
            for (int r = first; r < first + SCROLL_ROWS; r++)
                if (BandOfRow(sb, r) == b)
                    DrawRow(sb, layout, r);
            continue;
        }
        for (int c = l0 + 1; c <= l1; c++)
            DrawBandColumn(sb, layout, b, c + RIGHT_EDGE_X / 16);
        for (int c = l0 - 1; c >= l1; c--)
            DrawBandColumn(sb, layout, b, c - 1);
    }
    memcpy(sb->drawn_x, sb->x, sizeof(sb->x));
    sb->drawn_y = sb->y;
}
