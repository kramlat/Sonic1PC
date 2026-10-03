; ---------------------------------------------------------------------------
; Sprite mappings - helix of spikes on a pole (GHZ)
; ---------------------------------------------------------------------------
; (converted to Sonic 2 mapping format: see _MapMacros.asm and engine/Mappings.h)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_Hel_internal:	mappingsTable
	mappingsTableEntry.w	Map_Hel_Frame00
	mappingsTableEntry.w	Map_Hel_Frame01
	mappingsTableEntry.w	Map_Hel_Frame02
	mappingsTableEntry.w	Map_Hel_Frame03
	mappingsTableEntry.w	Map_Hel_Frame04
	mappingsTableEntry.w	Map_Hel_Frame05
	mappingsTableEntry.w	Map_Hel_Frame06
	mappingsTableEntry.w	Map_Hel_Frame07

Map_Hel_Frame00:	spriteHeader
	spritePiece	-4, -16, 1, 2, 0, 0, 0, 0, 0
Map_Hel_Frame00_End

Map_Hel_Frame01:	spriteHeader
	spritePiece	-8, -11, 2, 2, 2, 0, 0, 0, 0
Map_Hel_Frame01_End

Map_Hel_Frame02:	spriteHeader
	spritePiece	-8, -8, 2, 2, 6, 0, 0, 0, 0
Map_Hel_Frame02_End

Map_Hel_Frame03:	spriteHeader
	spritePiece	-8, -5, 2, 2, 10, 0, 0, 0, 0
Map_Hel_Frame03_End

Map_Hel_Frame04:	spriteHeader
	spritePiece	-4, 0, 1, 2, 14, 0, 0, 0, 0
Map_Hel_Frame04_End

Map_Hel_Frame05:	spriteHeader
	spritePiece	-3, 4, 1, 1, 16, 0, 0, 0, 0
Map_Hel_Frame05_End

Map_Hel_Frame06:	spriteHeader
Map_Hel_Frame06_End

Map_Hel_Frame07:	spriteHeader
	spritePiece	-3, -12, 1, 1, 17, 0, 0, 0, 0
Map_Hel_Frame07_End

