#include "Slide.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Sound.h"

void Slide_Update(const SlideSurface *surfaces, int count) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)&player->scratch;

    if (player->status.p.f.in_air) {
        if (f_slidemode) {
            scratch->control_lock = 5;
            f_slidemode = false;
        }
        return;
    }

    uint8_t chunk = LEVEL_LAYOUT_FG((player->pos.l.y.f.u >> 7) & 0xF)[(player->pos.l.x.f.u >> 7) & 0x7F];

    int match = -1;
    for (int i = 0; i < count; i++) {
        if (surfaces[i].chunk == chunk) {
            match = i;
            break;
        }
    }

    if (match < 0) {
        if (f_slidemode) {
            scratch->control_lock = 5;
            f_slidemode = false;
        }
        return;
    }

    int8_t speed = surfaces[match].speed;
    player->status.p.f.x_flip = speed < 0;
    player->inertia = (int16_t)(speed << 8);
    player->anim = SonAnimId_WaterSlid;
    f_slidemode = true;

    if (((uint8_t)frame_count & 0x1F) == 0)
        QueueSound2(sfx_Waterfall);
}
