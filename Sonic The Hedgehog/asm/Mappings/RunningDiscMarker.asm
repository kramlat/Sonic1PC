; ---------------------------------------------------------------------------
; Sprite mappings - SBZ2 running disc (gear) debug marker. Same idea as
; SBZConveyorMarker (4 speed-shoe monitor icon tiles, $24 relative to
; ArtTile_Monitor) since Disc_MoveSonic works exactly like a conveyor's own
; touch-and-carry logic, just scaled to the gear's square attach-trigger
; (triggersize) instead of a rectangle -- 2 sizes, matching Disc_Main's own
; large/small (leftover, practically unused) choice.
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_RunningDiscMarker_internal:	mappingsTable
	mappingsTableEntry.w	@large
	mappingsTableEntry.w	@small

@large:	spriteHeader
	spritePiece	-$48, -$48, 2, 2, $24, 0, 0, 0, 0
	spritePiece	$38, -$48, 2, 2, $24, 0, 0, 0, 0
	spritePiece	-$48, $38, 2, 2, $24, 0, 0, 0, 0
	spritePiece	$38, $38, 2, 2, $24, 0, 0, 0, 0
@large_End

@small:	spriteHeader
	spritePiece	-$38, -$38, 2, 2, $24, 0, 0, 0, 0
	spritePiece	$28, -$38, 2, 2, $24, 0, 0, 0, 0
	spritePiece	-$38, $28, 2, 2, $24, 0, 0, 0, 0
	spritePiece	$28, $28, 2, 2, $24, 0, 0, 0, 0
@small_End

	even
