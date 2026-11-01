; ---------------------------------------------------------------------------
; The SEGA screen's sprites in the Sonic 2 pre-beta (the final game's): Sonic running across the logo, at twice his size (frames 0-3: the art is made in RAM by the screen and put at tile $88), and the two
; blocks that hide the trademark (frame 4: tile 3 of the logo's art). Map of its objects $B0 and $B1 (Sonic 2 mappings format).
; ---------------------------------------------------------------------------
.int:		dc.w .f0-.int
		dc.w .f1-.int
		dc.w .f2-.int
		dc.w .f3-.int
		dc.w .f4-.int
.f0:		dc.w 6
		dc.w $D80F, $0000, $0000, $FFF0
		dc.w $D807, $0010, $0008, $0010
		dc.w $F80F, $0018, $000C, $FFE0
		dc.w $180F, $0028, $0014, $FFE0
		dc.w $F80F, $0038, $001C, $0000
		dc.w $180F, $0048, $0024, $0000
.f1:		dc.w 6
		dc.w $D80F, $0058, $002C, $FFF0
		dc.w $D807, $0068, $0034, $0010
		dc.w $F80F, $0070, $0038, $FFE0
		dc.w $180F, $0080, $0040, $FFE0
		dc.w $F80F, $0090, $0048, $0000
		dc.w $180F, $00A0, $0050, $0000
.f2:		dc.w 6
		dc.w $D80F, $00B0, $0058, $FFF0
		dc.w $D807, $00C0, $0060, $0010
		dc.w $F80F, $00C8, $0064, $FFE0
		dc.w $180F, $00D8, $006C, $FFE0
		dc.w $F80F, $00E8, $0074, $0000
		dc.w $180F, $00F8, $007C, $0000
.f3:		dc.w 6
		dc.w $D80F, $0108, $0084, $FFF0
		dc.w $D807, $0118, $008C, $0010
		dc.w $F80F, $0120, $0090, $FFE0
		dc.w $180F, $0130, $0098, $FFE0
		dc.w $F80F, $0140, $00A0, $0000
		dc.w $180F, $0150, $00A8, $0000
.f4:		dc.w 2
		dc.w $FC00, $0000, $0000, $FFF8
		dc.w $FC00, $0000, $0000, $0000
