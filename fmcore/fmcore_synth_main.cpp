// "Custom synth piano": evolved from fmcore_gui_main.cpp's single-operator
// test rig into a 4-operator FM voice (fm_voice.h) with a free-form,
// Flowstone-style patch-cable interface for wiring operators together --
// click an operator's output pin, then click any input pin (another
// operator's, or OUT) to connect/disconnect them. Not limited to the real
// YM2612's 8 fixed algorithms; the point is to freely experiment with
// routing by ear before the next phase hardcodes the real 8 as presets on
// top of this same engine (see fm_voice.h's own comment).
//
// A 3-octave on-screen piano spans the window; physical home-row keys (same
// layout the old single-operator jig used) only drive the middle octave --
// the rest is mouse-only. Notes drive all 4 operators of one FMVoice at
// once (monophonic).

#include "fm_voice.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SOUND_FM_CLOCK  7670454u
#define OP_UPDATE_RATE  (SOUND_FM_CLOCK / 144)
#define OUT_SAMPLE_RATE 44100

static const uint16_t FNUM_TABLE[12] = {0x25E, 0x284, 0x2AB, 0x2D3, 0x2FE, 0x32D,
                                         0x35C, 0x38F, 0x3C5, 0x3FF, 0x43C, 0x47C};

// A full multi-octave piano instead of the single ~1.25-octave strip this
// used to be -- spans 3 octaves (blocks 3-5) so it actually fills a wide
// window. Physical keyboard keys stay bound only to the middle octave
// (block 4, the same home-row layout as before, plus K/L reaching one note
// into block 5) -- the octaves on either side are mouse-only on the
// on-screen keyboard, same as clicking always was. One entry per semitone
// per octave (12 per octave, black-vs-white is derived from the semitone,
// not stored separately) addressed as `octave*12 + semitone`.
#define KEYBOARD_OCTAVES    3
#define KEYBOARD_BASE_BLOCK 3 // leftmost octave shown; middle octave (index 1) = block 4, matching the old physical-key range
#define ALL_KEY_TOTAL       (12 * KEYBOARD_OCTAVES)

static const int WHITE_SEMITONES[7] = {0, 2, 4, 5, 7, 9, 11};
static const int BLACK_SEMITONES[5] = {1, 3, 6, 8, 10};
// Index by semitone (0-11): which white-key slot (0-6) within the octave a
// white note occupies, or -1 for a black note.
static const int SEMITONE_TO_WHITE_SLOT[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
// Index by semitone (0-11): which white-key slot a black note sits just
// after, or -1 for a white note.
static const int BLACK_AFTER_WHITE_SLOT[12] = {-1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1};

static SDL_Scancode BoundScancode(int semitone, int block) {
    static const struct {
        int semitone, block;
        SDL_Scancode sc;
    } BINDINGS[] = {
        {0, 4, SDL_SCANCODE_A}, {1, 4, SDL_SCANCODE_W}, {2, 4, SDL_SCANCODE_S},  {3, 4, SDL_SCANCODE_E},
        {4, 4, SDL_SCANCODE_D}, {5, 4, SDL_SCANCODE_F}, {6, 4, SDL_SCANCODE_T},  {7, 4, SDL_SCANCODE_G},
        {8, 4, SDL_SCANCODE_Y}, {9, 4, SDL_SCANCODE_H}, {10, 4, SDL_SCANCODE_U}, {11, 4, SDL_SCANCODE_J},
        {0, 5, SDL_SCANCODE_K}, {2, 5, SDL_SCANCODE_L},
    };
    for (const auto &b : BINDINGS)
        if (b.semitone == semitone && b.block == block)
            return b.sc;
    return SDL_SCANCODE_UNKNOWN;
}

struct OpParams {
    int dt = 0, mul = 1, rs = 0, ar = 31, am = 0, d1r = 12, d2r = 3, d1l = 8, rr = 6, tl = 0, fb = 0;
};

static void ApplyParams(FMVoice *voice, int i, const OpParams &p) {
    FMOperator_SetParams(&voice->ops[i], (uint8_t)p.dt, (uint8_t)p.mul, (uint8_t)p.rs, (uint8_t)p.ar, (uint8_t)p.am,
                          (uint8_t)p.d1r, (uint8_t)p.d2r, (uint8_t)p.d1l, (uint8_t)p.rr, (uint8_t)p.tl,
                          (uint8_t)p.fb);
}

// Fixed node-canvas layout: 4 operator boxes down the left side, one OUT
// sink box on the right. Not draggable (unlike a real Flowstone canvas) --
// only 5 nodes total ever exist here, so a fixed layout keeps the
// click-to-connect interaction simple without needing node-position drag
// state on top of connection-drag state.
static const ImVec2 OP_BOX_POS[FM_VOICE_OP_COUNT] = {
    ImVec2(20, 20), ImVec2(20, 110), ImVec2(20, 200), ImVec2(20, 290)};
static const ImVec2 OUT_BOX_POS = ImVec2(420, 155);
static const ImVec2 BOX_SIZE = ImVec2(160, 70);

static ImVec2 OutputPinPos(ImVec2 origin, ImVec2 box_pos) {
    return ImVec2(origin.x + box_pos.x + BOX_SIZE.x, origin.y + box_pos.y + BOX_SIZE.y / 2);
}
static ImVec2 InputPinPos(ImVec2 origin, ImVec2 box_pos) {
    return ImVec2(origin.x + box_pos.x, origin.y + box_pos.y + BOX_SIZE.y / 2);
}
static bool PinHit(ImVec2 pin, ImVec2 mouse, float radius = 9.0f) {
    float dx = pin.x - mouse.x, dy = pin.y - mouse.y;
    return dx * dx + dy * dy <= radius * radius;
}

int main(int, char **) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    // Sized against the user's own 1920x1200 monitor -- comfortably large
    // without covering the whole screen, still freely resizable either way.
    SDL_Window *window = SDL_CreateWindow("fmcore custom synth piano", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           1700, 1050, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = OUT_SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return 1;
    }
    SDL_PauseAudioDevice(dev, 0);

    FMVoice voice;
    FMVoice_Init(&voice);
    // This jig wants AM's tremolo audible for testing regardless of what any
    // real SMPS data would set -- max AMS depth plus a local free-running
    // LFO phase (advanced in the audio loop below and fed into
    // FMVoice_Clock each tick), standing in for the real chip-wide LFO
    // register $22 that a real backend (YM2612_FMCore.c) owns instead.
    FMVoice_SetAMS(&voice, 3);
    float lfo_phase = 0.0f;
    const float kLfoRateHz = 5.5f; // typical tremolo rate, tunable by ear
    OpParams op_params[FM_VOICE_OP_COUNT];
    for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
        ApplyParams(&voice, i, op_params[i]);
    // Default routing: a simple 2-operator chain (op1 modulates op2, op2 is
    // the only carrier) -- an audible starting point rather than silence
    // (a voice with no connect[][FM_VOICE_OUT] at all produces no sound).
    voice.connect[0][1] = true;
    voice.connect[1][FM_VOICE_OUT] = true;

    int selected_op = 1; // op2 is the carrier in the default patch, most interesting to look at first
    int dragging_from = -1; // -1 = not dragging a cable, else source operator index (0-3)

    bool key_held_prev[ALL_KEY_TOTAL] = {};
    int current_key = -1;

    uint32_t tick_accum = 0;
    int16_t last_sample = 0;
    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT)
                running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                dragging_from = -1; // cancel an in-progress cable drag
        }

        const uint8_t *keys = SDL_GetKeyboardState(NULL);
        bool held_this_frame[ALL_KEY_TOTAL];
        for (int oct = 0; oct < KEYBOARD_OCTAVES; oct++)
            for (int st = 0; st < 12; st++) {
                SDL_Scancode sc = BoundScancode(st, KEYBOARD_BASE_BLOCK + oct);
                held_this_frame[oct * 12 + st] = (sc != SDL_SCANCODE_UNKNOWN) && keys[sc] != 0;
            }

        static Uint64 last_time = SDL_GetPerformanceCounter();
        static double audio_time_debt = 0.0;
        Uint64 now = SDL_GetPerformanceCounter();
        audio_time_debt += (double)(now - last_time) / (double)SDL_GetPerformanceFrequency();
        last_time = now;
        if (audio_time_debt > 0.25)
            audio_time_debt = 0.25;

        int16_t out_buf[2048];
        int out_n = 0;
        while (audio_time_debt >= 1.0 / OUT_SAMPLE_RATE && out_n < 2048) {
            audio_time_debt -= 1.0 / OUT_SAMPLE_RATE;
            int64_t sum = 0;
            uint32_t taken = 0;
            tick_accum += OP_UPDATE_RATE;
            while (tick_accum >= OUT_SAMPLE_RATE) {
                tick_accum -= OUT_SAMPLE_RATE;
                lfo_phase += kLfoRateHz / (float)OP_UPDATE_RATE;
                if (lfo_phase >= 1.0f)
                    lfo_phase -= 1.0f;
                float lfo_unipolar = lfo_phase < 0.5f ? (2.0f * lfo_phase) : (2.0f * (1.0f - lfo_phase));
                int16_t s = FMVoice_Clock(&voice, lfo_unipolar);
                sum += s;
                taken++;
                last_sample = s;
            }
            out_buf[out_n++] = taken > 0 ? (int16_t)(sum / (int64_t)taken) : last_sample;
        }
        // Same tightened backlog cap as fmcore_gui_main.cpp -- see its own
        // comment on why /4 (~250ms) made fresh notes feel latent.
        if (out_n > 0 && SDL_GetQueuedAudioSize(dev) <= OUT_SAMPLE_RATE * sizeof(int16_t) / 16)
            SDL_QueueAudio(dev, out_buf, (uint32_t)out_n * sizeof(int16_t));

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Track the actual OS window size every frame (io.DisplaySize, kept
        // current by the SDL2 backend as the user drags the window's edges)
        // instead of a fixed size -- NoResize here just means "don't let
        // ImGui's own in-window resize grip fight the real OS window",
        // resizing still works normally via the OS window border/corners.
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("fmcore custom synth piano", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
        ImGui::Text("Home row A S D F G H J K L = white keys, W E T Y U = black keys (middle octave "
                    "only -- click the rest of the keyboard for the other octaves)");
        ImGui::Text(current_key >= 0 ? "Note held" : "(no note held)");
        ImGui::Separator();

        ImGui::BeginChild("canvas", ImVec2(650, 400), true);
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList *draw = ImGui::GetWindowDrawList();
        ImVec2 mouse = ImGui::GetIO().MousePos;

        // Existing connections, drawn as bezier cables between pins.
        for (int from = 0; from < FM_VOICE_OP_COUNT; from++) {
            ImVec2 p0 = OutputPinPos(origin, OP_BOX_POS[from]);
            for (int to = 0; to <= FM_VOICE_OP_COUNT; to++) {
                if (!voice.connect[from][to])
                    continue;
                ImVec2 p1 = (to == FM_VOICE_OUT) ? InputPinPos(origin, OUT_BOX_POS) : InputPinPos(origin, OP_BOX_POS[to]);
                ImVec2 c0(p0.x + 60, p0.y), c1(p1.x - 60, p1.y);
                draw->AddBezierCubic(p0, c0, c1, p1, IM_COL32(90, 200, 255, 255), 2.5f);
            }
        }
        // Live rubber-band line while a cable is being dragged out.
        if (dragging_from >= 0) {
            ImVec2 p0 = OutputPinPos(origin, OP_BOX_POS[dragging_from]);
            draw->AddLine(p0, mouse, IM_COL32(255, 220, 90, 255), 2.0f);
        }

        bool pin_clicked_this_frame = false;
        char label[16];
        for (int i = 0; i < FM_VOICE_OP_COUNT; i++) {
            ImVec2 box_min(origin.x + OP_BOX_POS[i].x, origin.y + OP_BOX_POS[i].y);
            ImVec2 box_max(box_min.x + BOX_SIZE.x, box_min.y + BOX_SIZE.y);
            bool is_selected = (selected_op == i);
            draw->AddRectFilled(box_min, box_max, is_selected ? IM_COL32(70, 70, 100, 255) : IM_COL32(50, 50, 55, 255), 4.0f);
            draw->AddRect(box_min, box_max, IM_COL32(150, 150, 160, 255), 4.0f);
            snprintf(label, sizeof(label), "OP%d", i + 1);
            draw->AddText(ImVec2(box_min.x + 8, box_min.y + 6), IM_COL32(255, 255, 255, 255), label);
            snprintf(label, sizeof(label), "FB %d", op_params[i].fb);
            draw->AddText(ImVec2(box_min.x + 8, box_min.y + 26), IM_COL32(180, 180, 190, 255), label);
            snprintf(label, sizeof(label), "TL %d", op_params[i].tl);
            draw->AddText(ImVec2(box_min.x + 8, box_min.y + 44), IM_COL32(180, 180, 190, 255), label);

            ImVec2 in_pin = InputPinPos(origin, OP_BOX_POS[i]);
            ImVec2 out_pin = OutputPinPos(origin, OP_BOX_POS[i]);
            draw->AddCircleFilled(in_pin, 7.0f, IM_COL32(200, 200, 90, 255));
            draw->AddCircleFilled(out_pin, 7.0f, IM_COL32(90, 220, 200, 255));

            ImGui::SetCursorScreenPos(box_min);
            ImGui::PushID(i);
            ImGui::InvisibleButton("opbox", BOX_SIZE);
            if (ImGui::IsItemClicked())
                selected_op = i;
            ImGui::PopID();

            if (ImGui::IsMouseClicked(0)) {
                if (dragging_from < 0 && PinHit(out_pin, mouse)) {
                    dragging_from = i;
                    pin_clicked_this_frame = true;
                } else if (dragging_from >= 0 && PinHit(in_pin, mouse)) {
                    voice.connect[dragging_from][i] = !voice.connect[dragging_from][i];
                    dragging_from = -1;
                    pin_clicked_this_frame = true;
                }
            }
        }

        {
            ImVec2 box_min(origin.x + OUT_BOX_POS.x, origin.y + OUT_BOX_POS.y);
            ImVec2 box_max(box_min.x + BOX_SIZE.x, box_min.y + BOX_SIZE.y);
            draw->AddRectFilled(box_min, box_max, IM_COL32(60, 45, 45, 255), 4.0f);
            draw->AddRect(box_min, box_max, IM_COL32(150, 150, 160, 255), 4.0f);
            draw->AddText(ImVec2(box_min.x + 8, box_min.y + 6), IM_COL32(255, 220, 200, 255), "OUT");
            ImVec2 in_pin = InputPinPos(origin, OUT_BOX_POS);
            draw->AddCircleFilled(in_pin, 7.0f, IM_COL32(200, 200, 90, 255));
            if (ImGui::IsMouseClicked(0) && dragging_from >= 0 && PinHit(in_pin, mouse)) {
                voice.connect[dragging_from][FM_VOICE_OUT] = !voice.connect[dragging_from][FM_VOICE_OUT];
                dragging_from = -1;
                pin_clicked_this_frame = true;
            }
        }

        // A click that hit neither an output pin (to start a drag) nor a
        // valid input pin (to finish one) cancels any drag in progress --
        // otherwise clicking empty canvas while dragging would strand
        // dragging_from set forever.
        if (ImGui::IsMouseClicked(0) && !pin_clicked_this_frame && dragging_from >= 0 &&
            !PinHit(OutputPinPos(origin, OP_BOX_POS[dragging_from]), mouse))
            dragging_from = -1;

        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 380));
        ImGui::Dummy(ImVec2(1, 1));
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("presets", ImVec2(190, 400));
        ImGui::TextDisabled("Presets");
        if (ImGui::Button("Clear all wires", ImVec2(170, 0))) {
            for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
                for (int j = 0; j <= FM_VOICE_OP_COUNT; j++)
                    voice.connect[i][j] = false;
        }
        if (ImGui::Button("Reset all ops", ImVec2(170, 0))) {
            for (int i = 0; i < FM_VOICE_OP_COUNT; i++) {
                op_params[i] = OpParams();
                ApplyParams(&voice, i, op_params[i]);
            }
        }
        ImGui::Separator();
        ImGui::TextDisabled("Real algorithms (0-7)");
        ImGui::TextWrapped("Loads that algorithm's wiring as a starting point -- wires stay fully "
                            "editable afterward, same as always.");
        static const char *ALG_LABEL[FM_VOICE_ALGORITHM_COUNT] = {
            "Alg 0: 1>2>3>4",          "Alg 1: (1+2)>3>4",        "Alg 2: 1+(2>3)>4",     "Alg 3: (1>2)+3>4",
            "Alg 4: (1>2)+(3>4)",      "Alg 5: 1>(2,3,4)",        "Alg 6: (1>2)+3+4",     "Alg 7: 1+2+3+4",
            "Alg 8: Ring (cyclic)",    "Alg 9: Reciprocal pairs", "Alg 10: Triple mod>4", "Alg 11: Diamond",
            "Alg 12: Recip->carrier",  "Alg 13: Drone+harmonics", "Alg 14: Cross carriers", "Alg 15: Chaos",
        };
        for (int a = 0; a < 8; a++) {
            ImGui::PushID(a + 1000);
            if (ImGui::Button(ALG_LABEL[a], ImVec2(170, 0))) {
                for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
                    for (int j = 0; j <= FM_VOICE_OP_COUNT; j++)
                        voice.connect[i][j] = FM_VOICE_ALGORITHM_CONNECT[a][i][j];
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        ImGui::TextDisabled("fmcore-original algorithms (8-15)");
        ImGui::TextWrapped("No real YM2612 equivalent -- routings only a free-form, cycle-safe graph "
                            "can do (see fm_voice.c's own comments on each).");
        for (int a = 8; a < FM_VOICE_ALGORITHM_COUNT; a++) {
            ImGui::PushID(a + 1000);
            if (ImGui::Button(ALG_LABEL[a], ImVec2(170, 0))) {
                for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
                    for (int j = 0; j <= FM_VOICE_OP_COUNT; j++)
                        voice.connect[i][j] = FM_VOICE_ALGORITHM_CONNECT[a][i][j];
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        ImGui::TextWrapped("Click an output pin (right, teal) then an input pin (left, yellow) to "
                            "connect/disconnect. Esc cancels a drag.");
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Text("Editing OP%d", selected_op + 1);
        OpParams &p = op_params[selected_op];
        bool changed = false;
        changed |= ImGui::SliderInt("Attack Rate (AR)", &p.ar, 0, 31);
        changed |= ImGui::SliderInt("Decay Rate 1 (D1R)", &p.d1r, 0, 31);
        changed |= ImGui::SliderInt("Decay Rate 2 (D2R)", &p.d2r, 0, 31);
        changed |= ImGui::SliderInt("Decay Level (D1L)", &p.d1l, 0, 15);
        changed |= ImGui::SliderInt("Release Rate (RR)", &p.rr, 0, 15);
        changed |= ImGui::SliderInt("Total Level (TL)", &p.tl, 0, 127);
        changed |= ImGui::SliderInt("Feedback (FB)", &p.fb, 0, 7);
        ImGui::Separator();
        changed |= ImGui::SliderInt("Multiple (MUL)", &p.mul, 0, 15);
        changed |= ImGui::SliderInt("Detune (DT)", &p.dt, 0, 7);
        changed |= ImGui::SliderInt("Rate Scale (RS)", &p.rs, 0, 3);
        changed |= ImGui::SliderInt("Amp Mod (AM)", &p.am, 0, 1);
        ImGui::TextDisabled("AM currently gates a fixed-rate/fixed-depth test LFO (fm_voice.c), not");
        ImGui::TextDisabled("real hardware's per-channel AMS depth register -- placeholder for now.");
        if (changed)
            ApplyParams(&voice, selected_op, p);

        ImGui::Separator();
        // Full KEYBOARD_OCTAVES-wide piano (see this file's top-of-file
        // comment) -- white keys first so black keys draw on top of them at
        // the boundary, same layering as a real piano.
        ImVec2 kb_origin = ImGui::GetCursorScreenPos();
        const float white_w = 46, white_h = 170, black_w = 28, black_h = 105;
        for (int oct = 0; oct < KEYBOARD_OCTAVES; oct++) {
            for (int w = 0; w < 7; w++) {
                int semitone = WHITE_SEMITONES[w];
                int ki = oct * 12 + semitone;
                float x = (oct * 7 + w) * white_w;
                ImGui::PushID(ki);
                ImGui::SetCursorScreenPos(ImVec2(kb_origin.x + x, kb_origin.y));
                ImGui::InvisibleButton("white", ImVec2(white_w - 1, white_h));
                bool held = ImGui::IsItemActive();
                held_this_frame[ki] = held_this_frame[ki] || held;
                ImVec2 p0 = ImGui::GetItemRectMin(), p1 = ImGui::GetItemRectMax();
                draw->AddRectFilled(p0, p1, held ? IM_COL32(220, 60, 60, 255) : IM_COL32(0xEE, 0xEE, 0xEE, 255));
                draw->AddRect(p0, p1, IM_COL32(80, 80, 80, 255));
                ImGui::PopID();
            }
        }
        for (int oct = 0; oct < KEYBOARD_OCTAVES; oct++) {
            for (int b = 0; b < 5; b++) {
                int semitone = BLACK_SEMITONES[b];
                int ki = oct * 12 + semitone;
                float cx = (oct * 7 + BLACK_AFTER_WHITE_SLOT[semitone] + 1) * white_w;
                ImGui::PushID(ki + 1000);
                ImGui::SetCursorScreenPos(ImVec2(kb_origin.x + cx - black_w / 2, kb_origin.y));
                ImGui::InvisibleButton("black", ImVec2(black_w, black_h));
                bool held = ImGui::IsItemActive();
                held_this_frame[ki] = held_this_frame[ki] || held;
                ImVec2 p0 = ImGui::GetItemRectMin(), p1 = ImGui::GetItemRectMax();
                draw->AddRectFilled(p0, p1, held ? IM_COL32(220, 60, 60, 255) : IM_COL32(0xAA, 0xAA, 0xAA, 255));
                ImGui::PopID();
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(kb_origin.x, kb_origin.y + white_h + 4));
        ImGui::Dummy(ImVec2(KEYBOARD_OCTAVES * 7 * white_w, 1));

        for (int i = 0; i < ALL_KEY_TOTAL; i++) {
            bool held = held_this_frame[i];
            int semitone = i % 12, block = KEYBOARD_BASE_BLOCK + i / 12;
            if (held && !key_held_prev[i]) {
                int fnum = FNUM_TABLE[semitone];
                FMVoice_SetFreq(&voice, (uint16_t)fnum, (uint8_t)block);
                FMVoice_KeyOn(&voice);
                current_key = i;
            }
            if (!held && key_held_prev[i] && current_key == i) {
                FMVoice_KeyOff(&voice);
                current_key = -1;
            }
            key_held_prev[i] = held;
        }

        ImGui::End();
        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_CloseAudioDevice(dev);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
