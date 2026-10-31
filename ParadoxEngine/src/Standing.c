#include "Standing.h"

void Standing_Get(uint8_t id, StandingInfo *out) {
    if (id < STANDING_SST) { // (nothing)
        out->x = 0;
        out->width = 0;
        out->flag7 = false;
        return;
    }
    const Sst *o = &sst[(id - STANDING_SST) % SST_COUNT];
    out->x = o->x_pos.f.u;
    out->width = o->width_pixels;
    out->flag7 = (o->status & 0x80) != 0;
}

void Standing_Release(uint8_t id, StandingRelease how) {
    if (id < STANDING_SST)
        return;
    Sst *o = &sst[(id - STANDING_SST) % SST_COUNT];
    o->status &= (uint8_t)~0x08;
    if (how != STANDING_RELEASE_FLAG)
        o->routine_secondary = 0;
    if (how == STANDING_RELEASE_PLATFORM && o->routine == 4)
        o->routine = 2;
}
