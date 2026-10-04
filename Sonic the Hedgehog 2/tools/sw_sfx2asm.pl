#!/usr/bin/perl
# Turns the Simon Wai prototype's sound effects, which its disassembly (s2b.asm) only has as bytes (Sfx_A0 ... Sfx_CF), back into SMPS2ASM source the converter understands (asm2json): header, channels with their
# notes and coordination flags, and the FM voices.
#
#   sw_sfx2asm.pl [--src s2b.asm] [--out DIR] ID...      ID is A0 ... CF (hex)
#
# Writes DIR/sndID.asm (default DIR = the current directory). The bytes are those of the Z80 driver's format (SonicDriverVer 2), where a pointer is the address in the driver's bank window
# ($8000 + the ROM address mod $8000), which is turned back into a label by the offset from the effect's own start (the "loc_FFxxx" in the label's comment).
use strict;
use warnings;

my $src = "$ENV{HOME}/Projects/s2sw-disasm/s2b.asm";
my $out = '.';
my @ids;
while (@ARGV) {
    my $a = shift @ARGV;
    if ($a eq '--src') { $src = shift @ARGV }
    elsif ($a eq '--out') { $out = shift @ARGV }
    else { push @ids, map { uc } split /\s+/, $a }
}
die "usage: sw_sfx2asm.pl [--src s2b.asm] [--out DIR] ID...\n" unless @ids;

# the effects' bytes and where each is in the Z80 window
open my $h, '<', $src or die "$src: $!";
my (%bytes, %base);
my $cur;
while (my $l = <$h>) {
    $l =~ s/\r//;
    if ($l =~ /^Sfx_([0-9A-F]{2}):\s*;\s*loc_([0-9A-F]+):/i) {
        $cur = uc $1;
        $base{$cur} = (hex($2) & 0x7FFF) | 0x8000;
        $bytes{$cur} = [];
    } elsif (defined $cur && $l =~ /^\s*dc\.b\s+(.*)$/) {
        my $list = $1;
        $list =~ s/;.*//;
        push @{ $bytes{$cur} }, map { hex } ($list =~ /\$([0-9A-Fa-f]{1,2})/g);
    } elsif (defined $cur && $l =~ /^\S/) {
        $cur = undef;
    }
}
close $h;

my %chan = (0x00 => 'cFM1', 0x01 => 'cFM2', 0x02 => 'cFM3', 0x04 => 'cFM4', 0x05 => 'cFM5', 0x06 => 'cFM6', 0x80 => 'cPSG1', 0xA0 => 'cPSG2', 0xC0 => 'cPSG3', 0xE0 => 'cNoise');
my %pan = (0x00 => 'panNone', 0x40 => 'panRight', 0x80 => 'panLeft', 0xC0 => 'panCentre');
# coordination flag => [macro, number of parameter bytes]
my %flag = (
    0xE1 => ['smpsAlterNote', 1], 0xE2 => ['smpsNop', 1], 0xE3 => ['smpsReturn', 0], 0xE4 => ['smpsFade', 0], 0xE5 => ['smpsChanTempoDiv', 1],
    0xE6 => ['smpsAlterVol', 1], 0xE8 => ['smpsNoteFill', 1], 0xE9 => ['smpsAlterPitch', 1], 0xEA => ['smpsSetTempoMod', 1], 0xEB => ['smpsSetTempoDiv', 1],
    0xEC => ['smpsPSGAlterVol', 1], 0xED => ['smpsClearPush', 0], 0xEE => ['smpsStopSpecial', 0], 0xEF => ['smpsSetvoice', 1], 0xF0 => ['smpsModSet', 4],
    0xF1 => ['smpsModOn', 0], 0xF2 => ['smpsStop', 0], 0xF3 => ['smpsPSGform', 1], 0xF4 => ['smpsModOff', 0], 0xF5 => ['smpsPSGvoice', 1],
);

sub hx { sprintf '$%02X', $_[0] }

# The FM voices (25 bytes each) as smpsVc macros. The bytes of each group are in the order op4, op2, op3, op1.
sub voice_lines {
    my ($label, @bytes) = @_;
    die "$label: voice data is not a whole number of voices\n" if @bytes % 25;
    my @L = ("$label:");
    for my $vi (0 .. @bytes / 25 - 1) {
        my @v = @bytes[ 25 * $vi .. 25 * $vi + 24 ];
        push @L, sprintf('; Voice %s', hx($vi));
        my ($alg, $fb, $unused) = ($v[0] & 7, ($v[0] >> 3) & 7, $v[0] >> 6);
        my @slot = (3, 1, 2, 0); # the position within a group => the logical operator (0-3)
        my (@dt, @cf, @rs, @ar, @am, @d1r, @d2r, @dl, @rr, @tl);
        for my $g (0 .. 3) {
            my $op = $slot[$g];
            my $x = sub { $v[1 + $_[0] * 4 + $g] };
            $dt[$op] = ($x->(0) >> 4) & 7;  $cf[$op] = $x->(0) & 15;
            $rs[$op] = $x->(1) >> 6;        $ar[$op] = $x->(1) & 31;
            $am[$op] = $x->(2) >> 5;        $d1r[$op] = $x->(2) & 31;
            $d2r[$op] = $x->(3);
            $dl[$op] = $x->(4) >> 4;        $rr[$op] = $x->(4) & 15;
            $tl[$op] = $x->(5) & 0x7F;
        }
        push @L, "\tsmpsVcAlgorithm     " . hx($alg), "\tsmpsVcFeedback      " . hx($fb), "\tsmpsVcUnusedBits    " . hx($unused);
        push @L, "\tsmpsVcDetune        " . join(', ', map { hx($_) } @dt);
        push @L, "\tsmpsVcCoarseFreq    " . join(', ', map { hx($_) } @cf);
        push @L, "\tsmpsVcRateScale     " . join(', ', map { hx($_) } @rs);
        push @L, "\tsmpsVcAttackRate    " . join(', ', map { hx($_) } @ar);
        push @L, "\tsmpsVcAmpMod        " . join(', ', map { hx($_) } @am);
        push @L, "\tsmpsVcDecayRate1    " . join(', ', map { hx($_) } @d1r);
        push @L, "\tsmpsVcDecayRate2    " . join(', ', map { hx($_) } @d2r);
        push @L, "\tsmpsVcDecayLevel    " . join(', ', map { hx($_) } @dl);
        push @L, "\tsmpsVcReleaseRate   " . join(', ', map { hx($_) } @rr);
        push @L, "\tsmpsVcTotalLevel    " . join(', ', map { hx($_) } @tl);
    }
    return @L;
}

# What an effect's header says, and where its voices end (the end of the region they are in: where the next track, or the data, begins)
sub info {
    my ($id) = @_;
    my $bb = $bytes{$id} or return;
    my $base = $base{$id};
    my $n = $bb->[3];
    my @ptr = map { ($bb->[4 + 6 * $_ + 2] | ($bb->[4 + 6 * $_ + 3] << 8)) - $base } 0 .. $n - 1;
    my $voice = $bb->[0] | ($bb->[1] << 8);
    my $voice_off = $voice ? $voice - $base : undef;
    my $len = @$bb;
    my $own = defined $voice_off && $voice_off >= 0 && $voice_off < $len;
    my @starts = sort { $a <=> $b } (@ptr, ($own ? ($voice_off) : ()));
    my $voice_end = $len;
    if ($own) { for my $s (@starts) { if ($s > $voice_off) { $voice_end = $s; last } } }
    return { bb => $bb, base => $base, len => $len, voice_off => $voice_off, own => $own, voice_end => $voice_end };
}

for my $id (@ids) {
    my $bb = $bytes{$id} or die "no Sfx_$id in $src\n";
    my $base = $base{$id};
    my $len = @$bb;
    my $name = "snd$id";
    my $voice = $bb->[0] | ($bb->[1] << 8);
    my $tick = $bb->[2];
    my $n = $bb->[3];
    my $hdr = 4 + 6 * $n;
    my @ch;
    for my $i (0 .. $n - 1) {
        my $o = 4 + 6 * $i;
        push @ch, { ctl => $bb->[$o], id => $bb->[$o + 1], ptr => ($bb->[$o + 2] | ($bb->[$o + 3] << 8)) - $base, pitch => $bb->[$o + 4], vol => $bb->[$o + 5] };
    }
    my $voice_off = $voice ? $voice - $base : undef;
    my @foreign_voice;   # (the voice is in another effect: its bytes are copied here, after the tracks)
    if (defined $voice_off && ($voice_off < 0 || $voice_off >= $len)) {
        my $found;
        for my $o (sort keys %bytes) {
            my $i = info($o);
            if ($i->{own} && $voice >= $i->{base} + $i->{voice_off} && $voice < $i->{base} + $i->{voice_end}) { $found = $i; last }
        }
        die sprintf("%s: its voice at \$%04X is in no effect\n", $id, $voice) unless $found;
        my $from = $voice - $found->{base};
        @foreign_voice = @{ $found->{bb} }[ $from .. $found->{voice_end} - 1 ];
        $voice_off = undef;
    }

    # the regions: header, the voices, and the tracks, each up to where the next begins
    my %start;
    $start{$_->{ptr}} = 'track' for @ch;
    $start{$voice_off} = 'voice' if defined $voice_off;
    my @starts = sort { $a <=> $b } keys %start;
    die "$id: header overlaps data\n" if @starts && $starts[0] < $hdr;
    my %label;
    my %chan_label;
    for my $c (@ch) {
        my $nm = $chan{ $c->{id} } // die "$id: unknown channel $c->{id}\n";
        (my $short = $nm) =~ s/^c//;
        my $lab = "${name}_$short";
        $chan_label{ $c->{ptr} } //= $lab;
        $c->{label} = $chan_label{ $c->{ptr} };
        $label{ $c->{ptr} } = $chan_label{ $c->{ptr} };
    }
    my $voices_label = "${name}_Voices";

    # pass 1: decode the track regions into items, noting the offsets that are jumped to
    my %item_at;     # offset => [text lines]
    my @order;       # offsets of items, in order
    my (%targets, %kind_count);
    my $new_label = sub {
        my ($off, $kind) = @_;
        return $label{$off} if $label{$off};
        my $k = ++$kind_count{$kind};
        return $label{$off} = sprintf('%s_%s%02d', $name, $kind, $k);
    };
    my @tracks;
    for my $i (0 .. $#starts) {
        next unless $start{ $starts[$i] } eq 'track';
        my $end = $i < $#starts ? $starts[$i + 1] : $len;
        push @tracks, [$starts[$i], $end];
    }
    # (labels for jump targets are named in pass 1; the offsets they point at are inside some track's range)
    my %decoded;
    for my $t (@tracks) {
        my ($o, $end) = @$t;
        my @pending;
        while ($o < $end) {
            my $c = $bb->[$o];
            if ($c < 0xE0 || $c == 0xE7) {
                my $text = $c == 0xE7 ? 'smpsNoAttack' : hx($c);
                $decoded{$o} = { kind => 'data', text => $text, size => 1 };
                $o++;
            } elsif ($c == 0xE0) {
                my $p = $bb->[$o + 1];
                $decoded{$o} = { kind => 'op', text => sprintf("smpsPan %s, %s", $pan{ $p & 0xC0 }, hx($p & 0x3F)), size => 2 };
                $o += 2;
            } elsif ($c == 0xF6 || $c == 0xF8 || $c == 0xF7) {
                my ($mac, $skip) = $c == 0xF6 ? ('smpsJump', 1) : $c == 0xF8 ? ('smpsCall', 1) : ('smpsLoop', 3);
                my $po = $o + 1 + ($c == 0xF7 ? 2 : 0);
                my $ptr = ($bb->[$po] | ($bb->[$po + 1] << 8)) - $base;
                my $kind = $c == 0xF6 ? 'Jump' : $c == 0xF8 ? 'Call' : 'Loop';
                $targets{$ptr} = $kind;
                my $text = $c == 0xF7 ? sprintf('smpsLoop %s, %s, @@%d@@', hx($bb->[$o + 1]), hx($bb->[$o + 2]), $ptr) : sprintf('%s @@%d@@', $mac, $ptr);
                $decoded{$o} = { kind => 'op', text => $text, size => $c == 0xF7 ? 5 : 3 };
                $o += $decoded{$o}{size};
            } elsif ($flag{$c}) {
                my ($mac, $np) = @{ $flag{$c} };
                my @a = map { hx($bb->[$o + 1 + $_]) } 0 .. $np - 1;
                $decoded{$o} = { kind => 'op', text => join(' ', $mac, join(', ', @a)), size => 1 + $np };
                $o += 1 + $np;
            } else {
                die sprintf("%s: unknown coordination flag \$%02X at offset \$%X\n", $id, $c, $o);
            }
        }
    }
    $new_label->($_, $targets{$_}) for sort { $a <=> $b } keys %targets;

    # pass 2: write it out
    my @L;
    push @L, "${name}_Header:";
    push @L, "\tsmpsHeaderStartSong 2";
    push @L, "\tsmpsHeaderVoice     $voices_label"; # (an effect without voices still names where they would be: the end)
    push @L, "\tsmpsHeaderTempoSFX  " . hx($tick);
    push @L, "\tsmpsHeaderChanSFX   " . hx($n);
    push @L, '';
    for my $c (@ch) {
        push @L, sprintf("\tsmpsHeaderSFXChannel %s, %s, %s, %s", $chan{ $c->{id} }, $c->{label}, hx($c->{pitch}), hx($c->{vol}));
    }
    push @L, '';
    for my $i (0 .. $#starts) {
        my $s = $starts[$i];
        my $end = $i < $#starts ? $starts[$i + 1] : $len;
        if ($start{$s} eq 'voice') {
            push @L, voice_lines($voices_label, @{$bb}[ $s .. $end - 1 ]);
            next;
        }
        # a track: its items from the start to the end, a label wherever one is wanted
        my $o = $s;
        my @row;
        my $flush = sub { push @L, "\tdc.b\t" . join(', ', @row) if @row; @row = () };
        while ($o < $end) {
            my $d = $decoded{$o} or die "$id: nothing decoded at \$$o\n";
            if (defined $label{$o}) {
                $flush->();
                push @L, "$label{$o}:";
            }
            if ($d->{kind} eq 'data') {
                push @row, $d->{text};
                $flush->() if @row >= 8;
            } else {
                $flush->();
                (my $t = $d->{text}) =~ s/\@\@(-?\d+)\@\@/defined $label{$1} ? $label{$1} : die("$id: jump to \$$1 is not at an instruction\n")/e;
                push @L, "\t$t";
            }
            $o += $d->{size};
        }
        $flush->();
        push @L, '';
    }
    push @L, voice_lines($voices_label, @foreign_voice) if @foreign_voice;
    push @L, "$voices_label:" unless $voice;
    open my $w, '>', "$out/$name.asm" or die "$out/$name.asm: $!";
    print $w join("\n", @L), "\n";
    close $w;
    print "$id: $n channel(s), ", scalar(@tracks), " track(s)\n";
}
