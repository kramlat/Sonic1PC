#include "SDL_render.h"
#include "SDL_timer.h"
#include "SDL_events.h"
#include "SDL_mouse.h"
#include "SDL_video.h"

#include "../VDP.h"
#include "../../Video.h"
#include "../../Console.h"

#include "../Qt/QtHost.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Not guaranteed by strict C99 (M_PI is POSIX/BSD, not ISO C) -- define our
// own rather than relying on a feature-test macro.
#define PIE_TAU 6.283185307179586f

// Render compile options
// #define DISPLAY_PADDING //Displays the internal VDP padding

// Real Sonic hardware ran on a CRT, whose phosphors keep glowing for a few
// milliseconds after being lit rather than switching off instantly. That
// persistence blends consecutive frames together for anything moving fast
// on screen (e.g. Sonic at terminal fall speed, ~23px/frame), smearing it
// into a soft trail instead of the discrete per-frame jumps you get once
// each frame is shown crisply on a modern flat panel with zero decay. Our
// physics/rendering are frame-accurate (verified against real disasm and
// live debug traces), so the "jump" is genuine per-frame motion becoming
// visible for the first time, not a bug -- this blend restores the visual
// smoothing the CRT used to provide for free.
#define CRT_MOTION_BLUR
#define CRT_MOTION_BLUR_ALPHA 176 // Out of 255; new-frame weight per blend

// The picture's size is chosen at run time (Video.h's Video_SetResolution): the texture and the buffers follow it.
static int texture_width, texture_height;
#ifdef DISPLAY_PADDING
#define TEXTURE_WIDTH (texture_width + (VDP_INTERNAL_PAD * 2))
#else
#define TEXTURE_WIDTH texture_width
#endif
#define TEXTURE_HEIGHT texture_height

// Icon
#include "Resource/Icon.h"

// The frame and all overlays are rendered by SDL's software renderer into this surface,
// which the Qt window (Backend/Qt/QtHost) then shows on a drawing widget.
static SDL_Surface* target = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;

// Render state
int vsync;
static Uint64 perf_freq;
static Uint64 next_frame_time;

#ifdef CRT_MOTION_BLUR
static uint32_t *prev_frame = NULL; // TEXTURE_HEIGHT rows of TEXTURE_WIDTH
static int prev_frame_valid;
#endif

// Backend render interface. SDL renders (software renderer) into an RGBA surface the size of the
// windowed logical frame, and QtHost shows that surface on a drawing widget in a QMainWindow.
// The overlays below only need an SDL_Renderer. Frame pacing is our own clock (no display vsync).
static void hex_font_invalidate(void);

// (Re)creates everything sized by the picture: the surface the frame is drawn on, its renderer and texture, and the
// buffer the motion blur compares with.
static int CreateTargets(void) {
    texture_width = SCREEN_WIDTH;
    texture_height = SCREEN_HEIGHT;
    const int w = TEXTURE_WIDTH * SCREEN_SCALE, h = TEXTURE_HEIGHT * SCREEN_SCALE;

    if (texture != NULL)
        SDL_DestroyTexture(texture);
    if (renderer != NULL)
        SDL_DestroyRenderer(renderer);
    if (target != NULL)
        SDL_FreeSurface(target);
    texture = NULL;
    renderer = NULL;
    target = NULL;
    hex_font_invalidate();

    // ABGR8888 packed = bytes R,G,B,A in memory, which is what the widget reads.
    if ((target = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888)) == NULL ||
        (renderer = SDL_CreateSoftwareRenderer(target)) == NULL) {
        printf("Render_Init: %s\n", SDL_GetError());
        return -1;
    }
    if ((texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, TEXTURE_WIDTH, TEXTURE_HEIGHT)) == NULL) {
        printf("Render_Init: %s\n", SDL_GetError());
        return -1;
    }
#ifdef CRT_MOTION_BLUR
    free(prev_frame);
    prev_frame = (uint32_t*)calloc((size_t)TEXTURE_WIDTH * TEXTURE_HEIGHT, sizeof(uint32_t));
    prev_frame_valid = 0;
#endif
    return 0;
}

int Render_Init(const MD_Header* header) {
    texture_width = SCREEN_WIDTH;
    texture_height = SCREEN_HEIGHT;
    const int w = TEXTURE_WIDTH * SCREEN_SCALE, h = TEXTURE_HEIGHT * SCREEN_SCALE;
    if (QtHost_Init(header->title, w, h, (const uint8_t*)res_Icon) != 0)
        return -1;
    if (CreateTargets() != 0)
        return -1;

    vsync = 0;
    perf_freq = SDL_GetPerformanceFrequency();
    next_frame_time = SDL_GetPerformanceCounter();
    return 0;
}

// The picture changed size (the window follows it): see Video_SetResolution.
void Render_SetPictureSize(void) {
    if (CreateTargets() == 0)
        QtHost_SetPictureSize(TEXTURE_WIDTH * SCREEN_SCALE, TEXTURE_HEIGHT * SCREEN_SCALE);
}

// F11 / View menu: the Qt window handles fullscreen (and hides its menu bar while in it).
void Render_ToggleFullscreen(void) {
    QtHost_ToggleFullscreen();
}

// GM_Countdown's pie-wipe progress indicator -- see the comment on
// Render_SetCountdownPie's declaration (Backend/VDP.h) for why this lives
// here (SDL_RenderGeometry-drawn, PC-only) rather than as VDP tile art.
static bool countdown_pie_active = false;
static float countdown_pie_fraction = 0.0f;
static int countdown_pie_seconds = 0;

void Render_SetCountdownPie(bool active, float fraction, int seconds_left) {
    countdown_pie_active = active;
    countdown_pie_fraction = fraction;
    countdown_pie_seconds = seconds_left;
}

// Simple LED/7-segment digit, filled rectangles -- no font asset needed.
// Segment bit order: top, top-right, bottom-right, bottom, bottom-left,
// top-left, middle. 10-15 are hex A-F (upper A/C/E, lower b/d for the ones
// that would otherwise collide with a digit shape). Still used by the
// countdown's seconds-remaining number -- Z80 Peek's hex grid uses the
// game's own level-select/HUD font instead (see DrawHexByteFont below).
static const uint8_t segment_digits[16] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
                                            0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};

static void DrawSegmentDigit(int digit, int x, int y, int w, int h, int t, SDL_Color color) {
    if (digit < 0 || digit > 15)
        return;
    uint8_t segs = segment_digits[digit];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int half = h / 2;
    SDL_Rect r;
    if (segs & 0x01) { // top
        r.x = x + t; r.y = y; r.w = w - 2 * t; r.h = t;
        SDL_RenderFillRect(renderer, &r);
    }
    if (segs & 0x02) { // top-right
        r.x = x + w - t; r.y = y + t; r.w = t; r.h = half - t - t / 2;
        SDL_RenderFillRect(renderer, &r);
    }
    if (segs & 0x04) { // bottom-right
        r.x = x + w - t; r.y = y + half + t / 2; r.w = t; r.h = half - t - t / 2;
        SDL_RenderFillRect(renderer, &r);
    }
    if (segs & 0x08) { // bottom
        r.x = x + t; r.y = y + h - t; r.w = w - 2 * t; r.h = t;
        SDL_RenderFillRect(renderer, &r);
    }
    if (segs & 0x10) { // bottom-left
        r.x = x; r.y = y + half + t / 2; r.w = t; r.h = half - t - t / 2;
        SDL_RenderFillRect(renderer, &r);
    }
    if (segs & 0x20) { // top-left
        r.x = x; r.y = y + t; r.w = t; r.h = half - t - t / 2;
        SDL_RenderFillRect(renderer, &r);
    }
    if (segs & 0x40) { // middle
        r.x = x + t; r.y = y + half - t / 2; r.w = w - 2 * t; r.h = t;
        SDL_RenderFillRect(renderer, &r);
    }
}

// Z80 Peek's hex digits reuse the game's own level-select/HUD font
// (Art_Text, Resource/Art/Text.h -- see HUD_WriteHex and GM_Title.c's
// LevSelCharToTile for the two other places this same asset is used) rather
// than the 7-segment shapes above, per request. Declared, not #included --
// Game.c is the one place that includes the real header, so including it
// again here would double-define the array at link time (same reasoning as
// GM_Title.c's own copy of this extern).
extern const uint8_t Art_Text[];

// One 8x8 glyph per hex digit (0-9, A-F), decoded once from Art_Text into a
// small RGBA atlas texture -- Art_Text's tile format is the real VDP 4bpp
// format (2 pixels/byte, 4 bytes/row, 8 rows/tile = 32 bytes/tile, same
// layout VDP.c's own renderer decodes -- see its CRAMPAL indexing for the
// same nibble math). Palette index 0 -> transparent, anything else -> solid
// white; DrawHexByteFont tints it per call via SDL_SetTextureColorMod.
static SDL_Texture* hex_font_texture = NULL;
static void hex_font_invalidate(void) { hex_font_texture = NULL; } // its renderer is gone: it is rebuilt on next use

static void BuildHexFontTexture(void) {
    if (hex_font_texture)
        return;

    uint32_t pixels[8 * 8 * 16];
    for (int glyph = 0; glyph < 16; glyph++) {
        // Matches HUD_WriteHex's own digit->tile-index math exactly.
        int tile = glyph;
        if (tile >= 0xA)
            tile += 7;
        const uint8_t* src = Art_Text + tile * 32;
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                uint8_t byte = src[row * 4 + col / 2];
                uint8_t nibble = (col & 1) ? (byte & 0xF) : (byte >> 4);
                pixels[row * (8 * 16) + glyph * 8 + col] = nibble ? 0xFFFFFFFFu : 0x00000000u;
            }
        }
    }

    hex_font_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 8 * 16, 8);
    SDL_SetTextureBlendMode(hex_font_texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(hex_font_texture, NULL, pixels, 8 * 16 * 4);
}

static void DrawHexDigitFont(int digit, int x, int y, int scale, SDL_Color color) {
    if (digit < 0 || digit > 15)
        return;
    SDL_SetTextureColorMod(hex_font_texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(hex_font_texture, color.a);
    SDL_Rect src = {digit * 8, 0, 8, 8};
    SDL_Rect dst = {x, y, 8 * scale, 8 * scale};
    SDL_RenderCopy(renderer, hex_font_texture, &src, &dst);
}

static void DrawHexByteFont(uint8_t value, int x, int y, int scale, SDL_Color color) {
    DrawHexDigitFont((value >> 4) & 0xF, x, y, scale, color);
    DrawHexDigitFont(value & 0xF, x + 8 * scale, y, scale, color);
}

// Z80 Peek: live FM/PSG register dump. See Backend/VDP.h for why this data
// arrives pre-gathered from Game.c rather than this file reaching into
// Sound.c itself.
void Render_SetZ80Peek(bool active, const Z80PeekData *data) {
    QtHost_SetZ80Peek(active, data); // shown in the Sound Viewer window
}

// Debug console only -- saves the raw pre-overlay frame (this file's own
// CRT-blur history buffer, already exactly what was actually displayed
// each frame sans DrawCountdownPie/DrawZ80Peek, all of which
// draw AFTER this buffer is populated) as an uncompressed PPM -- no
// external image library needed for a debug-only dump.
#ifdef CRT_MOTION_BLUR
bool Render_SaveScreenshot(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    fprintf(f, "P6\n%d %d\n255\n", TEXTURE_WIDTH, TEXTURE_HEIGHT);
    for (int y = 0; y < TEXTURE_HEIGHT; y++) {
        for (int x = 0; x < TEXTURE_WIDTH; x++) {
            const uint8_t *px = (const uint8_t *)&prev_frame[y * TEXTURE_WIDTH + x]; // R,G,B,A byte order (SDL_PIXELFORMAT_RGBA8888)
            uint8_t rgb[3] = {px[0], px[1], px[2]};
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    return true;
}
#else
bool Render_SaveScreenshot(const char *path) {
    (void)path;
    return false; // no frame history buffer without CRT_MOTION_BLUR
}
#endif

static void DrawCountdownPie(void) {
    if (!countdown_pie_active)
        return;

    const int segments = 60;
    const float cx = (TEXTURE_WIDTH * SCREEN_SCALE) / 2.0f;
    const float cy = (TEXTURE_HEIGHT * SCREEN_SCALE) / 2.0f;
    const float radius = 72.0f * SCREEN_SCALE;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Dim full-circle backdrop, drawn first, so the pie shape reads clearly
    // even right at the start (fraction near 0) or end (near 1).
    {
        SDL_Vertex verts[62];
        SDL_Color dim = {32, 32, 40, 190};
        verts[0].position.x = cx;
        verts[0].position.y = cy;
        verts[0].color = dim;
        verts[0].tex_coord.x = verts[0].tex_coord.y = 0;
        for (int i = 0; i <= segments; i++) {
            float angle = (float)i / segments * PIE_TAU;
            verts[i + 1].position.x = cx + sinf(angle) * radius;
            verts[i + 1].position.y = cy - cosf(angle) * radius;
            verts[i + 1].color = dim;
            verts[i + 1].tex_coord.x = verts[i + 1].tex_coord.y = 0;
        }
        SDL_RenderGeometry(renderer, NULL, verts, segments + 2, NULL, 0);
    }

    // Bright fill sweeping clockwise from 12 o'clock, proportional to how
    // much of the countdown has elapsed.
    if (countdown_pie_fraction > 0.0f) {
        int fill_segments = (int)(segments * countdown_pie_fraction);
        if (fill_segments < 1)
            fill_segments = 1;
        if (fill_segments > segments)
            fill_segments = segments;
        SDL_Vertex verts[62];
        SDL_Color bright = {80, 200, 255, 255};
        verts[0].position.x = cx;
        verts[0].position.y = cy;
        verts[0].color = bright;
        verts[0].tex_coord.x = verts[0].tex_coord.y = 0;
        for (int i = 0; i <= fill_segments; i++) {
            float angle = (float)i / segments * PIE_TAU;
            verts[i + 1].position.x = cx + sinf(angle) * radius;
            verts[i + 1].position.y = cy - cosf(angle) * radius;
            verts[i + 1].color = bright;
            verts[i + 1].tex_coord.x = verts[i + 1].tex_coord.y = 0;
        }
        SDL_RenderGeometry(renderer, NULL, verts, fill_segments + 2, NULL, 0);
    }

    // Countdown number, drawn on top so it composites over the pie fill
    // instead of being covered by it.
    {
        int seconds = countdown_pie_seconds;
        if (seconds < 0)
            seconds = 0;
        if (seconds > 99)
            seconds = 99;
        int tens = seconds / 10, ones = seconds % 10;
        SDL_Color white = {255, 255, 255, 255};
        const int digit_w = 22 * SCREEN_SCALE / 2, digit_h = 34 * SCREEN_SCALE / 2, digit_t = 4 * SCREEN_SCALE / 2;
        const int gap = 8 * SCREEN_SCALE / 2;
        int total_w = digit_w * 2 + gap;
        int start_x = (int)(cx - total_w / 2);
        int start_y = (int)(cy - digit_h / 2);
        DrawSegmentDigit(tens, start_x, start_y, digit_w, digit_h, digit_t, white);
        DrawSegmentDigit(ones, start_x + digit_w + gap, start_y, digit_w, digit_h, digit_t, white);
    }
}

void Render_Quit(void) {
    // Destroy screen texture
    if (texture != NULL)
        SDL_DestroyTexture(texture);

    // Destroy renderer, surface and window
    if (renderer != NULL)
        SDL_DestroyRenderer(renderer);
    if (target != NULL)
        SDL_FreeSurface(target);
    QtHost_Quit();
}

// This takes in the internal VDP screen buffer positioned after the padding
void Render_Screen(const uint32_t* screen) {
    // Lock screen texture
    uint8_t* to;
    int pitch;
    SDL_LockTexture(texture, NULL, (void**)&to, &pitch);

// Copy screen
#ifdef DISPLAY_PADDING
    screen -= VDP_INTERNAL_PAD;
#endif
#ifdef CRT_MOTION_BLUR
    if (!prev_frame_valid) {
        // First frame -- nothing to blend with yet, show it as-is.
        for (size_t i = 0; i < TEXTURE_HEIGHT; i++) {
            memcpy(to, screen, TEXTURE_WIDTH << 2);
            memcpy(prev_frame + (size_t)i * TEXTURE_WIDTH, screen, TEXTURE_WIDTH << 2);
            to += pitch;
            screen += SCREEN_WIDTH + (VDP_INTERNAL_PAD * 2);
        }
        prev_frame_valid = 1;
    } else {
        for (size_t i = 0; i < TEXTURE_HEIGHT; i++) {
            uint8_t* out_row = to;
            const uint8_t* new_row = (const uint8_t*)screen;
            uint8_t* prev_row = (uint8_t*)(prev_frame + (size_t)i * TEXTURE_WIDTH);
            for (size_t x = 0; x < (TEXTURE_WIDTH << 2); x++) {
                uint8_t blended = (uint8_t)((new_row[x] * CRT_MOTION_BLUR_ALPHA + prev_row[x] * (255 - CRT_MOTION_BLUR_ALPHA)) / 255);
                out_row[x] = blended;
                prev_row[x] = blended;
            }
            to += pitch;
            screen += SCREEN_WIDTH + (VDP_INTERNAL_PAD * 2);
        }
    }
#else
    for (size_t i = 0; i < TEXTURE_HEIGHT; i++) {
        memcpy(to, screen, TEXTURE_WIDTH << 2);
        to += pitch;
        screen += SCREEN_WIDTH + (VDP_INTERNAL_PAD * 2);
    }
#endif

    // Unlock screen texture and draw to window
    SDL_UnlockTexture(texture);

    SDL_RenderCopy(renderer, texture, NULL, NULL);
    DrawCountdownPie();
    QtHost_Present(target->pixels, target->pitch);

    // Pace ourselves against our own monotonic clock: the Qt widget presents without
    // waiting for the display's refresh, so this is what holds the game at 60 FPS.
    next_frame_time += perf_freq / 60;

    Uint64 now = SDL_GetPerformanceCounter();
    if (next_frame_time > now) {
        Uint64 remaining = next_frame_time - now;
        Uint32 ms = (Uint32)(remaining * 1000 / perf_freq);
        // Sleep for the coarse remainder, leaving a little headroom
        // since SDL_Delay can overshoot on some platforms/schedulers.
        if (ms > 1)
            SDL_Delay(ms - 1);
        // Spin for the last sliver for sub-millisecond precision.
        while (SDL_GetPerformanceCounter() < next_frame_time)
            ;
    } else {
        // We're behind schedule (e.g. after a debugger pause or a
        // long hitch). Resync rather than trying to burn through a
        // backlog of missed frames at full speed.
        next_frame_time = now;
    }
}
