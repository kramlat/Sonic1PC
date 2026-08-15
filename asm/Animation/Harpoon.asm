; ---------------------------------------------------------------------------
; Animation script - harpoon (LZ)
; ---------------------------------------------------------------------------

Ani_Harp:	dc.w @h_extending-Ani_Harp
		dc.w @h_retracting-Ani_Harp
		dc.w @v_extending-Ani_Harp
		dc.w @v_retracting-Ani_Harp

@h_extending:	dc.b 3,	1, 2, $FC
		even

@h_retracting:	dc.b 3,	1, 0, $FC
		even

@v_extending:	dc.b 3,	4, 5, $FC
		even

@v_retracting:	dc.b 3,	4, 3, $FC
		even
