; ---------------------------------------------------------------------------
; Sprite mappings - monitors
; ---------------------------------------------------------------------------
	include	"Mappings/_MapMacros.asm"

Map_Monitor_internal:	mappingsTable
	mappingsTableEntry.w	@static0
	mappingsTableEntry.w	@static1
	mappingsTableEntry.w	@static2
	mappingsTableEntry.w	@eggman
	mappingsTableEntry.w	@sonic
	mappingsTableEntry.w	@shoes
	mappingsTableEntry.w	@shield
	mappingsTableEntry.w	@invincible
	mappingsTableEntry.w	@rings
	mappingsTableEntry.w	@s
	mappingsTableEntry.w	@goggles
	mappingsTableEntry.w	@broken

@static0:	spriteHeader	; static monitor
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@static0_End

@static1:	spriteHeader	; static monitor
	spritePiece	-8, -$B, 2, 2, $10, 0, 0, 0, 0
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@static1_End

@static2:	spriteHeader	; static monitor
	spritePiece	-8, -$B, 2, 2, $14, 0, 0, 0, 0
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@static2_End

@eggman:	spriteHeader	; Eggman monitor
	spritePiece	-8, -$B, 2, 2, $18, 0, 0, 0, 0
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@eggman_End

@sonic:	spriteHeader	; Sonic	monitor
	spritePiece	-8, -$B, 2, 2, $1C, 0, 0, 0, 0
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@sonic_End

@shoes:	spriteHeader	; speed	shoes monitor
	spritePiece	-8, -$B, 2, 2, $24, 0, 0, 0, 0
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@shoes_End

@shield:	spriteHeader	; shield monitor
	spritePiece	-8, -$B, 2, 2, $28, 0, 0, 0, 0
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@shield_End

@invincible:	spriteHeader	; invincibility	monitor
	spritePiece	-8, -$B, 2, 2, $2C, 0, 0, 0, 0
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@invincible_End

@rings:	spriteHeader	; 10 rings monitor
	spritePiece	-8, -$B, 2, 2, $30, 0, 0, 0, 0
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@rings_End

@s:	spriteHeader	; 'S' monitor
	spritePiece	-8, -$B, 2, 2, $34, 0, 0, 0, 0
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@s_End

@goggles:	spriteHeader	; goggles monitor
	spritePiece	-8, -$B, 2, 2, $20, 0, 0, 0, 0
	spritePiece	-$10, -$11, 4, 4, 0, 0, 0, 0, 0
@goggles_End

@broken:	spriteHeader	; broken monitor
	spritePiece	-$10, -1, 4, 2, $38, 0, 0, 0, 0
@broken_End

	even
