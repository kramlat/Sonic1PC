; ---------------------------------------------------------------------------
; Sprite mappings - Path Swapper debug marker (4 rings, vertical/horizontal,
; 3 sizes each). Ported from s1disasm ProjectSonic1TwoEight's own
; "_maps/Collision Switcher.asm" (Map_PathSwapper_Internal) -- real
; hardware's DebugPathSwappers feature. All pieces are 2x2 tiles (16x16)
; using tile 0 relative to the object's own base tile (ArtTile_Ring).
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_PathSwapper_internal:	mappingsTable
	mappingsTableEntry.w	@f1
	mappingsTableEntry.w	@f2
	mappingsTableEntry.w	@f3
	mappingsTableEntry.w	@f3
	mappingsTableEntry.w	@f4
	mappingsTableEntry.w	@f5
	mappingsTableEntry.w	@f6
	mappingsTableEntry.w	@f6

@f1:	spriteHeader
	spritePiece	-8, -$20, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-8, -$10, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-8, 0, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-8, $10, 2, 2, 0, 0, 0, 0, 0
@f1_End

@f2:	spriteHeader
	spritePiece	-8, -$40, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-8, -$20, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-8, 0, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-8, $30, 2, 2, 0, 0, 0, 0, 0
@f2_End

@f3:	spriteHeader
	spritePiece	-8, -$80, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-8, -$20, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-8, 0, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-8, $70, 2, 2, 0, 0, 0, 0, 0
@f3_End

@f4:	spriteHeader
	spritePiece	-$20, -8, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-$10, -8, 2, 2, 0, 0, 0, 0, 0
	spritePiece	0, -8, 2, 2, 0, 0, 0, 0, 0
	spritePiece	$10, -8, 2, 2, 0, 0, 0, 0, 0
@f4_End

@f5:	spriteHeader
	spritePiece	-$40, -8, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-$20, -8, 2, 2, 0, 0, 0, 0, 0
	spritePiece	0, -8, 2, 2, 0, 0, 0, 0, 0
	spritePiece	$30, -8, 2, 2, 0, 0, 0, 0, 0
@f5_End

@f6:	spriteHeader
	spritePiece	-$80, -8, 2, 2, 0, 0, 0, 0, 0
	spritePiece	-$20, -8, 2, 2, 0, 0, 0, 0, 0
	spritePiece	0, -8, 2, 2, 0, 0, 0, 0, 0
	spritePiece	$70, -8, 2, 2, 0, 0, 0, 0, 0
@f6_End

	even
