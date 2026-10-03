; ---------------------------------------------------------------------------
; Sprite mappings - waterfall SFX trigger debug marker (2x2 tight cluster,
; goggles icon -- swimming/water association -- instead of InvisibleBarrier's
; Eggman icon). Same tight layout as Map_Invis's own .solid frame, just a
; different relative tile ($20, the Goggles monitor content icon).
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_WaterfallMarker_internal:	mappingsTable
	mappingsTableEntry.w	@solid

@solid:	spriteHeader
	spritePiece	-$10, -$10, 2, 2, $20, 0, 0, 0, 0
	spritePiece	0, -$10, 2, 2, $20, 0, 0, 0, 0
	spritePiece	-$10, 0, 2, 2, $20, 0, 0, 0, 0
	spritePiece	0, 0, 2, 2, $20, 0, 0, 0, 0
@solid_End

	even
