; ---------------------------------------------------------------------------
; Sprite mappings - lava tag debug marker (4 Eggman-icon tiles, scaled to
; each subtype's real hurt-hitbox corners). Ported from s2disasm's own
; Obj31 "Lava collision marker" (mappings/sprite/obj31_b.asm) -- that uses
; a "?" question-mark monitor icon (Sonic 2 has one, Sonic 1 doesn't); this
; project substitutes the Eggman icon instead, same tile ($18, relative to
; ArtTile_Monitor) InvisibleBarrier's own Map_Invis already uses.
; Frame 0/1/2 match LavaTag.c's own ltag_col_types indices (64x64/128x64/
; 256x64 hurt boxes) exactly -- obj->frame is just set to the subtype.
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_LavaTagMarker_internal:	mappingsTable
	mappingsTableEntry.w	@f0
	mappingsTableEntry.w	@f1
	mappingsTableEntry.w	@f2

@f0:	spriteHeader
	spritePiece	-$20, -$20, 2, 2, $18, 0, 0, 0, 0
	spritePiece	$10, -$20, 2, 2, $18, 0, 0, 0, 0
	spritePiece	-$20, $10, 2, 2, $18, 0, 0, 0, 0
	spritePiece	$10, $10, 2, 2, $18, 0, 0, 0, 0
@f0_End

@f1:	spriteHeader
	spritePiece	-$40, -$20, 2, 2, $18, 0, 0, 0, 0
	spritePiece	$30, -$20, 2, 2, $18, 0, 0, 0, 0
	spritePiece	-$40, $10, 2, 2, $18, 0, 0, 0, 0
	spritePiece	$30, $10, 2, 2, $18, 0, 0, 0, 0
@f1_End

@f2:	spriteHeader
	spritePiece	-$80, -$20, 2, 2, $18, 0, 0, 0, 0
	spritePiece	$70, -$20, 2, 2, $18, 0, 0, 0, 0
	spritePiece	-$80, $10, 2, 2, $18, 0, 0, 0, 0
	spritePiece	$70, $10, 2, 2, $18, 0, 0, 0, 0
@f2_End

	even
