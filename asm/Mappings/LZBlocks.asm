; ---------------------------------------------------------------------------
; Sprite mappings - blocks (LZ)
; ---------------------------------------------------------------------------
Map_LBlock_internal:
		dc.w @sinkblock-Map_LBlock_internal
		dc.w @riseplatform-Map_LBlock_internal
		dc.w @cork-Map_LBlock_internal
		dc.w @block-Map_LBlock_internal
@sinkblock:	dc.b 1
		dc.b $F0, $F, 0, 0, $F0
@riseplatform:	dc.b 2
		dc.b $F4, $E, 0, $69, $E0
		dc.b $F4, $E, 0, $75, 0
@cork:		dc.b 1
		dc.b $F0, $F, 1, $1A, $F0
@block:		dc.b 1
		dc.b $F0, $F, $FF, $FA, $F0
		even
