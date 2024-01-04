; ---------------------------------------------------------------------------
; Animation script - energy ball launcher (FZ)
; ---------------------------------------------------------------------------



Ani_PLaunch:	dc.w @red-Ani_PLaunch
		dc.w @redsparking-Ani_PLaunch
		dc.w @whitesparking-Ani_PLaunch
@red:		dc.b $7E, 0, $FF
		even
@redsparking:	dc.b 1,	0, 2, 0, 3, $FF
		even
@whitesparking:	dc.b 1,	1, 2, 1, 3, $FF
		even
