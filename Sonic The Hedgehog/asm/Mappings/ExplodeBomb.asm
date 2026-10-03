; ---------------------------------------------------------------------------
; Sprite mappings - Explosion from a bomb or a destroyed boss part (Map_ExplodeBomb)
;
; The original's table shares three of these five frames with Map_ExplodeItem through negative table offsets, which this
; engine cannot follow (it reads frame offsets as unsigned), so the shared frames are written out here in full.
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_ExplodeBomb_internal:	mappingsTable
	mappingsTableEntry.w	@bomb_0
	mappingsTableEntry.w	@bomb_1
	mappingsTableEntry.w	@bomb_2
	mappingsTableEntry.w	@bomb_3
	mappingsTableEntry.w	@bomb_4

@bomb_0:	spriteHeader
	spritePiece	-$C, -8, 3, 2, 0, 0, 0, 0, 0
@bomb_0_End

@bomb_1:	spriteHeader
	spritePiece	-$10, -$10, 4, 4, $40, 0, 0, 0, 0
@bomb_1_End

@bomb_2:	spriteHeader
	spritePiece	-$10, -$10, 4, 4, $50, 0, 0, 0, 0
@bomb_2_End

@bomb_3:	spriteHeader
	spritePiece	-$14, -$14, 3, 3, $26, 0, 0, 0, 0
	spritePiece	4, -$14, 2, 2, $2F, 0, 0, 0, 0
	spritePiece	-$14, 4, 2, 2, $2F, 1, 1, 0, 0
	spritePiece	-4, -4, 3, 3, $26, 1, 1, 0, 0
@bomb_3_End

@bomb_4:	spriteHeader
	spritePiece	-$14, -$14, 3, 3, $33, 0, 0, 0, 0
	spritePiece	4, -$14, 2, 2, $3C, 0, 0, 0, 0
	spritePiece	-$14, 4, 2, 2, $3C, 1, 1, 0, 0
	spritePiece	-4, -4, 3, 3, $33, 1, 1, 0, 0
@bomb_4_End

	even
