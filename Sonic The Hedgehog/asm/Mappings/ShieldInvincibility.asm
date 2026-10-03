; ---------------------------------------------------------------------------
; Sprite mappings - shield and invincibility stars
; ---------------------------------------------------------------------------
; (converted to Sonic 2 mapping format: see _MapMacros.asm and engine/Mappings.h)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_Shield_internal:	mappingsTable
	mappingsTableEntry.w	Map_Shield_Frame00
	mappingsTableEntry.w	Map_Shield_Frame01
	mappingsTableEntry.w	Map_Shield_Frame02
	mappingsTableEntry.w	Map_Shield_Frame03
	mappingsTableEntry.w	Map_Shield_Frame04
	mappingsTableEntry.w	Map_Shield_Frame05
	mappingsTableEntry.w	Map_Shield_Frame06
	mappingsTableEntry.w	Map_Shield_Frame07

Map_Shield_Frame00:	spriteHeader
Map_Shield_Frame00_End

Map_Shield_Frame01:	spriteHeader
	spritePiece	-24, -24, 3, 3, 0, 0, 0, 0, 0
	spritePiece	0, -24, 3, 3, 9, 0, 0, 0, 0
	spritePiece	-24, 0, 3, 3, 0, 0, 1, 0, 0
	spritePiece	0, 0, 3, 3, 9, 0, 1, 0, 0
Map_Shield_Frame01_End

Map_Shield_Frame02:	spriteHeader
	spritePiece	-23, -24, 3, 3, 18, 1, 0, 0, 0
	spritePiece	0, -24, 3, 3, 18, 0, 0, 0, 0
	spritePiece	-23, 0, 3, 3, 18, 1, 1, 0, 0
	spritePiece	0, 0, 3, 3, 18, 0, 1, 0, 0
Map_Shield_Frame02_End

Map_Shield_Frame03:	spriteHeader
	spritePiece	-24, -24, 3, 3, 9, 1, 0, 0, 0
	spritePiece	0, -24, 3, 3, 0, 1, 0, 0, 0
	spritePiece	-24, 0, 3, 3, 9, 1, 1, 0, 0
	spritePiece	0, 0, 3, 3, 0, 1, 1, 0, 0
Map_Shield_Frame03_End

Map_Shield_Frame04:	spriteHeader
	spritePiece	-24, -24, 3, 3, 0, 0, 0, 0, 0
	spritePiece	0, -24, 3, 3, 9, 0, 0, 0, 0
	spritePiece	-24, 0, 3, 3, 9, 1, 1, 0, 0
	spritePiece	0, 0, 3, 3, 0, 1, 1, 0, 0
Map_Shield_Frame04_End

Map_Shield_Frame05:	spriteHeader
	spritePiece	-24, -24, 3, 3, 9, 1, 0, 0, 0
	spritePiece	0, -24, 3, 3, 0, 1, 0, 0, 0
	spritePiece	-24, 0, 3, 3, 0, 0, 1, 0, 0
	spritePiece	0, 0, 3, 3, 9, 0, 1, 0, 0
Map_Shield_Frame05_End

Map_Shield_Frame06:	spriteHeader
	spritePiece	-24, -24, 3, 3, 18, 0, 0, 0, 0
	spritePiece	0, -24, 3, 3, 27, 0, 0, 0, 0
	spritePiece	-24, 0, 3, 3, 27, 1, 1, 0, 0
	spritePiece	0, 0, 3, 3, 18, 1, 1, 0, 0
Map_Shield_Frame06_End

Map_Shield_Frame07:	spriteHeader
	spritePiece	-24, -24, 3, 3, 27, 1, 0, 0, 0
	spritePiece	0, -24, 3, 3, 18, 1, 0, 0, 0
	spritePiece	-24, 0, 3, 3, 18, 0, 1, 0, 0
	spritePiece	0, 0, 3, 3, 27, 0, 1, 0, 0
Map_Shield_Frame07_End

