#!/usr/bin/perl
# Renders sprite frames from a character's own data (mappings, DPLC and art, with a palette), as a contact sheet PPM: a check that they match what Nick Arcade shows, independent of the game.
#
#   render_frames.pl MAPPINGS DPLC ART PALETTE FIRST LAST > sheet.ppm
#
# MAPPINGS is the binary table (Sonic 2 format: a word count, 8-byte pieces), DPLC the binary DPLC table, ART the uncompressed 4bpp tiles, PALETTE words of 9-bit colour (the first 16 are used, for palette line 0;
# pieces of other lines use the same). Frames FIRST to LAST (hex) are laid out 8 to a row, each in a 96x96 cell with the frame's origin in the middle.
use strict;
use warnings;

my ($mapf, $dplcf, $artf, $palf, $first, $last) = @ARGV;
$first = hex($first); $last = hex($last);
sub slurp { open my $h, '<:raw', $_[0] or die "$_[0]: $!"; local $/; my $d = <$h>; close $h; $d }
my ($map, $dplc, $art, $pal) = map { slurp($_) } ($mapf, $dplcf, $artf, $palf);

my @colour;
for my $i (0 .. 15) {
    my $w = unpack('n', substr($pal, $i * 2, 2));
    my ($b, $g, $r) = (($w >> 9) & 7, ($w >> 5) & 7, ($w >> 1) & 7);
    $colour[$i] = [$r * 36, $g * 36, $b * 36];
}

sub tile_pixels {   # an 8x8 tile of the art as 64 colour indices
    my ($index) = @_;
    my $off = $index * 32;
    my @p;
    for my $row (0 .. 7) {
        for my $col (0 .. 3) {
            my $byte = ord(substr($art, $off + $row * 4 + $col, 1) // "\0");
            push @p, $byte >> 4, $byte & 15;
        }
    }
    return \@p;
}

my $cell = 96;
my $count = $last - $first + 1;
my $cols = 8;
my $rows = int(($count + $cols - 1) / $cols);
my ($W, $H) = ($cols * $cell, $rows * $cell);
my @img = map { [ (0x30, 0x30, 0x50) x $W ] } 1 .. $H;
sub plot { my ($x, $y, $c) = @_; return if $x < 0 || $y < 0 || $x >= $W || $y >= $H; @{ $img[$y] }[$x * 3 .. $x * 3 + 2] = @$c }

for my $f ($first .. $last) {
    my $cx = (($f - $first) % $cols) * $cell + $cell / 2;
    my $cy = int(($f - $first) / $cols) * $cell + $cell / 2;
    # the DPLC: the tiles the frame has loaded, in order
    my $o = unpack('n', substr($dplc, $f * 2, 2));
    my $n = unpack('n', substr($dplc, $o, 2));
    my @loaded;
    for my $k (0 .. $n - 1) {
        my $w = unpack('n', substr($dplc, $o + 2 + $k * 2, 2));
        push @loaded, ($w & 0xFFF) + $_ for 0 .. (($w >> 12) & 0xF);
    }
    # the mapping's pieces
    my $m = unpack('n', substr($map, $f * 2, 2));
    my $pieces = unpack('n', substr($map, $m, 2));
    for my $k (0 .. $pieces - 1) {
        my $b = $m + 2 + $k * 8;
        my ($yy, $size, $tile, $tile2p, $xx) = unpack('c C n n s>', substr($map, $b, 8));
        my ($w, $h) = ((($size >> 2) & 3) + 1, ($size & 3) + 1);
        my $xflip = ($tile >> 11) & 1; my $yflip = ($tile >> 12) & 1;
        my $first_tile = $tile & 0x7FF;
        for my $tx (0 .. $w - 1) {
            for my $ty (0 .. $h - 1) {
                my $src = $first_tile + $tx * $h + $ty;       # tiles run down the columns
                my $idx = $loaded[$src];
                my $px = tile_pixels(defined $idx ? $idx : 0);
                for my $py (0 .. 7) {
                    for my $pxx (0 .. 7) {
                        my $c = $px->[$py * 8 + $pxx];
                        next unless $c;
                        my $dx = $xflip ? ($w * 8 - 1 - ($tx * 8 + $pxx)) : ($tx * 8 + $pxx);
                        my $dy = $yflip ? ($h * 8 - 1 - ($ty * 8 + $py)) : ($ty * 8 + $py);
                        plot($cx + $xx + $dx, $cy + $yy + $dy, $colour[$c]);
                    }
                }
            }
        }
    }
}
binmode STDOUT;
print "P6\n$W $H\n255\n";
for my $row (@img) { print pack('C*', @$row) }
