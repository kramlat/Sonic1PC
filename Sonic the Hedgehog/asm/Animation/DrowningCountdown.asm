; ---------------------------------------------------------------------------
; Animation script - drowning countdown number bubbles (LZ). Frame indices
; reference Map_Bub (Mappings/Bubbles.asm), not a separate mapping table --
; the "appear" scripts animate a plain bubble inflating (frames 0-4) then
; morphing into a partially-formed digit (9-12) and finally the full digit
; (13-18); the "flash" scripts alternate the full digit with the blank
; frame (22).
; ---------------------------------------------------------------------------
Animation_DrowningCountdown:
		dc.w @zeroappear-Animation_DrowningCountdown
		dc.w @oneappear-Animation_DrowningCountdown
		dc.w @twoappear-Animation_DrowningCountdown
		dc.w @threeappear-Animation_DrowningCountdown
		dc.w @fourappear-Animation_DrowningCountdown
		dc.w @fiveappear-Animation_DrowningCountdown
		dc.w @smallbubble-Animation_DrowningCountdown
		dc.w @zeroflash-Animation_DrowningCountdown
		dc.w @oneflash-Animation_DrowningCountdown
		dc.w @twoflash-Animation_DrowningCountdown
		dc.w @threeflash-Animation_DrowningCountdown
		dc.w @fourflash-Animation_DrowningCountdown
		dc.w @fiveflash-Animation_DrowningCountdown
		dc.w @blank-Animation_DrowningCountdown
		dc.w @mediumbubble-Animation_DrowningCountdown

@zeroappear:	dc.b 5, 0, 1, 2, 3, 4, 9, $D, $FC
@oneappear:	dc.b 5, 0, 1, 2, 3, 4, $C, $12, $FC
@twoappear:	dc.b 5, 0, 1, 2, 3, 4, $C, $11, $FC
@threeappear:	dc.b 5, 0, 1, 2, 3, 4, $B, $10, $FC
@fourappear:	dc.b 5, 0, 1, 2, 3, 4, 9, $F, $FC
@fiveappear:	dc.b 5, 0, 1, 2, 3, 4, $A, $E, $FC
@smallbubble:	dc.b 14, 0, 1, 2, $FC
@zeroflash:	dc.b 7, $16, $D, $16, $D, $16, $D, $FC
@oneflash:	dc.b 7, $16, $12, $16, $12, $16, $12, $FC
@twoflash:	dc.b 7, $16, $11, $16, $11, $16, $11, $FC
@threeflash:	dc.b 7, $16, $10, $16, $10, $16, $10, $FC
@fourflash:	dc.b 7, $16, $F, $16, $F, $16, $F, $FC
@fiveflash:	dc.b 7, $16, $E, $16, $E, $16, $E, $FC
@blank:		dc.b 14, $FC
@mediumbubble:	dc.b 14, 1, 2, 3, 4, $FC
		even
