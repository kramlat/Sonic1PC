#include "SDL_render.h"
#include "SDL_timer.h"

#include "../VDP.h"
#include "../../Video.h"
#include "../../Console.h"

#include <math.h>
#include <stdio.h>

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

#ifdef DISPLAY_PADDING
#define TEXTURE_WIDTH (SCREEN_WIDTH + (VDP_INTERNAL_PAD * 2))
#define TEXTURE_HEIGHT SCREEN_HEIGHT
#else
#define TEXTURE_WIDTH SCREEN_WIDTH
#define TEXTURE_HEIGHT SCREEN_HEIGHT
#endif

// Icon
#include "Resource/Icon.h"

// Window and renderer
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;

// Render state
int vsync;
static int use_vsync_present; //Whether display vsync itself is trustworthy for pacing (exact 60Hz multiple)
static Uint64 perf_freq;
static Uint64 next_frame_time;

#ifdef CRT_MOTION_BLUR
static uint32_t prev_frame[TEXTURE_HEIGHT][TEXTURE_WIDTH];
static int prev_frame_valid;
#endif

// Backend render interface
int Render_Init(const MD_Header* header) {
    // Create window
    if ((window = SDL_CreateWindow(header->title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, TEXTURE_WIDTH * SCREEN_SCALE, TEXTURE_HEIGHT * SCREEN_SCALE, SDL_WINDOW_HIDDEN)) == NULL) {
        printf("Render_Init: %s\n", SDL_GetError());
        return -1;
    }

    // Load icon
    SDL_Surface* icon_surface;
    if ((icon_surface = SDL_CreateRGBSurfaceWithFormatFrom((void*)res_Icon, 16, 16, 24, 16 * 3, SDL_PIXELFORMAT_RGB24)) == NULL) {
        printf("Render_Init: %s\n", SDL_GetError());
    } else {
        SDL_SetWindowIcon(window, icon_surface);
        SDL_FreeSurface(icon_surface);
    }

    // Show window now that the icon's been loaded
    SDL_ShowWindow(window);

    // Check if VSync should be used
    SDL_DisplayMode display_mode;
    SDL_GetWindowDisplayMode(window, &display_mode);
    if (display_mode.refresh_rate > 0 && (display_mode.refresh_rate % 60) == 0) {
        // Display refresh rate is a clean multiple of 60Hz -- hardware vsync
        // itself gives us correct pacing, for free and tear-free.
        vsync = display_mode.refresh_rate / 60;
        use_vsync_present = 1;
    } else {
        // Non-standard/uneven refresh rate (75Hz, 90Hz, 144Hz, 165Hz, etc.),
        // or none detected at all (e.g. headless). Hardware vsync's cadence
        // can't be trusted to average out to 60Hz here, so we pace frames
        // ourselves against a monotonic clock instead. This used to just
        // present once with no delay at all in this case, running the game
        // completely unthrottled.
        vsync = 0;
        use_vsync_present = 0;
    }

    // Create renderer
    if ((renderer = SDL_CreateRenderer(window, -1, use_vsync_present ? SDL_RENDERER_PRESENTVSYNC : 0)) == NULL) {
        printf("Render_Init: %s\n", SDL_GetError());
        return -1;
    }

    // Set up our own frame clock. Used as the sole pacing source when
    // display vsync isn't trustworthy, and to keep the two in sync
    // (avoiding drift) when it is.
    perf_freq = SDL_GetPerformanceFrequency();
    next_frame_time = SDL_GetPerformanceCounter();

    // Create screen texture
    if ((texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, TEXTURE_WIDTH, TEXTURE_HEIGHT)) == NULL) {
        printf("Render_Init: %s\n", SDL_GetError());
        return -1;
    }

    return 0;
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
static bool z80_peek_active = false;
static Z80PeekData z80_peek_data;

void Render_SetZ80Peek(bool active, const Z80PeekData *data) {
    z80_peek_active = active;
    if (active && data)
        z80_peek_data = *data;
}

static void DrawZ80Peek(void) {
    if (!z80_peek_active)
        return;
    BuildHexFontTexture();

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_Rect bg = {4, 4, 300, 210};
    SDL_RenderFillRect(renderer, &bg);

    const int scale = 1, byte_w = 8 * scale * 2, row_h = 14;
    const SDL_Color white = {255, 255, 255, 255};
    const SDL_Color yellow = {255, 220, 80, 255};
    const SDL_Color dim = {110, 110, 110, 255};
    int y = 8;

    // PSG: 3 tone channels (period, attenuation) + noise (shift rate/fb mode, attenuation)
    for (int c = 0; c < 3; c++) {
        int x = 8;
        SDL_Color atten_color = (z80_peek_data.psg_tone_atten[c] < 15) ? yellow : dim; // <15 = audible
        DrawHexByteFont((uint8_t)(z80_peek_data.psg_tone_period[c] >> 8), x, y, scale, white);
        x += byte_w + 4;
        DrawHexByteFont((uint8_t)(z80_peek_data.psg_tone_period[c] & 0xFF), x, y, scale, white);
        x += byte_w + 10;
        DrawHexByteFont(z80_peek_data.psg_tone_atten[c], x, y, scale, atten_color);
        y += row_h;
    }
    {
        int x = 8;
        uint8_t noise_byte = (uint8_t)((z80_peek_data.psg_noise_fb_white << 2) | z80_peek_data.psg_noise_shift_rate);
        SDL_Color atten_color = (z80_peek_data.psg_noise_atten < 15) ? yellow : dim;
        DrawHexByteFont(noise_byte, x, y, scale, white);
        x += byte_w + 10;
        DrawHexByteFont(z80_peek_data.psg_noise_atten, x, y, scale, atten_color);
        y += row_h + 6;
    }

    // FM: 6 channels (2 ports x 3), each: alg/feedback byte, 4 operator TL bytes, key-on dot.
    for (int port = 0; port < 2; port++) {
        for (int ch = 0; ch < 3; ch++) {
            int chan_num = port * 3 + ch; // 0-5 = FM1-6
            int x = 8;

            bool keyed_on = (z80_peek_data.fm_keyon & (1 << chan_num)) != 0;
            SDL_SetRenderDrawColor(renderer, keyed_on ? 80 : 60, keyed_on ? 220 : 60, keyed_on ? 100 : 60, 255);
            SDL_Rect dot = {x, y + 1, 8, 8};
            SDL_RenderFillRect(renderer, &dot);
            x += 14;

            DrawHexByteFont(z80_peek_data.fm_alg_fb[port][ch], x, y, scale, white);
            x += byte_w + 10;
            for (int op = 0; op < 4; op++) {
                SDL_Color tl_color = (z80_peek_data.fm_tl[port][ch][op] < 100) ? yellow : dim;
                DrawHexByteFont(z80_peek_data.fm_tl[port][ch][op], x, y, scale, tl_color);
                x += byte_w + 4;
            }
            y += row_h;
        }
    }
}

// ---------------------------------------------------------------------
// Debug console overlay (Console.c) -- SonicSmoke only, console_enabled
// stays false in the real Sonic executable so none of this ever runs
// there. Needs a real alphabet (command text, variable names), unlike
// Z80 Peek's hex-only font above, so it gets its own small embedded 3x5
// dot-matrix font instead of extending Art_Text (which was never meant
// to cover more than hex digits) -- each glyph written as literal
// on/off rows right here so it's checkable by eye, not a giant opaque
// hex table. Uppercase-only (lowercase input is folded to uppercase for
// display; the console's own command parsing is untouched -- see
// Console.c, which stores/compares the original typed case).
// ---------------------------------------------------------------------

// One row per array entry, low 3 bits = left/mid/right pixel (bit2=left).
static bool GetGlyphRows(char c, uint8_t rows[5]) {
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    switch (c) {
        case '0': rows[0]=7; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=7; return true;
        case '1': rows[0]=2; rows[1]=6; rows[2]=2; rows[3]=2; rows[4]=7; return true;
        case '2': rows[0]=7; rows[1]=1; rows[2]=7; rows[3]=4; rows[4]=7; return true;
        case '3': rows[0]=7; rows[1]=1; rows[2]=3; rows[3]=1; rows[4]=7; return true;
        case '4': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=1; rows[4]=1; return true;
        case '5': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=1; rows[4]=7; return true;
        case '6': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=5; rows[4]=7; return true;
        case '7': rows[0]=7; rows[1]=1; rows[2]=1; rows[3]=1; rows[4]=1; return true;
        case '8': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=7; return true;
        case '9': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=1; rows[4]=7; return true;
        case 'A': rows[0]=2; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=5; return true;
        case 'B': rows[0]=6; rows[1]=5; rows[2]=6; rows[3]=5; rows[4]=6; return true;
        case 'C': rows[0]=3; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=3; return true;
        case 'D': rows[0]=6; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=6; return true;
        case 'E': rows[0]=7; rows[1]=4; rows[2]=6; rows[3]=4; rows[4]=7; return true;
        case 'F': rows[0]=7; rows[1]=4; rows[2]=6; rows[3]=4; rows[4]=4; return true;
        case 'G': rows[0]=3; rows[1]=4; rows[2]=5; rows[3]=5; rows[4]=3; return true;
        case 'H': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=5; return true;
        case 'I': rows[0]=7; rows[1]=2; rows[2]=2; rows[3]=2; rows[4]=7; return true;
        case 'J': rows[0]=1; rows[1]=1; rows[2]=1; rows[3]=5; rows[4]=2; return true;
        case 'K': rows[0]=5; rows[1]=5; rows[2]=6; rows[3]=5; rows[4]=5; return true;
        case 'L': rows[0]=4; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=7; return true;
        case 'M': rows[0]=5; rows[1]=7; rows[2]=7; rows[3]=5; rows[4]=5; return true;
        case 'N': rows[0]=5; rows[1]=7; rows[2]=7; rows[3]=7; rows[4]=5; return true;
        case 'O': rows[0]=2; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=2; return true;
        case 'P': rows[0]=6; rows[1]=5; rows[2]=6; rows[3]=4; rows[4]=4; return true;
        case 'Q': rows[0]=2; rows[1]=5; rows[2]=5; rows[3]=2; rows[4]=1; return true;
        case 'R': rows[0]=6; rows[1]=5; rows[2]=6; rows[3]=5; rows[4]=5; return true;
        case 'S': rows[0]=3; rows[1]=4; rows[2]=2; rows[3]=1; rows[4]=6; return true;
        case 'T': rows[0]=7; rows[1]=2; rows[2]=2; rows[3]=2; rows[4]=2; return true;
        case 'U': rows[0]=5; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=2; return true;
        case 'V': rows[0]=5; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=2; return true;
        case 'W': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=7; rows[4]=5; return true;
        case 'X': rows[0]=5; rows[1]=5; rows[2]=2; rows[3]=5; rows[4]=5; return true;
        case 'Y': rows[0]=5; rows[1]=5; rows[2]=2; rows[3]=2; rows[4]=2; return true;
        case 'Z': rows[0]=7; rows[1]=1; rows[2]=2; rows[3]=4; rows[4]=7; return true;
        case '=': rows[0]=0; rows[1]=7; rows[2]=0; rows[3]=7; rows[4]=0; return true;
        case '_': rows[0]=0; rows[1]=0; rows[2]=0; rows[3]=0; rows[4]=7; return true;
        case '-': rows[0]=0; rows[1]=0; rows[2]=7; rows[3]=0; rows[4]=0; return true;
        case '.': rows[0]=0; rows[1]=0; rows[2]=0; rows[3]=0; rows[4]=2; return true;
        case ',': rows[0]=0; rows[1]=0; rows[2]=0; rows[3]=2; rows[4]=4; return true;
        case '\'':rows[0]=2; rows[1]=2; rows[2]=0; rows[3]=0; rows[4]=0; return true;
        case '?': rows[0]=7; rows[1]=1; rows[2]=2; rows[3]=0; rows[4]=2; return true;
        case ':': rows[0]=0; rows[1]=2; rows[2]=0; rows[3]=2; rows[4]=0; return true;
        case '>': rows[0]=4; rows[1]=2; rows[2]=1; rows[3]=2; rows[4]=4; return true;
        case '<': rows[0]=1; rows[1]=2; rows[2]=4; rows[3]=2; rows[4]=1; return true;
        case '/': rows[0]=1; rows[1]=1; rows[2]=2; rows[3]=4; rows[4]=4; return true;
        case ' ': rows[0]=0; rows[1]=0; rows[2]=0; rows[3]=0; rows[4]=0; return true;
        default:  return false; // unsupported char -- caller skips it (blank space)
    }
}

#define CONSOLE_GLYPH_W 3
#define CONSOLE_GLYPH_H 5
#define CONSOLE_GLYPH_SCALE 2 // on-screen pixels per font-pixel
#define CONSOLE_CHAR_W ((CONSOLE_GLYPH_W + 1) * CONSOLE_GLYPH_SCALE) // +1 column of spacing
#define CONSOLE_CHAR_H ((CONSOLE_GLYPH_H + 2) * CONSOLE_GLYPH_SCALE) // +2 rows of spacing

static void DrawConsoleChar(char c, int x, int y, SDL_Color color) {
    uint8_t rows[5];
    if (!GetGlyphRows(c, rows))
        return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int row = 0; row < CONSOLE_GLYPH_H; row++) {
        for (int col = 0; col < CONSOLE_GLYPH_W; col++) {
            if (!(rows[row] & (1 << (CONSOLE_GLYPH_W - 1 - col))))
                continue;
            SDL_Rect r = {x + col * CONSOLE_GLYPH_SCALE, y + row * CONSOLE_GLYPH_SCALE,
                          CONSOLE_GLYPH_SCALE, CONSOLE_GLYPH_SCALE};
            SDL_RenderFillRect(renderer, &r);
        }
    }
}

static void DrawConsoleText(const char *text, int x, int y, SDL_Color color) {
    int cx = x;
    for (const char *c = text; *c != '\0'; c++) {
        DrawConsoleChar(*c, cx, y, color);
        cx += CONSOLE_CHAR_W;
    }
}

static void DrawConsole(void) {
    if (!console_enabled || !Console_IsOpen())
        return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 10, 10, 20, 220);
    SDL_Rect bg = {0, 0, TEXTURE_WIDTH * SCREEN_SCALE, (CONSOLE_LOG_LINES + 2) * CONSOLE_CHAR_H};
    SDL_RenderFillRect(renderer, &bg);

    const SDL_Color white = {255, 255, 255, 255};
    const SDL_Color green = {120, 255, 120, 255};

    int y = CONSOLE_CHAR_H / 2;
    for (int i = CONSOLE_LOG_LINES - 1; i >= 0; i--) {
        const char *line = Console_GetLogLine(i);
        if (line != NULL)
            DrawConsoleText(line, CONSOLE_CHAR_W, y, white);
        y += CONSOLE_CHAR_H;
    }

    // Input line with a simple blinking-block cursor at the current
    // position (Console_GetCursor is always end-of-buffer for now, no
    // mid-line editing -- see Console.c's own comment).
    char prompt[CONSOLE_LINE_LEN + 2];
    snprintf(prompt, sizeof(prompt), "> %s", Console_GetInputLine());
    DrawConsoleText(prompt, CONSOLE_CHAR_W, y, green);
    if (((SDL_GetTicks() / 400) & 1) == 0) {
        int cursor_x = CONSOLE_CHAR_W + (2 + Console_GetCursor()) * CONSOLE_CHAR_W;
        SDL_SetRenderDrawColor(renderer, 120, 255, 120, 255);
        SDL_Rect cur = {cursor_x, y, CONSOLE_CHAR_W - CONSOLE_GLYPH_SCALE, CONSOLE_CHAR_H - CONSOLE_GLYPH_SCALE};
        SDL_RenderFillRect(renderer, &cur);
    }
}

// Debug console only -- saves the raw pre-overlay frame (this file's own
// CRT-blur history buffer, already exactly what was actually displayed
// each frame sans DrawCountdownPie/DrawZ80Peek/DrawConsole, all of which
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
            const uint8_t *px = (const uint8_t *)&prev_frame[y][x]; // R,G,B,A byte order (SDL_PIXELFORMAT_RGBA8888)
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

    // Destroy window and renderer
    if (renderer != NULL)
        SDL_DestroyRenderer(renderer);
    if (window != NULL)
        SDL_DestroyWindow(window);
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
            memcpy(prev_frame[i], screen, TEXTURE_WIDTH << 2);
            to += pitch;
            screen += SCREEN_WIDTH + (VDP_INTERNAL_PAD * 2);
        }
        prev_frame_valid = 1;
    } else {
        for (size_t i = 0; i < TEXTURE_HEIGHT; i++) {
            uint8_t* out_row = to;
            const uint8_t* new_row = (const uint8_t*)screen;
            uint8_t* prev_row = (uint8_t*)prev_frame[i];
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

    if (use_vsync_present) {
        // Let display vsync present at the right cadence to reduce tearing.
        for (int i = 0; i < vsync; i++) {
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            DrawCountdownPie();
            DrawZ80Peek();
            DrawConsole();
            SDL_RenderPresent(renderer);
        }
    } else {
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        DrawCountdownPie();
        DrawZ80Peek();
        DrawConsole();
        SDL_RenderPresent(renderer);
    }

    // Always pace ourselves against our own monotonic clock as the
    // authoritative backstop, regardless of whether hardware vsync is
    // in play. Presenting with SDL_RENDERER_PRESENTVSYNC is *supposed*
    // to block until the display's next refresh, but that can't be
    // trusted blindly -- SDL's dummy video driver (and some real
    // broken drivers/VMs/remote desktop setups) silently doesn't
    // block at all despite reporting a perfectly clean 60Hz-multiple
    // refresh rate. If vsync did block us past our target time, this
    // wait becomes a no-op; if it didn't, this is what actually
    // enforces correct speed.
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
