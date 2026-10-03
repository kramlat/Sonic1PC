; ---------------------------------------------------------------------------
; Animation script - fireball that sits on the floor (MZ). Alternates
; between the mapping table's two "vertical" frames, x-flipping every
; other tick for a bit of visual variety while it burns.
; ---------------------------------------------------------------------------
Animation_GrassFire:
		dc.w @burn-Animation_GrassFire

@burn:		dc.b 5
		dc.b 0, $20, 1, $21 ; frames 0/1, second copy of each x-flipped ($20 = aniXFlip)
		dc.b $FF ; afEnd -- loop
		even
