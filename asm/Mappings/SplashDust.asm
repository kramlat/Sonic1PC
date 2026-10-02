; ---------------------------------------------------------------------------
; Sprite mappings - water splash and spin dash dust (Obj_Splash)
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_SplashDust_internal:	mappingsTable
	mappingsTableEntry.w	@dust_0
	mappingsTableEntry.w	@dust_1
	mappingsTableEntry.w	@dust_2
	mappingsTableEntry.w	@dust_3
	mappingsTableEntry.w	@dust_4
	mappingsTableEntry.w	@dust_5
	mappingsTableEntry.w	@dust_6
	mappingsTableEntry.w	@dust_7
	mappingsTableEntry.w	@dust_8
	mappingsTableEntry.w	@dust_9
	mappingsTableEntry.w	@dust_10
	mappingsTableEntry.w	@dust_11
	mappingsTableEntry.w	@dust_12
	mappingsTableEntry.w	@dust_13
	mappingsTableEntry.w	@dust_14
	mappingsTableEntry.w	@dust_15
	mappingsTableEntry.w	@dust_16
	mappingsTableEntry.w	@dust_17
	mappingsTableEntry.w	@dust_18
	mappingsTableEntry.w	@dust_19
	mappingsTableEntry.w	@dust_20
	mappingsTableEntry.w	@dust_21

@dust_0:	spriteHeader
@dust_0_End

@dust_1:	spriteHeader
	spritePiece	-$10, -$E, 4, 2, $0, 0, 0, 0, 0
@dust_1_End

@dust_2:	spriteHeader
	spritePiece	-$10, -$1E, 4, 4, $0, 0, 0, 0, 0
@dust_2_End

@dust_3:	spriteHeader
	spritePiece	-$10, -$1E, 4, 4, $0, 0, 0, 0, 0
@dust_3_End

@dust_4:	spriteHeader
	spritePiece	-$10, -$1E, 4, 4, $0, 0, 0, 0, 0
@dust_4_End

@dust_5:	spriteHeader
	spritePiece	-$10, -$1E, 4, 4, $0, 0, 0, 0, 0
@dust_5_End

@dust_6:	spriteHeader
	spritePiece	-$10, -$1E, 4, 4, $0, 0, 0, 0, 0
@dust_6_End

@dust_7:	spriteHeader
	spritePiece	-$10, -$E, 4, 2, $0, 0, 0, 0, 0
@dust_7_End

@dust_8:	spriteHeader
	spritePiece	-$10, -$E, 4, 2, $0, 0, 0, 0, 0
@dust_8_End

@dust_9:	spriteHeader
	spritePiece	-$10, -$E, 4, 2, $0, 0, 0, 0, 0
@dust_9_End

@dust_10:	spriteHeader
	spritePiece	-$20, $4, 4, 2, $0, 0, 0, 0, 0
@dust_10_End

@dust_11:	spriteHeader
	spritePiece	-$20, $4, 4, 2, $0, 0, 0, 0, 0
@dust_11_End

@dust_12:	spriteHeader
	spritePiece	-$20, $4, 4, 2, $0, 0, 0, 0, 0
@dust_12_End

@dust_13:	spriteHeader
	spritePiece	-$18, -$C, 1, 2, $0, 0, 0, 0, 0
	spritePiece	-$20, $4, 4, 2, $2, 0, 0, 0, 0
@dust_13_End

@dust_14:	spriteHeader
	spritePiece	-$18, -$C, 2, 2, $0, 0, 0, 0, 0
	spritePiece	-$20, $4, 4, 2, $4, 0, 0, 0, 0
@dust_14_End

@dust_15:	spriteHeader
	spritePiece	-$20, -$C, 3, 2, $0, 0, 0, 0, 0
	spritePiece	-$20, $4, 4, 2, $6, 0, 0, 0, 0
@dust_15_End

@dust_16:	spriteHeader
	spritePiece	-$20, -$C, 3, 2, $0, 0, 0, 0, 0
	spritePiece	-$20, $4, 4, 2, $6, 0, 0, 0, 0
@dust_16_End

@dust_17:	spriteHeader
	spritePiece	-$8, -$8, 2, 2, $0, 0, 0, 0, 0
@dust_17_End

@dust_18:	spriteHeader
	spritePiece	-$8, -$8, 2, 2, $4, 0, 0, 0, 0
@dust_18_End

@dust_19:	spriteHeader
	spritePiece	-$8, -$8, 2, 2, $8, 0, 0, 0, 0
@dust_19_End

@dust_20:	spriteHeader
	spritePiece	-$8, -$8, 2, 2, $C, 0, 0, 0, 0
@dust_20_End

@dust_21:	spriteHeader
@dust_21_End

	even
