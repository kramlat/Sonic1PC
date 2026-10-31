#!/usr/bin/perl
# Converts a binary sprite mappings file (res/Mappings/NAME) from Sonic 2's format (8-byte pieces with the 2-player tile word) to Sonic 3 & Knuckles' (6-byte pieces: y and size, tile, x), in place:
# the table of frame offsets, then each frame as a word piece count and its pieces. Frames the table points at more than once are written once.
# Usage: mappings_s2_to_s3.pl FILE...      (a file that is already in the new format cannot be told apart: convert each once)
use strict;
use warnings;
for my $path (@ARGV) {
    open my $in, '<:raw', $path or die "$path: $!";
    local $/;
    my $d = <$in>;
    close $in;
    my $first = unpack('n', $d);
    my $frames = $first / 2;
    my @off = unpack("n$frames", $d);
    my %new;
    my $out = '';
    my $at = $frames * 2;
    for my $o (@off) {
        next if exists $new{$o};
        my $count = unpack('n', substr($d, $o, 2));
        $new{$o} = $at;
        my $frame = pack('n', $count);
        for my $p (0 .. $count - 1) {
            my ($ys, $tile, $tile2p, $x) = unpack('nnnn', substr($d, $o + 2 + $p * 8, 8));
            $frame .= pack('nnn', $ys, $tile, $x);
        }
        $out .= $frame;
        $at += length $frame;
    }
    my $table = pack('n*', map { $new{$_} } @off);
    open my $o, '>:raw', $path or die "$path: $!";
    print $o $table . $out;
    close $o;
}
