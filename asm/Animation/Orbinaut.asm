; ---------------------------------------------------------------------------
; Animation script - Orbinaut enemy
; ---------------------------------------------------------------------------



Ani_Orb:	dc.w @normal-Ani_Orb
		dc.w @angers-Ani_Orb
@normal:	dc.b $F, 0, $FF
		even
@angers:	dc.b $F, 1, 2, $FE, 1
		even
