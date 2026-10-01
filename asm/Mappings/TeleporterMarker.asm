; ---------------------------------------------------------------------------
; Sprite mappings - SBZ2 teleporter debug marker (4x4 grid of 1-Up monitor
; icon tiles, $1C relative to ArtTile_Monitor). Purely decorative/thematic
; (no real trigger-boundary shape to scale to) -- just a distinct, dense
; marker for a system that otherwise has zero visual footprint at all.
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_TeleporterMarker_internal:	mappingsTable
	mappingsTableEntry.w	@grid

@grid:	spriteHeader
	spritePiece	-$20, -$20, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	-$10, -$20, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	0, -$20, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	$10, -$20, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	-$20, -$10, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	-$10, -$10, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	0, -$10, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	$10, -$10, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	-$20, 0, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	-$10, 0, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	0, 0, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	$10, 0, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	-$20, $10, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	-$10, $10, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	0, $10, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	$10, $10, 2, 2, $1C, 0, 0, 0, 0
@grid_End

	even
