; ---------------------------------------------------------------------------
; Animation script - bubbles (LZ)
; ---------------------------------------------------------------------------
Animation_Bubbles:
		dc.w @small-Animation_Bubbles
		dc.w @medium-Animation_Bubbles
		dc.w @large-Animation_Bubbles
		dc.w @incroutine-Animation_Bubbles
		dc.w @incroutine-Animation_Bubbles
		dc.w @burst-Animation_Bubbles
		dc.w @bubmaker-Animation_Bubbles

@small:		dc.b 14
		dc.b 0, 1, 2
		dc.b $FC ; afRoutine -- small bubble forming
@medium:	dc.b 14
		dc.b 1, 2, 3, 4
		dc.b $FC ; afRoutine -- medium bubble forming
@large:		dc.b 14
		dc.b 2, 3, 4, 5, 6
		dc.b $FC ; afRoutine -- full size bubble forming
@incroutine:	dc.b 4
		dc.b $FC ; afRoutine -- increment routine counter (no animation)
@burst:		dc.b 4
		dc.b 6, 7, 8
		dc.b $FC ; afRoutine -- large bubble bursts
@bubmaker:	dc.b 15
		dc.b $13, $14, $15
		dc.b $FF ; afEnd -- bubble maker on the floor (loop last frame)
		even
