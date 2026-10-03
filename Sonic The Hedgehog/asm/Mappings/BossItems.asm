; ---------------------------------------------------------------------------
; Sprite mappings - extra boss items (e.g. swinging ball on a chain in GHZ)
; ---------------------------------------------------------------------------
; (converted to Sonic 2 mapping format: see _MapMacros.asm and engine/Mappings.h)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_BossItems_internal:	mappingsTable
	mappingsTableEntry.w	Map_BossItems_Frame00
	mappingsTableEntry.w	Map_BossItems_Frame01
	mappingsTableEntry.w	Map_BossItems_Frame02
	mappingsTableEntry.w	Map_BossItems_Frame03
	mappingsTableEntry.w	Map_BossItems_Frame04
	mappingsTableEntry.w	Map_BossItems_Frame05
	mappingsTableEntry.w	Map_BossItems_Frame06
	mappingsTableEntry.w	Map_BossItems_Frame07

Map_BossItems_Frame00:	spriteHeader
	spritePiece	-8, -8, 2, 2, 0, 0, 0, 0, 0
Map_BossItems_Frame00_End

Map_BossItems_Frame01:	spriteHeader
	spritePiece	-8, -4, 2, 1, 4, 0, 0, 0, 0
	spritePiece	-8, -8, 2, 2, 0, 0, 0, 0, 0
Map_BossItems_Frame01_End

Map_BossItems_Frame02:	spriteHeader
	spritePiece	-4, -4, 1, 1, 6, 0, 0, 0, 0
Map_BossItems_Frame02_End

Map_BossItems_Frame03:	spriteHeader
	spritePiece	-12, 20, 3, 2, 7, 0, 0, 0, 0
Map_BossItems_Frame03_End

Map_BossItems_Frame04:	spriteHeader
	spritePiece	-8, 20, 2, 2, 13, 0, 0, 0, 0
Map_BossItems_Frame04_End

Map_BossItems_Frame05:	spriteHeader
	spritePiece	-8, -16, 2, 1, 17, 0, 0, 0, 0
	spritePiece	-8, -8, 1, 2, 19, 0, 0, 0, 0
	spritePiece	0, -8, 1, 2, 19, 1, 0, 0, 0
	spritePiece	-8, 8, 2, 1, 21, 0, 0, 0, 0
Map_BossItems_Frame05_End

Map_BossItems_Frame06:	spriteHeader
	spritePiece	0, 0, 2, 2, 23, 0, 0, 0, 0
	spritePiece	16, 0, 1, 1, 27, 0, 0, 0, 0
Map_BossItems_Frame06_End

Map_BossItems_Frame07:	spriteHeader
	spritePiece	0, 24, 2, 1, 28, 0, 0, 0, 0
	spritePiece	16, 0, 3, 4, 30, 0, 0, 0, 0
Map_BossItems_Frame07_End

