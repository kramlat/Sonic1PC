#include "SDL_render.h"
#include "SDL_timer.h"

#include "../VDP.h"
#include "../../Video.h"

#include <stdio.h>

// Render compile options
// #define DISPLAY_PADDING //Displays the internal VDP padding

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
    for (size_t i = 0; i < TEXTURE_HEIGHT; i++) {
        memcpy(to, screen, TEXTURE_WIDTH << 2);
        to += pitch;
        screen += SCREEN_WIDTH + (VDP_INTERNAL_PAD * 2);
    }

    // Unlock screen texture and draw to window
    SDL_UnlockTexture(texture);

    if (use_vsync_present) {
        // Let display vsync present at the right cadence to reduce tearing.
        for (int i = 0; i < vsync; i++) {
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }
    } else {
        SDL_RenderCopy(renderer, texture, NULL, NULL);
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
