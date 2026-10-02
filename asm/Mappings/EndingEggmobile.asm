; ---------------------------------------------------------------------------
; Sprite mappings - Eggman's wrecked Eggmobile in the distance of the ending (art: "Unused - Eggman Ending").
; The art holds the Eggmobile pod (tiles 0-3), three pieces of debris (4-15), a small fireball (16, 17) and
; a big one (21-24), a starburst explosion (25-28) and two smoke puffs (29-37 dense, 38-46 fading).
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_EShip_internal:	mappingsTable
	mappingsTableEntry.w	@pod
	mappingsTableEntry.w	@burning
	mappingsTableEntry.w	@debris1
	mappingsTableEntry.w	@debris2
	mappingsTableEntry.w	@debris3
	mappingsTableEntry.w	@explosion
	mappingsTableEntry.w	@fireball
	mappingsTableEntry.w	@spark
	mappingsTableEntry.w	@ember
	mappingsTableEntry.w	@smoke
	mappingsTableEntry.w	@smokefade

@pod:	spriteHeader
	spritePiece	-8, -8, 2, 2, 0, 0, 0, 0, 0
@pod_End

@burning:	spriteHeader
	spritePiece	-8, -8, 2, 2, 0, 0, 0, 0, 0
	spritePiece	0, -12, 2, 2, $15, 0, 0, 0, 0
@burning_End

@debris1:	spriteHeader
	spritePiece	-8, -8, 2, 2, 4, 0, 0, 0, 0
@debris1_End

@debris2:	spriteHeader
	spritePiece	-8, -8, 2, 2, 8, 0, 0, 0, 0
@debris2_End

@debris3:	spriteHeader
	spritePiece	-8, -8, 2, 2, $C, 0, 0, 0, 0
@debris3_End

@explosion:	spriteHeader
	spritePiece	-8, -8, 2, 2, $19, 0, 0, 0, 0
@explosion_End

@fireball:	spriteHeader
	spritePiece	-8, -8, 2, 2, $15, 0, 0, 0, 0
@fireball_End

@spark:	spriteHeader
	spritePiece	-4, -4, 1, 1, $10, 0, 0, 0, 0
@spark_End

@ember:	spriteHeader
	spritePiece	-4, -4, 1, 1, $11, 0, 0, 0, 0
@ember_End

@smoke:	spriteHeader
	spritePiece	-$C, -$C, 3, 3, $1D, 0, 0, 0, 0
@smoke_End

@smokefade:	spriteHeader
	spritePiece	-$C, -$C, 3, 3, $26, 0, 0, 0, 0
@smokefade_End

	even
