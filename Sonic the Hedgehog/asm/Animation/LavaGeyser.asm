; ---------------------------------------------------------------------------
; Animation script - geyser of lava (MZ)
; ---------------------------------------------------------------------------



Ani_Geyser:	dc.w @bubble1-Ani_Geyser
		dc.w @bubble2-Ani_Geyser
		dc.w @end-Ani_Geyser
		dc.w @bubble3-Ani_Geyser
		dc.w @blank-Ani_Geyser
		dc.w @bubble4-Ani_Geyser
@bubble1:	dc.b 2,	0, 1, 0, 1, 4, 5, 4, 5,	$FC
@bubble2:	dc.b 2,	2, 3, $FF
@end:		dc.b 2,	6, 7, $FF
@bubble3:	dc.b 2,	2, 3, 0, 1, 0, 1, $FC
@blank:		dc.b $F, $13, $FF
		even
@bubble4:	dc.b 2,	$11, $12, $FF
		even
