; ---------------------------------------------------------------------------
; Sprite mappings - chain of spiked balls (SYZ)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_SBall_internal:	mappingsTable
	mappingsTableEntry.w	@0

@0:	spriteHeader
	spritePiece	-8, -8, 2, 2, 0, 0, 0, 0, 0
@0_End

	even
