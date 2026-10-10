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
