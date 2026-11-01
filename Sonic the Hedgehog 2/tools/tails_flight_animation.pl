#!/usr/bin/perl
# Adds the animations of Tails' flight to the animation tables in res/Animation (the guide "Add Tails' flying and swimming to Sonic 2", Sonic Retro SCHG):
#   Tails   animations $21-$29 after the table's $20 (Fly): FlyUp, Carry, CarryUp, Tired, CarryTired, Swim, SwimUp, SwimCarry, SwimTired
#   Sonic   animation $20, Hang2: Sonic hanging from Tails (frames $6B and $6C, the final game's)
#   TailsTails  animations $B and $C, Flying and FlyingFast (the spinning tails: frames $5E and $5F of the new art, where the tails are separate sprites)
# The animations use Tails' present flying frames ($5E, $5F, with the tails on them) until his new art is in; with --art they use the guide's frames ($8B-$9B) and the Fly animation becomes the guide's.
# Run it in the Sonic 2 folder: tails_flight_animation.pl [--art]. It does nothing to a table that already has the animations.
use strict;
use warnings;

my $art = grep { $_ eq '--art' } @ARGV;

sub slurp { my $p = shift; open my $f, '<:raw', $p or die "$p: $!\n"; local $/; my $d = <$f>; close $f; $d }
sub spit { my ($p, $d) = @_; open my $f, '>:raw', $p or die "$p: $!\n"; print $f $d; close $f }

# adds scripts (bytes) to a table (words of offsets from its start, then the scripts) whose first new entry is number $first
sub extend {
    my ($path, $first, @scripts) = @_;
    my $d = slurp($path);
    my $n = unpack('n', $d) / 2;
    if ($n >= $first + @scripts) { print "$path: has them already\n"; return }
    die "$path: has $n animations, expected $first\n" unless $n == $first;
    my @off = unpack("n$n", $d);
    my $body = substr($d, $n * 2);
    my $shift = @scripts * 2;
    my @new = map { $_ + $shift } @off;
    my $at = length($body) + $n * 2 + $shift;
    for my $s (@scripts) { push @new, $at; $at += length $s }
    spit($path, pack('n*', @new) . $body . join('', @scripts));
    print "$path: ", scalar(@scripts), " animations added\n";
}

# Points animation $idx of a table at a new script (appended at the end of it)
sub repoint {
    my ($path, $idx, $script) = @_;
    my $d = slurp($path);
    my $n = unpack('n', $d) / 2;
    my @off = unpack("n$n", $d);
    return if substr($d, $off[$idx], length $script) eq $script;
    $off[$idx] = length $d;
    spit($path, pack('n*', @off) . substr($d, $n * 2) . $script);
    print "$path: animation $idx set again\n";
}

my ($f1, $f2) = (0x5E, 0x5F);
# The user's art has six flying sprites without tails ($8B-$90) where the guide's has seven, so Tired is $8E, $8F and CarryTired the one $90 (the guide: $8E $8F, $90 $91); the swimming ones are one less than the
# guide's too ($91-$95; $96 and $97 with the legs up; $98-$9A tired)
my @art = (
    pack('C*', 0x3F, 0x8B, 0xFF), pack('C*', 0x3F, 0x8B, 0xFF), pack('C*', 0x3F, 0x8C, 0xFF), pack('C*', 0x3F, 0x8D, 0xFF), pack('C*', 0x0B, 0x8E, 0x8F, 0xFF), pack('C*', 0x0B, 0x90, 0xFF),
    pack('C*', 0x07, 0x91, 0x92, 0x93, 0x94, 0x95, 0xFF), pack('C*', 0x03, 0x91, 0x92, 0x93, 0x94, 0x95, 0xFF), pack('C*', 0x04, 0x96, 0x97, 0xFF), pack('C*', 0x0B, 0x98, 0x99, 0x9A, 0x99, 0xFF),
); # (the first is animation $20, Fly, which was frames $5E and $5F: now the spinning tails alone)
my @interim = (
    pack('C*', 1, $f1, $f2, 0xFF), pack('C*', 1, $f1, $f2, 0xFF), pack('C*', 1, $f1, $f2, 0xFF), pack('C*', 0x0B, $f1, $f2, 0xFF), pack('C*', 0x0B, $f1, $f2, 0xFF),
    pack('C*', 7, $f1, $f2, 0xFF), pack('C*', 3, $f1, $f2, 0xFF), pack('C*', 4, $f1, $f2, 0xFF), pack('C*', 0x0B, $f1, $f2, 0xFF),
);
extend('res/Animation/Tails', 0x21, @interim);
if ($art) { repoint('res/Animation/Tails', 0x20 + $_, $art[$_]) for 0 .. $#art }
extend('res/Animation/Sonic', 0x20, pack('C*', 0x13, 0x6B, 0x6C, 0xFF));
extend('res/Animation/TailsTails', 0x0B, pack('C*', 1, 0x5E, 0x5F, 0xFF), pack('C*', 0, 0x5E, 0x5F, 0xFF));
