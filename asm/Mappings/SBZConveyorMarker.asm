; ---------------------------------------------------------------------------
; Sprite mappings - SBZ conveyor debug marker (4 speed-shoe monitor icon
; tiles, scaled to each of the object's 2 real trigger widths -- same
; corner-scaling idea as LavaTagMarker, just with the Shoes icon ($24,
; relative to ArtTile_Monitor) instead of Eggman, and only 2 sizes since
; Conv_Main only ever picks one of 2 widths (256 or 112, height fixed).
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_SBZConveyorMarker_internal:	mappingsTable
	mappingsTableEntry.w	@wide
	mappingsTableEntry.w	@narrow

@wide:	spriteHeader
	spritePiece	-$80, -$18, 2, 2, $24, 0, 0, 0, 0
	spritePiece	$70, -$18, 2, 2, $24, 0, 0, 0, 0
	spritePiece	-$80, 8, 2, 2, $24, 0, 0, 0, 0
	spritePiece	$70, 8, 2, 2, $24, 0, 0, 0, 0
@wide_End

@narrow:	spriteHeader
	spritePiece	-$38, -$18, 2, 2, $24, 0, 0, 0, 0
	spritePiece	$28, -$18, 2, 2, $24, 0, 0, 0, 0
	spritePiece	-$38, 8, 2, 2, $24, 0, 0, 0, 0
	spritePiece	$28, 8, 2, 2, $24, 0, 0, 0, 0
@narrow_End

	even
