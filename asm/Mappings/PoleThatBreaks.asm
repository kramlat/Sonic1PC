; ---------------------------------------------------------------------------
; Sprite mappings - pole that breaks (LZ)
; ---------------------------------------------------------------------------
Map_Pole_internal:
		dc.w @normal-Map_Pole_internal
		dc.w @broken-Map_Pole_internal
@normal:	dc.b 2
		dc.b $E0, 3, 0,	0, $FC
		dc.b 0,	3, $10, 0, $FC
@broken:	dc.b 4
		dc.b $E0, 1, 0,	0, $FC
		dc.b $F0, 5, 0,	4, $FC
		dc.b 0,	5, $10, 4, $FC
		dc.b $10, 1, $10, 0, $FC
		even
