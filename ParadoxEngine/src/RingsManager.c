#include "RingsManager.h"

#include <string.h>

// The window logic is the ObjectsManager's (Sonic 2's rings manager is its twin), on 4-byte entries and with a callback instead of an object slot. There are no
// respawn indexes: an entry's index is where it sits in the layout.

#define ENTRY_SIZE 4

static uint16_t Word(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void LoadEntry(RingsManager *m, const uint8_t *entry, RingSpawnFunc spawn, void *user) {
    const uint16_t y = Word(entry + 2);
    const uint16_t entry_index = (uint16_t)((entry - m->layout) / ENTRY_SIZE);
    const int count = ((y >> 12) & 7) + 1;
    const bool vertical = (y & 0x8000) != 0;
    for (int k = 0; k < count; k++) {
        if (RingsManager_Collected(m, entry_index, (uint8_t)k))
            continue;
        RingSpawn ring;
        ring.x = (int16_t)(Word(entry) + (vertical ? 0 : k * RING_SPACING));
        ring.y = (int16_t)((y & 0xFFF) + (vertical ? k * RING_SPACING : 0));
        ring.base_x = (int16_t)Word(entry);
        ring.entry = entry_index;
        ring.bit = (uint8_t)k;
        spawn(user, &ring);
    }
}

void RingsManager_Init(RingsManager *m, const ObjectsManagerConfig *config, uint8_t *status, uint16_t status_count, const uint8_t *layout, int16_t camera_x,
                       RingSpawnFunc spawn, void *user) {
    static const uint8_t empty[ENTRY_SIZE] = {0xFF, 0xFF, 0, 0};
    memset(m, 0, sizeof(*m));
    m->config = *config;
    m->status = status;
    m->status_count = status_count;
    if (status != NULL)
        memset(status, 0, status_count);
    m->layout = layout != NULL ? layout : empty;
    m->load_right = m->load_left = m->layout;

    // One chunk to the left of the camera, not below zero, in steps of 128: entries behind it are not loaded
    uint16_t d6 = (uint16_t)camera_x;
    d6 = (d6 < (uint16_t)m->config.behind) ? 0 : (uint16_t)(d6 - (uint16_t)m->config.behind);
    d6 &= 0xFF80;
    const uint8_t *a0 = m->load_right;
    while (Word(a0) < d6)
        a0 += ENTRY_SIZE;
    m->load_right = a0;

    a0 = m->load_left;
    if (d6 >= (uint16_t)m->config.behind) {
        d6 = (uint16_t)(d6 - (uint16_t)m->config.behind);
        while (Word(a0) < d6)
            a0 += ENTRY_SIZE;
    }
    m->load_left = a0;

    m->camera_x_last = -1; // makes sure the first Update goes forward
    RingsManager_Update(m, camera_x, spawn, user);
}

void RingsManager_Update(RingsManager *m, int16_t camera_x, RingSpawnFunc spawn, void *user) {
    const uint16_t behind = (uint16_t)m->config.behind;
    const uint16_t ahead = (uint16_t)m->config.ahead;

    m->camera_x_coarse = (int16_t)(((uint16_t)camera_x - behind) & 0xFF80);

    int16_t d6 = (int16_t)((uint16_t)camera_x & 0xFF80);
    if (d6 == m->camera_x_last)
        return; // the same 128-pixel column as last time

    if (d6 < m->camera_x_last) {
        // The camera is moving back: load the entries now in range on the left, let go of the ones past the right edge
        m->camera_x_last = d6;

        const uint8_t *a0 = m->load_left;
        const bool borrow = (uint16_t)d6 < behind;
        uint16_t u6 = (uint16_t)(d6 - behind);
        if (!borrow) {
            while (a0 > m->layout && (int16_t)u6 < (int16_t)Word(a0 - ENTRY_SIZE)) {
                a0 -= ENTRY_SIZE;
                LoadEntry(m, a0, spawn, user);
            }
        }
        m->load_left = a0;

        a0 = m->load_right;
        u6 = (uint16_t)(u6 + ahead + behind);
        while (a0 > m->layout && (int16_t)u6 <= (int16_t)Word(a0 - ENTRY_SIZE))
            a0 -= ENTRY_SIZE;
        m->load_right = a0;
        return;
    }

    // The camera is moving forward
    m->camera_x_last = d6;

    const uint8_t *a0 = m->load_right;
    uint16_t u6 = (uint16_t)((uint16_t)d6 + ahead);
    while (Word(a0) < u6) {
        LoadEntry(m, a0, spawn, user);
        a0 += ENTRY_SIZE;
    }
    m->load_right = a0;

    a0 = m->load_left;
    if (u6 >= (uint16_t)(ahead + behind)) {
        u6 = (uint16_t)(u6 - (uint16_t)(ahead + behind));
        while (Word(a0) < u6)
            a0 += ENTRY_SIZE;
    }
    m->load_left = a0;
}

bool RingsManager_Collected(const RingsManager *m, uint16_t entry, uint8_t bit) {
    return m->status != NULL && entry < m->status_count && (m->status[entry] & (1u << bit)) != 0;
}

void RingsManager_Collect(RingsManager *m, uint16_t entry, uint8_t bit) {
    if (m->status != NULL && entry < m->status_count)
        m->status[entry] |= (uint8_t)(1u << bit);
}
