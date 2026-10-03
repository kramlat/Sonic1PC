; ---------------------------------------------------------------------------
; Sprite mappings - moving block (LZ)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_MBlockLZ_internal:	mappingsTable
	mappingsTableEntry.w	@0

@0:	spriteHeader
	spritePiece	-$10, -8, 4, 2, 0, 0, 0, 0, 0
@0_End

	even
