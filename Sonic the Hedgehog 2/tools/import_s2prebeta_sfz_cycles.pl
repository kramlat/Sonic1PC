#!/usr/bin/perl
# The four palette cycles of Sky Fortress in the Sonic 2 pre-beta (Pal_SFzCyc1 to 4 of its S2140992.asm: S2PREBETA gives the folder, default ~/Projects/s2prebeta-disasm) into res/S2Palette/SFZCycle1 to 4 as words.
# Run it in the Sonic 2 folder.
use strict;
use warnings;
my $path = ($ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm") . "/S2140992.asm";
open my $f, '<', $path or die "$path: $!\n";
my (%data, $label);
while (<$f>) {
    s/;.*//;
    if (/^(Pal_SFzCyc\d):/) { $label = $1; $data{$label} = ''; next }
    if (defined $label && /^\s*dc\.w\s+(.*\S)/) { $data{$label} .= pack('n*', map { hex s/^\s*\$//r } split /,/, $1); next }
    undef $label if defined $label && /^\w+:/;
}
for my $n (1 .. 4) {
    my $d = $data{"Pal_SFzCyc$n"} // die "no cycle $n\n";
    open my $o, '>:raw', "res/S2Palette/SFZCycle$n" or die $!;
    print $o $d;
    close $o;
}
print "SFz cycles written: ", join(' ', map { length $data{"Pal_SFzCyc$_"} } 1 .. 4), " bytes\n";
