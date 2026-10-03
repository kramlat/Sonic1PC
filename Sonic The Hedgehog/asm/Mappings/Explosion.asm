; ---------------------------------------------------------------------------
; Sprite mappings - explosion from a badnik or monitor
; ---------------------------------------------------------------------------
; (converted to Sonic 2 mapping format: see _MapMacros.asm and engine/Mappings.h)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_ExplodeItem_internal:	mappingsTable
	mappingsTableEntry.w	Map_ExplodeItem_Frame00
	mappingsTableEntry.w	Map_ExplodeItem_Frame01
	mappingsTableEntry.w	Map_ExplodeItem_Frame02
	mappingsTableEntry.w	Map_ExplodeItem_Frame03
	mappingsTableEntry.w	Map_ExplodeItem_Frame04

Map_ExplodeItem_Frame00:	spriteHeader
	spritePiece	-12, -8, 3, 2, 0, 0, 0, 0, 0
Map_ExplodeItem_Frame00_End

Map_ExplodeItem_Frame01:	spriteHeader
	spritePiece	-16, -16, 4, 4, 6, 0, 0, 0, 0
Map_ExplodeItem_Frame01_End

Map_ExplodeItem_Frame02:	spriteHeader
	spritePiece	-16, -16, 4, 4, 22, 0, 0, 0, 0
Map_ExplodeItem_Frame02_End

Map_ExplodeItem_Frame03:	spriteHeader
	spritePiece	-20, -20, 3, 3, 38, 0, 0, 0, 0
	spritePiece	4, -20, 2, 2, 47, 0, 0, 0, 0
	spritePiece	-20, 4, 2, 2, 47, 1, 1, 0, 0
	spritePiece	-4, -4, 3, 3, 38, 1, 1, 0, 0
Map_ExplodeItem_Frame03_End

Map_ExplodeItem_Frame04:	spriteHeader
	spritePiece	-20, -20, 3, 3, 51, 0, 0, 0, 0
	spritePiece	4, -20, 2, 2, 60, 0, 0, 0, 0
	spritePiece	-20, 4, 2, 2, 60, 1, 1, 0, 0
	spritePiece	-4, -4, 3, 3, 51, 1, 1, 0, 0
Map_ExplodeItem_Frame04_End

