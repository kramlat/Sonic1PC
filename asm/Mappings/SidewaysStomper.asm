; ---------------------------------------------------------------------------
; Sprite mappings - spiked metal block from beta version (MZ, unused)
; ---------------------------------------------------------------------------
; Real hardware's own mapping data has a bug here: .pole5's header end
; label is misplaced 2 pieces too early, so its header claims only 8
; pieces even though 10 are present in the data -- the last 2 (at x=$60,
; $70) are orphaned bytes nothing ever reads. Faithfully reproduced: this
; frame here has the same 8 reachable pieces as .pole4 (matching the real,
; unfixed prototype behavior described in the object's own SStom_Len
; comment about the pole looking "too short").
Map_SSto_internal:
		dc.w @block-Map_SSto_internal
		dc.w @spikes-Map_SSto_internal
		dc.w @wallbracket-Map_SSto_internal
		dc.w @pole1-Map_SSto_internal
		dc.w @pole2-Map_SSto_internal
		dc.w @pole3-Map_SSto_internal
		dc.w @pole4-Map_SSto_internal
		dc.w @pole5-Map_SSto_internal
		dc.w @pole5-Map_SSto_internal
@block:		dc.b 3
		dc.b $E0, $B, 0, $1F, $F4
		dc.b 0,	$B, $10, $1F, $F4
		dc.b $F0, 3, 0,	$2B, $C
@spikes:	dc.b 3
		dc.b $E8, $C, $12, $1B, $F0
		dc.b $FC, $C, $12, $1B, $F0
		dc.b $10, $C, $12, $1B, $F0
@wallbracket:	dc.b 1
		dc.b $F0, 3, 8, $2B, $FC
@pole1:		dc.b 2
		dc.b $F8, 5, 0,	$41, $E0
		dc.b $F8, 5, 0,	$41, $F0
@pole2:		dc.b 4
		dc.b $F8, 5, 0,	$41, $E0
		dc.b $F8, 5, 0,	$41, $F0
		dc.b $F8, 5, 0,	$41, 0
		dc.b $F8, 5, 0,	$41, $10
@pole3:		dc.b 6
		dc.b $F8, 5, 0,	$41, $E0
		dc.b $F8, 5, 0,	$41, $F0
		dc.b $F8, 5, 0,	$41, 0
		dc.b $F8, 5, 0,	$41, $10
		dc.b $F8, 5, 0,	$41, $20
		dc.b $F8, 5, 0,	$41, $30
@pole4:		dc.b 8
		dc.b $F8, 5, 0,	$41, $E0
		dc.b $F8, 5, 0,	$41, $F0
		dc.b $F8, 5, 0,	$41, 0
		dc.b $F8, 5, 0,	$41, $10
		dc.b $F8, 5, 0,	$41, $20
		dc.b $F8, 5, 0,	$41, $30
		dc.b $F8, 5, 0,	$41, $40
		dc.b $F8, 5, 0,	$41, $50
@pole5:		dc.b 8
		dc.b $F8, 5, 0,	$41, $E0
		dc.b $F8, 5, 0,	$41, $F0
		dc.b $F8, 5, 0,	$41, 0
		dc.b $F8, 5, 0,	$41, $10
		dc.b $F8, 5, 0,	$41, $20
		dc.b $F8, 5, 0,	$41, $30
		dc.b $F8, 5, 0,	$41, $40
		dc.b $F8, 5, 0,	$41, $50
		even
