#!/usr/bin/perl
# Turns a sprite mappings table of a Sonic 2 pre-beta object source (dc.w offsets from the table's label, then each frame's dc.w count and dc.l pairs: the disassembly's own layout, Sonic 2's piece format) into the
# resource's binary. A table whose single frame is written under the table's label (no offset list) is not handled: the pre-beta's all have the list.
#   usage: asm_dc_mappings.pl <source> <table label> <output>
use strict;
use warnings;
my ($src, $table, $out) = @ARGV;
die "usage: $0 SOURCE TABLE OUTPUT\n" unless defined $out;
open my $f, '<', $src or die "$src: $!\n";
my @lines = <$f>;
close $f;
s/;.*// for @lines;
my (%body, $current);
for (@lines) {
    if (/^(\w+):?\s*$/) { $current = $1; $body{$current} = []; next }
    push @{ $body{$current} }, $_ if defined $current;
}
my @names = map { /^\s*dc\.w\s+(\w+)-\Q$table\E\s*$/ ? $1 : () } @{ $body{$table} // die "no table $table in $src\n" };
die "$table has no frames\n" unless @names;
my $pos = 2 * @names;
my (@offsets, $data);
$data = '';
for my $n (@names) {
    my @l = grep { /\S/ } @{ $body{$n} // die "no frame $n\n" };
    $l[0] =~ /^\s*dc\.w\s+\$([0-9A-Fa-f]+)\s*$/ or die "$n: no count\n";
    my $count = hex $1;
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
    $data .= $frame;
}
open my $o, '>:raw', $out or die "$out: $!\n";
print $o pack('n*', @offsets) . $data;
close $o;
