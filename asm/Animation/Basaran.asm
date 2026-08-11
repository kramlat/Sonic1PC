; ---------------------------------------------------------------------------
; Animation script - Basaran enemy (MZ)
; ---------------------------------------------------------------------------
Animation_Basaran:
		dc.w @still-Animation_Basaran
		dc.w @fall-Animation_Basaran
		dc.w @fly-Animation_Basaran

@still:		dc.b 15
		dc.b 0
		dc.b $FF ; afEnd
		even

@fall:		dc.b 15
		dc.b 1
		dc.b $FF ; afEnd
		even

@fly:		dc.b 3
		dc.b 1, 2, 3, 2
		dc.b $FF ; afEnd
		even
