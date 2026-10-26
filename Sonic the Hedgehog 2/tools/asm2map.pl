#!/usr/bin/perl
# Turns a sprite mappings source in the final game's macros (mappingsTable, spriteHeader, spritePiece: see map2asm.pl, which writes them) into the resource's binary format (8-byte pieces: y, size, tile word,
# 2 player tile word, x). The frames are in the order of the mappingsTableEntry lines; a frame starts at its label (a line with the label then spriteHeader); the other labels (the _End ones) are ignored.
#   usage: asm2map.pl <source> <output>      (or require it and call asm_to_mappings($text))
use strict;
use warnings;

sub asm_to_mappings {
    my ($text) = @_;
    my @order = $text =~ /^\s*mappingsTableEntry\.w\s+(\w+)/mg;
    my %frame;
    my $current;
    for my $line (split /\r?\n/, $text) {
        $line =~ s/;.*//;
        if ($line =~ /^(\w+):\s*spriteHeader/) { $current = $1; $frame{$current} = []; next }
        next unless defined $current && $line =~ /^\s*spritePiece\s+(.*\S)/;
        my ($x, $y, $w, $h, $tile, $xf, $yf, $pal, $pri) = map { my $v = $_; $v =~ s/^\s+|\s+$//g; $v =~ s/^\$/0x/; $v =~ /^-?0x/i ? hex($v) : $v + 0 } split /,/, $1;
        my $flags = ($pri << 15) | ($pal << 13) | ($yf << 12) | ($xf << 11);
        push @{ $frame{$current} }, pack('cCnnn', $y, (($w - 1) << 2) | ($h - 1), $flags | $tile, $flags | ($tile >> 1), $x & 0xFFFF);
    }
    my @bodies;
    for my $name (@order) {
        die "no frame $name\n" unless $frame{$name};
        push @bodies, pack('n', scalar @{ $frame{$name} }) . join('', @{ $frame{$name} });
    }
    my @offsets;
    my $at = 2 * @bodies;
    for my $b (@bodies) { push @offsets, $at; $at += length $b }
    return pack('n*', @offsets) . join('', @bodies);
}

unless (caller) {
    my ($in, $out) = @ARGV;
    die "usage: $0 source output\n" unless defined $out;
    open my $fh, '<', $in or die "$in: $!\n";
    local $/;
    my $text = <$fh>;
    open my $o, '>:raw', $out or die "$out: $!\n";
    print $o asm_to_mappings($text);
}
1;
