#!/usr/bin/perl
# Brings the Simon Wai prototype's levels into res/ and writes src/Sonic2LevelData.c, from its disassembly (default ~/Projects/s2sw-disasm; set S2SW to use another). The disassembly's own SonLVL project
# ("SonLVL INI Files/SonLVL.ini") says which files make up each act, and it is what this reads:
#
#   tiles      Nemesis tilesets (one or more, a later one over an earlier at a byte offset) -> one Kosinski tileset (Level.c decompresses it at level start)   res/S2Art/<ZONE>
#   blocks     16x16 block mappings (raw; a later file over an earlier at a byte offset) -> Kosinski                                                          res/S2Map16/<ZONE>
#   chunks     128x128 chunk mappings (Kosinski already, Sonic 2's final format: copied as they are)                                                          res/S2Map128/<ZONE>
#   fg/bglayout  the layouts, stored apart (a width-1, height-1 header, then the rows): put side by side as the game has them in RAM (a row of 0x100 bytes: the FG's first, the BG's from +0x80), Kosinski   res/S2Layout/<ZONE><ACT>
#   objects    Sonic 1's kind of entries (flips in the Y word's bits 14-15, remember-state in the id): to the engine's (flips in 13-14, remember state in Y bit 15)           res/S2Objects/<ZONE><ACT>
#   rings, colind1/2, palette                                                                                                                               res/S2Rings, res/S2Collision/<ZONE>1,2, res/S2Palette/<ZONE>
#
# and the level size and start position tables (from s2b.asm and the start position files). The disassembly calls its first zone Green Hill, but it is Emerald Hill (EHZ here), the zone its data and objects are.
# The zones the Sonic 2 alpha (Aug 21st 1992, ~/Projects/s2aug21-disasm, S2ALPHA to use another) has data for (%alpha below) take that data instead, as the project moves toward that build (M3): its level files, level size table and
# start positions. Its objects are in the engine's format already (flips in bits 13-14 and remember state in bit 15 of the Y word, the whole id byte).
# Needs the kosenc and nem2kos tools (KOSENC and NEM2KOS give their paths; they build from ParadoxEngine/tools). Run it in the Sonic 2 folder: import_s2sw_levels.pl
use strict;
use warnings;
use File::Path qw(make_path);

my $sw = $ENV{S2SW} // "$ENV{HOME}/Projects/s2sw-disasm";
my $kosenc = $ENV{KOSENC} // 'kosenc';
my $nem2kos = $ENV{NEM2KOS} // 'nem2kos';
my $alpha_dir = $ENV{S2ALPHA} // "$ENV{HOME}/Projects/s2aug21-disasm";
my $ini_dir = "$sw/SonLVL INI Files";
my $res = 'res';

# The zones: SonLVL name => [the name here, the zone slot (the prototype's zone id)]
my %zones = (
    'Green Hill'     => ['EHZ', 0x00], 'Wood'           => ['WZ',   0x02], 'Metropolis'     => ['MTZ',  0x04], 'Hill Top'       => ['HTZ',  0x07],
    'Hidden Palace'  => ['HPZ', 0x08], 'Oil Ocean'      => ['OOZ',  0x0A], 'Dust Hill'      => ['DHZ',  0x0B], 'Casino Night'   => ['CNZ',  0x0C],
    'Chemical Plant' => ['CPZ', 0x0D], 'Neo Green Hill' => ['NGHZ', 0x0F],
);
# The zones taken from the alpha: the folder of its level files, and the files' names. (Its shared chunk file, Level/Shared/Chunks.kos, is Emerald Hill's and Hill Top's together, as the prototype's was.)
my %alpha = (
    EHZ => { dir => 'Emerald Hill Zone', fg => ['Fg_Map1.dat', 'Fg_Map2.dat'], bg => ['Bg_Map.dat', 'Bg_Map.dat'], obj => ['Obj_Act1.dat', 'Obj_Act2.dat'], rng => ['Rng_Act1.dat', 'Rng_Act2.dat'],
             blocks => 'Blocks.dat', tiles => 'Tiles.nem', chunks => '../Shared/Chunks.kos', col => ['../Shared/Ghz_Col1.dat', '../Shared/Ghz_Col2.dat'], pal => '../../Palettes/GHz.pal' },
);
# What each zone's header names: its sprite art list (PlcId; the zones that are not built yet share Hill Top's, which has the common art) and its palette (PalId)
my %plc = (EHZ => 'PlcId_SLZ', WZ => '0', MTZ => 'PlcId_MTZ', HTZ => 'PlcId_SBZ', HPZ => 'PlcId_SYZ', OOZ => 'PlcId_OOZ', DHZ => 'PlcId_DHZ', CPZ => 'PlcId_MZ', NGHZ => 'PlcId_GHZ', CNZ => 'PlcId_LZ');
my %plc2 = (WZ => 'PlcId_SLZ2', MTZ => 'PlcId_MTZ2', HTZ => 'PlcId_SBZ2', OOZ => 'PlcId_OOZ2', DHZ => 'PlcId_DHZ2', CPZ => 'PlcId_MZ2', NGHZ => 'PlcId_GHZ2', CNZ => 'PlcId_LZ2'); # (the second list of a zone, loaded with the first: only the zones that have a second one)
# The zones whose objects are ported: the others have the object layouts imported but none put in their level yet (their objects come with the zone)
my %objects_built = map { $_ => 1 } qw(EHZ MTZ HTZ HPZ OOZ DHZ CPZ NGHZ);
my %pal = (EHZ => 'PalId_EHZ', HTZ => 'PalId_HTZ', HPZ => 'PalId_HPZ', CPZ => 'PalId_CPZ', WZ => 'PalId_WZ', MTZ => 'PalId_MTZ', OOZ => 'PalId_OOZ', DHZ => 'PalId_DHZ', CNZ => 'PalId_CNZ', NGHZ => 'PalId_NGHZ');

sub slurp { my $p = shift; open my $f, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$f>; close $f; $d }
sub spit { my ($p, $d) = @_; open my $f, '>:raw', $p or die "$p: $!\n"; print $f $d; close $f }
sub kos { my ($p, $d) = @_; spit("$p.raw", $d); system($kosenc, "$p.raw", $p) == 0 or die "kosenc failed\n"; unlink "$p.raw" }
sub path { my $p = shift; $p =~ s{^\.\./}{}; return "$sw/$p" }

# SonLVL.ini
open my $h, '<', "$ini_dir/SonLVL.ini" or die "SonLVL.ini: $!\n";
my (%sec, @order, $cur);
while (<$h>) {
    s/\r?\n$//;
    if (/^\[(.+)\]$/) { $cur = $1; push @order, $cur; next }
    if (defined $cur && /^(\w+)=(.*)$/) { $sec{$cur}{$1} = $2 }
}
close $h;

make_path(map "$res/$_", qw(S2Layout S2Map16 S2Map128 S2Collision S2Objects S2Rings S2Palette S2Art));
my (%done, @resources, %info);        # %info: zone => { acts => { n => {...} } }

for my $name (@order) {
    next unless $name =~ /^(.+) Zone Act (\d+)$/ && $zones{$1};
    my ($zone, $slot) = @{ $zones{$1} };
    my $act = $2;
    my $s = $sec{$name};
    # Metropolis's third act is the prototype's zone $05 (MTZ2), act 1
    my ($zslot, $zact) = ($slot, $act - 1);
    ($zslot, $zact) = (0x05, 0) if $zone eq 'MTZ' && $act == 3;
    my $key = "$zone$act";
    $info{$zone}{slot} //= $slot;
    $info{$zone}{acts}{$act} = { slot => $zslot, act => $zact, key => $key };

    unless ($done{$zone}++) {
        # tilesets: "a|b:0x3F80" is b over a from that byte
        my $al = $alpha{$zone};
        my $alp = sub { "$alpha_dir/Level/$al->{dir}/" . shift };
        my @nem;
        if ($al) { push @nem, $alp->($al->{tiles}) }
        else { for my $t (split /\|/, $s->{tiles}) {
            my ($file, $off) = split /:/, $t;
            push @nem, path($file) . (defined $off ? '@' . (hex($off) / 32) : '');
        } }
        system($nem2kos, "$res/S2Art/$zone", @nem) == 0 or die "nem2kos failed for $zone\n";
        # blocks
        my $blocks = '';
        if ($al) { $blocks = slurp($alp->($al->{blocks})) }
        else { for my $t (split /\|/, $s->{blocks}) {
            my ($file, $off) = split /:/, $t;
            my $d = slurp(path($file));
            if (defined $off) { $blocks = substr($blocks, 0, hex $off) . $d } else { $blocks .= $d }
        } }
        kos("$res/S2Map16/$zone", $blocks);
        spit("$res/S2Map128/$zone", slurp($al ? $alp->($al->{chunks}) : path($s->{chunks})));      # (Kosinski already)
        kos("$res/S2Collision/${zone}1", slurp($al ? $alp->($al->{col}[0]) : path($s->{colind1})));
        kos("$res/S2Collision/${zone}2", slurp($al ? $alp->($al->{col}[1]) : path($s->{colind2})));
        # the level palette: the file after the first one (Sonic's), "x.bin:0:16:48" is lines 1-3 from file x
        my ($level_pal) = grep { !/Sonic and Tails/ } split /\|/, $s->{palette};
        $level_pal =~ s/:.*//;
        spit("$res/S2Palette/$zone", slurp($al ? $alp->($al->{pal}) : path($level_pal)));
        push @resources, "S2Art/$zone", "S2Map16/$zone", "S2Map128/$zone", "S2Collision/${zone}1", "S2Collision/${zone}2", "S2Palette/$zone";
    }

    # layouts, side by side
    my $al = $alpha{$zone};
    my $alp = sub { "$alpha_dir/Level/$al->{dir}/" . shift };
    my $fg = slurp($al ? $alp->($al->{fg}[$act - 1]) : path($s->{fglayout}));
    my $bg = slurp($al ? $alp->($al->{bg}[$act - 1]) : path($s->{bglayout}));
    my ($fw, $fh) = map { $_ + 1 } unpack('CC', $fg);
    my ($bw, $bh) = map { $_ + 1 } unpack('CC', $bg);
    die "$name: layout does not fit\n" if $fw > 0x80 || $fh > 16 || $bw > 0x80 || $bh > 16;
    my $blob = "\0" x (16 * 0x100);
    # (the prototype's loader (Interleave_Level_Layout) repeats each row across the 0x80 entries of the RAM layout, as many whole times as it fits: a narrow background is a pattern that repeats, not a strip followed by nothing)
    for my $row (0 .. $fh - 1) { substr($blob, $row * 0x100 + $_ * $fw, $fw) = substr($fg, 2 + $row * $fw, $fw) for 0 .. int(0x80 / $fw) - 1 }
    for my $row (0 .. $bh - 1) { substr($blob, $row * 0x100 + 0x80 + $_ * $bw, $bw) = substr($bg, 2 + $row * $bw, $bw) for 0 .. int(0x80 / $bw) - 1 }
    kos("$res/S2Layout/$key", $blob);

    # objects (an act without any has none), rings
    my $o = $s->{objects} ? slurp(path($s->{objects})) : '';
    my $out = '';
    if ($al) { $o = ''; $out = slurp($alp->($al->{obj}[$act - 1])); $out =~ s/\0+$//; $out = substr($out, 0, rindex($out, pack('n', 0xFFFF)) ) if $out =~ /\xFF\xFF/; $out .= pack('nnCC', 0xFFFF, 0, 0, 0); }
    for (my $i = 0; $i + 6 <= length $o; $i += 6) {
        my ($x, $yw, $id, $sub) = unpack('nnCC', substr($o, $i, 6));
        my $nyw = ($yw & 0x0FFF) | (($yw & 0x4000) >> 1) | (($yw & 0x8000) >> 1);
        $nyw |= 0x8000 if $id & 0x80;
        $out .= pack('nnCC', $x, $nyw, $id & 0x7F, $sub);
    }
    $out .= pack('nnCC', 0xFFFF, 0, 0, 0) unless $al;
    spit("$res/S2Objects/$key", $out);
    spit("$res/S2Rings/$key", $al ? slurp($alp->($al->{rng}[$act - 1])) : $s->{rings} ? slurp(path($s->{rings})) : pack('n', 0xFFFF));
    $info{$zone}{acts}{$act}{start} = [ unpack('nn', slurp(path((split /:/, $s->{startpos})[0]))) ];
    $info{$zone}{acts}{$act}{alpha} = 1 if $al;
    push @resources, "S2Layout/$key", "S2Objects/$key", "S2Rings/$key";
}

# the collision arrays (height and width maps of the 16x16 collision blocks) and their angles, which every zone shares
spit("$res/S2Collision/HeightMap", slurp("$sw/level/collision/Collision array 1.bin"));
spit("$res/S2Collision/WidthMap", slurp("$sw/level/collision/Collision array 2.bin"));
spit("$res/S2Collision/Angle", slurp("$sw/level/collision/Curve and resistance mappings.bin"));

# the level size table of s2b.asm: for each zone slot, act 1 then act 2, each a long of (minimum, maximum) X and one of Y
my @sizes;
{
    open my $f, '<:raw', "$sw/s2b.asm" or die;
    my $on = 0;
    while (<$f>) {
        s/\r//;
        $on = 1 if /^LevelSize:\s+zoneOrderedTable/;
        next unless $on;
        last if /^\s*zoneTableEnd/;
        push @sizes, [ map { hex } /\$([0-9A-Fa-f]{8})/g ] if /zoneTableEntry\.l\s+(\$[0-9A-Fa-f]{8}.*)/;
    }
}
die "level size table not found\n" unless @sizes == 17;
# the alpha's: Level_Size_Array (four longs for each zone slot: act 1's X and Y limits, act 2's) and Player_Start_Position_Array (four words: act 1's X, Y, act 2's)
my (@alpha_sizes, @alpha_starts);
{
    open my $f, '<:raw', "$alpha_dir/sonic2alpha.asm" or die "alpha disassembly: $!\n";
    my $in_size = 0; my $in_start = 0;
    while (<$f>) {
        s/\r//;
        if (/^Level_Size_Array:/) { $in_size = 1; next }
        if (/^Player_Start_Position_Array:/) { $in_start = 1; next }
        if ($in_size) { if (/dc\.l\s+\$(\w+),\s*\$(\w+),\s*\$(\w+),\s*\$(\w+)/) { push @alpha_sizes, [ map { hex } $1, $2, $3, $4 ] } else { $in_size = 0 } }
        if ($in_start) { if (/dc\.w\s+\$(\w+),\s*\$(\w+),\s*\$(\w+),\s*\$(\w+)/) { push @alpha_starts, [ map { hex } $1, $2, $3, $4 ] } else { $in_start = 0 } }
    }
}
die "the alpha's level size or start tables not found\n" unless @alpha_sizes == 17 && @alpha_starts == 17;

# ---------------------------------------------------------------------------------------------------------------------------------------------
# src/Sonic2LevelData.c
my @built = sort { $info{$a}{slot} <=> $info{$b}{slot} } keys %info;
my $c = <<'EOT';
// Sonic 2's level data: which art, maps, layouts, collision, objects and rings each zone slot and act is made of, where the player starts, how far the level reaches and how the background scrolls.
// GENERATED by tools/import_s2sw_levels.pl from the Simon Wai prototype's SonLVL project (its zone ids are the slots, see ZoneIds.h; the prototype's "Green Hill" is Emerald Hill). Level.c (the loading and the
// level's state) reads them through Level.h. The slots of zones that are not in the prototype's data are zero.
#include "Level.h"

#include "Constants.h"
#include "Game.h"
#include "PLC.h"
#include "Palette.h"

#include <string.h>

EOT
for my $z (@built) {
    $c .= "// $z\n";
    $c .= "#include \"Resource/S2$_/$z.h\"\n" for qw(Art Map16 Map128 Collision/x);
}
$c =~ s{#include "Resource/S2Collision/x/(\w+)\.h"\n}{#include "Resource/S2Collision/${1}1.h"\n#include "Resource/S2Collision/${1}2.h"\n}g;
for my $z (@built) {
    for my $a (sort { $a <=> $b } keys %{ $info{$z}{acts} }) {
        my $k = $info{$z}{acts}{$a}{key};
        $c .= "#include \"Resource/S2$_/$k.h\"\n" for qw(Layout Objects Rings);
    }
}
$c .= "\n";

$c .= "// An act without objects\nstatic const uint8_t obj_null[] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 };\n\n";

# the tables, by slot: collect the acts of each slot
my %slot;     # slot => act => key
for my $z (@built) { for my $a (keys %{ $info{$z}{acts} }) { my $i = $info{$z}{acts}{$a}; $slot{ $i->{slot} }{ $i->{act} } = { %$i, zone => $z } } }
my @slots = sort { $a <=> $b } keys %slot;
sub zid { my $s = shift; return sprintf('0x%02X', $s) }
sub act_or { my ($s, $a, $f, $d) = @_; return exists $slot{$s}{$a} ? $f->($slot{$s}{$a}) : $d }

$c .= "// Level definitions: one combined (interleaved FG+BG, Kosinski) blob per act. An act the prototype has no layout for starts the zone's first.\n";
$c .= "const struct LevelLayout level_layouts[ZoneId_Num][4] = {\n";
for my $s (@slots) {
    $c .= sprintf("    [%s] = {", zid($s));
    $c .= join(', ', map { my $e = $slot{$s}{$_} // $slot{$s}{0}; "{ S2Layout_$e->{key} }" } 0 .. 3);
    $c .= " }, // $slot{$s}{0}{zone}\n";
}
$c .= "};\n\n";

$c .= "// The level boundaries and camera shifts of each act. They depend on the size of the picture (the wider or taller it is, the further the camera can see past the edges), which is chosen at run time, so the table is built when it is read.\n";
$c .= "const int16_t *LevelSizes(int zone, int act) {\n    const int16_t table[ZoneId_Num][4][6] = {\n";
for my $s (@slots) {
    $c .= sprintf("        [%s] = { // $slot{$s}{0}{zone}\n", zid($s));
    for my $a (0 .. 3) {
        my $e = $info{ $slot{$s}{0}{zone} }{acts}{1}{alpha} ? $alpha_sizes[$s] : $sizes[$s];
        my ($xl, $yl) = $a % 2 ? ($e->[2], $e->[3]) : ($e->[0], $e->[1]);
        my ($xmin, $xmax, $ymin, $ymax) = ($xl >> 16, $xl & 0xFFFF, $yl >> 16, $yl & 0xFFFF);
        $c .= sprintf("            { 0x0004, 0x%04X, 0x%04X + SCREEN_WIDEADD2, 0x%04X, 0x%04X + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },\n", $xmin, $xmax, $ymin, $ymax);
    }
    $c .= "        },\n";
}
$c .= "    };\n    static int16_t result[6];\n    memcpy(result, table[zone][act], sizeof(result));\n    return result;\n}\n\n";

$c .= "// Player start positions\nconst int16_t StartLocArray[ZoneId_Num][4][2] = {\n";
for my $s (@slots) {
    my @row = map { my $a = exists $slot{$s}{$_} ? $_ : 0; my $e = $slot{$s}{$a}; $e->{alpha} ? sprintf('{ 0x%04X, 0x%04X }', @{ $alpha_starts[$s] }[ $a * 2, $a * 2 + 1 ]) : sprintf('{ 0x%04X, 0x%04X }', @{ $e->{start} }) } 0 .. 3;
    $c .= sprintf("    [%s] = { %s }, // $slot{$s}{0}{zone}\n", zid($s), join(', ', @row));
}
$c .= "};\n\n// Level scroll block sizes\nconst int16_t BGScrollBlockSizes[ZoneId_Num][4] = {\n";
$c .= sprintf("    [%s] = { 0x800, 0x100, 0x100, 0 },\n", zid($_)) for @slots;
$c .= "};\n\n// Level headers (sprite art list, tileset, second art list, blocks, palette, chunks); Hill Top's blocks and tileset have its own over Emerald Hill's (and its chunks, which were Emerald Hill's until that came from the alpha)\nconst LevelHeader level_header[ZoneId_Num] = {\n";
for my $s (@slots) {
    my $z = $slot{$s}{0}{zone};
    my $chunks = $z;
    $c .= sprintf("    [%s] = { %s, S2Art_%s, %s, S2Map16_%s, %s, S2Map128_%s },\n", zid($s), $plc{$z} // 'PlcId_SBZ', $z, $plc2{$z} // '0', $z, $pal{$z}, $chunks);
}
$c .= "};\n\n// Level collision indices, one file per path\nconst uint8_t* level_coli[ZoneId_Num - 1][2] = {\n";
$c .= sprintf("    [%s] = { S2Collision_%s1, S2Collision_%s2 },\n", zid($_), $slot{$_}{0}{zone}, $slot{$_}{0}{zone}) for @slots;
$c .= "};\n\n// Level object layouts (Sonic 2 format: ObjectsManager.h), and the ring layouts in the same places\n";
for my $kind (['Objects', 'level_obj'], ['Rings', 'level_ring']) {
    $c .= "const uint8_t* " . $kind->[1] . "[ZoneId_Num][4] = {\n";
    for my $s (@slots) {
        my $z = $slot{$s}{0}{zone};
        my $null = ($kind->[0] eq "Objects" && !$objects_built{$z}) ? "obj_null" : "";
        $c .= sprintf("    [%s] = { %s }, // $z\n", zid($s), join(', ', map { my $e = $slot{$s}{$_} // $slot{$s}{0}; $null ? $null : "S2$kind->[0]_$e->{key}" } 0 .. 3));
    }
    $c .= "};\n\n";
}
spit('src/Sonic2LevelData.c', $c);

# the resource list of the project file, between its markers
my $yml = slurp('paradoxmakefile.yml');
my $block = "  # BEGIN levels (tools/import_s2sw_levels.pl)\n" . join('', map { "  - \"$_\"\n" } @resources) . "  # END levels\n";
if ($yml =~ /  # BEGIN levels.*?  # END levels\n/s) { $yml =~ s/  # BEGIN levels.*?  # END levels\n/$block/s }
else { die "no '# BEGIN levels' marker in paradoxmakefile.yml: put one in the resources list\n" }
spit('paradoxmakefile.yml', $yml);
print scalar(@resources), " resources, ", scalar(@slots), " zone slots\n";
