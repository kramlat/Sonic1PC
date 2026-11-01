#!/usr/bin/perl
# The boss of Chemical Plant in the Sonic 2 pre-beta (object $5D: S2PREBETA gives the disassembly's folder, default ~/Projects/s2prebeta-disasm) into res/, from its Data/Objects/Obj_0x5D.asm and Data/CPz/Boss.nem:
#   Art/CPZBoss              the boss's own art (Nemesis: the list $28 puts it at tile $500)
#   Mappings/CPZBoss         the boss's parts (39 frames, in Sonic 2's format: the build converts them)
#   Mappings/CPZBoss1        the ship's body and its flame (3 frames), Mappings/CPZBoss2 the light (2), Mappings/CPZBoss3 the vapour (4)
#   Animation/CPZBoss        the parts' scripts (27), Animation/CPZBoss1 the ship's two
# The art of the ship, its boost, the smoke and the explosions are Emerald Hill's boss's, byte for byte.
# Run it in the Sonic 2 folder: import_s2prebeta_cpzboss.pl
use strict;
use warnings;

my $base = $ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm";
my $src = "$base/Data/Objects/Obj_0x5D.asm";
open my $f, '<', $src or die "$src: $!\n";
my @lines = <$f>;
close $f;
s/;.*// for @lines;

# every label's lines
my (@labels, %body, $current);
for (@lines) {
    if (/^(\w+):\s*$/) { $current = $1; push @labels, $current; $body{$current} = []; next }
    push @{ $body{$current} }, $_ if defined $current;
}

# a table: its label's dc.w entries are offsets from the table's start (the entry's label less the table's)
sub entries {
    my ($table) = @_;
    my @e;
    for (@{ $body{$table} // die "no table $table\n" }) {
        push @e, $1 if /^\s*dc\.w\s+(\w+)-\Q$table\E\s*$/;
    }
    return @e;
}

sub mappings {
    my ($table) = @_;
    my @names = entries($table);
    my $pos = 2 * @names;
    my (@offsets, $body, %at);
    $body = '';
    for my $n (@names) {
        my @l = grep { /\S/ } @{ $body{$n} // die "no frame $n\n" };
        my ($count) = $l[0] =~ /^\s*dc\.w\s+\$([0-9A-Fa-f]+)\s*$/ or die "$n: no count\n";
        $count = hex $count;
        my @pieces;
        for my $i (1 .. $#l) {
            last if @pieces == $count;
            $l[$i] =~ /^\s*dc\.l\s+\$([0-9A-Fa-f]{8})\s*,\s*\$([0-9A-Fa-f]{8})/ or die "$n: $l[$i]\n";
            push @pieces, pack('NN', hex $1, hex $2);
        }
        die "$n: $count pieces, found " . scalar @pieces . "\n" unless @pieces == $count;
        my $frame = pack('n', $count) . join('', @pieces);
        push @offsets, $pos;
        $pos += length $frame;
        $body .= $frame;
    }
    return pack('n*', @offsets) . $body;
}

# an animation: the offsets, then each script's bytes (every script's label's dc.b lines, up to the next label)
sub animation {
    my ($table) = @_;
    my @names = entries($table);
    my $pos = 2 * @names;
    my (@offsets, $body);
    $body = '';
    for my $n (@names) {
        my $bytes = '';
        for (@{ $body{$n} }) {
            next unless /^\s*dc\.b\s+(.*\S)/;
            $bytes .= pack('C*', map { hex } map { s/^\s*\$//r } split /,/, $1);
        }
        push @offsets, $pos;
        $pos += length $bytes;
        $body .= $bytes;
    }
    return pack('n*', @offsets) . $body;
}

sub slurp { my $p = shift; open my $in, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$in>; close $in; $d }
sub spit { my ($p, $d) = @_; open my $o, '>:raw', $p or die "$p: $!\n"; print $o $d; close $o }

spit('res/Art/CPZBoss', slurp("$base/Data/CPz/Boss.nem"));
spit('res/Mappings/CPZBoss', mappings('CPz_Boss_Mappings'));
spit('res/Mappings/CPZBoss1', mappings('CPz_Boss_Mappings_1'));
spit('res/Mappings/CPZBoss2', mappings('CPz_Boss_Mappings_2'));
spit('res/Mappings/CPZBoss3', mappings('CPz_Boss_Mappings_3'));
spit('res/Animation/CPZBoss', animation('CPz_Boss_Animate_Data'));
spit('res/Animation/CPZBoss1', animation('CPz_Boss_Animate_Data_1'));
print "Chemical Plant's boss: art, mappings and animations written\n";
