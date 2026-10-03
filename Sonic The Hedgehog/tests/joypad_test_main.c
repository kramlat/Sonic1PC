// Joypad test tool: shows a row of icons (the four D-pad directions, then
// A/B/C/Start) that light up while the matching button is held and dim
// back down when it's released. Reads real input through the same
// Joypad_GetState1()/Input_HandleEvents() path the game itself uses --
// keyboard (including the WASD/arrows + JKL scheme) and any connected
// SDL_GameController (including Steam Input's virtual pad) both work here
// exactly as they would in-game.
//
// Usage: SonicJoypadTest

#include "Backend/Joypad.h"

#include <SDL.h>
#include <math.h>
#include <stdbool.h>

// Not declared in any header (VDP.c forward-declares it locally) -- pumps
// SDL events, returns nonzero on SDL_QUIT, and (as of the gamepad support
// added to Input.c) opens/closes the SDL_GameController on hotplug.
extern int Input_HandleEvents(void);

#define ICON_COUNT   8
#define ICON_SIZE    64
#define ICON_GAP     24
#define WINDOW_MARGIN 40

static const uint8_t icon_bits[ICON_COUNT] = {
    JPAD_UP, JPAD_DOWN, JPAD_LEFT, JPAD_RIGHT,
    JPAD_A, JPAD_B, JPAD_C, JPAD_START,
};

// Base hue per icon so they stay visually distinguishable even when dim
// (arrows: cool blue-white; A/B/C: red/green/blue; Start: yellow).
static const SDL_Color icon_colour[ICON_COUNT] = {
    {200, 220, 255, 255}, {200, 220, 255, 255}, {200, 220, 255, 255}, {200, 220, 255, 255},
    {255, 80, 80, 255}, {80, 255, 120, 255}, {80, 160, 255, 255}, {255, 220, 60, 255},
};

static void FillTriangle(SDL_Renderer *r, SDL_Point p0, SDL_Point p1, SDL_Point p2) {
    // Standard top-to-bottom scanline fill: sort by y, then interpolate the
    // left/right edge x for every row between the three points.
    SDL_Point pts[3] = {p0, p1, p2};
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2 - i; j++)
            if (pts[j].y > pts[j + 1].y) {
                SDL_Point tmp = pts[j];
                pts[j] = pts[j + 1];
                pts[j + 1] = tmp;
            }

    for (int y = pts[0].y; y <= pts[2].y; y++) {
        bool second_half = y > pts[1].y || pts[1].y == pts[0].y;
        int seg_top = second_half ? pts[1].y : pts[0].y;
        int seg_bot = second_half ? pts[2].y : pts[1].y;
        float alpha = (float)(y - pts[0].y) / (float)(pts[2].y - pts[0].y ? pts[2].y - pts[0].y : 1);
        float beta = seg_bot != seg_top ? (float)(y - seg_top) / (float)(seg_bot - seg_top) : 0.0f;

        int xa = pts[0].x + (int)((pts[2].x - pts[0].x) * alpha);
        int xb = second_half
                     ? pts[1].x + (int)((pts[2].x - pts[1].x) * beta)
                     : pts[0].x + (int)((pts[1].x - pts[0].x) * beta);

        if (xa > xb) { int tmp = xa; xa = xb; xb = tmp; }
        SDL_RenderDrawLine(r, xa, y, xb, y);
    }
}

static void FillCircle(SDL_Renderer *r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)lround(sqrt((double)(radius * radius - dy * dy)));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void DrawIcon(SDL_Renderer *r, int index, int cx, int cy, bool held) {
    SDL_Color c = icon_colour[index];
    float scale = held ? 1.0f : 0.28f;
    SDL_SetRenderDrawColor(r, (uint8_t)(c.r * scale), (uint8_t)(c.g * scale), (uint8_t)(c.b * scale), 255);

    const int s = ICON_SIZE / 2;
    switch (index) {
        case 0: // Up
            FillTriangle(r, (SDL_Point){cx, cy - s}, (SDL_Point){cx - s, cy + s}, (SDL_Point){cx + s, cy + s});
            break;
        case 1: // Down
            FillTriangle(r, (SDL_Point){cx, cy + s}, (SDL_Point){cx - s, cy - s}, (SDL_Point){cx + s, cy - s});
            break;
        case 2: // Left
            FillTriangle(r, (SDL_Point){cx - s, cy}, (SDL_Point){cx + s, cy - s}, (SDL_Point){cx + s, cy + s});
            break;
        case 3: // Right
            FillTriangle(r, (SDL_Point){cx + s, cy}, (SDL_Point){cx - s, cy - s}, (SDL_Point){cx - s, cy + s});
            break;
        case 4: // A
        case 5: // B
        case 6: // C
            FillCircle(r, cx, cy, s);
            break;
        case 7: { // Start
            SDL_Rect box = {cx - s, cy - s, ICON_SIZE, ICON_SIZE};
            SDL_RenderFillRect(r, &box);
            break;
        }
    }

    // Faint outline so a fully-dim icon is still visible against the background.
    SDL_SetRenderDrawColor(r, 60, 60, 70, 255);
    SDL_Rect outline = {cx - s - 4, cy - s - 4, ICON_SIZE + 8, ICON_SIZE + 8};
    SDL_RenderDrawRect(r, &outline);
}

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    int window_w = WINDOW_MARGIN * 2 + ICON_COUNT * ICON_SIZE + (ICON_COUNT - 1) * ICON_GAP;
    int window_h = ICON_SIZE + WINDOW_MARGIN * 2;

    SDL_Window *window = SDL_CreateWindow("Joypad Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           window_w, window_h, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!window || !renderer) {
        SDL_Log("Failed to create window/renderer: %s", SDL_GetError());
        return 1;
    }

    for (;;) {
        if (Input_HandleEvents())
            break;

        uint8_t state = Joypad_GetState1();

        SDL_SetRenderDrawColor(renderer, 24, 24, 28, 255);
        SDL_RenderClear(renderer);

        int cy = window_h / 2;
        for (int i = 0; i < ICON_COUNT; i++) {
            int cx = WINDOW_MARGIN + ICON_SIZE / 2 + i * (ICON_SIZE + ICON_GAP);
            DrawIcon(renderer, i, cx, cy, (state & icon_bits[i]) != 0);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
