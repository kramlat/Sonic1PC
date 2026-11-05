; ---------------------------------------------------------------------------
; Sprite mappings - solid blocks and blocks that fall from the ceiling (MZ)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacrosS3.asm"

Map_Brick_internal:	mappingsTable
	mappingsTableEntry.w	@brick

@brick:	spriteHeader
	spritePiece	-$10, -$10, 4, 4, 1, 0, 0, 0, 0
@brick_End

	even
