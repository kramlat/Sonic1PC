#include "Sound.h"

SoundChipSet sound_music;
SoundChipSet sound_sfx;

void Sound_Init(void) {
    SN76489_Init(&sound_music.psg);
    SN76489_Init(&sound_sfx.psg);
}

void Sound_Generate(int32_t *out, uint32_t count, uint32_t sample_rate) {
    SN76489_Generate(&sound_music.psg, out, count, sample_rate, SOUND_PSG_CLOCK);
    SN76489_Generate(&sound_sfx.psg, out, count, sample_rate, SOUND_PSG_CLOCK);
}
