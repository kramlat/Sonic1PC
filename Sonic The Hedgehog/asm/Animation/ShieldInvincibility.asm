; ---------------------------------------------------------------------------
; Animation script - shield and invincibility stars
; ---------------------------------------------------------------------------



Ani_Shield:	dc.w @shield-Ani_Shield
		dc.w @stars1-Ani_Shield
		dc.w @stars2-Ani_Shield
		dc.w @stars3-Ani_Shield
		dc.w @stars4-Ani_Shield
@shield:	dc.b 1,	1, 0, 2, 0, 3, 0, $FF
@stars1:	dc.b 5,	4, 5, 6, 7, $FF
@stars2:	dc.b 0,	4, 4, 0, 4, 4, 0, 5, 5,	0, 5, 5, 0, 6, 6, 0, 6
		dc.b 6,	0, 7, 7, 0, 7, 7, 0, $FF
@stars3:	dc.b 0,	4, 4, 0, 4, 0, 0, 5, 5,	0, 5, 0, 0, 6, 6, 0, 6
		dc.b 0,	0, 7, 7, 0, 7, 0, 0, $FF
@stars4:	dc.b 0,	4, 0, 0, 4, 0, 0, 5, 0,	0, 5, 0, 0, 6, 0, 0, 6
		dc.b 0,	0, 7, 0, 0, 7, 0, 0, $FF
		even
