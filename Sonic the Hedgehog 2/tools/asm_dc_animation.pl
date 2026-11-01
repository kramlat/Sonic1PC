#!/usr/bin/perl
# Turns an animation table of a Sonic 2 pre-beta object source (dc.w offsets from the table's label, then each script's dc.b bytes under its label) into the resource's binary: the offsets, then the scripts.
#   usage: asm_dc_animation.pl <source> <table label> <output>
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
my $pos = 2 * @names;
my (@offsets, $data);
$data = '';
for my $n (@names) {
    my $bytes = '';
    for (@{ $body{$n} // die "no script $n\n" }) {
        next unless /^\s*dc\.b\s+(.*\S)/;
        $bytes .= pack('C*', map { hex } map { s/^\s*\$//r } split /,/, $1);
    }
    push @offsets, $pos;
    $pos += length $bytes;
    $data .= $bytes;
}
open my $o, '>:raw', $out or die "$out: $!\n";
print $o pack('n*', @offsets) . $data;
close $o;
