; ---------------------------------------------------------------------------
; Sprite mappings - invisible solid blocks (debug-only preview). Ported from
; s1disasm ProjectSonic1TwoEight's own "_maps/Invisible Barriers.asm" --
; real hardware's own Map_Invis, a dedicated 2x2-icon-only marker (tile $18
; relative to ArtTile_Monitor), NOT Monitor's own box+icon combo frames.
; Frame 0 (.solid) is the only one placed in real levels; frames 1/2 are
; wider-spread unused variants, kept for completeness.
; ---------------------------------------------------------------------------
Map_Invis_internal:
		dc.w @solid-Map_Invis_internal
		dc.w @unused1-Map_Invis_internal
		dc.w @unused2-Map_Invis_internal
@solid:		dc.b 4
		dc.b $F0, 5, 0, $18, $F0
		dc.b $F0, 5, 0, $18, 0
		dc.b 0,   5, 0, $18, $F0
		dc.b 0,   5, 0, $18, 0
@unused1:	dc.b 4
		dc.b $E0, 5, 0, $18, $C0
		dc.b $E0, 5, 0, $18, $30
		dc.b $10, 5, 0, $18, $C0
		dc.b $10, 5, 0, $18, $30
@unused2:	dc.b 4
		dc.b $E0, 5, 0, $18, $80
		dc.b $E0, 5, 0, $18, $70
		dc.b $10, 5, 0, $18, $80
		dc.b $10, 5, 0, $18, $70
		even
