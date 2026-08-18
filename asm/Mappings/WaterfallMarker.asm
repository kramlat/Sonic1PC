; ---------------------------------------------------------------------------
; Sprite mappings - waterfall SFX trigger debug marker (2x2 tight cluster,
; goggles icon -- swimming/water association -- instead of InvisibleBarrier's
; Eggman icon). Same tight layout as Map_Invis's own .solid frame, just a
; different relative tile ($20, the Goggles monitor content icon).
; ---------------------------------------------------------------------------
Map_WaterfallMarker_internal:
		dc.w @solid-Map_WaterfallMarker_internal
@solid:		dc.b 4
		dc.b $F0, 5, 0, $20, $F0
		dc.b $F0, 5, 0, $20, 0
		dc.b 0,   5, 0, $20, $F0
		dc.b 0,   5, 0, $20, 0
		even
