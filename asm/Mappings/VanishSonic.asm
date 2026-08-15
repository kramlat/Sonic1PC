; ---------------------------------------------------------------------------
; Sprite mappings - special stage entry from beta (unused)
; ---------------------------------------------------------------------------
Map_Vanish_internal:
		dc.w @flash1-Map_Vanish_internal
		dc.w @flash2-Map_Vanish_internal
		dc.w @flash3-Map_Vanish_internal
		dc.w @sparkle1-Map_Vanish_internal
		dc.w @sparkle2-Map_Vanish_internal
		dc.w @sparkle3-Map_Vanish_internal
		dc.w @sparkle4-Map_Vanish_internal
		dc.w @blank-Map_Vanish_internal ; real hardware points this at a random $00 byte inside sparkle4's own data as a space-saving hack; a proper 0-piece frame is equivalent and far less fragile to hand-transcribe

@flash1:	dc.b 3
		dc.b $F8, $0, $0, $00, $08
		dc.b $00, $4, $0, $01, $00
		dc.b $08, $0, $10, $00, $08

@flash2:	dc.b 3
		dc.b $F0, $D, $0, $03, $F0
		dc.b $00, $C, $0, $0B, $F0
		dc.b $08, $D, $10, $03, $F0

@flash3:	dc.b 5
		dc.b $E4, $E, $0, $0F, $F4
		dc.b $EC, $2, $0, $1B, $EC
		dc.b $FC, $C, $0, $1E, $F4
		dc.b $04, $E, $10, $0F, $F4
		dc.b $04, $1, $10, $1B, $EC

@sparkle1:	dc.b 9
		dc.b $F0, $8, $0, $22, $F8
		dc.b $F8, $E, $0, $25, $F0
		dc.b $10, $8, $0, $31, $F0
		dc.b $00, $5, $0, $34, $10
		dc.b $F8, $0, $8, $25, $10
		dc.b $F0, $0, $18, $36, $18
		dc.b $F8, $0, $18, $25, $20
		dc.b $00, $0, $8, $25, $28
		dc.b $F8, $0, $0, $25, $30

@sparkle2:	dc.b 18
		dc.b $00, $0, $18, $25, $F0
		dc.b $F8, $4, $0, $38, $F8
		dc.b $F0, $0, $0, $26, $08
		dc.b $00, $0, $0, $25, $00
		dc.b $08, $0, $18, $25, $F8
		dc.b $10, $0, $10, $26, $00
		dc.b $08, $0, $10, $38, $08
		dc.b $F8, $0, $0, $29, $10
		dc.b $00, $0, $0, $26, $10
		dc.b $00, $0, $0, $2D, $18
		dc.b $08, $0, $8, $26, $18
		dc.b $08, $0, $0, $29, $20
		dc.b $F8, $0, $0, $26, $20
		dc.b $F8, $0, $0, $2D, $28
		dc.b $00, $0, $0, $3A, $28
		dc.b $F8, $0, $18, $26, $30
		dc.b $00, $0, $10, $25, $38
		dc.b $F8, $0, $10, $25, $40

@sparkle3:	dc.b 17
		dc.b $F8, $0, $8, $25, $00
		dc.b $F0, $0, $0, $38, $10
		dc.b $10, $0, $8, $25, $00
		dc.b $00, $0, $18, $25, $10
		dc.b $08, $0, $10, $25, $18
		dc.b $F8, $0, $18, $25, $20
		dc.b $00, $0, $10, $26, $28
		dc.b $F8, $0, $10, $25, $30
		dc.b $00, $0, $0, $25, $30
		dc.b $08, $0, $8, $25, $30
		dc.b $00, $0, $8, $26, $38
		dc.b $08, $0, $0, $29, $38
		dc.b $F8, $0, $8, $26, $40
		dc.b $00, $0, $0, $2D, $40
		dc.b $F8, $0, $8, $25, $48
		dc.b $00, $0, $0, $25, $48
		dc.b $00, $0, $10, $25, $50

@sparkle4:	dc.b 9
		dc.b $FC, $0, $8, $26, $30
		dc.b $04, $0, $8, $25, $28
		dc.b $04, $0, $10, $27, $38
		dc.b $04, $0, $8, $26, $40
		dc.b $FC, $0, $10, $25, $40
		dc.b $FC, $0, $10, $26, $48
		dc.b $0C, $0, $8, $27, $48
		dc.b $04, $0, $18, $26, $50
		dc.b $04, $0, $8, $27, $58

@blank:		dc.b 0
		even
