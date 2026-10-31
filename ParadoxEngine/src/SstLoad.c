#include "SstLoad.h"

#include <string.h>

// A port of Load_Sprites (sonic3k.asm, loc_1B6A8 to loc_1BAF0). The original's pair of a pointer into the layout and a pointer into the respawn table, always one entry apart, is one entry number here.

#define ENTRY_SIZE 6

uint8_t sst_respawn_table[SST_RESPAWN_COUNT];

static const uint8_t empty_layout[ENTRY_SIZE] = {0xFF, 0xFF, 0, 0, 0, 0};

static SstObjectIndex object_index;
static const uint8_t *layout = empty_layout;
// One load window, for one camera. The second is the other player's camera in a split screen (S3&K keeps a second set of these for Player_2's camera); both fill the one respawn table, so an object in range of both is loaded once.
typedef struct {
    int front, back;             // Object_load_addr_front / _back (and the respawn indexes): the entries from back up to front are the ones in the load window
    int16_t x_coarse, y_coarse;  // Camera_X_pos_coarse / Camera_Y_pos_coarse
    int16_t x_coarse_back, y_coarse_back;
} LoadWindow;
static LoadWindow windows[2];
static LoadWindow *w = &windows[0];  // the window the passes below work on
static bool second_active;
static Sst *slot;                   // the free slot the next object goes in (a1)

static uint16_t Word(int entry, int offset) {
    const uint8_t *p = layout + entry * ENTRY_SIZE + offset;
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint16_t EntryX(int entry) {
    return Word(entry, 0);
}

bool SstLoad_Handles(uint8_t id) {
    return object_index.codes != NULL && id < object_index.code_count && object_index.codes[id] != NULL;
}

int16_t SstLoad_CoarseBackX(void) {
    return windows[0].x_coarse_back;
}

int16_t SstLoad_CoarseBackY(void) {
    return windows[0].y_coarse_back;
}

// The next free slot after `from` (a slot index), as the original looks for one: up to and including the first of the level's fixed slots (see Sst_Allocate)
static Sst *FreeAfter(int from) {
    for (int i = from + 1; i <= SST_FIRST_LEVEL; i++)
        if (sst[i].code == NULL)
            return &sst[i];
    return NULL;
}

// Puts the entry's object in the held slot and finds the next free one (CreateNewSprite4). Returns true when there is none: the loader has to stop.
static bool Place(int entry, uint16_t y, uint16_t y_word) {
    const uint8_t *p = layout + entry * ENTRY_SIZE;
    Sst *o = slot;
    sst_respawn_table[entry] |= 0x80;
    o->x_pos.f.u = (int16_t)EntryX(entry);
    o->y_pos.f.u = (int16_t)(y & 0xFFF);
    o->render_flags = o->status = (uint8_t)((y_word >> 13) & 3);
    o->code = object_index.codes[p[4]];
    o->subtype = p[5];
    o->respawn = (uint16_t)(entry + 1);
    slot = FreeAfter((int)(o - sst));
    return slot == NULL;
}

// The two loaders (loc_1BA92, for the usual window, and loc_1BA40, for the one of a level that wraps vertically with the window straddling the wrap): load the entry if it is not out there and it is within the Y
// window [d3, d4] (bit 15 of its Y word: whatever the Y). Returns true when the slots are full after loading it.
static bool Check(int entry, uint16_t d3, uint16_t d4, bool wrapped) {
    if (entry >= SST_RESPAWN_COUNT || (sst_respawn_table[entry] & 0x80))
        return false;
    if (!SstLoad_Handles(layout[entry * ENTRY_SIZE + 4]))
        return false;
    uint16_t word = Word(entry, 2);
    uint16_t y = word & 0xFFF;
    if (!(word & 0x8000)) {
        bool inside = wrapped ? (y >= d3 || y <= d4) : (y >= d3 && y <= d4);
        if (!inside)
            return false;
    }
    return Place(entry, y, word);
}

// The loader's own reading of the camera (loc_1B7F2): the Y window, and whether it wraps
static void Window(const SstLoadCamera *c, uint16_t *d3, uint16_t *d4, bool *wrapped) {
    *d3 = (uint16_t)c->y & 0xFF80;
    *d4 = (uint16_t)(*d3 + 0x200);
    *d3 = (uint16_t)(*d3 - 0x80);
    *wrapped = false;
    if (c->wraps_y) {
        if ((int16_t)*d3 >= 0) {
            if (*d4 > (uint16_t)(c->y_wrap + 1)) {
                *d4 &= c->y_wrap;
                *wrapped = true;
            }
        } else {
            *d3 &= c->y_wrap;
            *wrapped = true;
        }
    } else if ((int16_t)*d3 < 0) {
        *d3 = 0;
    }
}

// Seeks the current window to a camera: the front to the first entry at or past one chunk left of the camera, the back one chunk before that
static void SeekWindow(const SstLoadCamera *camera) {
    uint16_t d6 = (uint16_t)camera->x - 0x80;
    if ((uint16_t)camera->x < 0x80)
        d6 = 0;
    d6 &= 0xFF80;
    int i = 0;
    while (EntryX(i) < d6)
        i++;
    w->front = i;
    i = 0;
    if (d6 >= 0x80) {
        d6 = (uint16_t)(d6 - 0x80);
        while (EntryX(i) < d6)
            i++;
    }
    w->back = i;

    w->x_coarse = -1;
    w->y_coarse = (int16_t)((uint16_t)camera->y & 0xFF80);
}

void SstLoad_Init(const SstObjectIndex *index, const uint8_t *entries, bool keep_table, const SstLoadCamera *camera) {
    object_index = *index;
    layout = entries != NULL ? entries : empty_layout;
    if (!keep_table)
        memset(sst_respawn_table, 0, sizeof(sst_respawn_table));
    second_active = false;
    w = &windows[0];
    SeekWindow(camera);
    SstLoad_Update(camera);
}

// The pass for a camera that has moved to another column. The original does all of it only when there is a free slot to load into; the pointers move on either way.
static void ColumnPass(const SstLoadCamera *c, uint16_t d3, uint16_t d4, bool wrapped) {
    uint16_t d6 = (uint16_t)c->x & 0xFF80;
    if ((int16_t)d6 == w->x_coarse)
        return;
    slot = FreeAfter(SST_FIRST_DYNAMIC);
    if ((int16_t)d6 < w->x_coarse) {
        // Moving left: load the entries now in range at the back, and give up the ones past the front
        w->x_coarse = (int16_t)d6;
        bool borrow = d6 < 0x80;
        d6 = (uint16_t)(d6 - 0x80);
        int a = w->back;
        if (!borrow && slot != NULL) {
            while (a > 0 && (int16_t)d6 < (int16_t)EntryX(a - 1)) {
                a--;
                if (Check(a, d3, d4, wrapped))
                    break; // (the slots are full: the original is a pair of pointers off here, which loads the entries just before it twice; it is kept to the entry)
            }
        }
        w->back = a;
        a = w->front;
        d6 = (uint16_t)(d6 + 0x300);
        while (a > 0 && !((int16_t)d6 > (int16_t)EntryX(a - 1)))
            a--;
        w->front = a;
        return;
    }

    // Moving right: load the entries now in range at the front, and give up the ones past the back
    w->x_coarse = (int16_t)d6;
    d6 = (uint16_t)(d6 + 0x280);
    int a = w->front;
    if (slot != NULL) {
        while (EntryX(a) < d6) {
            bool full = Check(a, d3, d4, wrapped);
            a++;
            if (full)
                break;
        }
    }
    w->front = a;
    a = w->back;
    bool borrow = d6 < 0x300;
    d6 = (uint16_t)(d6 - 0x300);
    if (!borrow) {
        while (EntryX(a) < d6)
            a++;
    }
    w->back = a;
}

// The pass for a camera that has moved to another row (loc_1B91A): the entries in the window whose Y has come into range
static void RowPass(const SstLoadCamera *c) {
    uint16_t d6 = (uint16_t)c->y & 0xFF80;
    uint16_t d3 = d6;
    if ((int16_t)d6 == w->y_coarse)
        return;

    bool load = true;
    if ((int16_t)d6 < w->y_coarse) { // moving up
        if (!c->wraps_y) {
            d3 -= 0x80;
            if ((int16_t)d3 < 0)
                load = false;
        } else if (d6 != 0 || w->y_coarse == 0x80) {
            d3 -= 0x80; // (loc_1B940)
            if ((int16_t)d3 < 0)
                d3 &= c->y_wrap;
        } else {
            d3 += 0x180; // (loc_1B968)
            if (d3 >= c->y_wrap)
                d3 &= c->y_wrap;
        }
    } else { // moving down
        if (!c->wraps_y) {
            d3 += 0x180;
            if (d3 > c->y_wrap)
                load = false;
        } else if (w->y_coarse != 0 || d6 == 0x80) {
            d3 += 0x180;
            if (d3 >= c->y_wrap)
                d3 &= c->y_wrap;
        } else {
            d3 -= 0x80;
            if ((int16_t)d3 < 0)
                d3 &= c->y_wrap;
        }
    }

    if (load) {
        slot = FreeAfter(SST_FIRST_DYNAMIC);
        uint16_t d4 = (uint16_t)(d3 + 0x80);
        for (int a = w->back; slot != NULL && a < w->front; a++) {
            if ((sst_respawn_table[a] & 0x80) || a >= SST_RESPAWN_COUNT)
                continue;
            if (!SstLoad_Handles(layout[a * ENTRY_SIZE + 4]))
                continue;
            uint16_t y = Word(a, 2) & 0xFFF;
            if (y < d3 || y > d4)
                continue;
            Place(a, y, Word(a, 2));
        }
    }
    w->y_coarse = (int16_t)d6;
}

static void UpdateWindow(const SstLoadCamera *camera) {
    w->y_coarse_back = (int16_t)(((uint16_t)camera->y - 0x80) & 0xFF80);
    w->x_coarse_back = (int16_t)(((uint16_t)camera->x - 0x80) & 0xFF80);
    uint16_t d3, d4;
    bool wrapped;
    Window(camera, &d3, &d4, &wrapped);
    ColumnPass(camera, d3, d4, wrapped);
    RowPass(camera);
}

void SstLoad_Update(const SstLoadCamera *camera) {
    w = &windows[0];
    UpdateWindow(camera);
}

void SstLoad_UpdateSecond(const SstLoadCamera *camera) {
    w = &windows[1];
    if (!second_active) {
        SeekWindow(camera);
        second_active = true;
    }
    UpdateWindow(camera);
    w = &windows[0];
}

void SstLoad_EndSecond(void) {
    second_active = false;
}

void SstLoad_Seek(int16_t camera_x) {
    w = &windows[0];
    uint16_t d6 = (uint16_t)((uint16_t)camera_x + 0x400) & 0xFF80;
    if ((int16_t)d6 == w->x_coarse)
        return;
    if ((int16_t)d6 < w->x_coarse) {
        w->x_coarse = (int16_t)d6;
        int a = w->back;
        bool borrow = d6 < 0x80;
        d6 = (uint16_t)(d6 - 0x80);
        if (!borrow)
            while (a > 0 && (int16_t)d6 < (int16_t)EntryX(a - 1))
                a--;
        w->back = a;
        a = w->front;
        d6 = (uint16_t)(d6 + 0x300);
        while (a > 0 && !((int16_t)d6 > (int16_t)EntryX(a - 1)))
            a--;
        w->front = a;
        return;
    }
    w->x_coarse = (int16_t)d6;
    d6 = (uint16_t)(d6 + 0x280);
    int a = w->front;
    while (EntryX(a) < d6)
        a++;
    w->front = a;
    a = w->back;
    bool borrow = d6 < 0x300;
    d6 = (uint16_t)(d6 - 0x300);
    if (!borrow)
        while (EntryX(a) < d6)
            a++;
    w->back = a;
}

void SstLoad_ClearLoaded(void) {
    for (int i = SST_FIRST_DYNAMIC + 1; i <= SST_FIRST_LEVEL; i++)
        if (sst[i].code != NULL)
            SstLoad_Forget(&sst[i]);
}

void SstLoad_Forget(const Sst *obj) {
    if (obj->respawn != 0 && obj->respawn <= SST_RESPAWN_COUNT)
        sst_respawn_table[obj->respawn - 1] &= 0x7F;
}

bool sst_load_everything_in_range; // (the tests that place objects anywhere in a level: nothing is out of range)

// Whether an X is more than 0x280 past the coarse back edge of a window's camera
static bool PastWindow(const LoadWindow *win, int16_t x) {
    return (uint16_t)((uint16_t)((uint16_t)x & 0xFF80) - (uint16_t)win->x_coarse_back) > 0x280;
}

// Out of range is out of range of both cameras
bool SstOutOfRange(int16_t x) {
    if (sst_load_everything_in_range)
        return false;
    return PastWindow(&windows[0], x) && (!second_active || PastWindow(&windows[1], x));
}

void SstOnScreenTest2(Sst *obj, int16_t x) {
    if (SstOutOfRange(x)) {
        SstLoad_Forget(obj);
        Sst_Delete(obj);
        return;
    }
    Sst_Draw(obj);
}

void SstOnScreenTest(Sst *obj) {
    SstOnScreenTest2(obj, obj->x_pos.f.u);
}

void SstDeleteIfNotInRange(Sst *obj) {
    if (SstOutOfRange(obj->x_pos.f.u)) {
        SstLoad_Forget(obj);
        Sst_Delete(obj);
    }
}

Sst *Sst_SpawnCode(SstCode code, int16_t x, int16_t y) {
    Sst *o = Sst_Allocate();
    if (o == NULL)
        return NULL;
    o->code = code;
    o->x_pos.f.u = x;
    o->y_pos.f.u = y;
    return o;
}

Sst *Sst_Spawn(uint8_t id, int16_t x, int16_t y) {
    return Sst_SpawnCode(sst_object_codes[id], x, y);
}

void SstDeleteIfNotInRange2(Sst *obj, int16_t x) {
    if (SstOutOfRange(x)) {
        SstLoad_Forget(obj);
        Sst_Delete(obj);
    }
}

uint8_t *SstLoad_State(const Sst *obj) {
    static uint8_t none;
    if (obj->respawn == 0 || obj->respawn > SST_RESPAWN_COUNT) {
        none = 0;
        return &none;
    }
    return &sst_respawn_table[obj->respawn - 1];
}
