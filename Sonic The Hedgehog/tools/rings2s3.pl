#!/usr/bin/perl
# Converts ring layouts from the group format (a straight line of 1 to 8 rings in an entry, Sonic 2's) to Sonic 3 & Knuckles': one 4-byte entry for every ring, x then y, sorted by x,
# ended by an entry whose X is $FFFF (Rings.h).
#
#   group:  u16 x,  u16 y (bits 0-11) | rings in the group - 1 (bits 12-14) | vertical (bit 15)
#   S3&K:   u16 x,  u16 y
#
# A line goes RING_SPACING ($18) pixels between rings, right or (vertical) down. The rings of the groups are put in X order, rings with the same X staying in the order they had, so the
# result is the same for any layout converted twice (an entry with no count and no direction is the same in both formats).
#
# Usage: rings2s3.pl FILE...   (each file is rewritten)
use strict;
use warnings;

my $spacing = 0x18;
my $max = 511; # the status table of Rings.h

for my $path (@ARGV) {
    open my $in, '<:raw', $path or die "$path: $!";
    local $/;
    my $data = <$in>;
    close $in;
    die "$path: size is not a multiple of 4 (or 4 and a bare \$FFFF end mark)\n" if length($data) % 4 and length($data) % 4 != 2;

    my @rings;
    for (my $i = 0; $i + 2 <= length($data); $i += 4) {
        my ($x, $yw) = unpack('nn', substr($data, $i, 4) . "\0\0");
        last if $x == 0xFFFF;
        my $count = (($yw >> 12) & 7) + 1;
        my $vertical = ($yw & 0x8000) != 0;
        my $y = $yw & 0xFFF;
        for my $k (0 .. $count - 1) {
            push @rings, [$x + ($vertical ? 0 : $k * $spacing), $y + ($vertical ? $k * $spacing : 0), scalar @rings];
        }
    }
    die sprintf("%s: %d rings (the status table holds %d)\n", $path, scalar @rings, $max) if @rings > $max;

    my $out = '';
    $out .= pack('nn', $_->[0], $_->[1]) for sort { $a->[0] <=> $b->[0] || $a->[2] <=> $b->[2] } @rings;
    $out .= pack('nn', 0xFFFF, 0);

    open my $o, '>:raw', $path or die "$path: $!";
    print $o $out;
    close $o;
    printf "%s: %d rings\n", $path, scalar @rings;
}
