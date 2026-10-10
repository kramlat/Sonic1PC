#!/usr/bin/perl
# Turns Sonic 2's music sources (SMPS2ASM .asm files written for the Sonic 2 driver, as in the final game's sound/music folder and the alpha's) into the
# port's converted JSONC: runs asm2json on each (with the SMPS2ASM include line the converter expects added). The tempos stay Sonic 2's (the engine plays a song with
# Sonic 2's tempo when its driver version is SOUND_DRIVER_VERSION_2_TEMPO); --s1-tempo changes them to Sonic 1's with SMPS2ASM's s2TempotoS1 (the main tempo byte of the
# header, and of any smpsSetTempoMod, becomes (((768 - n) >> 1) / (256 - n)) & $FF), which cannot play the slow ones: below half speed it has nothing, and its steps are coarse.
#
# Usage: import_s2_music.pl [--s1-tempo] <asm2json> <input folder> <output folder> [name ...]   (no names: every .asm of the input folder)
#        import_s2_music.pl --tempo 0xE0 0x80 ...                                              (prints the Sonic 1 tempo of each Sonic 2 tempo)
use strict;
use warnings;

sub s2_tempo_to_s1 {
    my ($n) = @_;
    die "Invalid main tempo of 0 in a Sonic 2 song\n" if $n == 0;
    return int(int((768 - $n) / 2) / (256 - $n)) & 0xFF;
}

sub hex_value {
    my ($s) = @_;
    return $s =~ /^0x/i ? hex($s) : $s + 0;
}

if (@ARGV && $ARGV[0] eq '--tempo') {
    shift @ARGV;
    printf("%s -> 0x%02X\n", $_, s2_tempo_to_s1(hex_value($_))) for @ARGV;
    exit 0;
}

my $s1_tempo = 0;
if (@ARGV && $ARGV[0] eq '--s1-tempo') {
    shift @ARGV;
    $s1_tempo = 1;
}
die "usage: $0 [--s1-tempo] <asm2json> <input folder> <output folder> [name ...]\n" if @ARGV < 3;
my ($tool, $in, $out, @names) = @ARGV;
if (!@names) {
    opendir(my $dh, $in) or die "cannot open $in: $!\n";
    @names = sort map { s/\.asm$//r } grep { /\.asm$/ } readdir($dh);
    closedir($dh);
}

for my $name (@names) {
    open(my $fh, '<', "$in/$name.asm") or die "cannot open $in/$name.asm: $!\n";
    my $src = do { local $/; <$fh> };
    close($fh);
    $src = "\tinclude \"_smps2asm_inc.asm\"\n\n$src" unless $src =~ /include\s+"_smps2asm_inc\.asm"/;
    my $tmp = "/tmp/import_s2_music_$$.asm";
    open(my $tf, '>', $tmp) or die "cannot write $tmp: $!\n";
    print $tf $src;
    close($tf);

    my $json = `"$tool" "$tmp"`;
    die "asm2json failed for $name\n" if $? != 0;
    warn "$name: unrecognized macro left in the output\n" if $json =~ /_unrecognized_macro/;

    my $changed = $s1_tempo ? 0 : 1;
    $json =~ s/("smpsHeaderTempo":\s*\[\s*"[^"]*",\s*")([^"]*)(")/$changed++; $1 . sprintf("0x%X", s2_tempo_to_s1(hex_value($2))) . $3/e if $s1_tempo;
    $json =~ s/("smpsSetTempoMod":\s*")([^"]*)(")/$changed++; $1 . sprintf("0x%X", s2_tempo_to_s1(hex_value($2))) . $3/ge if $s1_tempo;
    warn "$name: no tempo found\n" unless $changed;

    open(my $of, '>', "$out/$name.jsonc") or die "cannot write $out/$name.jsonc: $!\n";
    print $of $json;
    close($of);
    unlink($tmp);
    print "$name\n";
}
