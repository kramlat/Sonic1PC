#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Kosinski Moduled archives, the Sonic 3 & Knuckles way of loading art (sonic3k.asm: Queue_Kos_Module, Process_Kos_Module_Queue_Init, Process_Kos_Module_Queue).
//
// An archive is a big-endian word with the uncompressed size in bytes (a size of $A000 is read as $8000, as the original does), then the modules: every $1000 bytes of the data
// Kosinski compressed on its own (the last module holds what is left), each module padded to a multiple of 16 bytes counted from its own start. The modules can be decompressed
// one at a time, so the art streams into the tile space a module a frame (ProcessDPLC calls ProcessKosinskiModuleQueue) instead of stopping the game to decompress it all.
//
// Up to KOSM_QUEUE_SIZE archives are queued and worked through in order. The destination is a byte offset in the tile space (tile number * 32). A level's secondary art goes where the
// primary's ends: that is destination + KosinskiModuleSize(primary).

#define KOSM_MODULE_SIZE 0x1000
#define KOSM_QUEUE_SIZE 4

// Queues an archive for the tile space at byte offset `destination`. False if the queue is full (or the archive is empty).
bool QueueKosinskiModule(const uint8_t *archive, size_t destination);

// Decompresses one module of the first archive and writes it to the tile space; call it once a frame. Nothing happens when the queue is empty.
void ProcessKosinskiModuleQueue(void);

bool KosinskiModuleQueueBusy(void);  // an archive is still being loaded
void KosinskiModuleQueueWait(void);  // works through the whole queue now (a level that must have its art before it starts)
void KosinskiModuleQueueClear(void); // drops everything that is queued

size_t KosinskiModuleSize(const uint8_t *archive); // the archive's uncompressed size
// All of an archive in one go, into RAM (tools and tests); returns the number of bytes written
size_t KosinskiModuleDecompress(const uint8_t *archive, void *destination);
