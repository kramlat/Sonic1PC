; ---------------------------------------------------------------------------
; Sprite mappings - SCORE, TIME, RINGS
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_HUD_internal:	mappingsTable
	mappingsTableEntry.w	@allyellow
	mappingsTableEntry.w	@ringred
	mappingsTableEntry.w	@timered
	mappingsTableEntry.w	@allred
	mappingsTableEntry.w	@allyellow_nolives
	mappingsTableEntry.w	@ringred_nolives
	mappingsTableEntry.w	@timered_nolives
	mappingsTableEntry.w	@allred_nolives
	mappingsTableEntry.w	@lives

@allyellow:	spriteHeader
	spritePiece	0, -$80, 4, 2, 0, 0, 0, 1, 1
	spritePiece	$20, -$80, 4, 2, $18, 0, 0, 1, 1
	spritePiece	$40, -$80, 4, 2, $20, 0, 0, 1, 1
	spritePiece	0, -$70, 4, 2, $10, 0, 0, 1, 1
	spritePiece	$28, -$70, 4, 2, $28, 0, 0, 1, 1
	spritePiece	0, -$60, 4, 2, 8, 0, 0, 1, 1
	spritePiece	$20, -$60, 1, 2, 0, 0, 0, 1, 1
	spritePiece	$30, -$60, 3, 2, $30, 0, 0, 1, 1
	spritePiece	0, $40, 2, 2, $10A, 0, 0, 0, 1
	spritePiece	$10, $40, 4, 2, $10E, 0, 0, 1, 1
@allyellow_End
	dc.b 0

@ringred:	spriteHeader
	spritePiece	0, -$80, 4, 2, 0, 0, 0, 1, 1
	spritePiece	$20, -$80, 4, 2, $18, 0, 0, 1, 1
	spritePiece	$40, -$80, 4, 2, $20, 0, 0, 1, 1
	spritePiece	0, -$70, 4, 2, $10, 0, 0, 1, 1
	spritePiece	$28, -$70, 4, 2, $28, 0, 0, 1, 1
	spritePiece	0, -$60, 4, 2, 8, 0, 0, 0, 1
	spritePiece	$20, -$60, 1, 2, 0, 0, 0, 0, 1
	spritePiece	$30, -$60, 3, 2, $30, 0, 0, 1, 1
	spritePiece	0, $40, 2, 2, $10A, 0, 0, 0, 1
	spritePiece	$10, $40, 4, 2, $10E, 0, 0, 1, 1
@ringred_End
	dc.b 0

@timered:	spriteHeader
	spritePiece	0, -$80, 4, 2, 0, 0, 0, 1, 1
	spritePiece	$20, -$80, 4, 2, $18, 0, 0, 1, 1
	spritePiece	$40, -$80, 4, 2, $20, 0, 0, 1, 1
	spritePiece	0, -$70, 4, 2, $10, 0, 0, 0, 1
	spritePiece	$28, -$70, 4, 2, $28, 0, 0, 1, 1
	spritePiece	0, -$60, 4, 2, 8, 0, 0, 1, 1
	spritePiece	$20, -$60, 1, 2, 0, 0, 0, 1, 1
	spritePiece	$30, -$60, 3, 2, $30, 0, 0, 1, 1
	spritePiece	0, $40, 2, 2, $10A, 0, 0, 0, 1
	spritePiece	$10, $40, 4, 2, $10E, 0, 0, 1, 1
@timered_End
	dc.b 0

@allred:	spriteHeader
	spritePiece	0, -$80, 4, 2, 0, 0, 0, 1, 1
	spritePiece	$20, -$80, 4, 2, $18, 0, 0, 1, 1
	spritePiece	$40, -$80, 4, 2, $20, 0, 0, 1, 1
	spritePiece	0, -$70, 4, 2, $10, 0, 0, 0, 1
	spritePiece	$28, -$70, 4, 2, $28, 0, 0, 1, 1
	spritePiece	0, -$60, 4, 2, 8, 0, 0, 0, 1
	spritePiece	$20, -$60, 1, 2, 0, 0, 0, 0, 1
	spritePiece	$30, -$60, 3, 2, $30, 0, 0, 1, 1
	spritePiece	0, $40, 2, 2, $10A, 0, 0, 0, 1
	spritePiece	$10, $40, 4, 2, $10E, 0, 0, 1, 1
@allred_End

@allyellow_nolives:	spriteHeader
	spritePiece	0, -$80, 4, 2, 0, 0, 0, 1, 1
	spritePiece	$20, -$80, 4, 2, $18, 0, 0, 1, 1
	spritePiece	$40, -$80, 4, 2, $20, 0, 0, 1, 1
	spritePiece	0, -$70, 4, 2, $10, 0, 0, 1, 1
	spritePiece	$28, -$70, 4, 2, $28, 0, 0, 1, 1
	spritePiece	0, -$60, 4, 2, 8, 0, 0, 1, 1
	spritePiece	$20, -$60, 1, 2, 0, 0, 0, 1, 1
	spritePiece	$30, -$60, 3, 2, $30, 0, 0, 1, 1
@allyellow_nolives_End
	dc.b 0

@ringred_nolives:	spriteHeader
	spritePiece	0, -$80, 4, 2, 0, 0, 0, 1, 1
	spritePiece	$20, -$80, 4, 2, $18, 0, 0, 1, 1
	spritePiece	$40, -$80, 4, 2, $20, 0, 0, 1, 1
	spritePiece	0, -$70, 4, 2, $10, 0, 0, 1, 1
	spritePiece	$28, -$70, 4, 2, $28, 0, 0, 1, 1
	spritePiece	0, -$60, 4, 2, 8, 0, 0, 0, 1
	spritePiece	$20, -$60, 1, 2, 0, 0, 0, 0, 1
	spritePiece	$30, -$60, 3, 2, $30, 0, 0, 1, 1
@ringred_nolives_End
	dc.b 0

@timered_nolives:	spriteHeader
	spritePiece	0, -$80, 4, 2, 0, 0, 0, 1, 1
	spritePiece	$20, -$80, 4, 2, $18, 0, 0, 1, 1
	spritePiece	$40, -$80, 4, 2, $20, 0, 0, 1, 1
	spritePiece	0, -$70, 4, 2, $10, 0, 0, 0, 1
	spritePiece	$28, -$70, 4, 2, $28, 0, 0, 1, 1
	spritePiece	0, -$60, 4, 2, 8, 0, 0, 1, 1
	spritePiece	$20, -$60, 1, 2, 0, 0, 0, 1, 1
	spritePiece	$30, -$60, 3, 2, $30, 0, 0, 1, 1
@timered_nolives_End
	dc.b 0

@allred_nolives:	spriteHeader
	spritePiece	0, -$80, 4, 2, 0, 0, 0, 1, 1
	spritePiece	$20, -$80, 4, 2, $18, 0, 0, 1, 1
	spritePiece	$40, -$80, 4, 2, $20, 0, 0, 1, 1
	spritePiece	0, -$70, 4, 2, $10, 0, 0, 0, 1
	spritePiece	$28, -$70, 4, 2, $28, 0, 0, 1, 1
	spritePiece	0, -$60, 4, 2, 8, 0, 0, 0, 1
	spritePiece	$20, -$60, 1, 2, 0, 0, 0, 0, 1
	spritePiece	$30, -$60, 3, 2, $30, 0, 0, 1, 1
@allred_nolives_End
	dc.b 0

@lives:	spriteHeader
	spritePiece	0, 0, 2, 2, $10A, 0, 0, 0, 1
	spritePiece	$10, 0, 4, 2, $10E, 0, 0, 1, 1
@lives_End

	even
