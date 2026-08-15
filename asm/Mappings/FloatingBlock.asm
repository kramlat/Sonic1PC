; ---------------------------------------------------------------------------
; Sprite mappings - moving blocks (SYZ/SLZ/LZ)
; ---------------------------------------------------------------------------
Map_FBlock_internal:
		dc.w @syz1x1-Map_FBlock_internal
		dc.w @syz2x2-Map_FBlock_internal
		dc.w @syz1x2-Map_FBlock_internal
		dc.w @syzrect2x2-Map_FBlock_internal
		dc.w @syzrect1x3-Map_FBlock_internal
		dc.w @slz-Map_FBlock_internal
		dc.w @lzvert-Map_FBlock_internal
		dc.w @lzhoriz-Map_FBlock_internal
@syz1x1:	dc.b 1
		dc.b $F0, $F, 0, $61, $F0
@syz2x2:	dc.b 4
		dc.b $E0, $F, 0, $61, $E0
		dc.b $E0, $F, 0, $61, 0
		dc.b 0,	$F, 0, $61, $E0
		dc.b 0,	$F, 0, $61, 0
@syz1x2:	dc.b 2
		dc.b $E0, $F, 0, $61, $F0
		dc.b 0,	$F, 0, $61, $F0
@syzrect2x2:	dc.b 4
		dc.b $E6, $F, 0, $81, $E0
		dc.b $E6, $F, 0, $81, 0
		dc.b 0,	$F, 0, $81, $E0
		dc.b 0,	$F, 0, $81, 0
@syzrect1x3:	dc.b 3
		dc.b $D9, $F, 0, $81, $F0
		dc.b $F3, $F, 0, $81, $F0
		dc.b $D, $F, 0, $81, $F0
@slz:		dc.b 1
		dc.b $F0, $F, 0, $21, $F0
@lzvert:	dc.b 2
		dc.b $E0, 7, 0,	0, $F8
		dc.b 0,	7, $10, 0, $F8
@lzhoriz:	dc.b 4
		dc.b $F0, $F, 0, $22, $C0
		dc.b $F0, $F, 0, $22, $E0
		dc.b $F0, $F, 0, $22, 0
		dc.b $F0, $F, 0, $22, $20
		even
