// Hill Top's background, the prototype's: its scroll (Bg_Scroll_HTz, loc_6108) and the animated tiles that its parallax mountains are made of (the first part of its animated art routine, loc_2244E).
// The mountains are tiles that change as the camera moves: a set of tiles chosen by the camera's place is copied to VRAM $A000 whenever it changes, and a strip of eight tiles at $A300 is made every
// frame from a picture of the mountains, each of its 16 rows shifted by its own layer's scroll, so that the layers move by pixels and not by whole tiles.
#include "HTZBackground.h"
#include "Constants.h"

#include "Level.h"
#include "Camera.h"
#include "LevelScroll.h"
#include "Video.h"

#include <string.h>

#include "Backend/VDP.h"
#include "Nemesis.h"

#include "Resource/Art/HTZBackground.h"
#include "Resource/S2Art/HTZBgStrips.h"

int16_t htz_layerdef[2][0x12]; // (a set for each view of a split screen) TempArray_LayerDef: the 16 layers' scroll (words 0-15) and the drift of the mountains (word $11, the byte offset $22)

// loc_22D94: where in RAM the prototype puts each of the 48 chunks of the background's art (4 tiles each), by its place in the art; loc_224C4's words (the sets of chunks to show) name them by those places
static const uint16_t chunk_places[48] = {
    0x0080, 0x0280, 0x0380, 0x0580, 0x0600, 0x0880, 0x0980, 0x0A80, 0x0B80, 0x0C80, 0x0E80, 0x0F00,
    0x1080, 0x1180, 0x1200, 0x1280, 0x1300, 0x1380, 0x1400, 0x1480, 0x1500, 0x1600, 0x1900, 0x1D00,
    0x1D80, 0x1E00, 0x2280, 0x2400, 0x2580, 0x2600, 0x2680, 0x2780, 0x2B00, 0x3280, 0x3600, 0x3680,
    0x3C80, 0x3D00, 0x3F00, 0x3F80, 0x4080, 0x4480, 0x4580, 0x4880, 0x4900, 0x4B80, 0x4C80, 0x4D80,
};

// loc_224C4: the six chunks for each camera step, as windows of six words into this list
static const uint16_t chunk_sets[96] = {
    0x0080, 0x0280, 0x0380, 0x0580, 0x0600, 0x0880, 0x0080, 0x0280, 0x0380, 0x0580, 0x0600, 0x0880,
    0x0980, 0x0A80, 0x0B80, 0x0C80, 0x0E80, 0x0F00, 0x0980, 0x0A80, 0x0B80, 0x0C80, 0x0E80, 0x0F00,
    0x1080, 0x1180, 0x1200, 0x1280, 0x1300, 0x1380, 0x1080, 0x1180, 0x1200, 0x1280, 0x1300, 0x1380,
    0x1400, 0x1480, 0x1500, 0x1600, 0x1900, 0x1D00, 0x1400, 0x1480, 0x1500, 0x1600, 0x1900, 0x1D00,
    0x1D80, 0x1E00, 0x2280, 0x2400, 0x2580, 0x2600, 0x1D80, 0x1E00, 0x2280, 0x2400, 0x2580, 0x2600,
    0x2680, 0x2780, 0x2B00, 0x3280, 0x3600, 0x3680, 0x2680, 0x2780, 0x2B00, 0x3280, 0x3600, 0x3680,
    0x3C80, 0x3D00, 0x3F00, 0x3F80, 0x4080, 0x4480, 0x3C80, 0x3D00, 0x3F00, 0x3F80, 0x4080, 0x4480,
    0x4580, 0x4880, 0x4900, 0x4B80, 0x4C80, 0x4D80, 0x4580, 0x4880, 0x4900, 0x4B80, 0x4C80, 0x4D80,
};

static uint8_t chunk_art[48 * 0x80]; // the art, decompressed (the prototype's $FFFFB800 buffer, spread to the places above)
static bool chunk_art_loaded;
static uint8_t last_step[2];         // the camera step the chunks were last chosen for (the byte after the first animation counter)

void HTZBackground_Reset(void) {
    memset(htz_layerdef, 0, sizeof(htz_layerdef));
    last_step[0] = last_step[1] = 0xFF; // (no step: the first frame puts the chunks in. The prototype starts with step 0 and so shows what the title left at $A200 -- its TM -- until the camera's step changes)
    chunk_art_loaded = false;
}

// For the tests: the step the chunks were last chosen for (0xFF: none yet)
int HTZBackground_LastStep(void) {
    return last_step[0];
}

// loc_22D62: the background's art, decompressed once for the level
static void LoadChunkArt(void) {
    if (chunk_art_loaded)
        return;
    memset(chunk_art, 0, sizeof(chunk_art));
    NemDecToRAM(Art_HTZBackground, chunk_art);
    chunk_art_loaded = true;
}

// Which of the 48 chunks the prototype's offset `place` names
static int ChunkOf(uint16_t place) {
    for (int i = 0; i < 48; i++)
        if (chunk_places[i] == place)
            return i;
    return 0;
}

// The camera step (0-$2F) that decides which chunks show: the camera's x in sixteenths, less its eighth the other way, less $10, modulo $30 (loc_2244E)
int HTZBackground_Step(int16_t camera_x) {
    uint16_t d0 = (uint16_t)((uint16_t)camera_x >> 4);
    d0 = (uint16_t)(d0 + (uint16_t)(int16_t)(-camera_x >> 3));
    d0 = (uint16_t)(d0 - 0x10);
    return d0 % 0x30;
}

// The six chunks of a step: the window of six words that its low three bits and the next three give (the prototype's byte offset (step & 7) * 24 + ((step >> 3) & 7) * 2)
void HTZBackground_StepChunks(int step, int chunks[6]) {
    const int start = (step & 7) * 12 + ((step >> 3) & 7);
    for (int k = 0; k < 6; k++)
        chunks[k] = ChunkOf(chunk_sets[start + k]);
}

// loc_22584: the strip of tiles made from the picture of the mountains: 16 rows of 4 pixel lines of 8 pixels, row i shifted by layer i's scroll (and the camera's eighth) to the pixel (the picture has a second
// copy shifted by one pixel, $200 bytes on, for the odd ones). out is the 256 bytes that go to VRAM: line k of row i is at k * $40 + i * 4.
void HTZBackground_BuildStrips(int16_t camera_x, const int16_t *layers, uint8_t out[0x100]) {
    const int16_t d2 = (int16_t)(-camera_x >> 3);
    for (int i = 0; i < 16; i++) {
        int d0 = (int16_t)(-layers[i] + d2) & 0x1F;
        const int odd = d0 & 1;
        d0 >>= 1;
        if (odd)
            d0 += 0x200;
        const uint8_t *src = S2Art_HTZBgStrips + i * 0x20 + d0;
        for (int k = 0; k < 4; k++)
            memcpy(out + k * 0x40 + i * 4, src + k * 4, 4);
    }
}

// One view's mountains: the chunks when its camera's step has changed, the strip every frame; `vram` is where its set of 32 tiles starts (the first view's is where the prototype has it, $A000, and the second view's
// is $800 on, its background plane's tiles moved to it as they are drawn: SplitScreen.c)
static void AnimateView(int view, int16_t camera_x, uint32_t vram) {
    const int step = HTZBackground_Step(camera_x);
    if (step != last_step[view]) {
        last_step[view] = (uint8_t)step;
        int chunks[6];
        HTZBackground_StepChunks(step, chunks);
        for (int k = 0; k < 6; k++) {
            VDP_SeekVRAM(vram + k * 0x80);
            VDP_WriteVRAM(chunk_art + chunks[k] * 0x80, 0x80);
        }
    }
    uint8_t strips[0x100];
    HTZBackground_BuildStrips(camera_x, htz_layerdef[view], strips);
    VDP_SeekVRAM(vram + 0x300);
    VDP_WriteVRAM(strips, 0x100);
}

// The animated art routine's own part (loc_2244E, before the flowers). The prototype makes the mountains for one camera only (its two player scroll has no mountains); a split screen here gives each view
// its own set.
void HTZBackground_Animate(void) {
    LoadChunkArt();
    AnimateView(0, (int16_t)scrpos_x.f.u, 0xA000);
    if (camera_split)
        AnimateView(1, (int16_t)scrpos_x_p2.f.u, 0xA000 + HTZ_P2_TILES * 0x20);
}

// Bg_Scroll_HTz, the usual branch (loc_6108): the first $80 lines at an eighth of the camera, then bands that run on to a half of it, whose 16 layers' scroll (htz_layerdef) the strip above uses. The drift
// (word $11) adds 4 every frame.
void HTZBackground_Deform(int view) {
    int16_t *layers = htz_layerdef[view];
    const int16_t negx = (int16_t)-scrpos_x.f.u;
    int16_t *bufp = &hscroll_buffer[0][0];
    int left = SCREEN_HEIGHT;
    #define LINES(n, bg) do { for (int n_ = (n); n_ > 0 && left > 0; n_--, left--) { *bufp++ = negx; *bufp++ = (bg); } } while (0)
    int16_t bg = (int16_t)(negx >> 3);
    LINES(0x80, bg);

    const int16_t drift = layers[0x11];
    layers[0x11] = (int16_t)(drift + 4);
    const int16_t d2 = (int16_t)(negx - drift);
    const int16_t d1 = (int16_t)(d2 >> 4);
    const int16_t d0w = (int16_t)((int16_t)(d2 >> 1) - d1);
    int32_t step = (int32_t)(int16_t)(((int32_t)d0w * 256) / 0x70); // (divs.w: the quotient as a word)
    step <<= 8;

    uint32_t acc = (uint32_t)(uint16_t)d1 << 16;
    #define VALUE(n) ((int16_t)((acc + (uint32_t)(n) * (uint32_t)step) >> 16))
    // The 16 layers: 1, 2 and 3 (twice) steps on, then 5, 8, 11 and 14 (three layers each)
    layers[0] = VALUE(1);
    layers[1] = VALUE(2);
    layers[2] = layers[3] = VALUE(3);
    for (int g = 0; g < 4; g++)
        layers[4 + g * 3] = layers[5 + g * 3] = layers[6 + g * 3] = VALUE(5 + g * 3);

    // The rest of the lines, in bands of growing height, each four times the step further than the last: 17, 21, 25, then 33, 41 (two steps of four), 53, 65, 81 and 97
    const uint32_t step4 = (uint32_t)step * 4;
    uint32_t a = acc + 17u * (uint32_t)step;
    LINES(3, (int16_t)(a >> 16));
    a += step4;
    LINES(5, (int16_t)(a >> 16));
    a += step4;
    LINES(7, (int16_t)(a >> 16));
    a += step4 * 2;
    LINES(8, (int16_t)(a >> 16));
    a += step4 * 2;
    LINES(10, (int16_t)(a >> 16));
    a += step4 * 3;
    LINES(15, (int16_t)(a >> 16));
    a += step4 * 3;
    for (int g = 0; g < 3; g++) {
        LINES(16, (int16_t)(a >> 16));
        a += step4 * 4;
    }
    // (a picture taller than the original's lines goes on with the last band)
    LINES(left, (int16_t)((a - step4 * 4) >> 16));
    #undef VALUE
    #undef LINES
}
