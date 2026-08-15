; ---------------------------------------------------------------------------
; Animation script - Burrobot enemy (LZ)
; ---------------------------------------------------------------------------

Ani_Burro:	dc.w @still-Ani_Burro
		dc.w @moving-Ani_Burro
		dc.w @digging-Ani_Burro
		dc.w @fall-Ani_Burro

@still:		dc.b 3,	0, 6, $FF
		even

@moving:	dc.b 3,	0, 1, $FF
		even

@digging:	dc.b 3,	2, 3, $FF
		even

@fall:		dc.b 3,	4, $FF
		even
