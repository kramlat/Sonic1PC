; ---------------------------------------------------------------------------
; Sprite mappings - invisible solid blocks (debug-only preview). Ported from
; s1disasm ProjectSonic1TwoEight's own "_maps/Invisible Barriers.asm" --
; real hardware's own Map_Invis, a dedicated 2x2-icon-only marker (tile $18
; relative to ArtTile_Monitor), NOT Monitor's own box+icon combo frames.
; Frame 0 (.solid) is the only one placed in real levels; frames 1/2 are
; wider-spread unused variants, kept for completeness.
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_Invis_internal:	mappingsTable
	mappingsTableEntry.w	@solid
	mappingsTableEntry.w	@unused1
	mappingsTableEntry.w	@unused2

@solid:	spriteHeader
	spritePiece	-$10, -$10, 2, 2, $18, 0, 0, 0, 0
	spritePiece	0, -$10, 2, 2, $18, 0, 0, 0, 0
	spritePiece	-$10, 0, 2, 2, $18, 0, 0, 0, 0
	spritePiece	0, 0, 2, 2, $18, 0, 0, 0, 0
@solid_End

@unused1:	spriteHeader
	spritePiece	-$40, -$20, 2, 2, $18, 0, 0, 0, 0
	spritePiece	$30, -$20, 2, 2, $18, 0, 0, 0, 0
	spritePiece	-$40, $10, 2, 2, $18, 0, 0, 0, 0
	spritePiece	$30, $10, 2, 2, $18, 0, 0, 0, 0
@unused1_End

@unused2:	spriteHeader
	spritePiece	-$80, -$20, 2, 2, $18, 0, 0, 0, 0
	spritePiece	$70, -$20, 2, 2, $18, 0, 0, 0, 0
	spritePiece	-$80, $10, 2, 2, $18, 0, 0, 0, 0
	spritePiece	$70, $10, 2, 2, $18, 0, 0, 0, 0
@unused2_End

	even
