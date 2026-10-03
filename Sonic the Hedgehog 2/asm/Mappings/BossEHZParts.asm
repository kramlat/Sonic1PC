; ---------------------------------------------------------------------------
; Map_Obj58 of Nick Arcade (s2.asm), Sonic 2 mappings format
; ---------------------------------------------------------------------------
.int:		dc.w .f0-.int
		dc.w .f1-.int
		dc.w .f2-.int
		dc.w .f3-.int
		dc.w .f4-.int
		dc.w .f5-.int
		dc.w .f6-.int
.f0:		dc.w 1			
		dc.w $D805,    0,    0,	   2
.f1:		dc.w 5			
		dc.w $D805,    4,    2,	   2
		dc.w $D80D,   $C,    6,	 $12
		dc.w $D80D,   $C,    6,	 $32
		dc.w $D80D,   $C,    6,$FFE2
		dc.w $D80D,   $C,    6,$FFC2
.f2:		dc.w 5			
		dc.w $D805,    4,    2,	   2
		dc.w $D80D,   $C,    6,	 $12
		dc.w $D805,    8,    4,	 $32
		dc.w $D80D,   $C,    6,$FFE2
		dc.w $D805,    8,    4,$FFD2
.f3:		dc.w 3			
		dc.w $D805,    4,    2,	   2
		dc.w $D80D,   $C,    6,	 $12
		dc.w $D80D,   $C,    6,$FFE2
.f4:		dc.w 3			
		dc.w $D805,    4,    2,	   2
		dc.w $D805,    8,    4,	 $12
		dc.w $D805,    8,    4,$FFF2
.f5:		dc.w 3			
		dc.w $D805,    0,    0,	   2
		dc.w $D80D,   $C,    6,	 $12
		dc.w $D80D,   $C,    6,	 $32
.f6:		dc.w 3			
		dc.w $D805,    4,    2,	   2
		dc.w $D80D,   $C,    6,$FFE2
		dc.w $D80D,   $C,    6,$FFC2
