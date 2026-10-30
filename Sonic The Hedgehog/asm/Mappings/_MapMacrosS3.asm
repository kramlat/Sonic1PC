; ---------------------------------------------------------------------------
; Sprite mapping macros for the objects that have moved to the status table of
; Sonic 3 & Knuckles (engine/Sst.h), SN 68k (asm68k) syntax. The same macros as
; _MapMacros.asm, but for that game's mappings format (engine/Mappings.h,
; SPRITE_PIECE_SIZE_S3): a word piece count, then 6-byte pieces (y and size,
; the tile attributes, x), without the 2-player tile of the Sonic 2 format.
; A mappings file includes this one instead of _MapMacros.asm when the object
; that draws it has moved over; _MapMacros.asm goes when the last has.
; ---------------------------------------------------------------------------

mappingsTable macro
current_mappings_table set *
	endm

mappingsTableEntry macro ptr
	dc.\0	ptr-current_mappings_table
	endm

spriteHeader macro *
\*:
	dc.w	((\*_End-\*_Begin)/6)
\*_Begin:
	endm

; One sprite piece (6 bytes): y and size, the tile attributes, x.
spritePiece macro xpos,ypos,width,height,tile,xflip,yflip,pal,pri
	dc.w	((ypos&$FF)<<8)|(((width-1)&3)<<2)|((height-1)&3)
	dc.w	(((pri&1)<<15)|((pal&3)<<13)|((yflip&1)<<12)|((xflip&1)<<11))+(tile)
	dc.w	xpos
	endm
