; ---------------------------------------------------------------------------
; Animation script - Orbinaut enemy (LZ/SLZ)
; ---------------------------------------------------------------------------

Ani_Orb:	dc.w @normal-Ani_Orb
		dc.w @angry-Ani_Orb

@normal:	dc.b 15, 0, $FF
		even

@angry:		dc.b 15, 1, 2, $FE, 1
		even
