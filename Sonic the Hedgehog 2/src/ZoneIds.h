#ifndef _ZONEIDS_H
#define _ZONEIDS_H

// Sonic 2's 17 zone slots, in the order of the Simon Wai prototype (its zone ids $00-$10), which Sonic 1's Level.h includes when ZONE_SLOTS is set. The names are those of the prototype's time and are not
// final (the empty ones are slots the prototype leaves unused or empty). The slots of the zones that have been built so far: EHZ, HTZ, HPZ and CPZ.
typedef enum {
	ZoneId_EHZ = 0x00,
	ZoneId_Empty01,
	ZoneId_WZ = 0x02,
	ZoneId_SSZ = 0x03, // Sand Shower (the final game's data: see the importer)
	ZoneId_MTZ = 0x04,
	ZoneId_Empty05, // (the prototype's Metropolis third act: it is the third act of Metropolis' slot now)
	ZoneId_Empty06,
	ZoneId_HTZ = 0x07,
	ZoneId_HPZ = 0x08,
	ZoneId_Empty09,
	ZoneId_OOZ = 0x0A,
	ZoneId_MCZ = 0x0B,
	ZoneId_CNZ = 0x0C,
	ZoneId_CPZ = 0x0D,
	ZoneId_GCZ = 0x0E, // the alpha's Genocide City slot: Cyber City here (the final game's data: see the importer)
	ZoneId_ARZ = 0x0F,
	ZoneId_Empty10,
	ZoneId_Num, // 17

	// Sonic 1's names, for the Sonic 1 code that Sonic 2 still shares (its zone checks): none of them is a zone here, so they are empty slots and what they test for never happens
	ZoneId_GHZ = ZoneId_Empty01,
	ZoneId_LZ = ZoneId_Empty10,
	ZoneId_MZ = ZoneId_Empty06,
	ZoneId_SLZ = ZoneId_Empty09,
	ZoneId_SYZ = ZoneId_Empty10,
	ZoneId_SBZ = ZoneId_Empty10,
	ZoneId_EndZ = ZoneId_Empty10,
	ZoneId_SS = ZoneId_Empty10,
} ZoneId;

#endif //_ZONEIDS_H
