; ---------------------------------------------------------------------------
; Sprite mappings - invisible lava tag (MZ)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_LTag_internal:	mappingsTable
	mappingsTableEntry.w	@0

@0:	spriteHeader	; no sprite, because the tag is invisible!
@0_End

	even
