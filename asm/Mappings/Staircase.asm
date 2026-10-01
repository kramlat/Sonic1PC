; ---------------------------------------------------------------------------
; Sprite mappings - blocks that form a staircase (SLZ)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_Stair_internal:	mappingsTable
	mappingsTableEntry.w	@block

@block:	spriteHeader
	spritePiece	-$10, -$10, 4, 4, $21, 0, 0, 0, 0
@block_End

	even
