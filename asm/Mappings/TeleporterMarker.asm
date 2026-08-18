; ---------------------------------------------------------------------------
; Sprite mappings - SBZ2 teleporter debug marker (4x4 grid of 1-Up monitor
; icon tiles, $1C relative to ArtTile_Monitor). Purely decorative/thematic
; (no real trigger-boundary shape to scale to) -- just a distinct, dense
; marker for a system that otherwise has zero visual footprint at all.
; ---------------------------------------------------------------------------
Map_TeleporterMarker_internal:
		dc.w @grid-Map_TeleporterMarker_internal
@grid:		dc.b 16
		dc.b $E0, 5, 0, $1C, $E0
		dc.b $E0, 5, 0, $1C, $F0
		dc.b $E0, 5, 0, $1C, 0
		dc.b $E0, 5, 0, $1C, $10
		dc.b $F0, 5, 0, $1C, $E0
		dc.b $F0, 5, 0, $1C, $F0
		dc.b $F0, 5, 0, $1C, 0
		dc.b $F0, 5, 0, $1C, $10
		dc.b 0,   5, 0, $1C, $E0
		dc.b 0,   5, 0, $1C, $F0
		dc.b 0,   5, 0, $1C, 0
		dc.b 0,   5, 0, $1C, $10
		dc.b $10, 5, 0, $1C, $E0
		dc.b $10, 5, 0, $1C, $F0
		dc.b $10, 5, 0, $1C, 0
		dc.b $10, 5, 0, $1C, $10
		even
