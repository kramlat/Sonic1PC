.int:		dc.w .100-.int
		dc.w .200-.int
		dc.w .500-.int
		dc.w .1000-.int
		dc.w .10-.int
		dc.w .10000-.int
		dc.w .50000-.int
; word_A070:
.100:		dc.w 1
		dc.w $F805, 2, -8
; word_A07A:
.200:		dc.w 1
		dc.w $F805, 6, -8
; word_A084:
.500:		dc.w 1
		dc.w $F805, $A, -8
; word_A08E:
.1000:		dc.w 2
		dc.w $F801, 0, -8
		dc.w $F805, $E, 0
; word_A0A0:
.10:		dc.w 1
		dc.w $F801, 0, -4
; word_A0AA:
.10000:		dc.w 2
		dc.w $F805, 2, -$10
		dc.w $F805, $E, 0
; word_A0BC:
.50000:		dc.w 2
		dc.w $F805, $A, -$10
		dc.w $F805, $E, 0
