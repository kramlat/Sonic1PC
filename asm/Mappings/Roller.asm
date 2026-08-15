; ---------------------------------------------------------------------------
; Sprite mappings - Roller enemy (SYZ)
; ---------------------------------------------------------------------------
Map_Roll_internal:
		dc.w @stand-Map_Roll_internal
		dc.w @fold-Map_Roll_internal
		dc.w @roll1-Map_Roll_internal
		dc.w @roll2-Map_Roll_internal
		dc.w @roll3-Map_Roll_internal
@stand:		dc.b 2
		dc.b $DE, $E, 0, 0, $F0
		dc.b $F6, $E, 0, $C, $F0
@fold:		dc.b 2
		dc.b $E6, $E, 0, 0, $F0
		dc.b $FE, $D, 0, $18, $F0
@roll1:		dc.b 1
		dc.b $F0, $F, 0, $20, $F0
@roll2:		dc.b 1
		dc.b $F0, $F, 0, $30, $F0
@roll3:		dc.b 1
		dc.b $F0, $F, 0, $40, $F0
		even
