// SonicSoundTrace: loads a real song into sound_music and logs every
// note/duration event TickChannel decodes to stderr, so it can be diffed
// directly against the real .asm source instead of guessing from audio.
// Usage: SonicSoundTrace <sound_id_hex> <frames>
#include "../src/Sound.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    uint8_t id = (argc > 1) ? (uint8_t)strtol(argv[1], NULL, 16) : 0x81;
    int frames = (argc > 2) ? atoi(argv[2]) : 300;

    Sound_Init();
    Sound_SetTrace(1);
    PlayMusic(id);

    for (int i = 0; i < frames; i++)
        Sound_Frame();

    return 0;
}
