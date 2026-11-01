#!/usr/bin/perl
# Brings a zone of the Sonic 2 pre-beta (September 14, 1992: Neto's disassembly, S2PREBETA gives the folder, default ~/Projects/s2prebeta-disasm; its Data/<folder>) into res/, in the formats the engine reads now, for a zone that
# has no primary and secondary group (Emerald Hill and Hill Top have: tools/import_s2prebeta_ghz.pl). One argument: the zone's name here (CPZ).
#   tiles      <Tiles.nem> (Nemesis) -> Kosinski                                                         res/S2Art/<ZONE>
#   blocks     Blocks.kos -> res/S2Map16/<ZONE>;  chunks Chunks.kos (Kosinski already) -> res/S2Map128/<ZONE>
#   collision  <prefix>_Col1.dat and _Col2.dat (the two paths) -> res/S2Collision/<ZONE>, the Sonic 3 & Knuckles one file (solids2s3)
#   layouts    <prefix>_Map1.Kos, _Map2.Kos (16 rows of $100 bytes, the foreground first in each and the background from +$80, Kosinski) -> the Sonic 3 & Knuckles layout file (header with the size, row pointers, rows), Kosinski
#              res/S2Layout/<ZONE>1, 2
#   objects    Obj_Act1.dat, Obj_Act2.dat (the engine's format already: flips in bits 13-14 of the Y word, remember state in bit 15) -> res/S2Objects/<ZONE>1, 2
#   rings      Rng_Act1.dat, Rng_Act2.dat (groups) -> one entry for each ring, sorted by X                    res/S2Rings/<ZONE>1, 2
#   palettes   <pal> -> res/S2Palette/<ZONE>, and the water's <uw> -> res/S2Palette/<ZONE>Water, if the zone has one
# Needs the kosenc, kosdec, nem2kos and solids2s3 tools (KOSENC, KOSDEC, NEM2KOS and SOLIDS2S3 give their paths; they build from ParadoxEngine/tools).
# Run it in the Sonic 2 folder: import_s2prebeta_zone.pl CPZ
use strict;
use warnings;
use File::Temp qw(tempdir);

# the zones: where its files are in Data and how they are named
my %zones = (
    CPZ => { dir => 'CPz', prefix => 'CPz', pal => 'CPz.pal', water => 'CPz_UW.pal', water_res => 'CPZWater' },
    # Sky Fortress: one layout, one object list and one ring list for both acts (the pre-beta's labels for the second are the first's), and its files are named a little differently
    SFZ => { dir => 'SFz', prefix => 'SFz', pal => 'SFz.pal', blocks => 'Blocks.Kos', chunks => 'Chunks.Kos', maps => ['SFz_Map.Kos', 'SFz_Map.Kos'], objects => ['Obj_Act.dat', 'Obj_Act.dat'], rings => ['Rng_Act.dat', 'Rng_Act.dat'] },
);
my $zone = shift or die "usage: $0 ZONE\n";
my $z = $zones{$zone} or die "no zone $zone: ", join(', ', sort keys %zones), "\n";

my $src = ($ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm") . "/Data/$z->{dir}";
my ($kosenc, $kosdec, $nem2kos, $solids2s3) = map { $ENV{$_->[0]} // $_->[1] } (['KOSENC', 'kosenc'], ['KOSDEC', 'kosdec'], ['NEM2KOS', 'nem2kos'], ['SOLIDS2S3', 'solids2s3']);
my $res = 'res';
my $tmp = tempdir(CLEANUP => 1);

sub slurp { my $p = shift; open my $f, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$f>; close $f; $d }
sub spit { my ($p, $d) = @_; open my $f, '>:raw', $p or die "$p: $!\n"; print $f $d; close $f }
sub run { system(@_) == 0 or die "failed: @_\n" }
sub kos { my ($p, $d) = @_; spit("$tmp/k.raw", $d); run($kosenc, "$tmp/k.raw", $p) }
sub unkos { my $p = shift; run($kosdec, $p, "$tmp/u.raw"); slurp("$tmp/u.raw") }

# (as tools/import_s2prebeta_ghz.pl)
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

run($nem2kos, "$res/S2Art/$zone", "$src/Tiles.nem");
kos("$res/S2Map16/$zone", unkos("$src/" . ($z->{blocks} // 'Blocks.kos')));
spit("$res/S2Map128/$zone", slurp("$src/" . ($z->{chunks} // 'Chunks.kos')));
kos("$res/S2Collision/${zone}1", slurp("$src/$z->{prefix}_Col1.dat"));
kos("$res/S2Collision/${zone}2", slurp("$src/$z->{prefix}_Col2.dat"));
run($solids2s3, "$res/S2Collision/$zone", "$res/S2Collision/${zone}1", "$res/S2Collision/${zone}2");
unlink "$res/S2Collision/${zone}1", "$res/S2Collision/${zone}2";
spit("$res/S2Palette/$zone", slurp("$src/$z->{pal}"));
spit("$res/S2Palette/$z->{water_res}", slurp("$src/$z->{water}")) if $z->{water};

for my $act (1, 2) {
    my $blob = unkos("$src/" . ($z->{maps} ? $z->{maps}[$act - 1] : "$z->{prefix}_Map$act.Kos"));
    die "act $act: the layout is not 16 rows\n" unless length $blob == 16 * 0x100;
    kos("$res/S2Layout/$zone$act", s3_layout($blob));
    my $o = slurp("$src/" . ($z->{objects} ? $z->{objects}[$act - 1] : "Obj_Act$act.dat"));
    $o = substr($o, 0, rindex($o, pack('n', 0xFFFF))) if $o =~ /\xFF\xFF/;
    spit("$res/S2Objects/$zone$act", $o . pack('nnCC', 0xFFFF, 0, 0, 0));
    spit("$res/S2Rings/$zone$act", sort_rings(slurp("$src/" . ($z->{rings} ? $z->{rings}[$act - 1] : "Rng_Act$act.dat"))));
}
print "$zone (the pre-beta's) imported\n";
