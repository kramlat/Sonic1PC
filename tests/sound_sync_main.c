// SoundSync: plays each song through the driver and reports, per channel, the frames at which it takes its
// loop jump ($F6). Sonic 1's songs are written so every channel loops with the same length; a channel whose
// loop length differs from the others is out of sync.
//
// Usage: SoundSync [song id hex] [frames]   (no id: all songs $01-$13)

#include "Sound.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_JUMPS 256
static uint32_t jumps[SOUND_CHANNELS][MAX_JUMPS], targets[SOUND_CHANNELS][MAX_JUMPS]; // targets[] holds the jump's own address
static int njumps[SOUND_CHANNELS];

static void Hook(const SoundChipSet *cs, int ch, uint32_t frame, uint32_t target) {
    if (cs != &sound_music || njumps[ch] >= MAX_JUMPS)
        return;
    jumps[ch][njumps[ch]] = frame;
    targets[ch][njumps[ch]++] = target;
}

// Loop length: for each jump instruction (by its address in the song), the frames between its last two
// executions; the song's main loop jump runs once per loop, inner jumps more often, so the longest of these
// periods is the loop length. 0 if no jump repeated.
static int LoopLength(int ch) {
    int best = 0;
    for (int i = 0; i < njumps[ch]; i++) {
        int last = -1, period = 0;
        for (int j = 0; j < njumps[ch]; j++)
            if (targets[ch][j] == targets[ch][i]) {
                if (last >= 0)
                    period = (int)(jumps[ch][j] - jumps[ch][last]);
                last = j;
            }
        if (period > best)
            best = period;
    }
    return best;
}

static const char *Name(int ch) {
    static char b[8];
    if (ch == SOUND_CHANNEL_DAC) return "DAC";
    if (ch >= SOUND_CHANNEL_FM_BASE && ch < SOUND_CHANNEL_FM_BASE + 6) { snprintf(b, sizeof b, "FM%d", ch - SOUND_CHANNEL_FM_BASE + 1); return b; }
    snprintf(b, sizeof b, "PSG%d", ch - SOUND_CHANNEL_PSG_BASE + 1);
    return b;
}

static int Run(int id, int frames) {
    memset(njumps, 0, sizeof(njumps));
    Sound_Init();
    memset(sound_music.queue, 0, sizeof(sound_music.queue));
    memset(sound_sfx.queue, 0, sizeof(sound_sfx.queue));
    StopAllSound();
    if (getenv("SOUNDSYNC_RAW")) {
        static uint8_t raw[0x10000];
        FILE *rf = fopen(getenv("SOUNDSYNC_RAW"), "rb");
        if (rf) { fread(raw, 1, sizeof raw, rf); fclose(rf); }
        Sound_DebugPlayRawSong(raw, 0, 1);
        sound_music.tempo_timeout++; // the raw loader runs before the tick, the queued one after it
    } else
    if (getenv("SOUNDSYNC_JSON"))
        Sound_PlayFromJSON((uint8_t)id);
    else
        PlayMusic((uint8_t)id);
    for (int f = 0; f < frames; f++) {
        if (getenv("SOUNDSYNC_TICKS"))
            sound_music.main_tempo = 0; // no TempoWait: frames == ticks, for comparing with the original's data
        Sound_Frame();
    }

    // Loop length per channel = distance between its 1st and 2nd jump.
    int ref = -1, bad = 0;
    char line[1024];
    int n = snprintf(line, sizeof line, "song $%02X:", id);
    for (int ch = 0; ch < SOUND_CHANNELS; ch++) {
        int len = LoopLength(ch);
        if (len == 0)
            continue;
        n += snprintf(line + n, sizeof line - n, " %s=%d", Name(ch), len);
        if (ref < 0) ref = len;
        else if (len != ref) bad = 1;
    }
    printf("%s%s\n", bad ? "MISMATCH " : "ok       ", line);
    return bad;
}

int main(int argc, char **argv) {
    sound_jump_hook = Hook;
    int frames = argc > 2 ? atoi(argv[2]) : 60 * 60 * 6;
    if (argc > 1)
        return Run((int)strtol(argv[1], NULL, 16), frames);
    int bad = 0;
    for (int id = 0x01; id <= 0x13; id++)
        bad += Run(id, frames);
    return bad ? 1 : 0;
}
