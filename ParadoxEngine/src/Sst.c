#include "Sst.h"

#include <string.h>

Sst sst[SST_COUNT];
uint8_t sst_reverse_gravity;

static Sst *queue[SST_QUEUES][SST_QUEUE_SIZE];
static int queue_count[SST_QUEUES];

void Sst_ClearQueues(void) {
    memset(queue_count, 0, sizeof(queue_count));
}

int Sst_QueueCount(int q) {
    return queue_count[q];
}

Sst *Sst_QueueEntry(int q, int index) {
    return queue[q][index];
}

void Sst_Reset(void) {
    memset(sst, 0, sizeof(sst));
    Sst_ClearQueues();
}

void Sst_Draw(Sst *obj) {
    int q = obj->priority >> 7;
    if (q >= SST_QUEUES)
        q = SST_QUEUES - 1;
    for (; q < SST_QUEUES; q++)
        if (queue_count[q] < SST_QUEUE_SIZE) {
            queue[q][queue_count[q]++] = obj;
            return;
        }
}

void Sst_Delete(Sst *obj) {
    memset(obj, 0, sizeof(*obj));
}

static void RunRange(int first, int last) {
    for (int i = first; i <= last; i++)
        if (sst[i].code != NULL)
            sst[i].code(&sst[i]);
}

void Sst_Process(bool freeze_level) {
    if (!freeze_level) {
        RunRange(0, SST_COUNT - 1);
        return;
    }
    RunRange(0, SST_FIRST_LEVEL); // (the original runs one slot past the dynamic ones here: the first of the level's)
    for (int i = SST_FIRST_LEVEL + 1; i < SST_COUNT; i++)
        if (sst[i].code != NULL && (sst[i].render_flags & SST_RENDER_ON_SCREEN))
            Sst_Draw(&sst[i]);
}

Sst *Sst_Allocate(void) {
    for (int i = SST_FIRST_DYNAMIC + 1; i <= SST_FIRST_LEVEL; i++)
        if (sst[i].code == NULL)
            return &sst[i];
    return NULL;
}

Sst *Sst_AllocateAfter(const Sst *after) {
    int from = (int)(after - sst) + 1;
    if (from < SST_FIRST_DYNAMIC)
        from = SST_FIRST_DYNAMIC;
    for (int i = from; i < SST_FIRST_LEVEL; i++)
        if (sst[i].code == NULL)
            return &sst[i];
    return NULL;
}

// A velocity in 1/256 pixels added to a position's pixel and sub-pixel together
static void Add(dword_s *pos, int16_t velocity) {
    pos->v += (int32_t)velocity * 256;
}

void Sst_MoveSprite(Sst *obj) {
    Add(&obj->x_pos, obj->x_vel);
    int16_t y = obj->y_vel;
    obj->y_vel = (int16_t)(obj->y_vel + 0x38);
    Add(&obj->y_pos, y);
}

void Sst_MoveSprite2(Sst *obj) {
    Add(&obj->x_pos, obj->x_vel);
    Add(&obj->y_pos, obj->y_vel);
}

void Sst_MoveSpriteTestGravity(Sst *obj) {
    if (!sst_reverse_gravity) {
        Sst_MoveSprite(obj);
        return;
    }
    Add(&obj->x_pos, obj->x_vel);
    int16_t y = obj->y_vel;
    obj->y_vel = (int16_t)(obj->y_vel + 0x38);
    Add(&obj->y_pos, (int16_t)-y);
}

void Sst_MoveSpriteTestGravity2(Sst *obj) {
    if (!sst_reverse_gravity) {
        Sst_MoveSprite2(obj);
        return;
    }
    Add(&obj->x_pos, obj->x_vel);
    Add(&obj->y_pos, (int16_t)-obj->y_vel);
}

static const uint8_t *Script(const uint8_t *table, unsigned anim) {
    return table + ((table[anim * 2] << 8) | table[anim * 2 + 1]);
}

// The frame is set, and the flips of `status` go into the render flags
static void SetFrame(Sst *obj, uint8_t frame) {
    obj->mapping_frame = frame;
    obj->render_flags = (uint8_t)((obj->render_flags & 0xFC) | (obj->status & 3));
    obj->anim_frame++;
}

static bool Restart(Sst *obj) {
    if (obj->anim != obj->prev_anim) {
        obj->prev_anim = obj->anim;
        obj->anim_frame = 0;
        obj->anim_frame_timer = 0;
    }
    return --obj->anim_frame_timer == 0xFF; // (subq.b #1 with a borrow: the timer ran out)
}

void Sst_Animate(Sst *obj, const uint8_t *table) {
    if (!Restart(obj))
        return;
    const uint8_t *s = Script(table, obj->anim);
    obj->anim_frame_timer = s[0];
    unsigned d1 = obj->anim_frame;
    uint8_t d0 = s[1 + d1];
    if (!(d0 & 0x80)) {
        SetFrame(obj, d0);
        return;
    }
    switch (d0) {
    case 0xFF: // back to the first frame
        obj->anim_frame = 0;
        SetFrame(obj, s[1]);
        break;
    case 0xFE: { // back by the byte after the code
        uint8_t back = s[2 + d1];
        obj->anim_frame = (uint8_t)(obj->anim_frame - back);
        d1 = (uint8_t)(d1 - back);
        SetFrame(obj, s[1 + d1]);
        break;
    }
    case 0xFD: // another animation
        obj->anim = s[2 + d1];
        break;
    case 0xFC: // the routine counter on by 2
        obj->routine = (uint8_t)(obj->routine + 2);
        obj->anim_frame_timer = 0;
        obj->anim_frame++;
        break;
    case 0xFB: // off to the side
        obj->x_pos.f.u = 0x7F00;
        break;
    default:
        break;
    }
}

void Sst_AnimateIrregular(Sst *obj, const uint8_t *table) {
    if (!Restart(obj))
        return;
    const uint8_t *s = Script(table, obj->anim);
    unsigned d1 = (unsigned)obj->anim_frame * 2;
    uint8_t d0 = s[d1];
    if (d0 & 0x80) {
        switch (d0) {
        case 0xFF: // back to the first pair
            d1 = 0;
            obj->anim_frame = 0;
            d0 = s[0];
            break;
        case 0xFE: { // back by the number of pairs the byte after the code gives
            uint8_t back = s[d1 + 1];
            obj->anim_frame = (uint8_t)(obj->anim_frame - back);
            d1 = (uint8_t)(d1 - back * 2);
            d0 = s[d1];
            break;
        }
        case 0xFD:
            obj->anim = s[d1 + 1];
            return;
        case 0xFC:
            obj->routine = (uint8_t)(obj->routine + 2);
            obj->anim_frame_timer = 0;
            obj->anim_frame++;
            return;
        default:
            return;
        }
    }
    obj->anim_frame_timer = s[d1 + 1];
    SetFrame(obj, d0);
}
