#!/usr/bin/perl
# Converts a Sonic 1 object layout to Sonic 2 format, in place.
#
# Both are lists of 6-byte big-endian entries sorted by X and ended by an entry whose X is $FFFF:
#   Sonic 1:  u16 x,  u16 y (bits 0-11) | x flip (bit 14) | y flip (bit 15),  u8 id (bit 7: remember state),  u8 subtype
#   Sonic 2:  u16 x,  u16 y (bits 0-11) | x flip (bit 13) | y flip (bit 14) | remember state (bit 15),  u8 id,  u8 subtype
#
# Usage: objlayout_s1_to_s2.pl FILE...   (a file that is already in Sonic 2 format cannot be told apart from a Sonic 1 one: convert each only once)
use strict;
use warnings;

for my $path (@ARGV) {
    open my $in, '<:raw', $path or die "$path: $!";
    local $/;
    my $data = <$in>;
    close $in;
    die "$path: size is not a multiple of 6\n" if length($data) % 6;

    my $out = '';
    for (my $i = 0; $i < length($data); $i += 6) {
        my ($x, $yw, $id, $sub) = unpack('nnCC', substr($data, $i, 6));
        if ($x == 0xFFFF) {                      # the end marker
            $out .= substr($data, $i, 6);
            next;
        }
        die sprintf("$path: entry at %d has bits 12-13 set in Y (%04X)\n", $i, $yw) if $yw & 0x3000;
        my $nyw = ($yw & 0x0FFF) | (($yw & 0x4000) >> 1) | (($yw & 0x8000) >> 1);   # x flip 14 -> 13, y flip 15 -> 14
        $nyw |= 0x8000 if $id & 0x80;                                               # remember state moves to the Y word
        $out .= pack('nnCC', $x, $nyw, $id & 0x7F, $sub);
    }
    open my $o, '>:raw', $path or die "$path: $!";
    print $o $out;
    close $o;
}
