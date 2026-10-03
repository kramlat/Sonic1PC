; ---------------------------------------------------------------------------
; Animation script - Chopper enemy
; ---------------------------------------------------------------------------



Ani_Chop:	dc.w @slow-Ani_Chop
		dc.w @fast-Ani_Chop
		dc.w @still-Ani_Chop
@slow:		dc.b 7,	0, 1, $FF
@fast:		dc.b 3,	0, 1, $FF
@still:		dc.b 7,	0, $FF
		even
