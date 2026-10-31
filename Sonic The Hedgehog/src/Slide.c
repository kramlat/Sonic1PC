#include "Object/Sonic.h"
#include "Slide.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Sound.h"

void Slide_Update(const SlideSurface *surfaces, int count) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)player->own.u8;

    if (((player->status & SST_PSTATUS_IN_AIR) != 0)) {
        if (f_slidemode) {
            scratch->control_lock = 5;
            f_slidemode = false;
        }
        return;
    }

    uint8_t chunk = LevelLayout_Chunk(LEVEL_LAYOUT_PLANE_FG, player->x_pos.f.u >> 7, player->y_pos.f.u >> 7);

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
    SST_STATUS_SET(player, SST_PSTATUS_X_FLIP, speed < 0);
    player->ground_vel = (int16_t)(speed << 8);
    player->anim = SonAnimId_WaterSlid;
    f_slidemode = true;

    if (((uint8_t)frame_count & 0x1F) == 0)
        QueueSound2(sfx_Waterfall);
}
