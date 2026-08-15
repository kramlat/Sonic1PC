; ---------------------------------------------------------------------------
; Animation script - platform on conveyor belt (SBZ)
; ---------------------------------------------------------------------------

Ani_SpinConvey:	dc.w @spin-Ani_SpinConvey
		dc.w @still-Ani_SpinConvey

@spin:		dc.b 0
		dc.b 0, 1, 2, 3, 4
		dc.b 3|$40, 2|$40, 1|$40, 0|$40
		dc.b 1|$20|$40, 2|$20|$40, 3|$20|$40, 4|$20|$40
		dc.b 3|$20, 2|$20, 1|$20, 0
		dc.b $FF
		even

@still:		dc.b 15
		dc.b 0
		dc.b $FF
		even
