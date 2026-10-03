; Mus94 - "SSRG" -- short arrangement in the spirit of Sega AM2's driving,
; uptempo arcade-synth style (Space Harrier's main theme, the tune commonly
; reused as a jingle on Sonic Retro hack demo/title screens) -- NOT a slow
; brass fanfare, which the first draft wrongly was. A fast, syncopated synth
; lead over a rapid arpeggiated bass fits that style far better. Original
; arrangement written directly against smps2asm syntax -- there is no
; disassembly source for this, since it isn't part of Sonic 1.
Mus94_SSRG_Header:
	smpsHeaderStartSong 1
	smpsHeaderVoice     Mus94_SSRG_Voices
	smpsHeaderChan      $06, $03
	smpsHeaderTempo     $02, $04

	smpsHeaderDAC       Mus94_SSRG_DAC
	smpsHeaderFM        Mus94_SSRG_FM1,	$F4, $20
	smpsHeaderFM        Mus94_SSRG_FM2,	$F4, $10
	smpsHeaderFM        Mus94_SSRG_FM3,	$F4, $0C
	smpsHeaderFM        Mus94_SSRG_FM4,	$F4, $0C
	smpsHeaderFM        Mus94_SSRG_FM5,	$F4, $0E
	smpsHeaderPSG       Mus94_SSRG_PSG1,	$D0, $05, $00, fTone_05
	smpsHeaderPSG       Mus94_SSRG_PSG2,	$DC, $04, $00, fTone_04
	smpsHeaderPSG       Mus94_SSRG_PSG3,	$00, $02, $00, fTone_03

; FM1 -- punchy synth lead, sixteenth-note-driven and syncopated
Mus94_SSRG_FM1:
	smpsSetvoice        $00
	smpsPan             panCenter, $00

Mus94_SSRG_Loop00:
	dc.b	nRst, $04, nD6, $04, nD6, nFs6, nRst, nA6, $02, nA6
	dc.b	nG6, $04, nFs6, nRst, $02, nD6, $04, nD6, nFs6, nRst
	dc.b	nA6, $02, nA6, nB6, $04, nA6, nFs6, $08
	dc.b	nRst, $04, nE6, $04, nE6, nA6, nRst, nCs7, $02, nCs7
	dc.b	nB6, $04, nA6, nRst, $02, nE6, $04, nE6, nA6, nRst
	dc.b	nCs7, $02, nCs7, nD7, $04, nCs7, nA6, $08
	smpsLoop            $00, $01, Mus94_SSRG_Loop00
	smpsNoteFill        $00
	dc.b	nRst, $04, nD6, $04, nFs6, nA6, nD7, $08, nRst, $10
	smpsStop

; FM2 -- syncopated stabs a fourth below the lead, filling the offbeats
Mus94_SSRG_FM2:
	smpsSetvoice        $00
	smpsPan             panCenter, $00
	dc.b	nRst, $04

Mus94_SSRG_Loop01:
	dc.b	nA5, $02, nRst, $02, nA5, $02, nRst, $02, nD6, $02, nRst
	dc.b	$02, nA5, $02, nRst, $02, nA5, $02, nRst, $02, nA5, $02
	dc.b	nRst, $02, nD6, $02, nRst, $02, nCs6, $04, nRst, $04
	dc.b	nE6, $02, nRst, $02, nE6, $02, nRst, $02, nA6, $02, nRst
	dc.b	$02, nE6, $02, nRst, $02, nE6, $02, nRst, $02, nE6, $02
	dc.b	nRst, $02, nA6, $02, nRst, $02, nGs6, $04, nRst, $04
	smpsLoop            $00, $01, Mus94_SSRG_Loop01
	smpsNoteFill        $00
	dc.b	nRst, $10, nRst, $10
	smpsStop

; FM3/FM4 -- unused this arrangement, kept for the driver's fixed
; 6 FM / 3 PSG channel count (same as every real Sonic 1 song)
Mus94_SSRG_FM3:
	smpsSetvoice        $01
	smpsPan             panLeft, $00
	smpsJump            Mus94_SSRG_Loop00

Mus94_SSRG_FM4:
	smpsSetvoice        $00
	smpsPan             panRight, $00
	smpsJump            Mus94_SSRG_Loop04

; FM5 -- high sync-lead doubling, an octave up, thinned to every other hit
Mus94_SSRG_FM5:
	smpsSetvoice        $01
	smpsPan             panCenter, $00
	dc.b	nRst, $08

Mus94_SSRG_Loop02:
	dc.b	nRst, $04, nD7, $04, nRst, $04, nRst, $04, nA7, $02, nRst
	dc.b	$06, nRst, $04, nRst, $02, nD7, $04, nRst, $04, nRst, $04
	dc.b	nA7, $02, nRst, $02, nBb7, $04, nRst, $0C
	dc.b	nRst, $04, nE7, $04, nRst, $04, nRst, $04, nCs7, $02, nRst
	dc.b	$06, nRst, $04, nRst, $02, nE7, $04, nRst, $04, nRst, $04
	dc.b	nCs7, $02, nRst, $02, nD7, $04, nRst, $0C
	smpsLoop            $00, $01, Mus94_SSRG_Loop02
	smpsNoteFill        $00
	dc.b	nRst, $20
	smpsStop

; PSG1 -- fast root-fifth-octave arpeggio, the driving AM2-style bass
Mus94_SSRG_PSG1:
	smpsPSGvoice        $00

Mus94_SSRG_Loop03:
	dc.b	nD3, $02, nA3, nD4, nA3, nD3, $02, nA3, nD4, nA3
	dc.b	nD3, $02, nA3, nD4, nA3, nG3, $02, nD4, nG4, nD4
	dc.b	nD3, $02, nA3, nD4, nA3, nD3, $02, nA3, nD4, nA3
	dc.b	nD3, $02, nA3, nD4, nA3, nA2, $02, nE3, nA3, nE3
	dc.b	nE3, $02, nB3, nE4, nB3, nE3, $02, nB3, nE4, nB3
	dc.b	nE3, $02, nB3, nE4, nB3, nA3, $02, nE4, nA4, nE4
	dc.b	nE3, $02, nB3, nE4, nB3, nE3, $02, nB3, nE4, nB3
	dc.b	nE3, $02, nB3, nE4, nB3, nB2, $02, nFs3, nB3, nFs3
	smpsLoop            $00, $01, Mus94_SSRG_Loop03
	smpsNoteFill        $00
	dc.b	nD3, $02, nA3, nD4, nA3, nD3, $10
	smpsStop

; PSG2 -- offbeat upper chord stabs
Mus94_SSRG_PSG2:
	smpsPSGvoice        $01

Mus94_SSRG_Loop04:
	dc.b	nRst, $02, nFs4, $02, nRst, $02, nFs4, $02, nRst, $02, nA4, $02
	dc.b	nRst, $02, nFs4, $02, nRst, $02, nFs4, $02, nRst, $02, nFs4, $02
	dc.b	nRst, $02, nA4, $02, nRst, $02, nG4, $04, nRst, $04
	dc.b	nRst, $02, nCs5, $02, nRst, $02, nCs5, $02, nRst, $02, nE5, $02
	dc.b	nRst, $02, nCs5, $02, nRst, $02, nCs5, $02, nRst, $02, nCs5, $02
	dc.b	nRst, $02, nE5, $02, nRst, $02, nB4, $04, nRst, $04
	smpsLoop            $00, $01, Mus94_SSRG_Loop04
	smpsNoteFill        $00
	dc.b	nRst, $10, nRst, $10
	smpsStop

; PSG3 -- high accent stab on the lead's syncopated hits
Mus94_SSRG_PSG3:
	smpsPSGvoice        $02

Mus94_SSRG_Loop05:
	dc.b	nRst, $06, nA5, $02, nRst, $08
	dc.b	nRst, $06, nA5, $02, nRst, $04
	dc.b	nRst, $06, nCs6, $02, nRst, $08
	dc.b	nRst, $06, nCs6, $02, nRst, $04
	smpsLoop            $00, $01, Mus94_SSRG_Loop05
	smpsNoteFill        $00
	dc.b	nRst, $20
	smpsStop

; DAC -- driving four-on-the-floor kick with backbeat snare
Mus94_SSRG_DAC:
Mus94_SSRG_Loop06:
	dc.b	dKick, $04, dSnare, $04, dKick, $02, dKick, $02, dSnare, $04
	smpsLoop            $00, $0F, Mus94_SSRG_Loop06
	smpsNoteFill        $00
	smpsStop

; Voice table
Mus94_SSRG_Voices:

;	Voice $00 -- bright, percussive synth lead (fast attack, sharp decay)
	smpsVcAlgorithm     $07
	smpsVcFeedback      $05
	smpsVcUnusedBits    $00
	smpsVcDetune        $00, $00, $00, $00
	smpsVcCoarseFreq    $01, $02, $01, $01
	smpsVcRateScale     $01, $01, $01, $01
	smpsVcAttackRate    $1F, $1F, $1F, $1F
	smpsVcAmpMod        $00, $00, $00, $00
	smpsVcDecayRate1    $0A, $0A, $0A, $0A
	smpsVcDecayRate2    $04, $04, $04, $04
	smpsVcDecayLevel    $02, $02, $02, $02
	smpsVcReleaseRate   $09, $09, $09, $09
	smpsVcTotalLevel    $02, $02, $02, $18

;	Voice $01 -- thin sync-lead double, an octave doubler
	smpsVcAlgorithm     $07
	smpsVcFeedback      $03
	smpsVcUnusedBits    $00
	smpsVcDetune        $02, $02, $02, $02
	smpsVcCoarseFreq    $02, $02, $02, $02
	smpsVcRateScale     $01, $01, $01, $01
	smpsVcAttackRate    $1F, $1F, $1F, $1F
	smpsVcAmpMod        $00, $00, $00, $00
	smpsVcDecayRate1    $0C, $0C, $0C, $0C
	smpsVcDecayRate2    $05, $05, $05, $05
	smpsVcDecayLevel    $02, $02, $02, $02
	smpsVcReleaseRate   $0A, $0A, $0A, $0A
	smpsVcTotalLevel    $08, $08, $08, $20
