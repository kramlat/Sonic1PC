#include "ObjectsManager.h"

#include <string.h>

// A line-by-line port of Sonic 2's ObjectsManager (1-player mode), ChkLoadObj included, with the original's RAM in a struct (ObjectsManager) and its constants
// in a configuration. The respawn table's first two bytes (the two running indexes) are respawn_right / respawn_left, so marks is indexed by respawn index directly.

#define ENTRY_SIZE 6

static uint16_t Word(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static bool Remembers(const uint8_t *entry) { // tst.b 2(a0) / bpl: the object gets a respawn table entry
    return (entry[2] & 0x80) != 0;
}

// Whether m has loaded (or passed over as already loaded) the entry: it is between its two side pointers, and not one of the ones the start skipped.
static bool Holds(const ObjectsManager *m, const uint8_t *entry) {
    return entry >= m->load_left && entry < m->load_right && !(entry >= m->gap_start && entry < m->gap_end);
}

// ChkLoadObj: loads the object at *entry unless it was already loaded. Returns true when the slots are full (nothing changed then, but for the loaded mark).
static bool ChkLoadObj(ObjectsManager *m, const uint8_t **entry, uint8_t respawn_index) {
    const uint8_t *p = *entry;
    if (m->partner != NULL && Holds(m->partner, p)) { // the other camera's manager has this one out there already
        *entry += ENTRY_SIZE;
        return false;
    }
    if (Remembers(p) && respawn_index < m->mark_count) { // (past the table an object just is not remembered)
        uint8_t *mark = &m->marks[respawn_index];
        bool was_loaded = (*mark & 0x80) != 0;
        *mark |= 0x80;
        if (was_loaded) {
            *entry += ENTRY_SIZE; // already out there
            return false;
        }
    }

    Object *obj = FindFreeObj();
    if (obj == NULL)
        return true;

    obj->pos.l.x.f.u = (int16_t)Word(p);
    uint16_t w = Word(p + 2);
    if (w & 0x8000)
        obj->respawn_index = respawn_index;
    obj->pos.l.y.f.u = (int16_t)(w & 0xFFF);
    uint8_t flips = (uint8_t)((w >> 13) & 3); // x flip, y flip
    obj->render.b = flips;
    obj->status.b = flips;
    obj->type = p[4];
    OBJECT_SUBTYPE(obj) = p[5];
    *entry += ENTRY_SIZE;
    return false;
}

static void Init(ObjectsManager *m, const ObjectsManagerConfig *config, uint8_t *marks, uint16_t mark_count, const uint8_t *layout, int16_t camera_x,
                 const ObjectsManager *partner) {
    static const uint8_t empty[ENTRY_SIZE] = {0xFF, 0xFF, 0, 0, 0, 0};
    memset(m, 0, sizeof(*m));
    m->config = *config;
    m->marks = marks;
    m->mark_count = mark_count;
    m->partner = partner;
    m->layout = layout != NULL ? layout : empty;
    m->load_right = m->load_left = m->layout;
    m->respawn_right = 1; // respawn indexes start at 1: 0 means "does not remember"
    m->respawn_left = 1;

    // d6: one chunk to the left of the camera, not below zero, in steps of 128
    uint16_t d6 = (uint16_t)camera_x;
    d6 = (d6 < (uint16_t)m->config.behind) ? 0 : (uint16_t)(d6 - (uint16_t)m->config.behind);
    d6 &= 0xFF80;

    // Objects behind that point are not loaded, but the ones that remember their state still take their respawn index
    const uint8_t *a0 = m->load_right;
    while (Word(a0) < d6) {
        if (Remembers(a0))
            m->respawn_right++;
        a0 += ENTRY_SIZE;
    }
    m->load_right = a0;
    m->gap_end = a0;

    // ... and further back, count the ones that are out of range on the left
    a0 = m->load_left;
    if (d6 >= (uint16_t)m->config.behind) {
        d6 = (uint16_t)(d6 - (uint16_t)m->config.behind);
        while (Word(a0) < d6) {
            if (Remembers(a0))
                m->respawn_left++;
            a0 += ENTRY_SIZE;
        }
    }
    m->load_left = a0;
    m->gap_start = a0;

    m->camera_x_last = -1; // makes sure the first Update goes forward
    ObjectsManager_Update(m, camera_x);
}

void ObjectsManager_Init(ObjectsManager *m, const ObjectsManagerConfig *config, uint8_t *marks, uint16_t mark_count, const uint8_t *layout, int16_t camera_x) {
    if (marks != NULL)
        memset(marks, 0, mark_count);
    Init(m, config, marks, mark_count, layout, camera_x, NULL);
}

void ObjectsManager_Update(ObjectsManager *m, int16_t camera_x) {
    const uint16_t behind = (uint16_t)m->config.behind;
    const uint16_t ahead = (uint16_t)m->config.ahead;

    m->camera_x_coarse = (int16_t)(((uint16_t)camera_x - behind) & 0xFF80);

    uint8_t d2 = 0;
    int16_t d6 = (int16_t)((uint16_t)camera_x & 0xFF80);
    if (d6 == m->camera_x_last)
        return; // the same 128-pixel column as last time

    if (d6 < m->camera_x_last) {
        // The camera is moving back
        m->camera_x_last = d6;

        const uint8_t *a0 = m->load_left;
        bool borrow = (uint16_t)d6 < behind;
        uint16_t u6 = (uint16_t)(d6 - behind); // a chunk to the left
        if (!borrow) {
            // Load all objects left of the screen that are now in range
            while (a0 > m->layout && (int16_t)u6 < (int16_t)Word(a0 - ENTRY_SIZE)) {
                a0 -= ENTRY_SIZE;
                if (Remembers(a0)) {
                    m->respawn_left--;
                    d2 = m->respawn_left;
                }
                if (ChkLoadObj(m, &a0, d2)) {
                    // The slots are full: undo the index, and go back to the last object
                    if (Remembers(a0))
                        m->respawn_left++;
                    a0 += ENTRY_SIZE;
                    break;
                }
                a0 -= ENTRY_SIZE;
            }
        }
        m->load_left = a0;

        // Objects past the right edge go out of range: take their respawn indexes back
        a0 = m->load_right;
        u6 = (uint16_t)(u6 + ahead + behind);
        while (a0 > m->layout && (int16_t)u6 <= (int16_t)Word(a0 - ENTRY_SIZE)) {
            if (Remembers(a0 - ENTRY_SIZE))
                m->respawn_right--;
            a0 -= ENTRY_SIZE;
        }
        m->load_right = a0;
        return;
    }

    // The camera is moving forward
    m->camera_x_last = d6;

    const uint8_t *a0 = m->load_right;
    uint16_t u6 = (uint16_t)((uint16_t)d6 + ahead);
    while (Word(a0) < u6) {
        // Load all objects right of the screen that are now in range
        if (Remembers(a0)) {
            d2 = m->respawn_right;
            m->respawn_right++;
        }
        if (ChkLoadObj(m, &a0, d2))
            break; // the slots are full
    }
    m->load_right = a0;

    // Objects far enough behind the left edge go out of range
    a0 = m->load_left;
    if (u6 >= (uint16_t)(ahead + behind)) {
        u6 = (uint16_t)(u6 - (uint16_t)(ahead + behind));
        while (Word(a0) < u6) {
            if (Remembers(a0))
                m->respawn_left++;
            a0 += ENTRY_SIZE;
        }
    }
    m->load_left = a0;
}

void ObjectsManager_Forget(ObjectsManager *m, const Object *obj) {
    if (obj->respawn_index != 0 && obj->respawn_index < m->mark_count)
        m->marks[obj->respawn_index] &= 0x7F;
}

// The partners point at each other's current state, so they are set again every time (a copied ObjectsManager2P then still works on its own views).
void ObjectsManager2P_Init(ObjectsManager2P *m, const ObjectsManagerConfig *config, uint8_t *marks, uint16_t mark_count, const uint8_t *layout, int16_t camera_x, int16_t camera_x_p2) {
    if (marks != NULL)
        memset(marks, 0, mark_count);
    Init(&m->view[0], config, marks, mark_count, layout, camera_x, NULL); // the first camera loads its window; nothing is held yet to leave out
    Init(&m->view[1], config, marks, mark_count, layout, camera_x_p2, &m->view[0]); // the second leaves out what the first holds
    m->view[0].partner = &m->view[1];
}

void ObjectsManager2P_Update(ObjectsManager2P *m, int16_t camera_x, int16_t camera_x_p2) {
    m->view[0].partner = &m->view[1];
    m->view[1].partner = &m->view[0];
    ObjectsManager_Update(&m->view[0], camera_x);
    ObjectsManager_Update(&m->view[1], camera_x_p2);
}

void ObjectsManager2P_Forget(ObjectsManager2P *m, const Object *obj) {
    ObjectsManager_Forget(&m->view[0], obj);
}
