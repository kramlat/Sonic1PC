; ---------------------------------------------------------------------------
; Animation script - Newtron enemy
; ---------------------------------------------------------------------------



Ani_Newt:	dc.w A_Newt_Blank-Ani_Newt
		dc.w A_Newt_Drop-Ani_Newt
		dc.w A_Newt_Fly1-Ani_Newt
		dc.w A_Newt_Fly2-Ani_Newt
		dc.w A_Newt_Fires-Ani_Newt
A_Newt_Blank:	dc.b $F, $A, $FF
		even
A_Newt_Drop:	dc.b $13, 0, 1,	3, 4, 5, $FE, 1
A_Newt_Fly1:	dc.b 2,	6, 7, $FF
A_Newt_Fly2:	dc.b 2,	8, 9, $FF
A_Newt_Fires:	dc.b $13, 0, 1,	1, 2, 1, 1, 0, $FC
		even
