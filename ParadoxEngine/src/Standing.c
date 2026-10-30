#include "Standing.h"

void Standing_Get(uint8_t id, StandingInfo *out) {
    if (id >= STANDING_SST) {
        const Sst *o = &sst[(id - STANDING_SST) % SST_COUNT];
        out->x = o->x_pos.f.u;
        out->width = o->width_pixels;
        out->flag7 = (o->status & 0x80) != 0;
        return;
    }
    const Object *o = &objects[id];
    out->x = o->pos.l.x.f.u;
    out->width = o->width_pixels;
    out->flag7 = o->status.o.f.flag7;
}

void Standing_Release(uint8_t id, StandingRelease how) {
    if (id >= STANDING_SST) {
        Sst *o = &sst[(id - STANDING_SST) % SST_COUNT];
        o->status &= (uint8_t)~0x08;
        if (how != STANDING_RELEASE_FLAG)
            o->routine_secondary = 0;
        if (how == STANDING_RELEASE_PLATFORM && o->routine == 4)
            o->routine = 2;
        return;
    }
    Object *o = &objects[id];
    o->status.o.f.player_stand = false;
    if (how != STANDING_RELEASE_FLAG)
        o->routine_sec = 0;
    if (how == STANDING_RELEASE_PLATFORM && o->routine == 4)
        o->routine = 2;
}
