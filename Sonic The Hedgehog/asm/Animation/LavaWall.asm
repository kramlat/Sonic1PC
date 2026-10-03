; ---------------------------------------------------------------------------
; Animation script - advancing wall of lava (MZ act 2)
; ---------------------------------------------------------------------------

Ani_LWall:	dc.w @lavawall-Ani_LWall

@lavawall:	dc.b 9,	0, 1, 2, 3, $FF
		even
