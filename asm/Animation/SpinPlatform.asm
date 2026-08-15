; ---------------------------------------------------------------------------
; Animation script - stationary spinning platforms and trapdoors (SBZ)
; ---------------------------------------------------------------------------

Ani_Spin:	dc.w @trapopen-Ani_Spin
		dc.w @trapclose-Ani_Spin
		dc.w @spin1-Ani_Spin
		dc.w @spin2-Ani_Spin

@trapopen:	dc.b 3
		dc.b 0, 1
		dc.b 2
		dc.b $FE, 1
		even

@trapclose:	dc.b 3
		dc.b 2, 1
		dc.b 0
		dc.b $FE, 1
		even

@spin1:		dc.b 1
		dc.b 0, 1, 2, 3, 4
		dc.b 3|$40, 2|$40, 1|$40, 0|$40
		dc.b 1|$20|$40, 2|$20|$40, 3|$20|$40, 4|$20|$40
		dc.b 3|$20, 2|$20, 1|$20
		dc.b 0
		dc.b $FE, 1
		even

@spin2:		dc.b 1
		dc.b 0, 1, 2, 3, 4
		dc.b 3|$40, 2|$40, 1|$40, 0|$40
		dc.b 1|$20|$40, 2|$20|$40, 3|$20|$40, 4|$20|$40
		dc.b 3|$20, 2|$20, 1|$20
		dc.b 0
		dc.b $FE, 1
		even
