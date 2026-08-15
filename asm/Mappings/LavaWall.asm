; ---------------------------------------------------------------------------
; Sprite mappings - advancing wall of lava (MZ)
; ---------------------------------------------------------------------------
Map_LWall_internal:
		dc.w @lava0-Map_LWall_internal
		dc.w @lava1-Map_LWall_internal
		dc.w @lava2-Map_LWall_internal
		dc.w @lava3-Map_LWall_internal
		dc.w @lava_back-Map_LWall_internal
@lava0:		dc.b 9
		dc.b $E0, $F, 0, $60, $20
		dc.b 0,	$F, 0, $70, $3C
		dc.b 0,	$F, $FF, $2A, $20
		dc.b $E0, $F, $FF, $2A, 0
		dc.b 0,	$F, $FF, $2A, 0
		dc.b $E0, $F, $FF, $2A, $E0
		dc.b 0,	$F, $FF, $2A, $E0
		dc.b $E0, $F, $FF, $2A, $C0
		dc.b 0,	$F, $FF, $2A, $C0
@lava1:		dc.b 9
		dc.b $E0, $F, 0, $70, $20
		dc.b 0,	$F, 0, $80, $3C
		dc.b 0,	$F, $FF, $2A, $20
		dc.b $E0, $F, $FF, $2A, 0
		dc.b 0,	$F, $FF, $2A, 0
		dc.b $E0, $F, $FF, $2A, $E0
		dc.b 0,	$F, $FF, $2A, $E0
		dc.b $E0, $F, $FF, $2A, $C0
		dc.b 0,	$F, $FF, $2A, $C0
@lava2:		dc.b 9
		dc.b $E0, $F, 0, $80, $20
		dc.b 0,	$F, 0, $70, $3C
		dc.b 0,	$F, $FF, $2A, $20
		dc.b $E0, $F, $FF, $2A, 0
		dc.b 0,	$F, $FF, $2A, 0
		dc.b $E0, $F, $FF, $2A, $E0
		dc.b 0,	$F, $FF, $2A, $E0
		dc.b $E0, $F, $FF, $2A, $C0
		dc.b 0,	$F, $FF, $2A, $C0
@lava3:		dc.b 9
		dc.b $E0, $F, 0, $70, $20
		dc.b 0,	$F, 0, $60, $3C
		dc.b 0,	$F, $FF, $2A, $20
		dc.b $E0, $F, $FF, $2A, 0
		dc.b 0,	$F, $FF, $2A, 0
		dc.b $E0, $F, $FF, $2A, $E0
		dc.b 0,	$F, $FF, $2A, $E0
		dc.b $E0, $F, $FF, $2A, $C0
		dc.b 0,	$F, $FF, $2A, $C0
@lava_back:	dc.b 8
		dc.b $E0, $F, $FF, $2A, $20
		dc.b 0,	$F, $FF, $2A, $20
		dc.b $E0, $F, $FF, $2A, 0
		dc.b 0,	$F, $FF, $2A, 0
		dc.b $E0, $F, $FF, $2A, $E0
		dc.b 0,	$F, $FF, $2A, $E0
		dc.b $E0, $F, $FF, $2A, $C0
		dc.b 0,	$F, $FF, $2A, $C0
		even
