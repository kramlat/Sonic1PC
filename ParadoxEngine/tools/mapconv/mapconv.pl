#!/usr/bin/env perl
# mapconv.pl -- converts Sonic 1 sprite-mapping .asm files from raw dc.b data
# to the mapping macros in Mappings/_MapMacros.asm (SN 68k / asm68k syntax:
# '@' local labels), and only keeps a converted file if it assembles to EXACTLY
# the same bytes as the original.
#
# Usage: mapconv.pl <clownassembler> <asm-dir> [--apply] [file.asm ...]
#   <asm-dir>   the project's asm/ directory (holds Mappings/_MapMacros.asm; the
#               assembler must run from here because includes are relative to
#               the working directory)
#   --apply     write converted files in place (default: report only)
#   file.asm    limit to these files in <asm-dir>/Mappings (default: all)
#
# Anything the parser does not fully understand is left untouched and listed.
use strict; use warnings; use File::Temp qw(tempdir); use File::Copy qw(copy); use File::Path qw(make_path); use File::Spec;

my ($asm_bin, $asm_dir, @rest) = @ARGV; die "usage: $0 <clownassembler> <asm-dir> [--apply] [files]\n" unless $asm_bin && $asm_dir;
$asm_bin = File::Spec->rel2abs($asm_bin); $asm_dir = File::Spec->rel2abs($asm_dir);
my $apply = grep { $_ eq '--apply' } @rest; my @only = grep { $_ ne '--apply' } @rest;
my $macros = "$asm_dir/Mappings/_MapMacros.asm"; die "missing $macros\n" unless -f $macros;
my $tmp = tempdir(CLEANUP => 1); make_path("$tmp/asm/Mappings"); copy($macros, "$tmp/asm/Mappings/_MapMacros.asm");

sub shq { my $s = shift; $s =~ s/'/'\\''/g; "'$s'" }
sub num { my $s = shift; $s =~ s/^\s+|\s+$//g; return hex($1) if $s =~ /^\$([0-9A-Fa-f]+)$/; return $s + 0 if $s =~ /^-?\d+$/; return undef }
sub sbyte { my $v = shift; $v -= 256 if $v > 127; $v }
sub fmt { my $v = shift; my $neg = $v < 0; $v = -$v if $neg; my $t = $v > 9 ? sprintf('$%X', $v) : "$v"; $neg ? "-$t" : $t }
sub slurp { open my $h, '<:raw', $_[0] or return undef; local $/; <$h> }
sub assemble { my ($dir, $file, $out) = @_;   # run from the asm root, like the real build
    my $rc = system('cd ' . shq($dir) . ' && ' . shq($asm_bin) . ' -i ' . shq($file) . ' -o ' . shq($out) . ' >/dev/null 2>&1');
    return $rc == 0 && -s $out }

my $LBL = qr/\@\w+|[A-Za-z_]\w*/;

sub convert {   # returns (new_text) or (undef, reason)
    my ($text) = @_; $text =~ s/\r//g; my @lines = split /\n/, $text, -1; pop @lines if @lines && $lines[-1] eq '';
    my $i = 0; my @out;
    while ($i < @lines && $lines[$i] =~ /^\s*(;.*)?$/) { push @out, $lines[$i]; $i++ }          # header comments
    return (undef, 'no table label') unless $i < @lines && $lines[$i] =~ /^([A-Za-z_]\w*):\s*(;.*)?$/;
    my ($tbl, $tcomment) = ($1, $2); $i++;
    my (@entries, @ecomments);
    while ($i < @lines && $lines[$i] =~ /^\s*dc\.w\s+(.*?)\s*(;.*)?$/) {
        my ($ops, $ec) = ($1, $2 // ''); $i++;
        my @ops = split /\s*,\s*/, $ops;
        for my $k (0 .. $#ops) { my $o = $ops[$k]; return (undef, "table entry '$o' is not <label>-$tbl") unless $o =~ /^($LBL)-\Q$tbl\E$/; push @entries, $1; push @ecomments, ($k == $#ops ? $ec : '') }
    }
    return (undef, 'empty table') unless @entries;
    my (@items, %seen);   # items: frames and standalone comment lines, in file order
    while ($i < @lines) {
        my $l = $lines[$i];
        if ($l =~ /^\s*$/) { $i++; next }
        if ($l =~ /^\s*(;.*)$/) { push @items, { comment => $1 }; $i++; next }
        last if $l =~ /^\s*even\s*(;.*)?$/i;
        if ($l =~ /^\s+(dc\.[bwl]\s+[^;]*?)\s*(;.*)?$/) { push @items, { raw => "\t$1" . ($2 ? "\t$2" : "") }; $i++; next }   # stray data (padding) between frames, kept verbatim
        my (@labels, $count, $lcomment); $lcomment = '';
        while (!defined $count && $i < @lines && $lines[$i] =~ /^($LBL):\s*(.*)$/) {
            push @labels, $1; my $rest = $2; $i++;
            if ($rest =~ /^dc\.b\s+([^;]*?)\s*(;.*)?$/) { $count = num($1); return (undef, "bad piece count '$1'") unless defined $count && $count >= 0; $lcomment = $2 // ''; }
            elsif ($rest =~ /^(;.*)?$/) { $lcomment ||= $1 // '' }
            else { return (undef, "unparsed after label: $rest") }
        }
        return (undef, "unexpected line: $lines[$i]") unless @labels && $i <= @lines;
        unless (defined $count) {   # count on the next line
            return (undef, "label '$labels[-1]' has no piece count") unless $i < @lines && $lines[$i] =~ /^\s*dc\.b\s+([^;,]*?)\s*(;.*)?$/;
            $count = num($1); return (undef, "bad piece count '$1'") unless defined $count && $count >= 0; $lcomment ||= $2 // ''; $i++;
        }
        my @pieces;
        for (1 .. $count) {
            return (undef, 'frame cut short') if $i >= @lines;
            return (undef, "expected a piece, got: $lines[$i]") unless $lines[$i] =~ /^\s*dc\.b\s+([^;]*?)\s*(;.*)?$/;
            my ($vals, $c) = ($1, $2 // ''); my @v = map { num($_) } split /\s*,\s*/, $vals; $i++;
            return (undef, "piece is not 5 numeric bytes: $vals") unless @v == 5 && !grep { !defined } @v;
            return (undef, "byte out of range: $vals") if grep { $_ < -128 || $_ > 255 } @v;
            my ($y, $sz, $th, $tl, $x) = @v; return (undef, "size byte has unused bits: $vals") if $sz < 0 || ($sz & ~0xF);
            $y = sbyte($y & 255); $x = sbyte($x & 255); $th &= 255; $tl &= 255;
            push @pieces, { x => $x, y => $y, w => (($sz >> 2) & 3) + 1, h => ($sz & 3) + 1, tile => (($th & 7) << 8) | $tl,
                            xf => ($th >> 3) & 1, yf => ($th >> 4) & 1, pal => ($th >> 5) & 3, pri => ($th >> 7) & 1, c => $c };
        }
        push @items, { labels => [@labels], pieces => \@pieces, c => $lcomment }; $seen{$_}++ for @labels;
    }
    my $even = 0; if ($i < @lines && $lines[$i] =~ /^\s*even\s*(;.*)?$/i) { $even = 1; $i++ }
    while ($i < @lines) { return (undef, "trailing content: $lines[$i]") unless $lines[$i] =~ /^\s*$/; $i++ }
    for my $e (@entries) { return (undef, "table references undefined frame $e") unless $seen{$e} }
    return (undef, 'duplicate frame label') if grep { $_ > 1 } values %seen;
    # emit (SN 68k style: frame labels keep their '@' / global spelling)
    push @out, "\tinclude\t\"Mappings/_MapMacros.asm\"", '';
    push @out, "$tbl:\tmappingsTable" . ($tcomment ? "\t$tcomment" : '');
    for my $k (0 .. $#entries) { push @out, "\tmappingsTableEntry.w\t$entries[$k]" . ($ecomments[$k] ? "\t$ecomments[$k]" : '') }
    for my $it (@items) {
        if (defined $it->{raw}) { push @out, $it->{raw}; next }
        push @out, ''; if (defined $it->{comment}) { push @out, $it->{comment}; next }
        my @l = @{ $it->{labels} }; my $main = pop @l;
        push @out, "$_:" for @l;
        push @out, "$main:\tspriteHeader" . ($it->{c} ? "\t$it->{c}" : '');
        for my $p (@{ $it->{pieces} }) {
            push @out, sprintf("\tspritePiece\t%s, %s, %d, %d, %s, %d, %d, %d, %d%s", fmt($p->{x}), fmt($p->{y}), $p->{w}, $p->{h},
                ($p->{tile} > 9 ? sprintf('$%X', $p->{tile}) : $p->{tile}), $p->{xf}, $p->{yf}, $p->{pal}, $p->{pri}, ($p->{c} ? "\t$p->{c}" : ''));
        }
        push @out, "${main}_End";
    }
    push @out, '', "\teven" if $even;
    return join("\n", @out) . "\n";
}

my @files = @only ? map { "$asm_dir/Mappings/$_" } @only : sort grep { !/_MapMacros\.asm$/ } glob("$asm_dir/Mappings/*.asm");
my (@ok, @skip, @bad);
for my $f (@files) {
    my ($name) = $f =~ m{([^/]+)$}; my $old = slurp($f); next unless defined $old;
    next if $old =~ /mappingsTable/;   # already converted
    my ($new, $why) = convert($old);
    if (!defined $new) { push @skip, "$name: $why"; next }
    my ($ob, $nb) = ("$tmp/old.bin", "$tmp/new.bin"); unlink $ob, $nb;
    unless (assemble($asm_dir, $f, $ob)) { push @skip, "$name: original does not assemble with this assembler"; next }
    open my $w, '>:raw', "$tmp/asm/Mappings/$name" or die; print $w $new; close $w;
    unless (assemble("$tmp/asm", "$tmp/asm/Mappings/$name", $nb)) { push @bad, "$name: converted file does not assemble"; next }
    if (slurp($ob) eq slurp($nb)) { push @ok, $name; if ($apply) { open my $o, '>:raw', $f or die; print $o $new; close $o } }
    else { push @bad, "$name: converted bytes DIFFER from original" }
}
printf "%d converted%s, %d skipped (left unchanged), %d FAILED verification (left unchanged)\n", scalar @ok, ($apply ? '' : ' (dry run)'), scalar @skip, scalar @bad;
print "SKIPPED:\n", map { "  $_\n" } @skip if @skip; print "FAILED:\n", map { "  $_\n" } @bad if @bad;
