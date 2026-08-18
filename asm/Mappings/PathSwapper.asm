; ---------------------------------------------------------------------------
; Sprite mappings - Path Swapper debug marker (4 rings, vertical/horizontal,
; 3 sizes each). Ported from s1disasm ProjectSonic1TwoEight's own
; "_maps/Collision Switcher.asm" (Map_PathSwapper_Internal) -- real
; hardware's DebugPathSwappers feature. All pieces are 2x2 tiles (16x16)
; using tile 0 relative to the object's own base tile (ArtTile_Ring).
; ---------------------------------------------------------------------------
Map_PathSwapper_internal:
		dc.w @f1-Map_PathSwapper_internal
		dc.w @f2-Map_PathSwapper_internal
		dc.w @f3-Map_PathSwapper_internal
		dc.w @f3-Map_PathSwapper_internal
		dc.w @f4-Map_PathSwapper_internal
		dc.w @f5-Map_PathSwapper_internal
		dc.w @f6-Map_PathSwapper_internal
		dc.w @f6-Map_PathSwapper_internal
@f1:		dc.b 4
		dc.b $E0, 5, 0, 0, $F8
		dc.b $F0, 5, 0, 0, $F8
		dc.b 0,   5, 0, 0, $F8
		dc.b $10, 5, 0, 0, $F8
@f2:		dc.b 4
		dc.b $C0, 5, 0, 0, $F8
		dc.b $E0, 5, 0, 0, $F8
		dc.b 0,   5, 0, 0, $F8
		dc.b $30, 5, 0, 0, $F8
@f3:		dc.b 4
		dc.b $80, 5, 0, 0, $F8
		dc.b $E0, 5, 0, 0, $F8
		dc.b 0,   5, 0, 0, $F8
		dc.b $70, 5, 0, 0, $F8
@f4:		dc.b 4
		dc.b $F8, 5, 0, 0, $E0
		dc.b $F8, 5, 0, 0, $F0
		dc.b $F8, 5, 0, 0, 0
		dc.b $F8, 5, 0, 0, $10
@f5:		dc.b 4
		dc.b $F8, 5, 0, 0, $C0
		dc.b $F8, 5, 0, 0, $E0
		dc.b $F8, 5, 0, 0, 0
		dc.b $F8, 5, 0, 0, $30
@f6:		dc.b 4
		dc.b $F8, 5, 0, 0, $80
		dc.b $F8, 5, 0, 0, $E0
		dc.b $F8, 5, 0, 0, 0
		dc.b $F8, 5, 0, 0, $70
		even
