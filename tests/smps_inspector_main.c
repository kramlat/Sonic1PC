// SMPSInspector: a real-time per-channel activity visualizer for the SMPS
// engine, inspired by Knuckles Chaotix's extended sound test screen -- FM1-6
// and PSG1-3 shown as piano-key strips (the currently-playing note's key
// lights up), the dedicated noise slot (PSG4) as a row of squares instead
// (it has no discrete pitch the way tone channels do), and DAC as 8 squares
// -- one per real distinct sound the $81-$8B command range defines on real
// hardware (Sega/Kick/Snare/Timpani, then Hi/Mid/Low/Floor-Timpani; see the
// $81-$8B DAC dispatch comment in Sound.c). Left/Right cycles through
// registered songs.
//
// Deliberately no texture-based rendering (no font atlas, no sprite
// blitting) -- everything is plain SDL primitive shapes (filled rects for
// keys/squares, a small local 7-segment digit drawer for the song ID
// header), same "PC-only overlay, no new art assets" spirit as
// GM_Countdown's pie wipe.

#include "../src/Sound.h"

#include <SDL.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define FRAME_HZ    60
#define WIN_W       960
#define WIN_H       540

// Piano strip covers Sound.c's full note_index range (0-95, 8 octaves) --
// see FM_FNUM_TABLE/PSG_PERIOD_TABLE's own 12-note-per-octave layout.
#define NOTE_COUNT 96

typedef struct {
    const char *label;
    int channel_index; // SOUND_CHANNEL_* -- -1 for the DAC row, handled separately
} ChannelRow;

static const ChannelRow rows[] = {
    {"FM1", SOUND_CHANNEL_FM_BASE + 0}, {"FM2", SOUND_CHANNEL_FM_BASE + 1}, {"FM3", SOUND_CHANNEL_FM_BASE + 2},
    {"FM4", SOUND_CHANNEL_FM_BASE + 3}, {"FM5", SOUND_CHANNEL_FM_BASE + 4}, {"FM6", SOUND_CHANNEL_FM_BASE + 5},
    {"PSG1", SOUND_CHANNEL_PSG_BASE + 0}, {"PSG2", SOUND_CHANNEL_PSG_BASE + 1}, {"PSG3", SOUND_CHANNEL_PSG_BASE + 2},
    {"NOISE", SOUND_CHANNEL_PSG_BASE + 3},
    {"DAC", -1},
};
#define ROW_COUNT (int)(sizeof(rows) / sizeof(rows[0]))

// Only show rows for channels the current song's own header actually
// allocates -- matches LoadMusic's own parsing (Sound.c): fm_count>0 means
// a DAC block is present (even if it's just a bare-stop placeholder for the
// FM6-trick case -- still worth showing, since that's a real header slot),
// and (fm_count-1) real FM tracks follow it; psg_count real PSG tone
// tracks, with the dedicated 4th "NOISE" slot only present when
// psg_count==4.
static bool RowVisible(int channel_index, uint8_t fm_count, uint8_t psg_count) {
    if (channel_index < 0) // DAC
        return fm_count > 0;
    if (channel_index >= SOUND_CHANNEL_FM_BASE && channel_index < SOUND_CHANNEL_FM_BASE + 6) {
        int real_fm_blocks = (fm_count > 0) ? fm_count - 1 : 0;
        return (channel_index - SOUND_CHANNEL_FM_BASE) < real_fm_blocks;
    }
    if (channel_index == SOUND_CHANNEL_PSG_BASE + 3) // dedicated NOISE slot
        return psg_count == 4;
    if (channel_index >= SOUND_CHANNEL_PSG_BASE && channel_index < SOUND_CHANNEL_PSG_BASE + 3)
        return (channel_index - SOUND_CHANNEL_PSG_BASE) < psg_count;
    return false;
}

// Small local 7-segment digit drawer for the song ID header -- primitives
// only, same reasoning as Render.c's countdown digits (no font asset).
static const uint8_t segment_digits[16] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
                                            0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};

static void DrawSegmentDigit(SDL_Renderer *r, int digit, int x, int y, int w, int h, int t) {
    if (digit < 0 || digit > 15)
        return;
    uint8_t segs = segment_digits[digit];
    int half = h / 2;
    SDL_Rect rc;
    if (segs & 0x01) { rc = (SDL_Rect){x + t, y, w - 2 * t, t}; SDL_RenderFillRect(r, &rc); }
    if (segs & 0x02) { rc = (SDL_Rect){x + w - t, y + t, t, half - t - t / 2}; SDL_RenderFillRect(r, &rc); }
    if (segs & 0x04) { rc = (SDL_Rect){x + w - t, y + half + t / 2, t, half - t - t / 2}; SDL_RenderFillRect(r, &rc); }
    if (segs & 0x08) { rc = (SDL_Rect){x + t, y + h - t, w - 2 * t, t}; SDL_RenderFillRect(r, &rc); }
    if (segs & 0x10) { rc = (SDL_Rect){x, y + half + t / 2, t, half - t - t / 2}; SDL_RenderFillRect(r, &rc); }
    if (segs & 0x20) { rc = (SDL_Rect){x, y + t, t, half - t - t / 2}; SDL_RenderFillRect(r, &rc); }
    if (segs & 0x40) { rc = (SDL_Rect){x + t, y + half - t / 2, w - 2 * t, t}; SDL_RenderFillRect(r, &rc); }
}

static void DrawHexByte(SDL_Renderer *r, uint8_t value, int x, int y, int w, int h, int t) {
    DrawSegmentDigit(r, (value >> 4) & 0xF, x, y, w, h, t);
    DrawSegmentDigit(r, value & 0xF, x + w + t, y, w, h, t);
}

// Small 5x7 dot-matrix font, primitive-drawn (filled squares per lit pixel,
// same "no textures" reasoning as the 7-segment digits above) -- only
// covers the letters this tool's row labels/header actually need.
// Row bits, MSB(bit4)=leftmost column, 7 rows top to bottom.
typedef struct { char c; uint8_t rows[7]; } FontGlyph;
static const FontGlyph font5x7[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'C', {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0F, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {'I', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
};
#define FONT5X7_COUNT (int)(sizeof(font5x7) / sizeof(font5x7[0]))

static void DrawChar5x7(SDL_Renderer *r, char c, int x, int y, int scale) {
    for (int i = 0; i < FONT5X7_COUNT; i++) {
        if (font5x7[i].c != c)
            continue;
        for (int row = 0; row < 7; row++) {
            uint8_t bits = font5x7[i].rows[row];
            for (int col = 0; col < 5; col++) {
                if (bits & (0x10 >> col)) {
                    SDL_Rect px = {x + col * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(r, &px);
                }
            }
        }
        return;
    }
}

static void DrawText5x7(SDL_Renderer *r, const char *text, int x, int y, int scale) {
    for (; *text; text++, x += 6 * scale)
        DrawChar5x7(r, *text, x, y, scale);
}

// Real piano key layout: 7 white keys (C,D,E,F,G,A,B) per octave drawn full
// height first, then 5 narrower/shorter black keys (C#,D#,F#,G#,A#) drawn on
// top straddling the boundary between the white keys on either side -- same
// visual pattern as an actual keyboard. lit_note is the currently-active
// note_index (0-95, 0=C) or -1 for none.
static void DrawPianoRow(SDL_Renderer *r, int strip_x, int y, int strip_w, int row_h, int lit_note) {
    static const int white_chromatic[7] = {0, 2, 4, 5, 7, 9, 11};   // C D E F G A B
    static const int black_chromatic[5] = {1, 3, 6, 8, 10};         // C# D# F# G# A#
    static const int black_before_white[5] = {0, 1, 3, 4, 5};       // which white key each black one follows
    const int white_count = NOTE_COUNT / 12 * 7;                    // 8 octaves * 7 = 56
    int white_w = strip_w / white_count;
    int black_w = white_w * 6 / 10, black_h = row_h * 6 / 10;

    for (int w = 0; w < white_count; w++) {
        int octave = w / 7, wi = w % 7;
        int note = octave * 12 + white_chromatic[wi];
        bool lit = (note == lit_note);
        SDL_SetRenderDrawColor(r, lit ? 220 : 0xEE, lit ? 60 : 0xEE, lit ? 60 : 0xEE, 255);
        SDL_Rect key = {strip_x + w * white_w + 1, y + 2, white_w - 1, row_h - 4};
        SDL_RenderFillRect(r, &key);
    }
    int black_count = NOTE_COUNT / 12 * 5;
    for (int b = 0; b < black_count; b++) {
        int octave = b / 5, bi = b % 5;
        int note = octave * 12 + black_chromatic[bi];
        bool lit = (note == lit_note);
        int boundary_white = octave * 7 + black_before_white[bi];
        int cx = strip_x + (boundary_white + 1) * white_w;
        SDL_SetRenderDrawColor(r, lit ? 220 : 0xAA, lit ? 60 : 0xAA, lit ? 60 : 0xAA, 255);
        SDL_Rect key = {cx - black_w / 2, y + 2, black_w, black_h};
        SDL_RenderFillRect(r, &key);
    }
}

int main(int argc, char **argv) {
    uint8_t song_id = (argc > 1) ? (uint8_t)strtol(argv[1], NULL, 16) : 0x81;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("SMPSInspector", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W,
                                           WIN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!window || !renderer) {
        fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return 1;
    }
    SDL_PauseAudioDevice(dev, 0);

    Sound_Init();
    PlayMusic(song_id);

    // Flash/sustain state: the audio catch-up loop below can run
    // Sound_Frame() several times per visual render (see audio_time_debt),
    // but rendering only happens once per loop iteration -- sampling
    // channel state only after the catch-up loop misses any note that
    // turned on AND off entirely within one bursty catch-up burst, and (the
    // "stuck key" symptom) can also show a stale note as if continuously
    // held when rapid short notes happened to always have *something*
    // playing at the exact instant of the final sample. Fix: every real
    // Sound_Frame() tick (not just the last one before a render) refreshes
    // a short timer for whatever's active right then; rendering reads the
    // timer, which decays to 0 (and shows key-up) once nothing's actually
    // retriggered it for a few real frames -- also directly implements
    // "track key-up too", since a note's off-transition simply stops
    // refreshing the timer instead of needing separate handling.
    #define FLASH_FRAMES 5
    int flash_timer[ROW_COUNT] = {0};
    int flash_note[ROW_COUNT] = {0};
    int dac_flash_timer[8] = {0};

    uint32_t samples_per_frame = SAMPLE_RATE / FRAME_HZ;
    int32_t *mix = calloc(2 * (size_t)samples_per_frame, sizeof(int32_t));
    int16_t *out = calloc(2 * (size_t)samples_per_frame, sizeof(int16_t));

    bool running = true;
    bool left_held_prev = false, right_held_prev = false;
    // Real-time accumulator: how many 60Hz audio frames are actually due
    // gets computed from wall-clock time each iteration, instead of
    // assuming exactly one frame elapsed per loop iteration. Rendering
    // (lots of individual SDL_RenderFillRect calls for the piano-key
    // strips) can easily take longer than 16.6ms on an unaccelerated
    // renderer -- without this, a slow render frame means the loop queues
    // audio slower than the device consumes it, draining the queue empty
    // and producing an audible buzz/glitch every time it falls behind.
    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 last_time = SDL_GetPerformanceCounter();
    double audio_time_debt = 0.0; // seconds of audio still owed
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                running = false;
        }

        const uint8_t *keys = SDL_GetKeyboardState(NULL);
        bool left_held = keys[SDL_SCANCODE_LEFT];
        bool right_held = keys[SDL_SCANCODE_RIGHT];
        if (keys[SDL_SCANCODE_ESCAPE])
            running = false;
        // Edge-detected step through every registered song ID, skipping gaps
        // (Sound_DebugGetSongData returns NULL for unregistered IDs).
        if (right_held && !right_held_prev) {
            uint8_t next = song_id;
            do
                next++;
            while (!Sound_DebugGetSongData(next) && next != song_id);
            if (Sound_DebugGetSongData(next)) {
                song_id = next;
                PlayMusic(song_id);
            }
        }
        if (left_held && !left_held_prev) {
            uint8_t next = song_id;
            do
                next--;
            while (!Sound_DebugGetSongData(next) && next != song_id);
            if (Sound_DebugGetSongData(next)) {
                song_id = next;
                PlayMusic(song_id);
            }
        }
        left_held_prev = left_held;
        right_held_prev = right_held;

        Uint64 now = SDL_GetPerformanceCounter();
        audio_time_debt += (double)(now - last_time) / (double)perf_freq;
        last_time = now;
        // Cap how far behind we let it get -- e.g. after a debugger pause or
        // window-drag stall, catching up hundreds of frames instantly would
        // itself sound like a burst of noise. Silently drop the excess
        // instead (same tradeoff a dropped-frame policy makes visually).
        if (audio_time_debt > 0.5)
            audio_time_debt = 0.5;

        while (audio_time_debt >= 1.0 / FRAME_HZ) {
            audio_time_debt -= 1.0 / FRAME_HZ;
            Sound_Frame();
            memset(mix, 0, 2 * (size_t)samples_per_frame * sizeof(int32_t));
            Sound_Generate(mix, samples_per_frame, SAMPLE_RATE);
            for (uint32_t i = 0; i < 2 * samples_per_frame; i++) {
                int32_t s = mix[i];
                if (s > 32767) s = 32767;
                else if (s < -32768) s = -32768;
                out[i] = (int16_t)s;
            }
            SDL_QueueAudio(dev, out, 2 * samples_per_frame * sizeof(int16_t));

            // Sample every real tick, not just whatever's left when it's
            // time to draw -- see the flash_timer comment above.
            for (int r = 0; r < ROW_COUNT; r++) {
                if (rows[r].channel_index < 0)
                    continue;
                SoundChannel *ch = &sound_music.channels[rows[r].channel_index];
                if (ch->key_on) {
                    flash_timer[r] = FLASH_FRAMES;
                    flash_note[r] = ch->note_index;
                }
            }
            bool dac_hit[8] = {
                sound_music.pcm_playing != 0,
                sound_music.dac_playing && sound_music.dac_sample_id == DAC_SAMPLE_KICK,
                sound_music.dac_playing && sound_music.dac_sample_id == DAC_SAMPLE_SNARE,
                sound_music.dac_playing && sound_music.dac_sample_id == DAC_SAMPLE_TIMPANI &&
                    sound_music.dac_timpani_variant < 0,
                sound_music.dac_playing && sound_music.dac_timpani_variant == 0,
                sound_music.dac_playing && sound_music.dac_timpani_variant == 1,
                sound_music.dac_playing && sound_music.dac_timpani_variant == 2,
                sound_music.dac_playing && sound_music.dac_timpani_variant == 3,
            };
            for (int i = 0; i < 8; i++)
                if (dac_hit[i])
                    dac_flash_timer[i] = FLASH_FRAMES;
        }

        // Decay all flash timers once per real render (not per audio tick
        // above) -- a fixed number of *visual* frames of sustain regardless
        // of how many audio ticks happened to land in this iteration.
        for (int r = 0; r < ROW_COUNT; r++)
            if (flash_timer[r] > 0)
                flash_timer[r]--;
        for (int i = 0; i < 8; i++)
            if (dac_flash_timer[i] > 0)
                dac_flash_timer[i]--;

        // Draw
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Song ID header, top-left -- "SND" label, then two hex digits, big.
        SDL_SetRenderDrawColor(renderer, 200, 200, 210, 255);
        DrawText5x7(renderer, "SND", 16, 12, 3);
        SDL_SetRenderDrawColor(renderer, 255, 220, 80, 255);
        DrawHexByte(renderer, song_id, 74, 8, 18, 28, 4);

        const uint8_t *song_data = Sound_DebugGetSongData(song_id);
        uint8_t fm_count = song_data ? song_data[2] : 0, psg_count = song_data ? song_data[3] : 0;
        int visible_rows[ROW_COUNT], visible_count = 0;
        for (int r = 0; r < ROW_COUNT; r++)
            if (RowVisible(rows[r].channel_index, fm_count, psg_count))
                visible_rows[visible_count++] = r;

        int row_top = 60, row_h = (WIN_H - row_top - 16) / (visible_count > 0 ? visible_count : 1);
        int strip_x = 90, strip_w = WIN_W - strip_x - 16;

        for (int vi = 0; vi < visible_count; vi++) {
            int r = visible_rows[vi];
            int y = row_top + vi * row_h;

            SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
            SDL_Rect label_bg = {8, y + 2, strip_x - 16, row_h - 4};
            SDL_RenderFillRect(renderer, &label_bg);
            SDL_SetRenderDrawColor(renderer, 230, 230, 235, 255);
            DrawText5x7(renderer, rows[r].label, 12, y + row_h / 2 - 7, 2);

            if (rows[r].channel_index < 0) {
                // DAC: exactly the 8 real distinct sounds the $81-$8B
                // command range defines on real hardware (Sega/Kick/Snare/
                // Timpani, then Hi/Mid/Low/Floor-Timpani -- $84-$86 are
                // invalid/noise on real hardware and never played, so
                // they're not shown at all; see the $81-$8B DAC dispatch
                // comment in Sound.c). Each lights individually for
                // whichever one is the current trigger.
                static const char *dac_labels[8] = {"SEGA", "KICK", "SNARE", "TIMP", "HI", "MID", "LOW", "FLOOR"};
                int dac_count = 8, dac_spacing = strip_w / dac_count;
                for (int i = 0; i < dac_count; i++) {
                    bool lit = dac_flash_timer[i] > 0;
                    SDL_Color col = lit ? (SDL_Color){220, 60, 60, 255} : (SDL_Color){0xEE, 0xEE, 0xEE, 255};
                    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
                    SDL_Rect sq = {strip_x + i * dac_spacing + 2, y + 4, dac_spacing - 4, row_h - 8};
                    SDL_RenderFillRect(renderer, &sq);
                    SDL_SetRenderDrawColor(renderer, 40, 40, 45, 255);
                    DrawText5x7(renderer, dac_labels[i], strip_x + i * dac_spacing + 5, y + row_h - 12, 1);
                }
                continue;
            }

            SoundChannel *ch = &sound_music.channels[rows[r].channel_index];
            int lit_note = flash_timer[r] > 0 ? flash_note[r] : -1;

            if (ch->psg_noise) {
                // Noise: same per-note tracking as the piano rows (note
                // value genuinely selects the character here -- real
                // noise-routed tracks use a wide range of different notes,
                // same idea as different notes selecting different pieces
                // of a drum kit, e.g. closed/open/muted hihat -- checked
                // against every noise-using track's own .asm source, not
                // one uniform on/off state), just rendered as plain squares
                // instead of piano-key shapes. Checking psg_noise itself
                // (not just "is this the dedicated PSG4 slot") also catches
                // PSG3 being redirected to drive the noise channel via the
                // $F3 coordination flag (smpsPSGform, "safe on PSG3" per
                // the real driver's own docs) -- LoadMusic sets
                // psg_noise=1 unconditionally for the dedicated slot too,
                // so this one check covers both cases without
                // special-casing by index.
                int square_count = NOTE_COUNT, square_w = strip_w / square_count;
                for (int i = 0; i < square_count; i++) {
                    bool lit = (i == lit_note);
                    SDL_Color col = lit ? (SDL_Color){220, 60, 60, 255} : (SDL_Color){0xEE, 0xEE, 0xEE, 255};
                    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
                    SDL_Rect sq = {strip_x + i * square_w + 1, y + 4, square_w - 2, row_h - 8};
                    SDL_RenderFillRect(renderer, &sq);
                }
                continue;
            }

            // FM/PSG tone channels: a real piano-key layout (white keys for
            // natural notes, narrower/shorter black keys straddling the
            // boundary between the white keys on either side, same relative
            // pattern as an actual keyboard -- no black key between E/F or
            // B/C) instead of 96 uniform bars, at the same overall strip
            // width/scale as before. note_index 0 = C, chromatic within the
            // octave via %12 (NOTE_COUNT=96 = a clean 8 octaves).
            DrawPianoRow(renderer, strip_x, y, strip_w, row_h, lit_note);
        }

        SDL_RenderPresent(renderer);
        // Small fixed delay just to avoid pegging a CPU core at 100% --
        // deliberately NOT 1000/FRAME_HZ (that was the original bug: a
        // fixed per-iteration delay on top of render time meant a slow
        // render frame directly stole time from audio pacing instead of
        // the two being independent).
        SDL_Delay(2);
    }

    free(mix);
    free(out);
    SDL_CloseAudioDevice(dev);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
