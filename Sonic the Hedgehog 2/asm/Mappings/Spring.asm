; ---------------------------------------------------------------------------
; Sprite mappings - springs (Nick Arcade, Map_obj41 family), Sonic 2 mappings format
; ---------------------------------------------------------------------------
.int:		dc.w .f0-.int
		dc.w .f1-.int
		dc.w .f2-.int
		dc.w .f3-.int
		dc.w .f4-.int
		dc.w .f5-.int
		dc.w .f6-.int
		dc.w .f7-.int
		dc.w .f8-.int
		dc.w .f9-.int
		dc.w .f10-.int
; word_EA4A
.f0:		dc.w 2
		dc.w $F00D,    0,    0,$FFF0
		dc.w	 5,    8,    4,$FFF8
; word_EA5C
.f1:		dc.w 1
		dc.w $F80D,    0,    0,$FFF0
; word_EA66
.f2:		dc.w 2
		dc.w $E00D,    0,    0,$FFF0
		dc.w $F007,   $C,    6,$FFF8
; word_EA78
.f3:		dc.w 2
		dc.w $F003,    0,    0,	   0
		dc.w $F801,    4,    2,$FFF8
; word_EA8A
.f4:		dc.w 1
		dc.w $F003,    0,    0,$FFF8
; word_EA94
.f5:		dc.w 2
		dc.w $F003,    0,    0,	 $10
		dc.w $F809,    6,    3,$FFF8
; word_EAA6
.f6:		dc.w 2
		dc.w	$D,$1000,$1000,$FFF0
		dc.w $F005,$1008,$1004,$FFF8
; word_EAB8
.f7:		dc.w 4
		dc.w $F00D,    0,    0,$FFF0
		dc.w	 5,    8,    4,	   0
		dc.w $FB05,   $C,    6,$FFF6
		dc.w	 5,$201C,$200E,$FFF0
; word_EADA
.f8:		dc.w 3
		dc.w $F60D,    0,    0,$FFEA
		dc.w  $605,    8,    4,$FFFA
		dc.w	 5,$201C,$200E,$FFF0
; word_EAF4
.f9:		dc.w 4
		dc.w $E60D,    0,    0,$FFFB
		dc.w $F605,    8,    4,	  $B
		dc.w $F30B,  $10,    8,$FFF6
		dc.w	 5,$201C,$200E,$FFF0
; word_EB16
.f10:		dc.w 4
		dc.w	$D,$1000,$1000,$FFF0
		dc.w $F005,$1008,$1004,	   0
		dc.w $F505,$100C,$1006,$FFF6
		dc.w $F005,$301C,$300E,$FFF0
