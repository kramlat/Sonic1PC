; ---------------------------------------------------------------------------
; Sprite mappings - spiked balls on the	seesaws	(SLZ)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_SSawBall_internal:	mappingsTable
	mappingsTableEntry.w	@red
	mappingsTableEntry.w	@silver

@red:	spriteHeader
	spritePiece	-$C, -$C, 3, 3, 0, 0, 0, 0, 0
@red_End

@silver:	spriteHeader
	spritePiece	-$C, -$C, 3, 3, 9, 0, 0, 0, 0
@silver_End

	even
