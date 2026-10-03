; ---------------------------------------------------------------------------
; Animation script - Buzz Bomber enemy
; ---------------------------------------------------------------------------



Ani_Buzz:	dc.w @fly1-Ani_Buzz
		dc.w @fly2-Ani_Buzz
		dc.w @fires-Ani_Buzz
@fly1:		dc.b 1,	0, 1, $FF
@fly2:		dc.b 1,	2, 3, $FF
@fires:		dc.b 1,	4, 5, $FF
		even
