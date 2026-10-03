#!/usr/bin/perl
# Brings one Nick Arcade prototype zone into res/: its level data from the s2na disassembly (default ~/Projects/s2na-disasm), converted to the engine's formats.
#
#   layout         FG (width-1, height-1 header, then rows) and BG laid out as the engine's 16 x 0x100 interleaved blob (FG at +0, BG at +0x80), Kosinski
#   Map128/Map16   the chunk and block tables, Kosinski          collision index (primary, secondary)  Kosinski
#   objects        S2NA object ids -> this port's ids (Sonic 1's table keeps its ids; Sonic 2's own objects are appended from $8E)
#   rings, palette, Nemesis art  copied
#
# Usage: import_s2na_zone.pl ZONE ACT...     e.g.  import_s2na_zone.pl EHZ 1 2       (KOSENC=path to the kosenc tool, built from ParadoxEngine/tools/kosenc)
use strict;
use warnings;
use File::Path qw(make_path);

my ($zone, @acts) = @ARGV;
die "usage: $0 ZONE ACT...\n" unless $zone && @acts;
my $s2na = $ENV{S2NA} // "$ENV{HOME}/Projects/s2na-disasm";
my $kosenc = $ENV{KOSENC} // 'kosenc';
my $res = 'res';

# Object ids stay Nick Arcade's own: the port's table for Sonic 2 (Sonic2Objects.c) follows Nick Arcade's Object Pointers.asm, so a layout's ids are used as they are.
my %remap = ();

# Zones that take part of their data from another: Hill Top uses Emerald Hill's art, chunks and collision, and its blocks are Emerald Hill's with its own from block $130 (Nick Arcade's MainLevelLoadBlock)
my %base = (HTZ => 'EHZ');
my $src = $base{$zone} // $zone;

sub slurp { my $p = shift; open my $f, '<:raw', $p or die "$p: $!"; local $/; my $d = <$f>; close $f; $d }
sub spit { my ($p, $d) = @_; open my $f, '>:raw', $p or die "$p: $!"; print $f $d; close $f }
sub kos { my ($p, $d) = @_; spit("$p.raw", $d); system($kosenc, "$p.raw", $p) == 0 or die "kosenc failed"; unlink "$p.raw" }

make_path(map "$res/$_", qw(S2Layout S2Map16 S2Map128 S2Collision S2Objects S2Rings S2Palette S2Art));

if ($zone eq $src) {
    kos("$res/S2Map128/$zone", slurp("$s2na/mappings/128x128/$zone.bin"));
    kos("$res/S2Map16/$zone", slurp("$s2na/mappings/16x16/$zone.bin"));
    kos("$res/S2Collision/${zone}1", slurp("$s2na/collision/$zone primary 16x16 collision index.bin"));
    kos("$res/S2Collision/${zone}2", slurp("$s2na/collision/$zone secondary 16x16 collision index.bin"));
} else {     # the zone's own blocks go over the base zone's from block $130 (its own chunks, collision and base art stay the base zone's)
    kos("$res/S2Map16/$zone", substr(slurp("$s2na/mappings/16x16/$src.bin"), 0, 0x980) . slurp("$s2na/mappings/16x16/$zone.bin"));
}
spit("$res/S2Palette/$zone", slurp("$s2na/art/palettes/$zone.bin"));
# The level's tileset, as the game's own Kosinski level art (Level.c decompresses it at level start): Hill Top's is Emerald Hill's with its own tiles over it from tile $1FC
my $nem2kos = $ENV{NEM2KOS} // 'nem2kos';
my $nem = "$s2na/art/nemesis/8x8 Tiles - ";
my @art = $zone eq $src ? ("$nem$zone.bin") : ("$nem$src.bin", "$nem$zone.bin\@0x1FC");
system($nem2kos, "$res/S2Art/$zone", @art) == 0 or die "nem2kos failed";
if (-e "$s2na/art/palettes/$zone Water.bin") {   # the zone's palette cycle, if it has one
    spit("$res/S2Palette/${zone}Cycle", slurp("$s2na/art/palettes/$zone Water.bin"));
}

my $bg = slurp("$s2na/level/layout/${zone}_BG.bin");
my ($bw, $bh) = map { $_ + 1 } unpack('CC', $bg);
for my $act (@acts) {
    if (-e "$s2na/level/layout/${zone}_$act.bin") {      # (an act with no layout of its own, like Hill Top's third, uses another act's)
        my $fg = slurp("$s2na/level/layout/${zone}_$act.bin");
        my ($fw, $fh) = map { $_ + 1 } unpack('CC', $fg);
        die "layout does not fit\n" if $fw > 0x80 || $fh > 16 || $bw > 0x80 || $bh > 16;
        my $blob = "\0" x (16 * 0x100);
        substr($blob, $_ * 0x100, $fw) = substr($fg, 2 + $_ * $fw, $fw) for 0 .. $fh - 1;
        substr($blob, $_ * 0x100 + 0x80, $bw) = substr($bg, 2 + $_ * $bw, $bw) for 0 .. $bh - 1;
        kos("$res/S2Layout/$zone$act", $blob);
    }

    my $o = -e "$s2na/level/objects/${zone}_$act.bin" ? slurp("$s2na/level/objects/${zone}_$act.bin") : '';
    my $out = '';
    for (my $i = 0; $i + 6 <= length $o; $i += 6) {     # S2NA: Sonic 1's entries (flips in Y bits 14-15, remember state in the id), no terminator
        my ($x, $yw, $id, $sub) = unpack('nnCC', substr($o, $i, 6));
        my $rid = $id & 0x7F;
        $rid = $remap{$rid} if exists $remap{$rid};
        my $nyw = ($yw & 0x0FFF) | (($yw & 0x4000) >> 1) | (($yw & 0x8000) >> 1);   # flips to bits 13-14
        $nyw |= 0x8000 if $id & 0x80;                                                  # remember state to the Y word
        $out .= pack('nnCC', $x, $nyw, $rid, $sub);
    }
    $out .= pack('nnCC', 0xFFFF, 0, 0, 0);
    spit("$res/S2Objects/$zone$act", $out);
    spit("$res/S2Rings/$zone$act", -e "$s2na/level/rings/${zone}_$act.bin" ? slurp("$s2na/level/rings/${zone}_$act.bin") : pack('n', 0xFFFF));
}
