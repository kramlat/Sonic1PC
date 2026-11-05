; ---------------------------------------------------------------------------
; Animation script - water splash and spin dash dust
; ---------------------------------------------------------------------------
Ani_SplashDust:
		dc.w	DustAni_0-Ani_SplashDust
		dc.w	DustAni_1-Ani_SplashDust
		dc.w	DustAni_2-Ani_SplashDust
		dc.w	DustAni_3-Ani_SplashDust
DustAni_0:	dc.b	31, 0, $FF
DustAni_1:	dc.b	3, 1, 2, 3, 4, 5, 6, 7, 8, 9, $FD, 0
DustAni_2:	dc.b	1, 10, 11, 12, 13, 14, 15, 16, $FF
DustAni_3:	dc.b	3, 17, 18, 19, 20, $FC
		even
