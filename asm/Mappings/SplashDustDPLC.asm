; ---------------------------------------------------------------------------
; Uncompressed graphics loading array for the water splash and spin dash dust
; (each entry: tile count - 1 in the top nibble, first tile of the art in the lower twelve bits)
; ---------------------------------------------------------------------------
SplashDustDPLC_internal:
		dc.w	DustPLC_0-SplashDustDPLC_internal
		dc.w	DustPLC_1-SplashDustDPLC_internal
		dc.w	DustPLC_2-SplashDustDPLC_internal
		dc.w	DustPLC_3-SplashDustDPLC_internal
		dc.w	DustPLC_4-SplashDustDPLC_internal
		dc.w	DustPLC_5-SplashDustDPLC_internal
		dc.w	DustPLC_6-SplashDustDPLC_internal
		dc.w	DustPLC_7-SplashDustDPLC_internal
		dc.w	DustPLC_8-SplashDustDPLC_internal
		dc.w	DustPLC_9-SplashDustDPLC_internal
		dc.w	DustPLC_10-SplashDustDPLC_internal
		dc.w	DustPLC_11-SplashDustDPLC_internal
		dc.w	DustPLC_12-SplashDustDPLC_internal
		dc.w	DustPLC_13-SplashDustDPLC_internal
		dc.w	DustPLC_14-SplashDustDPLC_internal
		dc.w	DustPLC_15-SplashDustDPLC_internal
		dc.w	DustPLC_16-SplashDustDPLC_internal
		dc.w	DustPLC_17-SplashDustDPLC_internal
		dc.w	DustPLC_18-SplashDustDPLC_internal
		dc.w	DustPLC_19-SplashDustDPLC_internal
		dc.w	DustPLC_20-SplashDustDPLC_internal
		dc.w	DustPLC_21-SplashDustDPLC_internal
DustPLC_0:	dc.b	0
DustPLC_1:	dc.b	1
		dc.b	$70, $00		; 8 tiles from tile $0
DustPLC_2:	dc.b	1
		dc.b	$F0, $08		; 16 tiles from tile $8
DustPLC_3:	dc.b	1
		dc.b	$F0, $18		; 16 tiles from tile $18
DustPLC_4:	dc.b	1
		dc.b	$F0, $28		; 16 tiles from tile $28
DustPLC_5:	dc.b	1
		dc.b	$F0, $38		; 16 tiles from tile $38
DustPLC_6:	dc.b	1
		dc.b	$F0, $48		; 16 tiles from tile $48
DustPLC_7:	dc.b	1
		dc.b	$70, $58		; 8 tiles from tile $58
DustPLC_8:	dc.b	1
		dc.b	$70, $60		; 8 tiles from tile $60
DustPLC_9:	dc.b	1
		dc.b	$70, $68		; 8 tiles from tile $68
DustPLC_10:	dc.b	1
		dc.b	$70, $70		; 8 tiles from tile $70
DustPLC_11:	dc.b	1
		dc.b	$70, $78		; 8 tiles from tile $78
DustPLC_12:	dc.b	1
		dc.b	$70, $80		; 8 tiles from tile $80
DustPLC_13:	dc.b	2
		dc.b	$10, $88		; 2 tiles from tile $88
		dc.b	$70, $8A		; 8 tiles from tile $8A
DustPLC_14:	dc.b	2
		dc.b	$30, $92		; 4 tiles from tile $92
		dc.b	$70, $96		; 8 tiles from tile $96
DustPLC_15:	dc.b	2
		dc.b	$50, $9E		; 6 tiles from tile $9E
		dc.b	$70, $A4		; 8 tiles from tile $A4
DustPLC_16:	dc.b	2
		dc.b	$50, $AC		; 6 tiles from tile $AC
		dc.b	$70, $B2		; 8 tiles from tile $B2
DustPLC_17:	dc.b	0
DustPLC_18:	dc.b	0
DustPLC_19:	dc.b	0
DustPLC_20:	dc.b	1
		dc.b	$F0, $BA		; 16 tiles from tile $BA
DustPLC_21:	dc.b	0
		even
