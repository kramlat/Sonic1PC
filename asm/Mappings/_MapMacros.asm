; ---------------------------------------------------------------------------
; Sprite mapping macros, SN 68k (asm68k) syntax.
;
; Same macros as the Sonic 1 disassembly's _maps/_MapMacros.asm, rewritten for
; the assembler this project uses (clownassembler, which mimics SN 68k). The
; original relies on {INTLABEL}, which is specific to the Macro Assembler AS;
; here a macro declared with the parameter '*' receives the label on its own
; invocation line as '\*' instead. Sonic 1 format only: 1-byte piece count,
; 5-byte pieces.
; ---------------------------------------------------------------------------

; Declares a mappings table. The label on the same line marks its start:
;	Map_Example_internal:	mappingsTable
mappingsTable macro
current_mappings_table set *
	endm

; One entry of a mappings table: offset from the table start to a frame.
;	mappingsTableEntry.w	.frame
mappingsTableEntry macro ptr
	dc.\0	ptr-current_mappings_table
	endm

; Starts a frame. The label on the same line names it, and a '<label>_End'
; label after its last piece makes the piece count automatic:
;	.frame:	spriteHeader
;		spritePiece ...
;	.frame_End
spriteHeader macro *
\*:
	dc.b	((\*_End-\*_Begin)/5)
\*_Begin:
	endm

; One sprite piece (5 bytes).
spritePiece macro xpos,ypos,width,height,tile,xflip,yflip,pal,pri
	dc.b	ypos
	dc.b	(((width-1)&3)<<2)|((height-1)&3)
	dc.b	((((pri&1)<<15)|((pal&3)<<13)|((yflip&1)<<12)|((xflip&1)<<11))+(tile))>>8
	dc.b	tile&$FF
	dc.b	xpos
	endm
