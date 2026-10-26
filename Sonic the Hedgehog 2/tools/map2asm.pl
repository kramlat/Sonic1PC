#!/usr/bin/perl
# Writes a sprite mappings resource (res/Mappings/<name>: the final game's binary format, 8-byte pieces: y, size, tile word, 2 player tile word, x) as the final game's source (the macros of s2.macros.asm:
# mappingsTable, spriteHeader, spritePiece), for editing.
#   usage: map2asm.pl <mappings file> <label prefix, e.g. Obj47>
use strict;
use warnings;

my ($file, $label) = @ARGV;
die "usage: $0 file label\n" unless defined $label;
open my $fh, '<:raw', $file or die "$file: $!\n";
local $/;
my $d = <$fh>;
my $first = unpack('n', $d);
my $frames = $first / 2;
my @offsets = unpack("n$frames", $d);
print "Map_$label:\tmappingsTable\n";
printf "\tmappingsTableEntry.w\tMapUnc_%s_Frame%d\n", $label, $_ for 0 .. $frames - 1;
print "\n";
for my $f (0 .. $frames - 1) {
    my $at = $offsets[$f];
    my $count = unpack('n', substr($d, $at, 2));
    print "MapUnc_${label}_Frame$f:\tspriteHeader\n";
    for my $i (0 .. $count - 1) {
        my ($y, $size, $tile, $tile2p, $x) = unpack('cCnnn', substr($d, $at + 2 + $i * 8, 8));
        $x -= 0x10000 if $x >= 0x8000;
        my ($w, $h) = ((($size >> 2) & 3) + 1, ($size & 3) + 1);
        my ($pri, $pal, $yf, $xf, $index) = (($tile >> 15) & 1, ($tile >> 13) & 3, ($tile >> 12) & 1, ($tile >> 11) & 1, $tile & 0x7FF);
        printf "\tspritePiece\t%d, %d, %d, %d, \$%X, %d, %d, %d, %d\n", $x, $y, $w, $h, $index, $xf, $yf, $pal, $pri;
    }
    print "MapUnc_${label}_Frame${f}_End\n\n";
}
