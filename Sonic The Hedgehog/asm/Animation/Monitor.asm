; ---------------------------------------------------------------------------
; Animation script - monitors
; ---------------------------------------------------------------------------



Ani_Monitor:	dc.w @static-Ani_Monitor, @eggman-Ani_Monitor, @sonic-Ani_Monitor
		dc.w @shoes-Ani_Monitor, @shield-Ani_Monitor, @invincible-Ani_Monitor
		dc.w @rings-Ani_Monitor, @s-Ani_Monitor, @goggles-Ani_Monitor
		dc.w @breaking-Ani_Monitor
@static:	dc.b 1,	0, 1, 2, $FF
		even
@eggman:	dc.b 1,	0, 3, 3, 1, 3, 3, 2, 3,	3, $FF
		even
@sonic:		dc.b 1,	0, 4, 4, 1, 4, 4, 2, 4,	4, $FF
		even
@shoes:		dc.b 1,	0, 5, 5, 1, 5, 5, 2, 5,	5, $FF
		even
@shield:	dc.b 1,	0, 6, 6, 1, 6, 6, 2, 6,	6, $FF
		even
@invincible:	dc.b 1,	0, 7, 7, 1, 7, 7, 2, 7,	7, $FF
		even
@rings:		dc.b 1,	0, 8, 8, 1, 8, 8, 2, 8,	8, $FF
		even
@s:		dc.b 1,	0, 9, 9, 1, 9, 9, 2, 9,	9, $FF
		even
@goggles:	dc.b 1,	0, $A, $A, 1, $A, $A, 2, $A, $A, $FF
		even
@breaking:	dc.b 2,	0, 1, 2, $B, $FE, 1
		even
