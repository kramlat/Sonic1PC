#!/usr/bin/perl
# Brings the Sonic 2 pre-beta's Green Hill (its first zone slot: Emerald Hill, EHZ, here) into res/: its tiles, blocks, chunks, collision indices, layouts, objects and rings (S2PREBETA gives the disassembly's folder,
# default ~/Projects/s2prebeta-disasm; the files are its Data/GHz). It replaces what the alpha had for the zone, in the formats the engine reads now:
#   tiles      Tiles.nem (Nemesis) -> Kosinski (res/S2Art/EHZ), and its secondary group from tile $1FC on (res/S2Art/EHZSecondary: the primary, shared with Hill Top, must be the one that is there already)
#   blocks     Blocks.kos -> res/S2Map16/EHZ, its secondary from block $130 on (res/S2Map16/EHZSecondary; the primary is Hill Top's too)
#   chunks     Chunks.kos (Kosinski already) -> res/S2Map128/EHZ
#   collision  GHz_Col1.dat, GHz_Col2.dat (the two paths) -> res/S2Collision/EHZ, the Sonic 3 & Knuckles one file (solids2s3)
#   layouts    GHz_Map1.Kos, GHz_Map2.Kos (16 rows of $100 bytes, the foreground first in each and the background from +$80, Kosinski) -> the Sonic 3 & Knuckles layout file (header with the size, row pointers,
#              the rows), Kosinski (res/S2Layout/EHZ1, EHZ2)
#   objects    Obj_Act1.dat, Obj_Act2.dat (the engine's format already: flips in bits 13-14 of the Y word, remember state in bit 15) -> res/S2Objects/EHZ1, EHZ2
#   rings      Rng_Act1.dat, Rng_Act2.dat (groups, as the alpha's) -> one entry for each ring, sorted by X (res/S2Rings/EHZ1, EHZ2)
# The palette (GHz.pal) and the sprite art lists are the alpha's already (checked: the same bytes), the level size is too (the pre-beta's Level_Size_Array); the second act's start differs (set in Sonic2LevelData.c).
# Needs the kosenc, kosdec, nem2kos, kossplit and solids2s3 tools (KOSENC, KOSDEC, NEM2KOS, KOSSPLIT and SOLIDS2S3 give their paths; they build from ParadoxEngine/tools).
# Run it in the Sonic 2 folder: import_s2prebeta_ghz.pl
use strict;
use warnings;
use File::Temp qw(tempdir);

my $src = ($ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm") . '/Data/GHz';
my ($kosenc, $kosdec, $nem2kos, $kossplit, $solids2s3) = map { $ENV{$_->[0]} // $_->[1] } (['KOSENC', 'kosenc'], ['KOSDEC', 'kosdec'], ['NEM2KOS', 'nem2kos'], ['KOSSPLIT', 'kossplit'], ['SOLIDS2S3', 'solids2s3']);
my $res = 'res';
my $tmp = tempdir(CLEANUP => 1);

sub slurp { my $p = shift; open my $f, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$f>; close $f; $d }
sub spit { my ($p, $d) = @_; open my $f, '>:raw', $p or die "$p: $!\n"; print $f $d; close $f }
sub run { system(@_) == 0 or die "failed: @_\n" }
sub kos { my ($p, $d) = @_; spit("$tmp/k.raw", $d); run($kosenc, "$tmp/k.raw", $p) }
sub unkos { my $p = shift; run($kosdec, $p, "$tmp/u.raw"); slurp("$tmp/u.raw") }

# The layout is the Sonic 3 & Knuckles format (engine LevelLayout.h): a header (the planes' chunks per row and rows), $40 row pointers ($8000 and the offset in the file), the foreground rows then the background
# rows, each plane only as wide and as tall as what is not empty ($blob is 16 rows of $100 bytes, the foreground first in each and the background from +$80: as tools/import_s2sw_levels.pl's s3_layout)
sub s3_layout {
    my $blob = shift;
    my %w = (0 => 0, 1 => 0); my %h = (0 => 0, 1 => 0);
    for my $plane (0, 1) { for my $row (0 .. 15) { for my $col (0 .. 0x7F) {
        next if ord(substr($blob, $row * 0x100 + $plane * 0x80 + $col, 1)) == 0;
        $w{$plane} = $col + 1 if $col + 1 > $w{$plane}; $h{$plane} = $row + 1 if $row + 1 > $h{$plane};
    } } }
    my $head = pack('n4', $w{0}, $w{1}, $h{0}, $h{1}); my @ptr = (0) x 0x40; my $rows = '';
    for my $plane (0, 1) { for my $row (0 .. $h{$plane} - 1) {
        $ptr[$row * 2 + $plane] = 0x8000 + 8 + 0x80 + length $rows;
        $rows .= substr($blob, $row * 0x100 + $plane * 0x80, $w{$plane});
    } }
    return $head . pack('n*', @ptr) . $rows;
}
# The ring layouts are groups (a line of up to 8 rings in an entry); the ring handler (Rings.h) wants one entry for each ring, in order by X (as tools/import_s2sw_levels.pl's sort_rings), with a 4-byte end mark
sub sort_rings {
    my $d = shift;
    my @e;
    for (my $i = 0; $i + 2 <= length $d; $i += 4) {
        my ($x, $yw) = unpack('nn', substr($d, $i, 4) . "\0\0");
        last if $x == 0xFFFF;
        my $vertical = $yw & 0x8000;
        for my $k (0 .. (($yw >> 12) & 7)) {
            push @e, [ $x + ($vertical ? 0 : $k * 0x18), ($yw & 0xFFF) + ($vertical ? $k * 0x18 : 0), scalar @e ];
        }
    }
    return join('', map { pack('nn', $_->[0], $_->[1]) } sort { $a->[0] <=> $b->[0] or $a->[2] <=> $b->[2] } @e) . pack('nn', 0xFFFF, 0);
}

# a whole file, then its secondary group from $offset on; the primary it would have must be the shared one that is in res already
sub group {
    my ($name, $whole, $offset) = @_;
    run($kossplit, "$res/$whole", sprintf('0x%X', $offset), "$tmp/primary", "$tmp/secondary");
    die "$name: the primary group is not the one Emerald Hill and Hill Top share\n" unless slurp("$tmp/primary") eq slurp("$res/${name}Primary");
    spit("$res/${name}Secondary", slurp("$tmp/secondary"));
}

run($nem2kos, "$res/S2Art/EHZ", "$src/Tiles.nem");
group('S2Art/EHZ', 'S2Art/EHZ', 0x1FC * 32);
kos("$res/S2Map16/EHZ", unkos("$src/Blocks.kos"));
group('S2Map16/EHZ', 'S2Map16/EHZ', 0x130 * 8);
spit("$res/S2Map128/EHZ", slurp("$src/Chunks.kos"));

kos("$res/S2Collision/EHZ1", slurp("$src/GHz_Col1.dat"));
kos("$res/S2Collision/EHZ2", slurp("$src/GHz_Col2.dat"));
run($solids2s3, "$res/S2Collision/EHZ", "$res/S2Collision/EHZ1", "$res/S2Collision/EHZ2");
unlink "$res/S2Collision/EHZ1", "$res/S2Collision/EHZ2";

for my $act (1, 2) {
    my $blob = unkos("$src/GHz_Map$act.Kos");
    die "act $act: the layout is not 16 rows\n" unless length $blob == 16 * 0x100;
    kos("$res/S2Layout/EHZ$act", s3_layout($blob));
    my $o = slurp("$src/Obj_Act$act.dat");
    $o = substr($o, 0, rindex($o, pack('n', 0xFFFF))) if $o =~ /\xFF\xFF/;
    spit("$res/S2Objects/EHZ$act", $o . pack('nnCC', 0xFFFF, 0, 0, 0));
    spit("$res/S2Rings/EHZ$act", sort_rings(slurp("$src/Rng_Act$act.dat")));
}
print "Emerald Hill (the pre-beta's Green Hill) imported\n";
