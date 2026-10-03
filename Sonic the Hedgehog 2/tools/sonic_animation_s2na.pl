#!/usr/bin/perl
# Builds res/Animation/Sonic from Nick Arcade's Sonic animation scripts (_incObj/01 - Sonic.asm: SonicAniData and the SonicAni_* scripts).
# The table is one big-endian word per animation (the offset of its script from the table's start) and then the scripts, as the port reads them. The port's own Sonic code
# also uses animation 31 for the spin dash (Nick Arcade has it as 9), so that entry points at the same script.
#
# Usage: sonic_animation_s2na.pl [S2NA-DIR] > res/Animation/Sonic
use strict;
use warnings;

my $dir = shift // "$ENV{HOME}/Projects/s2na-disasm";
open my $f, '<', "$dir/_incObj/01 - Sonic.asm" or die "$!";
local $/;
my $src = <$f>;
$src =~ s/\r//g;
my ($data) = $src =~ /^SonicAniData:(.*?)^\s*even/ms or die "no animation data";

my @order;                 # script labels in table order
my %script;                # label -> bytes
my $label;
for my $line (split /\n/, $data) {
    $line =~ s/;.*//;
    if ($line =~ /dc\.w\s+(\w+)-SonicAniData/) { push @order, $1; next }
    if ($line =~ /^(\w+):\s*(dc\.b.*)$/) { $label = $1; $line = $2 }
    next unless defined $label && $line =~ /dc\.b\s+(.*)/;
    for my $tok (split /,/, $1) {
        $tok =~ s/^\s+|\s+$//g;
        next if $tok eq '';
        push @{ $script{$label} }, $tok =~ /^\$(\w+)$/ ? hex($1) : $tok + 0;
    }
}
die "expected 31 animations, found " . scalar(@order) . "\n" unless @order == 31;
push @order, 'SonicAni_Spindash';                      # animation 31

my $count = @order;
my %offset;
my $body = '';
my $at = $count * 2;
for my $name (@order) {
    next if exists $offset{$name};
    die "no script $name\n" unless $script{$name};
    $offset{$name} = $at;
    my $bytes = pack('C*', @{ $script{$name} });
    $bytes .= "\0" if length($bytes) & 1;               # even
    $body .= $bytes;
    $at += length $bytes;
}
binmode STDOUT;
print pack('n*', map { $offset{$_} } @order), $body;
