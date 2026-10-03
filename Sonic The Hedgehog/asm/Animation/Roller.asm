; ---------------------------------------------------------------------------
; Animation script - Roller enemy (SYZ)
; ---------------------------------------------------------------------------

Ani_Roll:	dc.w @unfold-Ani_Roll
		dc.w @fold-Ani_Roll
		dc.w @roll-Ani_Roll

@unfold:	dc.b 15, 2, 1, 0, $FE, 1
		even

@fold:		dc.b 15, 1, 2, $FD, 2
		even

@roll:		dc.b 3,	3, 4, 2, $FF
		even
