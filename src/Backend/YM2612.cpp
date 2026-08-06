#include "YM2612.h"

#include "../../ymfm/src/ymfm_opn.h"

// ym2612 doesn't use ymfm_interface's external-memory/timer callbacks (those
// exist for chips with ADPCM sample ROM or busy-timer interrupts) -- the
// base class's default no-op implementations are all we need.
struct YM2612Interface : ymfm::ymfm_interface {};

struct YM2612 {
    YM2612Interface intf;
    ymfm::ym2612 chip;
    uint32_t tick_accum = 0;
    ymfm::ym2612::output_data last_sample{};

    YM2612() : chip(intf) { chip.reset(); }
};

extern "C" {

YM2612 *YM2612_Create(void) { return new YM2612(); }
void YM2612_Destroy(YM2612 *chip) { delete chip; }

void YM2612_Write(YM2612 *chip, uint32_t offset, uint8_t data) { chip->chip.write(offset, data); }

void YM2612_Generate(YM2612 *chip, int32_t *out, uint32_t count, uint32_t sample_rate, uint32_t clock_rate) {
    uint32_t native_rate = chip->chip.sample_rate(clock_rate);
    for (uint32_t i = 0; i < count; i++) {
        chip->tick_accum += native_rate;
        while (chip->tick_accum >= sample_rate) {
            chip->tick_accum -= sample_rate;
            chip->chip.generate(&chip->last_sample, 1);
        }
        out[2 * i + 0] += chip->last_sample.data[0]; // L
        out[2 * i + 1] += chip->last_sample.data[1]; // R
    }
}

} // extern "C"
