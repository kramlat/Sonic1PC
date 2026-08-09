#include "YM2612.h"

#include "../../contrib/ymfm/src/ymfm_opn.h"

// ym2612 doesn't use ymfm_interface's external-memory/timer callbacks (those
// exist for chips with ADPCM sample ROM or busy-timer interrupts) -- the
// base class's default no-op implementations are all we need.
struct YM2612Interface : ymfm::ymfm_interface {};

struct YM2612 {
    YM2612Interface intf;
    ymfm::ym2612 chip;
    uint32_t tick_accum = 0;
    ymfm::ym2612::output_data last_sample{};

    // Shadow register state for YM2612_PeekReg -- see its own comment
    // (Backend/YM2612.h) for why this exists (real hardware is write-only).
    // latched_addr tracks each port's most recent address-port write, the
    // same way the real chip's internal address latch works, so the
    // following data-port write knows which register it's actually for.
    uint8_t reg_shadow[2][0x100] = {};
    uint8_t latched_addr[2] = {0, 0};

    // $28 (key-on/off) doesn't fit the plain per-register shadow above --
    // real hardware only exposes it on port 0's address space regardless of
    // which channel it targets, encoding the target channel IN the data
    // byte itself (bits 0-1 = channel within group, bit 2 = group select,
    // bits 4-7 = per-operator key bits). So reg_shadow[0][0x28] only ever
    // holds the single most recent write, not "is channel N on" for all 6
    // channels at once -- keyon_mask decodes and accumulates that instead,
    // one persistent bit per FM channel (0-5, matching this project's
    // port*3+channel numbering everywhere else).
    uint8_t keyon_mask = 0;

    YM2612() : chip(intf) { chip.reset(); }
};

extern "C" {

YM2612 *YM2612_Create(void) { return new YM2612(); }
void YM2612_Destroy(YM2612 *chip) { delete chip; }

void YM2612_Write(YM2612 *chip, uint32_t offset, uint8_t data) {
    chip->chip.write(offset, data);

    int port = (int)(offset >> 1); // 0 = channels 1-3, 1 = channels 4-6
    if ((offset & 1) == 0) {
        chip->latched_addr[port] = data; // address-port write
    } else {
        chip->reg_shadow[port][chip->latched_addr[port]] = data; // data-port write
        if (port == 0 && chip->latched_addr[0] == 0x28) {
            int fm_channel = (data & 3) + ((data & 4) ? 3 : 0); // group*3 + local, 0-5
            if (fm_channel < 6) {
                bool on = (data & 0xF0) != 0; // any operator's key bit set
                if (on)
                    chip->keyon_mask |= (uint8_t)(1 << fm_channel);
                else
                    chip->keyon_mask &= (uint8_t)~(1 << fm_channel);
            }
        }
    }
}

uint8_t YM2612_PeekReg(const YM2612 *chip, int port, uint8_t reg) { return chip->reg_shadow[port & 1][reg]; }
uint8_t YM2612_PeekKeyOn(const YM2612 *chip) { return chip->keyon_mask; }

void YM2612_Generate(YM2612 *chip, int32_t *out, uint32_t count, uint32_t sample_rate, uint32_t clock_rate) {
    uint32_t native_rate = chip->chip.sample_rate(clock_rate);
    for (uint32_t i = 0; i < count; i++) {
        // Average every native-rate sample that falls within this output
        // sample's window instead of keeping only the last one -- plain
        // nearest-neighbor decimation has no anti-aliasing at all, and any
        // high-frequency content the chip generates near/above the output
        // Nyquist (feedback and high MUL values can produce plenty) folds
        // straight down into audible noise on top of the real waveform.
        // Averaging is a simple box filter that suppresses that.
        int64_t sum_l = 0, sum_r = 0;
        uint32_t taken = 0;
        chip->tick_accum += native_rate;
        while (chip->tick_accum >= sample_rate) {
            chip->tick_accum -= sample_rate;
            chip->chip.generate(&chip->last_sample, 1);
            sum_l += chip->last_sample.data[0];
            sum_r += chip->last_sample.data[1];
            taken++;
        }
        if (taken > 0) {
            out[2 * i + 0] += (int32_t)(sum_l / (int64_t)taken);
            out[2 * i + 1] += (int32_t)(sum_r / (int64_t)taken);
        } else {
            // Native rate < output rate this step (shouldn't normally
            // happen for YM2612's ~53KHz native vs typical 44.1KHz output,
            // but stay correct if it ever does): hold the last real sample.
            out[2 * i + 0] += chip->last_sample.data[0];
            out[2 * i + 1] += chip->last_sample.data[1];
        }
    }
}

} // extern "C"
