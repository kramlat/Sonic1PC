; ---------------------------------------------------------------------------
; Sprite mappings - Roller enemy (SYZ)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_Roll_internal:	mappingsTable
	mappingsTableEntry.w	@stand
	mappingsTableEntry.w	@fold
	mappingsTableEntry.w	@roll1
	mappingsTableEntry.w	@roll2
	mappingsTableEntry.w	@roll3

@stand:	spriteHeader
	spritePiece	-$10, -$22, 4, 3, 0, 0, 0, 0, 0
	spritePiece	-$10, -$A, 4, 3, $C, 0, 0, 0, 0
@stand_End

@fold:	spriteHeader
	spritePiece	-$10, -$1A, 4, 3, 0, 0, 0, 0, 0
	spritePiece	-$10, -2, 4, 2, $18, 0, 0, 0, 0
@fold_End

@roll1:	spriteHeader
	spritePiece	-$10, -$10, 4, 4, $20, 0, 0, 0, 0
@roll1_End

@roll2:	spriteHeader
	spritePiece	-$10, -$10, 4, 4, $30, 0, 0, 0, 0
@roll2_End

@roll3:	spriteHeader
	spritePiece	-$10, -$10, 4, 4, $40, 0, 0, 0, 0
@roll3_End

	even
