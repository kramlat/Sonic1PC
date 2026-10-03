#!/usr/bin/perl
# Builds Tails' animation tables from Nick Arcade (_incObj/02 & 05 - Tails.asm):
#   res/Animation/Tails       the 31 animations of TailsAniData (same ids as Sonic's; the port's spin dash, 31, is Tails' spin dash script)
#   res/Animation/TailsTails  the 8 scripts of Obj05_AniData (Tails' tails)
# Each table is one big-endian word per animation (the offset of its script from the table's start) and then the scripts.
#
# Usage: tails_animation_s2na.pl [S2NA-DIR] [OUT-DIR]
use strict;
use warnings;

my $dir = shift // "$ENV{HOME}/Projects/s2na-disasm";
my $out = shift // 'res/Animation';
open my $f, '<', "$dir/_incObj/02 & 05 - Tails.asm" or die "$!";
local $/;
my $src = <$f>;
$src =~ s/\r//g;

# Every dc.b script by label, wherever it is (continuation dc.b lines belong to the label above them)
my (%script, $label);
for my $line (split /\n/, $src) {
    $line =~ s/;.*//;
    if ($line =~ /^(\w+):\s*(?:dc\.w.*)?$/ && $line !~ /dc\.b/) { $label = undef unless $line =~ /dc\.w/; }
    if ($line =~ /^(\w+):\s*dc\.b\s+(.*)$/) { $label = $1; $line = "dc.b $2" }
    elsif ($line =~ /^(\w+):/) { $label = $1 if $line =~ /dc\.b/ }
    next unless defined $label && $line =~ /dc\.b\s+(.*)/;
    for my $tok (split /,/, $1) {
        $tok =~ s/^\s+|\s+$//g;
        next if $tok eq '';
        next unless $tok =~ /^(?:\$[0-9A-Fa-f]+|\d+)$/;     # (numbers only: other dc.b lines, music ids, are not scripts)
        push @{ $script{$label} }, $tok =~ /^\$(\w+)$/ ? hex($1) : $tok + 0;
    }
}

sub table {
    my ($name, $stop_re, $count) = @_;
    my ($data) = $src =~ /^$name:(.*?)(?=$stop_re)/ms or die "no $name";
    my @order;
    for my $line (split /\n/, $data) {
        $line =~ s/;.*//;
        next unless $line =~ /dc\.w\s+(.*)/;
        push @order, map { /(\w+)-$name/ ? $1 : () } split /,/, $1;
        last if @order >= $count;
    }
    die "$name: expected $count scripts, found " . scalar(@order) . "\n" unless @order == $count;
    return @order;
}

sub build {
    my ($file, @order) = @_;
    my $at = @order * 2;
    my (%offset, $body);
    $body = '';
    for my $n (@order) {
        next if exists $offset{$n};
        die "no script $n\n" unless $script{$n};
        $offset{$n} = $at;
        my $b = pack('C*', @{ $script{$n} });
        $b .= "\0" if length($b) & 1;
        $body .= $b;
        $at += length $b;
    }
    open my $o, '>:raw', $file or die "$file: $!";
    print $o pack('n*', map { $offset{$_} } @order), $body;
    close $o;
}

my @tails = table('TailsAniData', qr/^TailsAni_Walk:/m, 31);
push @tails, 'TailsAni_Spindash';                       # animation 31: the port's spin dash
build("$out/Tails", @tails);
build("$out/TailsTails", table('Obj05_AniData', qr/^byte_11E2A:/m, 8));
