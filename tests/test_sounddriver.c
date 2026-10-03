#include "test.h"

#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "Sound.h"
#include "Backend/YM2612.h"
#include "Object.h"
#include "converter.h"

// SMPS timing: a note of duration d lasts exactly d frames (the original decrements DurationTimeout
// first and reads the next command on the frame it reaches 0).

// One PSG track: notes $A0,$A2,$A0,$A2 each with duration 4, then stop. No voices, tempo 0 (no
// TempoWait slowdown), dividing timing 1.
static const uint8_t song[] = {
    0x00, 0x00,             // voice bank offset (unused)
    0x00, 0x01, 0x01, 0x00, // 0 FM/DAC blocks, 1 PSG track, dividing timing 1, main tempo 0
    0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, // PSG1: data at $000C, pitch 0, volume 0, mod 0, envelope 0
    0xA0, 0x04, 0xA2, 0x04, 0xA0, 0x04, 0xA2, 0x04, 0xF2,
};

static void Sound_NoteDurationIsExact(void) {
    Sound_Init();
    memset(sound_music.queue, 0, sizeof(sound_music.queue)); // earlier tests ran game code that queued music
    memset(sound_sfx.queue, 0, sizeof(sound_sfx.queue));
    Sound_DebugPlayRawSong(song, 0, 1);
    uint16_t last = 0xFFFF;
    int change_frames[8], changes = 0;
    for (int f = 0; f < 40 && changes < 8; f++) {
        Sound_Frame();
        uint16_t p = sound_music.psg.tone_period[0];
        if (p != last) {
            change_frames[changes++] = f;
            last = p;
        }
    }
    CHECK(changes >= 4);
    for (int i = 1; i < 4 && i < changes; i++)
        CHECK_EQ(change_frames[i] - change_frames[i - 1], 4);
}

static void ResetSound(void) {
    Sound_Init();
    memset(sound_music.queue, 0, sizeof(sound_music.queue));
    memset(sound_sfx.queue, 0, sizeof(sound_sfx.queue));
    StopAllSound();
}

// Frames at which PSG1's period register changes, over `frames` frames.
static int PeriodChanges(int frames, int *at, int max) {
    uint16_t last = 0xFFFF;
    int n = 0;
    for (int f = 0; f < frames && n < max; f++) {
        Sound_Frame();
        uint16_t p = sound_music.psg.tone_period[0];
        if (p != last) {
            at[n++] = f;
            last = p;
        }
    }
    return n;
}

// $E7 (no attack) before a note: the note changes pitch on time -- no extra frames are inserted.
static const uint8_t song_e7[] = {
    0x00, 0x00, 0x00, 0x01, 0x01, 0x00,
    0x00, 0x0C, 0x00, 0x00, 0x00, 0x00,
    0xA0, 0x04, 0xE7, 0xA2, 0x04, 0xA0, 0x04, 0xF2,
};

static void Sound_NoAttackAddsNoTime(void) {
    ResetSound();
    Sound_DebugPlayRawSong(song_e7, 0, 1);
    int at[8];
    int n = PeriodChanges(20, at, 8);
    CHECK(n >= 3);
    if (n >= 3) {
        CHECK_EQ(at[1] - at[0], 4);
        CHECK_EQ(at[2] - at[1], 4);
    }
}

// The driver runs the DAC track first, then FM, then PSG: a DAC track's $EB (all-track tempo divider) reaches a PSG track
// that reads its next note on the same frame -- Credits' LZ section depends on it.
static const uint8_t song_tempo_div_order[] = {
    0x00, 0x00, 0x01, 0x01, 0x01, 0x00,       // voices, 1 FM-or-DAC track, 1 PSG, divider 1, tempo 0
    0x00, 0x10, 0x00, 0x00,                   // DAC track
    0x00, 0x15, 0x00, 0x00, 0x00, 0x00,       // PSG1
    0xEB, 0x02, 0x81, 0x04, 0xF2,             // DAC: all dividers to 2, then a note
    0xA0, 0x04, 0xA2, 0x04, 0xF2,             // PSG1: two 4-tick notes
};

static void Sound_DacTempoDivideReachesPsgOnTheSameFrame(void) {
    ResetSound();
    Sound_DebugPlayRawSong(song_tempo_div_order, 0, 1);
    int at[8];
    int n = PeriodChanges(30, at, 8);
    CHECK(n >= 2);
    if (n >= 2)
        CHECK_EQ(at[1] - at[0], 8);
}

// $E1 (smpsAlterNote) is a fine detune added to the period, not a transposition.
static const uint8_t song_e1[] = {
    0x00, 0x00, 0x00, 0x01, 0x01, 0x00,
    0x00, 0x0C, 0x00, 0x00, 0x00, 0x00,
    0xA0, 0x04, 0xE1, 0x02, 0xA0, 0x04, 0xF2,
};

static void Sound_AlterNoteIsFineDetune(void) {
    ResetSound();
    Sound_DebugPlayRawSong(song_e1, 0, 1);
    Sound_Frame();
    uint16_t plain = sound_music.psg.tone_period[0];
    for (int i = 0; i < 4; i++)
        Sound_Frame();
    uint16_t detuned = sound_music.psg.tone_period[0];
    CHECK_EQ(detuned, plain + 2);
}

// Whether sound effect `id` was the one most recently started on some effect channel.
static int SfxLoaded(uint8_t id) {
    const uint8_t *data = Sound_DebugGetSongData(id);
    for (int i = 0; i < SOUND_CHANNELS; i++)
        if (sound_sfx.channels[i].active && sound_sfx.channels[i].song_base == data)
            return 1;
    return 0;
}

// The ring sound alternates between the left- and right-speaker versions.
static void Sound_RingAlternatesSpeakers(void) {
    ResetSound();
    PlaySound(sfx_Ring);
    Sound_Frame();
    CHECK(SfxLoaded(sfx_RingLeft));
    PlaySound(sfx_Ring);
    Sound_Frame();
    CHECK(SfxLoaded(sfx_Ring));
    PlaySound(sfx_Ring);
    Sound_Frame();
    CHECK(SfxLoaded(sfx_RingLeft));
}

// The jump sound has priority $80, which is never stored: a ring right after a jump still plays.
static void Sound_JumpDoesNotBlockRings(void) {
    ResetSound();
    PlaySound(sfx_Jump);
    Sound_Frame();
    CHECK(SfxLoaded(sfx_Jump));
    PlaySound(sfx_Ring);
    Sound_Frame();
    CHECK(SfxLoaded(sfx_RingLeft) || SfxLoaded(sfx_Ring));
}

// The push block requests its sound every frame: it must keep playing, not restart each frame.
static void Sound_PushSoundDoesNotRestart(void) {
    ResetSound();
    QueueSound2(sfx_Push);
    Sound_Frame();
    CHECK(sound_sfx.push_playing != 0);
    // Where the push sound's first track is after a few frames of re-requesting it.
    const uint8_t *before = NULL;
    for (int f = 0; f < 6; f++) {
        QueueSound2(sfx_Push);
        Sound_Frame();
    }
    for (int i = 0; i < SOUND_CHANNELS; i++)
        if (sound_sfx.channels[i].active) {
            before = sound_sfx.channels[i].data_ptr;
            break;
        }
    CHECK(before != NULL);
    CHECK(before != NULL && before != Sound_DebugGetSongData(sfx_Push)); // it has advanced, not been reloaded
}

// Music fades out over $28 steps of 4 frames instead of stopping at once.
static void Sound_FadeOutIsGradual(void) {
    ResetSound();
    PlayMusic(bgm_GHZ);
    for (int f = 0; f < 60; f++)
        Sound_Frame();
    FadeOutMusic();
    for (int f = 0; f < 20; f++)
        Sound_Frame();
    int active = 0;
    for (int i = 0; i < SOUND_CHANNELS; i++)
        active += sound_music.channels[i].active;
    CHECK(active > 0);                 // still playing (quieter) 20 frames into the fade
    CHECK(sound_music.channels[SOUND_CHANNEL_DAC].active == 0); // the DAC stops at once
    for (int f = 0; f < 200; f++)
        Sound_Frame();
    active = 0;
    for (int i = 0; i < SOUND_CHANNELS; i++)
        active += sound_music.channels[i].active;
    CHECK_EQ(active, 0);               // all done after ~160 frames
}

// YM2612 register rows +0/+4/+8/+C are operators 1/3/2/4. Algorithm 4 is (op1->op2) + (op3->op4), so
// with only the operator in row +8 (operator 2, a carrier) audible, the channel must make sound.
static int64_t FmEnergyAlg4WithOnlyRow(int row) {
    YM2612 *chip = YM2612_Create();
    #define W(r, d) do { YM2612_Write(chip, 0, (r)); YM2612_Write(chip, 1, (d)); } while (0)
    W(0xB0, 0x04);           // algorithm 4, no feedback
    W(0xB4, 0xC0);           // both speakers
    for (int k = 0; k < 4; k++) {
        W(0x30 + k * 4, 0x01);              // MUL 1
        W(0x40 + k * 4, k == row ? 0x00 : 0x7F); // only `row` audible
        W(0x50 + k * 4, 0x1F);              // fastest attack
        W(0x60 + k * 4, 0x00);
        W(0x70 + k * 4, 0x00);
        W(0x80 + k * 4, 0x0F);
    }
    W(0xA4, 0x22);           // block 4
    W(0xA0, 0x69);
    W(0x28, 0xF0);           // key on, channel 1, all operators
    #undef W
    static int32_t out[2 * 2048];
    memset(out, 0, sizeof(out));
    YM2612_Generate(chip, out, 2048, 44100, 7670454);
    int64_t e = 0;
    for (int i = 0; i < 2 * 2048; i++)
        e += (int64_t)out[i] * out[i] / 1024;
    YM2612_Destroy(chip);
    return e;
}

static void FM_RegisterRowsAreOperators1324(void) {
    CHECK(FmEnergyAlg4WithOnlyRow(2) > 0);  // row +8 = operator 2: a carrier in algorithm 4
    CHECK(FmEnergyAlg4WithOnlyRow(3) > 0);  // row +C = operator 4: a carrier
    CHECK_EQ(FmEnergyAlg4WithOnlyRow(1), 0); // row +4 = operator 3: a modulator of the silent operator 4
    CHECK_EQ(FmEnergyAlg4WithOnlyRow(0), 0); // row +0 = operator 1: a modulator
}

// The assembler sees consecutive dc.b lines as one byte stream: a note ending one line takes the duration
// starting the next. The converter used to treat that duration as a separate repeated note, inserting an
// extra note and knocking the channel out of sync (GHZ FM1/PSG2 and 150 more spots across the songs).
static void Converter_DurationContinuesAcrossLines(void) {
    const char *asm_text =
        "Song_Header:\n"
        "\tsmpsHeaderStartSong 1\n"
        "\tsmpsHeaderVoice     Song_Voices\n"
        "\tsmpsHeaderChan      $02, $00\n"
        "\tsmpsHeaderTempo     $01, $03\n"
        "\tsmpsHeaderDAC       Song_DAC\n"
        "\tsmpsHeaderFM        Song_FM1, $00, $00\n"
        "Song_FM1:\n"
        "\tdc.b\tnE6, $38, nC6\n"
        "\tdc.b\t$08, nC6, nE6\n"
        "\tsmpsStop\n"
        "Song_DAC:\n"
        "\tsmpsStop\n"
        "Song_Voices:\n";
    PSConvertResult r = ps_convert_asm(asm_text);
    CHECK(r.success);
    if (!r.success)
        return;
    const PJValue *track = pj_object_get(pj_object_get(r.result, "SMPSplaylist"), "Song_FM1");
    // Expect exactly four notes: E6 $38, C6 $08, C6 (no duration), E6 (no duration).
    int notes = 0, c6_with_8 = 0;
    for (size_t i = 0; track && i < pj_array_size(track); i++) {
        const PJValue *ev = pj_array_get(track, i);
        const PJValue *note = pj_type(ev) == PJ_OBJECT ? pj_object_get(ev, "note") : NULL;
        if (!note)
            continue;
        notes++;
        const PJValue *dur = pj_object_get(ev, "duration");
        if (strcmp(pj_get_string(note, ""), "C6") == 0 && dur && strcmp(pj_get_string(dur, ""), "0x8") == 0)
            c6_with_8++;
    }
    CHECK_EQ(notes, 4);
    CHECK_EQ(c6_with_8, 1);
    ps_convert_result_free(&r);
}

// A new song starts from silence: FM channels it doesn't use are keyed off with all operators at TL $7F.
static void Sound_SongChangeSilencesPreviousNotes(void) {
    ResetSound();
    PlayMusic(bgm_GHZ);
    for (int f = 0; f < 120; f++)
        Sound_Frame();
    PlayMusic(bgm_Boss); // a song without an FM6 track
    Sound_Frame();
    for (int ch = 0; ch < 6; ch++) {
        if (sound_music.channels[SOUND_CHANNEL_FM_BASE + ch].active)
            continue;
        CHECK((YM2612_PeekKeyOn(sound_music.fm) & (1 << ch)) == 0);
    }
    // Every channel the new song doesn't use is silent (TL $7F on its carrier, operator 4 = row +C).
    for (int ch = 0; ch < 6; ch++) {
        if (sound_music.channels[SOUND_CHANNEL_FM_BASE + ch].active)
            continue;
        CHECK_EQ(YM2612_PeekReg(sound_music.fm, ch / 3, (uint8_t)(0x4C + ch % 3)), 0x7F);
    }
}

// The 1-up jingle interrupts the level song, then the song comes back and fades in.
static void Sound_ExtraLifeCrossfadesBack(void) {
    ResetSound();
    PlayMusic(bgm_GHZ);
    for (int f = 0; f < 200; f++)
        Sound_Frame();
    PlayMusic(bgm_ExtraLife);
    Sound_Frame();
    CHECK_EQ(sound_music.current_music_id, bgm_ExtraLife);
    int back = -1;
    for (int f = 0; f < 1200 && back < 0; f++) {
        Sound_Frame();
        if (sound_music.current_music_id == bgm_GHZ)
            back = f;
    }
    CHECK(back > 0);
    CHECK(sound_music.fadein_active);          // fading back in
    int active = 0;
    for (int i = 0; i < SOUND_CHANNELS; i++)
        active += sound_music.channels[i].active;
    CHECK(active >= 8);                         // GHZ's tracks are back
    for (int f = 0; f < 200; f++)
        Sound_Frame();
    CHECK(!sound_music.fadein_active);          // 40 steps x 3 frames
    CHECK(!sound_music.dac_fadein_muted);       // drums back
}

// Speed shoes stay on across a song change: the new song starts at its sped-up tempo.
static void Sound_SpeedShoesSurviveSongChange(void) {
    ResetSound();
    PlayMusic(bgm_GHZ);
    Sound_Frame();
    SpeedUpMusic();
    PlayMusic(bgm_LZ);
    Sound_Frame();
    CHECK_EQ(sound_music.main_tempo, 0x72); // LZ's SpeedUpIndex entry
    SlowDownMusic();
    CHECK(sound_music.main_tempo != 0x72);
}

// Pause freezes the whole driver (music and effects) and silences it.
static void Sound_PauseFreezesEverything(void) {
    ResetSound();
    PlayMusic(bgm_GHZ);
    PlaySound(sfx_Jump);
    for (int f = 0; f < 3; f++)
        Sound_Frame();
    const uint8_t *m = sound_music.channels[SOUND_CHANNEL_FM_BASE].data_ptr;
    Sound_Pause();
    for (int f = 0; f < 30; f++)
        Sound_Frame();
    CHECK(sound_music.channels[SOUND_CHANNEL_FM_BASE].data_ptr == m);
    CHECK_EQ(YM2612_PeekKeyOn(sound_music.fm), 0);
    CHECK_EQ(YM2612_PeekKeyOn(sound_sfx.fm), 0);
    Sound_Resume();
    for (int f = 0; f < 30; f++)
        Sound_Frame();
    CHECK(sound_music.channels[SOUND_CHANNEL_FM_BASE].data_ptr != m);
}

// Stop keys every FM channel off.
static void Sound_StopSilencesEverything(void) {
    ResetSound();
    PlayMusic(bgm_GHZ);
    for (int f = 0; f < 60; f++)
        Sound_Frame();
    StopAllSound();
    CHECK_EQ(YM2612_PeekKeyOn(sound_music.fm), 0);
}

// Collecting a ring plays the ring sound; the 100th ring plays the extra-life jingle instead.
void Obj_Ring(Object *obj);
static void Ring_CollectPlaysSound(void) {
    ResetSound();
    // A ring that Sonic is touching: routine 4 is the "collected" step (see Ring.c).
    extern uint16_t rings; // Level.h
    extern uint8_t life_num, lives;
    rings = 0; life_num = 0; lives = 3;
    Object ring;
    memset(&ring, 0, sizeof(ring));
    ring.type = ObjId_Ring;
    ring.routine = 4;
    Obj_Ring(&ring);
    Sound_Frame();
    int heard = 0;
    for (int i = 0; i < SOUND_CHANNELS; i++)
        heard += sound_sfx.channels[i].active;
    CHECK(heard > 0);          // the ring chime is playing
    CHECK_EQ(rings, 1);

    ResetSound();
    rings = 99; life_num = 0;
    memset(&ring, 0, sizeof(ring));
    ring.type = ObjId_Ring;
    ring.routine = 4;
    Obj_Ring(&ring);
    Sound_Frame();
    CHECK_EQ(lives, 4);
    CHECK_EQ(sound_music.current_music_id, bgm_ExtraLife);
}

// Every song must keep ticking: a track that falls into dead data (e.g. a self-jump) used to spin
// the flag reader forever and freeze the whole game.
// Burning ($C8): smpsPSGform and the first note arrive in the same update. The note's volume must go to the noise
// channel (not left on tone 3 as a long beep).
static void Sound_NoiseFormThenNoteDrivesTheNoiseChannel(void) {
    ResetSound();
    PlaySound(sfx_Burning);
    for (int f = 0; f < 3; f++)
        Sound_Frame();
    CHECK(sound_sfx.psg.noise_atten < 15);
    CHECK_EQ(sound_sfx.psg.tone_atten[2], 15);
}

// Sonic 2's spin dash rev: each rev within 60 frames climbs a semitone (up to 11); after a pause it starts over.
static int RevTranspose(void) {
    PlaySound(sfx_SpindashRev);
    Sound_Frame();
    return sound_sfx.channels[SOUND_CHANNEL_FM_BASE + 4].transpose; // FM5
}

static void Sound_SpindashRevPitchClimbs(void) {
    ResetSound();
    int base = RevTranspose();
    CHECK_EQ(RevTranspose(), base + 1);
    CHECK_EQ(RevTranspose(), base + 2);
    for (int i = 0; i < 70; i++)
        Sound_Frame();
    CHECK_EQ(RevTranspose(), base);
}

static void Sound_EverySongTicks(void) {
    for (int id = bgm_GHZ; id <= bgm_SSRG; id++) {
        ResetSound();
        QueueSound1((uint8_t)id);
        alarm(5); // SIGALRM kills the process if a frame never returns
        for (int f = 0; f < 6000; f++)
            Sound_Frame();
        alarm(0);
        CHECK(1);
    }
}

// An effect that is sounding through the PSG noise generator keeps its noise mode when another effect starts (the chip-wide noise
// control used to be reset by every effect that loaded: the burning platforms of Marble Zone turned into a harsh buzz).
static void Sound_OverlappingEffectKeepsNoiseMode(void) {
    ResetSound();
    PlaySound(sfx_Burning);
    for (int f = 0; f < 6; f++)
        Sound_Frame();
    int fb = sound_sfx.psg.noise_fb_white, rate = sound_sfx.psg.noise_shift_rate;
    int noise_track = 0;
    for (int i = 0; i < SOUND_CHANNELS; i++)
        if (sound_sfx.channels[i].active && sound_sfx.channels[i].psg_noise)
            noise_track = 1;
    printf("\n    [diag] burning: noise mode fb=%d rate=%d, a noise track is active: %d", fb, rate, noise_track);
    PlaySound(sfx_Ring); // starts while the burning noise is still going
    for (int f = 0; f < 3; f++)
        Sound_Frame();
    printf("\n    [diag] after the ring: fb=%d rate=%d", sound_sfx.psg.noise_fb_white, sound_sfx.psg.noise_shift_rate);
    CHECK_EQ(sound_sfx.psg.noise_fb_white, fb);
    CHECK_EQ(sound_sfx.psg.noise_shift_rate, rate);
}
void RegisterSoundDriverTests(void) {
    RUN_TEST(Sound_OverlappingEffectKeepsNoiseMode);
    RUN_TEST(Ring_CollectPlaysSound);
    RUN_TEST(Sound_SongChangeSilencesPreviousNotes);
    RUN_TEST(Sound_ExtraLifeCrossfadesBack);
    RUN_TEST(Sound_SpeedShoesSurviveSongChange);
    RUN_TEST(Sound_PauseFreezesEverything);
    RUN_TEST(Sound_StopSilencesEverything);
    RUN_TEST(Converter_DurationContinuesAcrossLines);
    RUN_TEST(FM_RegisterRowsAreOperators1324);
    RUN_TEST(Sound_NoteDurationIsExact);
    RUN_TEST(Sound_NoAttackAddsNoTime);
    RUN_TEST(Sound_DacTempoDivideReachesPsgOnTheSameFrame);
    RUN_TEST(Sound_AlterNoteIsFineDetune);
    RUN_TEST(Sound_RingAlternatesSpeakers);
    RUN_TEST(Sound_JumpDoesNotBlockRings);
    RUN_TEST(Sound_PushSoundDoesNotRestart);
    RUN_TEST(Sound_FadeOutIsGradual);
    RUN_TEST(Sound_EverySongTicks);
    RUN_TEST(Sound_SpindashRevPitchClimbs);
    RUN_TEST(Sound_NoiseFormThenNoteDrivesTheNoiseChannel);
}
