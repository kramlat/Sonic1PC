#include "KosinskiModule.h"

#include <string.h>

#include "Kosinski.h"
#ifndef KOSM_NO_VDP // (the kosm tool is built without the video code: it only needs the codec)
#include "Backend/VDP.h"
#endif

size_t KosinskiModuleSize(const uint8_t *archive) {
    const size_t size = (size_t)((archive[0] << 8) | archive[1]);
    return size == 0xA000 ? 0x8000 : size; // ($A000 means $8000 "for some reason": the original's own note)
}

// Decompresses the module at `source` into `buffer`; returns where the next module starts (compressed length rounded up to 16 bytes from `source`)
static const uint8_t *DecompressModule(const uint8_t *source, uint8_t *buffer) {
    KosDec(source, buffer);
    const size_t used = (size_t)(KosDec_SourceEnd() - source);
    return source + ((used + 15) & ~(size_t)15);
}

#ifndef KOSM_NO_VDP
typedef struct {
    const uint8_t *next;   // the next module's compressed data
    size_t destination;    // where the next module goes (a byte offset in the tile space)
    size_t bytes_left;     // of the archive's data
} KosmJob;

static KosmJob queue[KOSM_QUEUE_SIZE];
static int queue_count;

bool QueueKosinskiModule(const uint8_t *archive, size_t destination) {
    if (archive == NULL || queue_count >= KOSM_QUEUE_SIZE)
        return false;
    const size_t size = KosinskiModuleSize(archive);
    if (size == 0)
        return false;
    queue[queue_count].next = archive + 2;
    queue[queue_count].destination = destination;
    queue[queue_count].bytes_left = size;
    queue_count++;
    return true;
}

void ProcessKosinskiModuleQueue(void) {
    if (queue_count == 0)
        return;
    static uint8_t buffer[KOSM_MODULE_SIZE * 2]; // (room to spare: a damaged module must not run off the end)
    KosmJob *job = &queue[0];
    job->next = DecompressModule(job->next, buffer);
    const size_t bytes = job->bytes_left < KOSM_MODULE_SIZE ? job->bytes_left : KOSM_MODULE_SIZE;
    VDP_SeekVRAM(job->destination);
    VDP_WriteVRAM(buffer, bytes);
    job->destination += bytes;
    job->bytes_left -= bytes;
    if (job->bytes_left == 0) { // (the archive is done: the next one moves up)
        memmove(&queue[0], &queue[1], sizeof(KosmJob) * (size_t)(queue_count - 1));
        queue_count--;
    }
}

bool KosinskiModuleQueueBusy(void) { return queue_count != 0; }

void KosinskiModuleQueueWait(void) {
    while (queue_count != 0)
        ProcessKosinskiModuleQueue();
}

void KosinskiModuleQueueClear(void) { queue_count = 0; }
#endif

size_t KosinskiModuleDecompress(const uint8_t *archive, void *destination) {
    static uint8_t buffer[KOSM_MODULE_SIZE * 2];
    uint8_t *out = destination;
    size_t left = KosinskiModuleSize(archive);
    const size_t total = left;
    const uint8_t *next = archive + 2;
    while (left != 0) {
        next = DecompressModule(next, buffer);
        const size_t bytes = left < KOSM_MODULE_SIZE ? left : KOSM_MODULE_SIZE;
        memcpy(out, buffer, bytes);
        out += bytes;
        left -= bytes;
    }
    return total;
}
