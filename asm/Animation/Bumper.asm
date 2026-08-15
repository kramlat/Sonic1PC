; ---------------------------------------------------------------------------
; Animation script - Bumper (SYZ)
; ---------------------------------------------------------------------------

Ani_Bump:	dc.w @idle-Ani_Bump
		dc.w @touched-Ani_Bump

@idle:		dc.b 15, 0, $FF
		even

@touched:	dc.b 3,	1, 2, 1, 2, $FD, 0
		even
