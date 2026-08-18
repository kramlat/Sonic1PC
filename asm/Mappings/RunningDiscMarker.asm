; ---------------------------------------------------------------------------
; Sprite mappings - SBZ2 running disc (gear) debug marker. Same idea as
; SBZConveyorMarker (4 speed-shoe monitor icon tiles, $24 relative to
; ArtTile_Monitor) since Disc_MoveSonic works exactly like a conveyor's own
; touch-and-carry logic, just scaled to the gear's square attach-trigger
; (triggersize) instead of a rectangle -- 2 sizes, matching Disc_Main's own
; large/small (leftover, practically unused) choice.
; ---------------------------------------------------------------------------
Map_RunningDiscMarker_internal:
		dc.w @large-Map_RunningDiscMarker_internal
		dc.w @small-Map_RunningDiscMarker_internal
@large:		dc.b 4
		dc.b $B8, 5, 0, $24, $B8
		dc.b $B8, 5, 0, $24, $38
		dc.b $38, 5, 0, $24, $B8
		dc.b $38, 5, 0, $24, $38
@small:		dc.b 4
		dc.b $C8, 5, 0, $24, $C8
		dc.b $C8, 5, 0, $24, $28
		dc.b $28, 5, 0, $24, $C8
		dc.b $28, 5, 0, $24, $28
		even
