; ---------------------------------------------------------------------------
; Sprite mappings - lava tag debug marker (4 Eggman-icon tiles, scaled to
; each subtype's real hurt-hitbox corners). Ported from s2disasm's own
; Obj31 "Lava collision marker" (mappings/sprite/obj31_b.asm) -- that uses
; a "?" question-mark monitor icon (Sonic 2 has one, Sonic 1 doesn't); this
; project substitutes the Eggman icon instead, same tile ($18, relative to
; ArtTile_Monitor) InvisibleBarrier's own Map_Invis already uses.
; Frame 0/1/2 match LavaTag.c's own ltag_col_types indices (64x64/128x64/
; 256x64 hurt boxes) exactly -- obj->frame is just set to the subtype.
; ---------------------------------------------------------------------------
Map_LavaTagMarker_internal:
		dc.w @f0-Map_LavaTagMarker_internal
		dc.w @f1-Map_LavaTagMarker_internal
		dc.w @f2-Map_LavaTagMarker_internal
@f0:		dc.b 4
		dc.b $E0, 5, 0, $18, $E0
		dc.b $E0, 5, 0, $18, $10
		dc.b $10, 5, 0, $18, $E0
		dc.b $10, 5, 0, $18, $10
@f1:		dc.b 4
		dc.b $E0, 5, 0, $18, $C0
		dc.b $E0, 5, 0, $18, $30
		dc.b $10, 5, 0, $18, $C0
		dc.b $10, 5, 0, $18, $30
@f2:		dc.b 4
		dc.b $E0, 5, 0, $18, $80
		dc.b $E0, 5, 0, $18, $70
		dc.b $10, 5, 0, $18, $80
		dc.b $10, 5, 0, $18, $70
		even
