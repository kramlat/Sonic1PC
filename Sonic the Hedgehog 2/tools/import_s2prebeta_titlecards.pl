#!/usr/bin/perl
# Imports the title cards of the Sonic 2 pre-beta (September 14, 1992: Neto's disassembly, S2PREBETA gives the folder, default ~/Projects/s2prebeta-disasm): the object $34's mappings
# (the name of each zone, ZONE, and the act numbers), its two Nemesis arts (the fixed part at tile $580: E, N, O, ZONE and the numbers; the alphabet at its own, from which the letters a zone
# needs are copied to tile $5DE on, in the order the name uses them: Load_Title_Cards_Art) and the table of those letters.
# Written: asm/Mappings/TitleCardNames.asm (Sonic 2 mappings format), src/TitleCardData.c, res/Art/TitleCardZone and res/Art/TitleCardLetters.
# The pre-beta's own names are kept as they are. Hidden Palace's, Wood's, Sand Shower's and Cyber City's are written here from the same alphabet (the user: do not port the pre-beta's Hidden
# Palace data; Wood, Sand Shower and Cyber City have no card in it): the letters 16 pixels wide (I is 8, M and W 24), 16 between words, the name centred where Green Hill's is.
# Run it in the Sonic 2 folder: import_s2prebeta_titlecards.pl
use strict;
use warnings;

my $src = $ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm";
my $asm = do { open my $f, '<', "$src/S2140992.asm" or die "$src/S2140992.asm: $!\n"; local $/; <$f> };

# the alphabet's letters in tc_ay.nem: [first tile, tiles] (vars.asm's TC_x for the ones the pre-beta uses; B, J, Q, V, W and X are the gaps between them)
my %alpha = (A => [0x00, 4], B => [0x04, 4], C => [0x08, 4], D => [0x0C, 4], F => [0x10, 4], G => [0x14, 4], H => [0x18, 4], I => [0x1C, 2], J => [0x1E, 4], K => [0x22, 4],
             L => [0x26, 4], M => [0x2A, 6], P => [0x30, 4], Q => [0x34, 4], R => [0x38, 4], S => [0x3C, 4], T => [0x40, 4], U => [0x44, 4], V => [0x48, 4], W => [0x4C, 6],
             X => [0x52, 4], Y => [0x56, 4]);
my %fixed = (E => 0x580, N => 0x584, O => 0x588);   # (in zone_123.nem, at tile $580)
my $LETTERS = 0x5DE;                                 # where the zone's letters are copied to

# the maps of the pre-beta, by label
sub map_of {
    my ($label) = @_;
    $asm =~ /^\Q$label\E:[^\n]*\n((?:[A-Za-z0-9_]+:[^\n]*\n)*)\s*dc\.w\s+\$([0-9A-Fa-f]+)[^\n]*\n((?:\s*dc\.l\s+[^\n]*\n)+)/m or die "no map $label\n";
    my @pieces;
    for my $line (split /\n/, $3) {
        $line =~ /dc\.l\s+\$([0-9A-Fa-f]{8}),\s*\$([0-9A-Fa-f]{8})(?:\s*;\s*(\S+))?/ or next;
        push @pieces, { a => hex $1, b => hex $2, text => $3 // '' };
    }
    return \@pieces;
}

my @names;    # [ frame comment, [pieces as 4 words], [letters used: (first tile, tiles) in the order] ]
sub verbatim {
    my ($comment, $label, $ndx) = @_;
    my $pieces = map_of($label);
    my (@words, %seen, @letters);
    for my $p (@$pieces) {
        push @words, [ $p->{a} >> 16, $p->{a} & 0xFFFF, $p->{b} >> 16, $p->{b} & 0xFFFF ];
        my $tile = $p->{a} & 0xFFFF;
        next if $tile < $LETTERS || $seen{$tile}++;
        my $l = $p->{text};
        die "$label: no letter for tile $tile\n" unless $alpha{$l};
        push @letters, [ $tile, @{ $alpha{$l} } ];
    }
    @letters = sort { $a->[0] <=> $b->[0] } @letters;
    push @names, [ $comment, \@words, [ map { [ $_->[1], $_->[2] ] } @letters ] ];
}
verbatim('GREEN HILL', 'GHz_TC_Map');
verbatim('METROPOLIS', 'Mz_TC_Map');
verbatim('HILL TOP', 'HTz_TC_Map');
verbatim('OIL OCEAN', 'OOz_TC_Map');
verbatim('DUST HILL', 'DHz_TC_Map');
verbatim('CASINO NIGHT', 'CNz_TC_Map');
verbatim('CHEMICAL PLANT', 'CPz_TC_Map');
verbatim('NEO GREEN HILL', 'NGHz_TC_Map');
verbatim('SKY CHASE', 'SCz_TC_Map');
verbatim('SKY FORTRESS', 'SFz_TC_Map');
verbatim('DEATH EGG', 'DEz_TC_Map');

# a name written from the alphabet
sub authored {
    my ($text) = @_;
    my $total = 0;
    my @chars = split //, $text;
    my $width = sub { my $c = shift; $c eq ' ' ? 16 : $c eq 'I' ? 8 : ($c eq 'M' || $c eq 'W') ? 24 : 16 };
    $total += $width->($_) for @chars;
    my $x = 0x34 - $total / 2;
    $x = int($x / 8) * 8;                    # (the pre-beta's names start on the eighth pixel)
    my ($next, %tile, @words, @letters) = ($LETTERS);
    for my $c (@chars) {
        if ($c ne ' ') {
            my ($t, $size);
            if (exists $fixed{$c}) { $t = $fixed{$c}; $size = 5 }
            else {
                die "no letter $c\n" unless $alpha{$c};
                unless (exists $tile{$c}) { $tile{$c} = $next; push @letters, [ $alpha{$c}[0], $alpha{$c}[1] ]; $next += $alpha{$c}[1] }
                $t = $tile{$c};
                $size = $alpha{$c}[1] == 2 ? 1 : $alpha{$c}[1] == 6 ? 9 : 5;
            }
            push @words, [ $size, $t, $t >> 1, $x & 0xFFFF ];
        }
        $x += $width->($c);
    }
    push @names, [ $text, \@words, \@letters ];
}
authored('WOOD');
authored('SAND SHOWER');
authored('CYBER CITY');
authored('HIDDEN PALACE');

# ZONE and the act numbers
my @fixed_maps;
for my $x (['ZONE', 'ZONE_TC_Map'], ['ACT 1', 'Act_1_TC_Map'], ['ACT 2', 'Act_2_TC_Map'], ['ACT 3', 'Act_3_TC_Map']) {
    my $pieces = map_of($x->[1]);
    push @fixed_maps, [ $x->[0], [ map { [ $_->{a} >> 16, $_->{a} & 0xFFFF, $_->{b} >> 16, $_->{b} & 0xFFFF ] } @$pieces ] ];
}
my @frames = (@names, map { [ $_->[0], $_->[1], [] ] } @fixed_maps);

open my $o, '>', 'asm/Mappings/TitleCardNames.asm' or die;
print $o "; ---------------------------------------------------------------------------\n; The title cards' names, ZONE and the act numbers: Map_Obj34 of the Sonic 2 pre-beta (Sonic 2 mappings format), tools/import_s2prebeta_titlecards.pl\n; Frames: the names (GREEN HILL ... DEATH EGG, then WOOD, SAND SHOWER, CYBER CITY, HIDDEN PALACE), ZONE, ACT 1, ACT 2, ACT 3 (TitleCard.c's frame table)\n; ---------------------------------------------------------------------------\n.int:";
print $o "\t\tdc.w .f$_-.int\n" for 0 .. $#frames;
for my $i (0 .. $#frames) {
    my ($c, $w) = @{ $frames[$i] };
    printf $o ".f%d:\t\tdc.w %d\t\t; %s\n", $i, scalar(@$w), $c;
    printf $o "\t\tdc.w \$%04X, \$%04X, \$%04X, \$%04X\n", @$_ for @$w;
}
close $o;

open my $c, '>', 'src/TitleCardData.c' or die;
print $c "// The letters each title card name needs from the alphabet art (Art_TitleCardLetters), as pairs of first tile and tiles, in the order they are copied to tile \$5DE on, ended by \$FF:\n// Load_Title_Cards_Art of the Sonic 2 pre-beta. Written by tools/import_s2prebeta_titlecards.pl (the frames are those of asm/Mappings/TitleCardNames.asm).\n#include <stdint.h>\n\n#define TITLECARD_NAMES 15 // (the frames of the names)\n\n";
printf $c "const uint8_t titlecard_letters[TITLECARD_NAMES][20] = {\n";
for my $i (0 .. $#names) {
    my @b = map { @$_ } @{ $names[$i][2] };
    printf $c "    /* %2d %s */ { %s0xFF },\n", $i, $names[$i][0], join('', map { sprintf '0x%02X, ', $_ } @b);
}
print $c "};\n";
close $c;
printf "%d frames (%d names)\n", scalar(@frames), scalar(@names);

for my $x (['ZONE_123.nem', 'TitleCardZone'], ['TC_AY.nem', 'TitleCardLetters']) {
    open my $in, '<:raw', "$src/Data/Art/$x->[0]" or die "$x->[0]: $!\n"; local $/; my $d = <$in>; close $in;
    open my $out, '>:raw', "res/Art/$x->[1]" or die; print $out $d; close $out;
}
