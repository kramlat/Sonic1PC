#!/usr/bin/perl
# Imports Sonic's and Tails' art, mappings, DPLC and animation data from the Sonic 2 alpha (Aug 21st 1992; Clownacy's disassembly and its built ROM, sonic2alpha.bin) into the
# port's res folder. The data is sliced from the ROM at the addresses the disassembly's labels give (the sizes are those of the .asm data, checked here against the label that follows), the
# art is the disassembly's uncompressed file.
#
# Usage: import_s2alpha_character.pl [ALPHA-DIR [RES-DIR]]   (defaults: ~/Projects/s2aug21-disasm, "Sonic the Hedgehog 2/res")
use strict;
use warnings;

my $dir = shift // "$ENV{HOME}/Projects/s2aug21-disasm";
my $res = shift // "Sonic the Hedgehog 2/res";

open my $rf, '<:raw', "$dir/sonic2alpha.bin" or die "cannot read the alpha ROM: $!\n";
local $/;
my $rom = <$rf>;
close $rf;

sub slice {
    my ($from, $to) = @_;
    return substr($rom, $from, $to - $from);
}

sub write_file {
    my ($path, $data) = @_;
    open my $o, '>:raw', "$res/$path" or die "cannot write $res/$path: $!\n";
    print $o $data;
    close $o;
    printf "%-28s %6d bytes\n", $path, length $data;
}

sub copy_file {
    my ($from, $path) = @_;
    open my $i, '<:raw', "$dir/$from" or die "cannot read $dir/$from: $!\n";
    my $data = <$i>;
    close $i;
    write_file($path, $data);
}

# Tails ("Miles"): art, mappings (Tails_Map to Tails_PLC), DPLC (Tails_PLC to its end), his animations (Offset_0x01227A to the tails' DPLC loader at $0123C6), and the tails'
# own animations (object 05's table at $0124EC, 11 scripts to $012544)
copy_file('Art/Uncompressed/Miles.dat', 'Art/Tails');
write_file('Mappings/Tails',     slice(0x0739E2, 0x07446C));
write_file('Mappings/TailsDPLC', slice(0x07446C, 0x074876));
write_file('Animation/Tails',     slice(0x01227A, 0x0123C6));
write_file('Animation/TailsTails', slice(0x0124EC, 0x012544));

# Sonic: art, mappings (Sonic_Map to Sonic_PLC), DPLC (Sonic_PLC to its end), and his animations ($010EB0: the table of the 32 animations and Super Sonic's, with their scripts, to $0110D4)
copy_file('Art/Uncompressed/Sonic.dat', 'Art/Sonic');
write_file('Mappings/Sonic',     slice(0x06FBE0, 0x0714E0));
write_file('Mappings/SonicDPLC', slice(0x0714E0, 0x071D8E));
write_file('Animation/Sonic',     slice(0x010EB0, 0x0110D4));

# The dust of the spin dash and the skid, and the splash of the water (object 08): its animations ($01339A), mappings ($0133C0), DPLC ($0134D6, to $013552) and art (SpshDust.dat, 202 tiles)
copy_file('Art/Uncompressed/SpshDust.dat', 'Art/DustSplash');
write_file('Animation/DustSplash',     slice(0x01339A, 0x0133C0));
write_file('Mappings/DustSplash',      slice(0x0133C0, 0x0134D6));
write_file('Mappings/DustSplashDPLC',  slice(0x0134D6, 0x013552));

# Super Sonic's stars (object 7E): the mappings ($013620, 116 bytes); the alpha has no art for them
write_file('Mappings/SuperSonicStars', slice(0x013620, 0x013694));

# The counting object (object 0A, the bubbles and the countdown of the air): its animations ($012A5E to $012AF0), the table the bubbles wobble by ($0126EC, 256 bytes), the mappings of both players ($014CFC, 204 bytes:
# Sonic's table, then Tails', which share their frames) and the art of the numbers (OxygNumb.dat, 6 frames of 6 tiles)
write_file('Animation/Countdown',        slice(0x012A5E, 0x012AF0));
write_file('Animation/CountdownWobble',  slice(0x0126EC, 0x0127EC));
write_file('Mappings/Countdown',         slice(0x014CFC, 0x014DC8));
copy_file('Art/Uncompressed/OxygNumb.dat', 'Art/CountdownNumbers');

# The bubbles' art (oxygen.nem: the bubbles of the counting object and the vents', at VRAM $AB60, tile $55B), as the alpha has it, still compressed; the numbers' art is above; and the animations of the vents' bubbles
# (object 24, $014CD2 to $014CFC)
copy_file('Art/Nemesis/Oxygen.nem', 'Art/OxygenBubbles');
copy_file('Art/Nemesis/Bubbles.nem', 'Art/CountdownBubbles'); # (the small bubbles: bubbles.nem, at VRAM $BD00, tile $5E8, which frames 1 to 4 of the bubbles' mappings point at)
write_file('Animation/OxygenBubbles', slice(0x014CD2, 0x014CFC));

# The animals that come out of a destroyed badnik (object 28): the mappings of their five kinds of frames ($00A978 to $00AA2C) and the art of the twelve animals, which the zone's art list brings in two at a time
write_file('Mappings/Animals', slice(0xA978, 0xAA2C));
for my $animal (qw(Rabbit Chicken Penguin Seal Pig BlueBird Squirrel Pigeon Mouse Monkey Turtle Bear)) {
    copy_file("Art/Nemesis/$animal.nem", "Art/Animal$animal");
}

# Emerald Hill's own: the Coconuts badnik (object 9D): the mappings ($029374, 4 frames), animations ($029366) and art, and the Buzzer's older art (Buzzer.nem: the alpha's Buzzer is the prototype's
# with other tiles)
write_file('Mappings/Coconuts',  slice(0x029374, 0x0293F4));
write_file('Animation/Coconuts', slice(0x029366, 0x029374));
copy_file('Art/Nemesis/Coconuts.nem', 'Art/Coconuts');
copy_file('Art/Nemesis/Buzzer.nem',   'Art/Buzzer');

# Emerald Hill's boss (objects 56, 5B and 58): its explosions (GHz_Boss_Mappings_02, $020430, seven frames) and their art (explosns.nem, at VRAM $B000: tile $580). The ship, car and blades are the prototype's, as the alpha has them.
write_file('Mappings/BossExplosion', slice(0x020430, 0x020484));
copy_file('Art/Nemesis/Explosns.nem', 'Art/BossExplosion');

# Metropolis's badniks: the Shellcracker (9F, with the claw's pieces as object A0: mappings $029866, animations $029856, art Shellcrc.nem) and the Asteron (A4: mappings $029FB0, animations $029FAA, art Asteron.nem)
write_file('Mappings/Shellcracker',  slice(0x029866, 0x029906));
write_file('Animation/Shellcracker', slice(0x029856, 0x029866));
copy_file('Art/Nemesis/Shellcrc.nem', 'Art/Shellcracker');
write_file('Mappings/Asteron',  slice(0x029FB0, 0x02A004));
write_file('Animation/Asteron', slice(0x029FAA, 0x029FB0));
copy_file('Art/Nemesis/Asteron.nem', 'Art/Asteron');

# The Slicer (A1) and its pincers (A2): mappings ($029AD8), the two animation tables ($029ACA, $029AD0) and art (Slicer.nem, at VRAM $8780)
write_file('Mappings/Slicer',         slice(0x029AD8, 0x029C34));
write_file('Animation/Slicer',        slice(0x029ACA, 0x029AD0));
write_file('Animation/SlicerPincers', slice(0x029AD0, 0x029AD8));
copy_file('Art/Nemesis/Slicer.nem', 'Art/Slicer');

# Metropolis's boss (objects 53, 54 and 55), the alpha's unfinished one: its mappings ($027DAA) and animations ($027D88), and the art its list $2E has (the balls and the mini Robotniks, bossball.nem, at $7D80; the ship's boost, shpboost.nem, at $A000)
write_file('Mappings/MTZBoss',  slice(0x027DAA, 0x027E8C));
write_file('Animation/MTZBoss', slice(0x027D88, 0x027DAA));
copy_file('Art/Nemesis/BossBall.nem', 'Art/MTZBossBalls');
copy_file('Art/Nemesis/ShpBoost.nem', 'Art/ShipBoost');

# Hill Top's badniks: the Spiker (92, with its drill 93: mappings $02868E, animations $028682, art spiker.nem at VRAM $A400), the Sol (95, with the fireballs that circle it: mappings $0288E2, animations $0288CE and $0288DC;
# its art is the see-saw's badnik's and the fireball's) and the Rexon (96, 97; art rexon.nem at $6FC0)
write_file('Mappings/SpikerBadnik',  slice(0x02868E, 0x0286FA));
write_file('Animation/SpikerBadnik', slice(0x028682, 0x02868E));
copy_file('Art/Nemesis/Spiker.nem', 'Art/SpikerBadnik');
write_file('Mappings/Sol',      slice(0x0288E2, 0x02891E));
write_file('Animation/Sol',     slice(0x0288CE, 0x0288DC));
write_file('Animation/SolFire', slice(0x0288DC, 0x0288E2));
copy_file('Art/Nemesis/Rexon.nem', 'Art/Rexon');

# Dust Hill: the drawbridge (77) has five frames and its own animation in the alpha (mappings $01E140), the roots that are switches (7F: $01E9EC) and that carry (80: $01EC14), the bridge that swings (81: $01EFD6),
# the Crawlton ($029598, crawlton.nem at $7800) and the Flasher ($029DB4, its animations $029D80, $029DA4 and $029DAA, flasher.nem at $7500)
write_file('Mappings/DHZGate', slice(0x01E140, 0x01E294));
write_file('Mappings/DHZVinesSwitch', slice(0x01E9EC, 0x01EA24));
write_file('Mappings/DHZVines', slice(0x01EC14, 0x01ED80));
write_file('Mappings/DHZVerticalBridge', slice(0x01EFD6, 0x01EFE6));
write_file('Mappings/Crawlton', slice(0x029598, 0x0295B2));
write_file('Mappings/Flasher', slice(0x029DB4, 0x029E94));
write_file('Animation/Flasher', slice(0x029D80, 0x029DA4));
write_file('Animation/FlasherAlert', slice(0x029DA4, 0x029DAA));
write_file('Animation/FlasherFade', slice(0x029DAA, 0x029DB4));
copy_file('Art/Nemesis/Crawlton.nem', 'Art/Crawlton');
copy_file('Art/Nemesis/Flasher.nem', 'Art/Flasher');

# Dust Hill's boss (object 57: mappings $027016, animations $026F34, dhzboss.nem at $7800 beside the ship at $A000) and the four tiles of the rocks it shakes loose (bossrock.dat, uncompressed, at tile $360)
write_file('Mappings/DHZBoss', slice(0x027016, 0x0271AC));
write_file('Animation/DHZBoss', slice(0x026F34, 0x027016));
copy_file('Art/Nemesis/DHZBoss.nem', 'Art/DHZBoss');
copy_file('Art/Uncompressed/BossRock.dat', 'Art/DHZBossRock');

# Casino Night: the art of its two lists (the first at $6F00 on: green platforms, spikeball/slot machine, box, elevator, slot machine starter ($7100: the cards of the prototype, which had them at $7A00), blue bumper,
# bumpers, the diagonal and vertical launchers; the second: green bumpers at $7C80 and the flippers at $8000, then the usual spikes and springs)
copy_file('Art/Nemesis/GreenPtf.nem', 'Art/CNZGreenPlatform');
copy_file('Art/Nemesis/Spikball.nem', 'Art/CNZSpikeball');
copy_file('Art/Nemesis/Box.nem', 'Art/CNZBox');
copy_file('Art/Nemesis/Elevator.nem', 'Art/CNZElevator');
copy_file('Art/Nemesis/BBumpers.nem', 'Art/CNZBlueBumper');
copy_file('Art/Nemesis/Bumpers.nem', 'Art/CNZBumpers');
copy_file('Art/Nemesis/D_Launch.nem', 'Art/CNZDiagLauncher');
copy_file('Art/Nemesis/V_Launch.nem', 'Art/CNZVertLauncher');
copy_file('Art/Nemesis/GBumpers.nem', 'Art/CNZGreenBumpers');
copy_file('Art/Nemesis/Flippers.nem', 'Art/CNZFlippers');

# Neo Green Hill's boss (object 89: ship mappings $026824, the pillars' and arrow launcher's $026752, the animations $0267E6, the pillars' $026716; nghzboss.nem at $7C00 beside the ship at $A000), the art of its
# automatic door ($7F00) and of the badniks of its list: the Chop Chop ($A760), the Whisp ($A000) and the Grounder ($A120)
write_file('Mappings/NGHZBoss', slice(0x026824, 0x02696C));
write_file('Mappings/NGHZBossLauncher', slice(0x026752, 0x0267E6));
write_file('Animation/NGHZBoss', slice(0x0267E6, 0x026824));
write_file('Animation/NGHZBossPillars', slice(0x026716, 0x026752));
copy_file('Art/Nemesis/NGHZBoss.nem', 'Art/NGHZBoss');
copy_file('Art/Nemesis/NGHZAutoDoor.nem', 'Art/NGHZAutoDoor');
copy_file('Art/Nemesis/ChopChop.nem', 'Art/ChopChop');
copy_file('Art/Nemesis/Whisp.nem', 'Art/Whisp');
copy_file('Art/Nemesis/Grounder.nem', 'Art/Grounder');
# their mappings and animations (Whisp 8C $028078/$028072, Grounder 8D/8E $028300 with the rocks' $02830A and the wall's $028310, animations $0282F2 and $0282FA, Chop Chop 91 $028506/$028500)
write_file('Mappings/Whisp', slice(0x028078, 0x0280A0));
write_file('Animation/Whisp', slice(0x028072, 0x028078));
write_file('Mappings/Grounder', slice(0x028300, 0x0283BC));
write_file('Mappings/GrounderRock', slice(0x02830A, 0x0283BC));
write_file('Mappings/GrounderWall', slice(0x028310, 0x0283BC));
write_file('Animation/GrounderWalk', slice(0x0282F2, 0x0282FA));
write_file('Animation/GrounderEmerge', slice(0x0282FA, 0x028300));
write_file('Mappings/ChopChop', slice(0x028506, 0x028518));
write_file('Animation/ChopChop', slice(0x028500, 0x028506));

# Neo Green Hill's collapsing platform (object 1F: mappings $009E7E, two frames; its pieces' delays are in the object)
write_file('Mappings/CollapsePlatformNGHZ', slice(0x009E7E, 0x009EE6));

# Chemical Plant's two badniks of the alpha that no layout places: the Spiny (its art is "Lander": lander.nem at $A5A0; objects A5 horizontal and A6 vertical, mappings $02A1E4, the animations $02A1D2, $02A1D8 and the shot's $02A1DE)
# and the Grabber (grabber.nem at $A000; A7 to AB: objects that only set themselves up and stay, mappings $02A374)
write_file('Mappings/Spiny', slice(0x02A1E4, 0x02A2D4));
write_file('Animation/SpinyHorizontal', slice(0x02A1D2, 0x02A1D8));
write_file('Animation/SpinyVertical', slice(0x02A1D8, 0x02A1DE));
write_file('Animation/SpinyShot', slice(0x02A1DE, 0x02A1E4));
copy_file('Art/Nemesis/Lander.nem', 'Art/Spiny');
write_file('Mappings/Grabber', slice(0x02A374, 0x02A3F4));
copy_file('Art/Nemesis/Grabber.nem', 'Art/Grabber');

# The sliding platforms of Chemical Plant (object 7A: the alpha's mapping at $01E640 is 48 pixels wide: Dust Hill has its own)
write_file('Mappings/CPZSlider', slice(0x01E640, 0x01E654));

# Casino Night's round bumper (object 44: mappings $014994, animations $014986; the bumpers' art is in the zone's list at $7340)
write_file('Mappings/CNZBumper', slice(0x014994, 0x0149CC));
write_file('Animation/CNZBumper', slice(0x014986, 0x014994));

# The Auto Spin (object 84: mappings $016324; for Hill Top, Hidden Palace and Casino Night) and the slot machine's cards (uncompressed art: the blue and pink cards and the checkered floor of its window: loaded by
# objects that are not in the alpha, kept as the art)
write_file('Mappings/AutoSpin', slice(0x016324, 0x016390));
copy_file('Art/Uncompressed/BlueCard.dat', 'Art/CNZBlueCard');
copy_file('Art/Uncompressed/PinkCard.dat', 'Art/CNZPinkCard');
copy_file('Art/Uncompressed/SM_Check.dat', 'Art/CNZSlotCheck');

# Casino Night's boss (object 51: mappings $02792C, animations $0278EA, boss.nem at $80E0 beside the ship at $74E0)
write_file('Mappings/CNZBoss', slice(0x02792C, 0x027A76));
write_file('Animation/CNZBoss', slice(0x0278EA, 0x02792C));
copy_file('Art/Nemesis/Boss.nem', 'Art/CNZBoss');

# Oil Ocean: the elevator's art is bigger in the alpha (its platform has a longer third frame: mappings $0170AE), the fire under the burner lid (object 33's child: mappings $018C20, animation $018C00,
# fire_bst.nem at $5C40), and the art of two objects that no layout places (brkbst_v.nem at $7FE0 and the fans of object 3F, fans.nem at $8060)
copy_file('Art/Nemesis/OOZElevator.nem', 'Art/OOZElevator');
copy_file('Art/Nemesis/Fire_Bst.nem', 'Art/OOZFireBooster');
copy_file('Art/Nemesis/BrkBst_V.nem', 'Art/OOZBreakVertical');
copy_file('Art/Nemesis/Fans.nem', 'Art/OOZFans');
write_file('Mappings/OOZElevator', slice(0x0170AE, 0x0170FC));
write_file('Mappings/OOZFire', slice(0x018C20, 0x018C5C));
write_file('Animation/OOZFire', slice(0x018C00, 0x018C0C));

# Hidden Palace's Rhinobot (4D: unused in the alpha's levels, art rhinobot.nem at $7640 in the zone's second list)
copy_file('Art/Nemesis/Rhinobot.nem', 'Art/Rhinobot');
write_file('Mappings/Rhinobot',  slice(0x022A56, 0x022BA6));
write_file('Animation/Rhinobot', slice(0x022A3A, 0x022A56));

# Hill Top's boss: the art its list $2A has besides the ship and the explosions (the fire and the cannon, bossfire.nem, at $8420; the smoke, boss_smk.nem, at $BC80)
copy_file('Art/Nemesis/BossFire.nem', 'Art/HTZBossFire');
copy_file('Art/Nemesis/Boss_Smk.nem', 'Art/BossSmoke');
write_file('Mappings/Rexon', slice(0x028C74, 0x028CA4));
write_file('Mappings/HTZBoss',        slice(0x025E3C, 0x025FB0));
write_file('Animation/HTZBoss',       slice(0x025E08, 0x025E3C));
write_file('Mappings/HTZBossSmoke',   slice(0x025DD8, 0x025E08));
