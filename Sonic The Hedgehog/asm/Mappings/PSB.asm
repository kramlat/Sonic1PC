; ---------------------------------------------------------------------------
; Sprite mappings - "PRESS START BUTTON" and "TM" from title screen
; ---------------------------------------------------------------------------
; (converted to Sonic 2 mapping format: see _MapMacros.asm and engine/Mappings.h)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_PSB_internal:	mappingsTable
	mappingsTableEntry.w	Map_PSB_Frame00
	mappingsTableEntry.w	Map_PSB_Frame01
	mappingsTableEntry.w	Map_PSB_Frame02
	mappingsTableEntry.w	Map_PSB_Frame03

Map_PSB_Frame00:	spriteHeader
Map_PSB_Frame00_End

Map_PSB_Frame01:	spriteHeader
	spritePiece	0, 0, 4, 1, 240, 0, 0, 0, 0
	spritePiece	32, 0, 1, 1, 243, 0, 0, 0, 0
	spritePiece	48, 0, 1, 1, 243, 0, 0, 0, 0
	spritePiece	56, 0, 4, 1, 244, 0, 0, 0, 0
	spritePiece	96, 0, 3, 1, 248, 0, 0, 0, 0
	spritePiece	120, 0, 3, 1, 251, 0, 0, 0, 0
Map_PSB_Frame01_End

Map_PSB_Frame02:	spriteHeader
	spritePiece	-128, -72, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -72, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -72, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -72, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -72, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -72, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -72, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -72, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -72, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -72, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -40, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -40, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -40, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -40, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -40, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -40, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -40, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -40, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -40, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -40, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -8, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -8, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -8, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -8, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -8, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -8, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -8, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -8, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -8, 4, 4, 0, 0, 0, 0, 0
	spritePiece	-128, -8, 4, 4, 0, 0, 0, 0, 0
Map_PSB_Frame02_End

Map_PSB_Frame03:	spriteHeader
	spritePiece	-8, -4, 2, 1, 0, 0, 0, 0, 0
Map_PSB_Frame03_End

