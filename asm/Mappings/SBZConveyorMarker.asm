; ---------------------------------------------------------------------------
; Sprite mappings - SBZ conveyor debug marker (4 speed-shoe monitor icon
; tiles, scaled to each of the object's 2 real trigger widths -- same
; corner-scaling idea as LavaTagMarker, just with the Shoes icon ($24,
; relative to ArtTile_Monitor) instead of Eggman, and only 2 sizes since
; Conv_Main only ever picks one of 2 widths (256 or 112, height fixed).
; ---------------------------------------------------------------------------
Map_SBZConveyorMarker_internal:
		dc.w @wide-Map_SBZConveyorMarker_internal
		dc.w @narrow-Map_SBZConveyorMarker_internal
@wide:		dc.b 4
		dc.b $E8, 5, 0, $24, $80
		dc.b $E8, 5, 0, $24, $70
		dc.b 8,   5, 0, $24, $80
		dc.b 8,   5, 0, $24, $70
@narrow:	dc.b 4
		dc.b $E8, 5, 0, $24, $C8
		dc.b $E8, 5, 0, $24, $28
		dc.b 8,   5, 0, $24, $C8
		dc.b 8,   5, 0, $24, $28
		even
