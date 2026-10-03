#!/usr/bin/perl
# Splits the rings out of a Sonic 1 level's object layout (Sonic 2 object format: objlayout_s1_to_s2.pl), the way Sonic 2 keeps them: in a ring layout of their own.
#
# Sonic 1 placed rings as objects (id $25): a group is a start position, a count (subtype bits 0-2, 7 meaning 7 rings, otherwise 1 + the number) and a formation
# (subtype bits 4-7, an offset between neighbouring rings). The ring layout is a list of 4-byte big-endian entries sorted by X, ended by X = $FFFF:
#     u16 x,   u16 y (bits 0-11) | rings in the group - 1 (bits 12-14) | vertical (bit 15)
# a group of up to 8 rings in a straight line, $18 pixels apart (Sonic 2's). The formations that are not that (other spacings, diagonals, the arcs) become single
# rings, so every ring ends up exactly where Sonic 1 put it.
#
# Usage: splitrings_s1.pl OBJECT_LAYOUT RING_LAYOUT   (rewrites the object layout without its rings; run once per layout: a second run finds none)
use strict;
use warnings;

my @pos = ([0x10, 0], [0x18, 0], [0x20, 0], [0, 0x10], [0, 0x18], [0, 0x20], [0x10, 0x10], [0x18, 0x18], [0x20, 0x20],
           [-0x10, 0x10], [-0x18, 0x18], [-0x20, 0x20], [0x10, 8], [0x18, 0x10], [-0x10, 8], [-0x18, 0x10]);   # Ring.c's ring_pos

my ($objpath, $ringpath) = @ARGV;
die "usage: $0 OBJECT_LAYOUT RING_LAYOUT\n" unless defined $ringpath;
open my $in, '<:raw', $objpath or die "$objpath: $!";
local $/;
my $data = <$in>;
close $in;
die "$objpath: size is not a multiple of 6\n" if length($data) % 6;

my ($objs, @rings, $n) = ('');
my $seq = 0;
for (my $i = 0; $i < length($data); $i += 6) {
    my ($x, $yw, $id, $sub) = unpack('nnCC', substr($data, $i, 6));
    if ($x == 0xFFFF) { $objs .= substr($data, $i, 6); next; }
    if ($id != 0x25) { $objs .= substr($data, $i, 6); next; }
    my $y = $yw & 0x0FFF;
    my $count = ($sub & 7) == 7 ? 7 : ($sub & 7) + 1;      # Obj_Ring: a 7 is folded down to 6, plus the first ring
    my ($dx, $dy) = @{ $pos[$sub >> 4] };
    if ($count > 1 && $dx == 0x18 && $dy == 0) {            # a straight line, $18 apart: one group
        push @rings, [$x, $y | (($count - 1) << 12), $seq++];
    } elsif ($count > 1 && $dx == 0 && $dy == 0x18) {
        push @rings, [$x, $y | (($count - 1) << 12) | 0x8000, $seq++];
    } else {
        for my $k (0 .. $count - 1) {
            my ($rx, $ry) = ($x + $k * $dx, $y + $k * $dy);
            die sprintf("$objpath: a ring at %04X,%04X is out of range\n", $rx, $ry) if $rx < 0 || $rx >= 0xFFFF || $ry < 0 || $ry > 0xFFF;
            push @rings, [$rx, $ry, $seq++];
        }
    }
    $n++;
}

@rings = sort { $a->[0] <=> $b->[0] || $a->[2] <=> $b->[2] } @rings;     # by X, in layout order where X is equal
my $out = join '', map { pack('nn', $_->[0], $_->[1]) } @rings;
$out .= pack('nn', 0xFFFF, 0);

open my $o, '>:raw', $objpath or die "$objpath: $!";
print $o $objs;
close $o;
open my $r, '>:raw', $ringpath or die "$ringpath: $!";
print $r $out;
close $r;
printf "%s: %d ring objects -> %d ring entries\n", $objpath, $n // 0, scalar @rings;
