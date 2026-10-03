; ---------------------------------------------------------------------------
; Animation script - lava balls (MZ, SLZ)
; ---------------------------------------------------------------------------
Animation_Fireballs:
		dc.w @vertical-Animation_Fireballs
		dc.w @vertcollide-Animation_Fireballs
		dc.w @horizontal-Animation_Fireballs
		dc.w @horicollide-Animation_Fireballs

@vertical:	dc.b 5
		dc.b 0, $20, 1, $21 ; frames 0/1, $20 = aniXFlip
		dc.b $FF ; afEnd -- loop
		even

@vertcollide:	dc.b 5
		dc.b 2
		dc.b $FC ; afRoutine -- advance obRoutine once this collide frame's timer expires
		even

@horizontal:	dc.b 5
		dc.b 3, $43, 4, $44 ; frames 3/4, $40 = aniYFlip
		dc.b $FF ; afEnd -- loop
		even

@horicollide:	dc.b 5
		dc.b 5
		dc.b $FC ; afRoutine
		even
