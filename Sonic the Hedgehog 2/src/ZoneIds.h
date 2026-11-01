#ifndef _ZONEIDS_H
#define _ZONEIDS_H

// Sonic 2's 18 zone slots (the prototype's 17, $00-$10, and one for Sonic 1's names), in the order of the Simon Wai prototype (its zone ids $00-$10), which Sonic 1's Level.h includes when ZONE_SLOTS is set. The names are those of the prototype's time and are not
// final (the empty ones are slots the prototype leaves unused or empty). The slots of the zones that have been built so far: EHZ, HTZ, HPZ and CPZ.
typedef enum {
	ZoneId_EHZ = 0x00,
	ZoneId_Empty01,
	ZoneId_WZ = 0x02,
	ZoneId_SSZ = 0x03, // Sand Shower (the final game's data: see the importer)
	ZoneId_MTZ = 0x04,
	ZoneId_Empty05, // (the prototype's Metropolis third act: it is the third act of Metropolis' slot now)
	ZoneId_SFZ = 0x06, // Sky Fortress (Wing Fortress: the pre-beta's data, see the importer); the slot Sonic 1's Marble Zone name pointed at, which points at the empty slot $0E now
	ZoneId_HTZ = 0x07,
	ZoneId_HPZ = 0x08,
	ZoneId_GCZ = 0x09, // Cyber City (the final game's data: see the importer); it was the alpha's Genocide City slot, $0E, and moved to leave the slots after Neo Green Hill for the zones that are yet to be ported
	ZoneId_OOZ = 0x0A,
	ZoneId_MCZ = 0x0B,
	ZoneId_CNZ = 0x0C,
	ZoneId_CPZ = 0x0D,
	ZoneId_DEZ = 0x0E, // Death Egg (the pre-beta's data, which shares Chemical Plant's tiles: see the importer); it was Cyber City's slot in the alpha
	ZoneId_ARZ = 0x0F,
	ZoneId_SCZ = 0x10, // Sky Chase (the prototype's Death Egg slot: the menu's text says so): no data yet
	ZoneId_Empty11,    // the slot Sonic 1's names below point at
	ZoneId_Num, // 18

	// Sonic 1's names, for the Sonic 1 code that Sonic 2 still shares (its zone checks): none of them is a zone here, so they are empty slots and what they test for never happens (they must stay clear of every
	// zone that is built, or that code would take it for a Sonic 1 zone: Sky Chase and Cyber City had them before they moved)
	ZoneId_GHZ = ZoneId_Empty01,
	ZoneId_LZ = ZoneId_Empty11,
	ZoneId_MZ = ZoneId_Empty11,
	ZoneId_SLZ = ZoneId_Empty05,
	ZoneId_SYZ = ZoneId_Empty11,
	ZoneId_SBZ = ZoneId_Empty11,
	ZoneId_EndZ = ZoneId_Empty11,
	ZoneId_SS = ZoneId_Empty11,
} ZoneId;

#endif //_ZONEIDS_H
