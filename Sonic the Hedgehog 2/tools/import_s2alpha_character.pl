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
