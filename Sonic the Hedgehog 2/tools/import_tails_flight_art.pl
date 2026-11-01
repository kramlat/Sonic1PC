#!/usr/bin/perl
# Brings Tails' flight art in from the staging folder (S2IMPORT, default "import and toss"), as the user's editor (Flex 2) wrote it after the guide "Add Tails' flying and swimming to Sonic 2": "Tails's art.bin" (the art),
# "Tails.asm" (the mappings, in the final game's macros) and "RTails.asm" (the DPLC: dplcHeader and dplcEntry count, first tile). Writes res/Art/Tails, res/Mappings/Tails (the 8-byte piece format: tools/asm2map.pl) and
# res/Mappings/TailsDPLC (a word of the offset of each frame's list, then in each a word with the count and a word for each run: ((tiles - 1) << 12) | first tile). Then run tools/tails_flight_animation.pl --art.
# Run it in the Sonic 2 folder: import_tails_flight_art.pl
use strict;
use warnings;
require './tools/asm2map.pl';

my $imp = $ENV{S2IMPORT} // 'import and toss';
sub slurp { my $p = shift; open my $f, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$f>; close $f; $d }
sub spit { my ($p, $d) = @_; open my $f, '>:raw', $p or die "$p: $!\n"; print $f $d; close $f }

spit('res/Art/Tails', slurp("$imp/Tails's art.bin"));
spit('res/Mappings/Tails', asm_to_mappings(slurp("$imp/Tails.asm")));

my $text = slurp("$imp/RTails.asm");
my @order = $text =~ /^\s*mappingsTableEntry\.w\s+(\w+)/mg;
my (%frame, $cur);
for my $line (split /\r?\n/, $text) {
    $line =~ s/;.*//;
    if ($line =~ /^(\w+):\s*dplcHeader/) { $cur = $1; $frame{$cur} = []; next }
    next unless defined $cur && $line =~ /^\s*dplcEntry\s+(\S+),\s*(\S+)/;
    my ($n, $first) = map { my $v = $_; $v =~ s/^\$/0x/; $v =~ /^0x/i ? hex($v) : $v + 0 } ($1, $2);
    push @{ $frame{$cur} }, ((($n - 1) << 12) | $first);
}
my @bodies = map { die "no DPLC $_\n" unless $frame{$_}; pack('n', scalar @{ $frame{$_} }) . pack('n*', @{ $frame{$_} }) } @order;
my ($at, @off) = (2 * @bodies);
for my $b (@bodies) { push @off, $at; $at += length $b }
spit('res/Mappings/TailsDPLC', pack('n*', @off) . join('', @bodies));
print scalar(@order), " frames\n";
