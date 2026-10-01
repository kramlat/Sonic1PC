; ---------------------------------------------------------------------------
; Sprite mappings - SLZ	lava thrower
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_Scen_internal:	mappingsTable
	mappingsTableEntry.w	byte_8858

byte_8858:	spriteHeader
	spritePiece	-8, -$10, 2, 4, 0, 0, 0, 0, 0
byte_8858_End

	even
