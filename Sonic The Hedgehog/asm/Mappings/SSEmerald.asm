; ---------------------------------------------------------------------------
; Sprite mappings - special stage chaos	emeralds
; ---------------------------------------------------------------------------
; (converted to Sonic 2 mapping format: see _MapMacros.asm and engine/Mappings.h)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_SS_Chaos1:	mappingsTable
	mappingsTableEntry.w	Chaos_Frame0
	mappingsTableEntry.w	Chaos_Frame1
Map_SS_Chaos2:	mappingsTable
	mappingsTableEntry.w	Chaos_Frame2
	mappingsTableEntry.w	Chaos_Frame1
Map_SS_Chaos3:	mappingsTable
	mappingsTableEntry.w	Chaos_Frame3
	mappingsTableEntry.w	Chaos_Frame1

Chaos_Frame0:	spriteHeader
	spritePiece	-8, -8, 2, 2, 0, 0, 0, 0, 0
Chaos_Frame0_End

Chaos_Frame1:	spriteHeader
	spritePiece	-8, -8, 2, 2, 12, 0, 0, 0, 0
Chaos_Frame1_End

Chaos_Frame2:	spriteHeader
	spritePiece	-8, -8, 2, 2, 4, 0, 0, 0, 0
Chaos_Frame2_End

Chaos_Frame3:	spriteHeader
	spritePiece	-8, -8, 2, 2, 8, 0, 0, 0, 0
Chaos_Frame3_End

