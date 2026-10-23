#!/usr/bin/perl
# Writes a major/minor swapped copy of a converted song (res/SMPS/converted/music/*.jsonc): in every melodic track the third, sixth and seventh of the
# song's key trade places with their major/minor twin (pitch classes 3<->4, 8<->9, 10<->11 above the tonic), so a minor tune comes out major and the other way
# round. The drum (DAC) track and the noise channel (PSG3) are left as they are. The tracks only carry octave transpositions, so the note names are the pitches.
#   usage: perl tools/make_mode_swap.pl <tonic, e.g. C> <in.jsonc> <out.jsonc> [skipped track suffixes: DAC PSG3]
use strict;
use warnings;

my ($tonic, $in, $out, @skip) = @ARGV;
die "usage: $0 tonic in.jsonc out.jsonc [skipped track suffixes]\n" unless defined $out;
@skip = qw(DAC PSG3) unless @skip;

my %pc = (C => 0, Cs => 1, Db => 1, D => 2, Ds => 3, Eb => 3, E => 4, F => 5, Fs => 6, Gb => 6, G => 7, Gs => 8, Ab => 8, A => 9, As => 10, Bb => 10, B => 11);
my @flat = qw(C Db D Eb E F Fs G Ab A Bb B);
die "unknown tonic $tonic\n" unless exists $pc{$tonic};
my $key = $pc{$tonic};
my %twin = (3 => 4, 4 => 3, 8 => 9, 9 => 8, 10 => 11, 11 => 10);

open(my $fh, '<', $in) or die "$in: $!\n";
my @lines = <$fh>;
close $fh;

my ($track, $changed) = ('', 0);
for (@lines) {
    $track = $1 if /^    "(\w+)": \[/;
    next if grep { $track =~ /_\Q$_\E$/ } @skip;
    s{("note": ")([A-G][sb]?)(\d)(")}{
        my ($head, $name, $oct, $tail) = ($1, $2, $3, $4);
        my $rel = ($pc{$name} - $key) % 12;
        if (exists $twin{$rel}) {
            my $new = $flat[($key + $twin{$rel}) % 12];
            $changed++;
            $name = $new;
        }
        "$head$name$oct$tail"
    }e;
}
open(my $o, '>', $out) or die "$out: $!\n";
print $o @lines;
close $o;
print "$changed notes swapped\n";
