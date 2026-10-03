// Clownacy's implementation

#include "EnginePLC.h"

#include "EngineConstants.h"
#include "GameInterface.h"
#include "Nemesis.h"

#include "Backend/VDP.h"

#include <string.h>

// PLC constants
#define PLC_SPEED_1 9 // How many tiles are loaded per frame during a 'loading' state
#define PLC_SPEED_2 3 // How many tiles are loaded per frame while the game's running

// PLC state
PLC plc_buffer[32]; // (the original has 16: see GameInfo.plc_capacity for how many are used)

static NemesisState plc_buffer_regs;
static uint16_t plc_buffer_reg18;
static uint16_t plc_buffer_reg1A;

// PLC interface
void AddPLC(PlcId plc) {
    // Get PLC list to load
    const PLCList* list = plcs[plc];
    if (list == NULL)
        return;

    // Find empty PLC slot
    const size_t capacity = game_info.plc_capacity ? game_info.plc_capacity : 16;
    PLC* const plc_end = plc_buffer + (capacity < sizeof(plc_buffer) / sizeof(*plc_buffer) ? capacity : sizeof(plc_buffer) / sizeof(*plc_buffer));
    PLC* plc_free = plc_buffer;
    while (plc_free < plc_end && plc_free->art != NULL)
        plc_free++;

    // Push PLCs to buffer
    for (size_t i = 0; i < list->plcs && plc_free + i < plc_end; i++)
        plc_free[i] = list->plc[i];
}

void NewPLC(PlcId plc) {
    // Get PLC list to load
    const PLCList* list = plcs[plc];
    if (list == NULL)
        return;

    // Clear previous PLCs
    ClearPLC();

    // Push PLCs to buffer
    for (size_t i = 0; i < list->plcs; i++)
        plc_buffer[i] = list->plc[i];
}

void ClearPLC(void) {
    // Clear PLC buffer
    plc_buffer_reg18 = 0;
    memset(plc_buffer, 0, sizeof(plc_buffer));
}

void RunPLC(void) {
    if (plc_buffer[0].art != NULL && plc_buffer_reg18 == 0) {
        plc_buffer_regs.source = plc_buffer[0].art;
        plc_buffer_regs.vram_mode = true;
        plc_buffer_regs.dictionary = nemesis_buffer;

        uint16_t header = (plc_buffer_regs.source[0] << 8) | plc_buffer_regs.source[1];

        plc_buffer_regs.source += 2;
        plc_buffer_regs.xor_mode = header & 0x8000;
        plc_buffer_reg18 = header & 0x7FFF;

        NemDecPrepare(&plc_buffer_regs);

        plc_buffer_regs.d5 = (plc_buffer_regs.source[0] << 8) | plc_buffer_regs.source[1];
        plc_buffer_regs.source += 2;

        plc_buffer_regs.d0 = 0;
        plc_buffer_regs.d1 = 0;
        plc_buffer_regs.d2 = 0;
        plc_buffer_regs.d6 = 0x10;
    }
}

static void ProcessDPLC_Main(size_t off) {
    VDP_SeekVRAM(off);

    do {
        plc_buffer_regs.remaining = 8;

        // Inlined NemDec_WriteIter
        plc_buffer_regs.d3 = 8;
        plc_buffer_regs.d4 = 0;

        NemDecRun(&plc_buffer_regs);

        if (--plc_buffer_reg18 == 0) {
            // Pop one request off the buffer so that the next one can be filled
            for (size_t i = 0; i < sizeof(plc_buffer) / sizeof(*plc_buffer) - 1; i++)
                plc_buffer[i] = plc_buffer[i + 1];
            plc_buffer[sizeof(plc_buffer) / sizeof(*plc_buffer) - 1] = (PLC){0};
            return;
        }
    } while (--plc_buffer_reg1A != 0);
}

void ProcessDPLC(void) {
    if (plc_buffer_reg18 != 0) {
        plc_buffer_reg1A = PLC_SPEED_1; // Process PLC_SPEED_1 tiles

        size_t off = plc_buffer[0].off;
        plc_buffer[0].off += PLC_SPEED_1 * 0x20;

        ProcessDPLC_Main(off);
    }
}

void ProcessDPLC2(void) {
    if (plc_buffer_reg18 != 0) {
        plc_buffer_reg1A = PLC_SPEED_2; // Process PLC_SPEED_2 tiles

        size_t off = plc_buffer[0].off;
        plc_buffer[0].off += PLC_SPEED_2 * 0x20;

        ProcessDPLC_Main(off);
    }
}

void QuickPLC(PlcId plc) {
    // Get PLC list to load and decompress immediately
    const PLCList* list = plcs[plc];
    if (list == NULL)
        return;
    for (size_t i = 0; i < list->plcs; i++) {
        VDP_SeekVRAM(list->plc[i].off);
        NemDec(list->plc[i].art);
    }
}
