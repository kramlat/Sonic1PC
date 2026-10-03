#!/usr/bin/perl
# Imports object assets from the Nick Arcade disassembly (~/Projects/s2na-disasm) into Sonic 2's folders and lists them in paradoxmakefile.yml:
#
#   import_s2na_assets.pl [--na DIR] ITEM...
#
#   art:NAME=art/nemesis/File.bin        copies the Nemesis art to res/Art/NAME
#   map:NAME=mappings/sprite/File.asm    copies a mappings table as asm/Mappings/NAME.asm (assembled to res/Mappings/NAME)
#   map:NAME=mappings/sprite/File.bin    copies a binary mappings table to res/Mappings/NAME
#   anim:NAME=FILE:Ani_label             writes res/Animation/NAME from the animation table `Ani_label` in FILE (a path in the disassembly: s2.asm or an _incObj file)
#
# New names only: an asset that Sonic 1 also has (res/Art/Ring, ...) must be dropped from the reuse list by hand.
use strict;
use warnings;
use File::Basename;
use File::Path qw(make_path);

my $na = "$ENV{HOME}/Projects/s2na-disasm";
use File::Spec;
my $root = File::Spec->rel2abs(dirname(File::Spec->rel2abs($0)) . "/..");
my @items;
while (@ARGV) {
    my $a = shift @ARGV;
    if ($a eq '--na') { $na = shift @ARGV; next; }
    push @items, $a;
}

sub slurp { my $f = shift; open my $h, '<:raw', $f or die "$f: $!"; local $/; my $d = <$h>; close $h; return $d; }
sub spit { my ($f, $d) = @_; make_path(dirname($f)); open my $h, '>:raw', $f or die "$f: $!"; print $h $d; close $h; }

my (@resources, @assemble);

# An animation table: the words are offsets from the table to each script, the scripts follow as dc.b lines (byte_xxxx: labels)
sub animation {
    my ($file, $table) = @_;
    my $text = slurp("$na/$file");
    $text =~ s/\r//g;
    my @l = split /\n/, $text;
    my $i = 0;
    $i++ while $i < @l && $l[$i] !~ /^\Q$table\E:/;
    die "no table $table in $file" if $i >= @l;
    my @entries;
    my $line = $l[$i];
    while (1) {
        $line =~ s/;.*//;
        if ($line =~ /dc\.w\s+([\w.]+)-\Q$table\E/) { push @entries, $1; }
        $i++;
        last if $i >= @l;
        $line = $l[$i];
        my $t = $line; $t =~ s/;.*//;
        next if $t =~ /^\s*$/; # (a comment line carried over)
        last unless $t =~ /dc\.w\s+[\w.]+-\Q$table\E/;
    }
    # the scripts: labelled dc.b blocks
    my (%at, $bytes); $bytes = '';
    my $cur;
    for (; $i < @l; $i++) {
        my $t = $l[$i]; $t =~ s/;.*//;
        next if $t =~ /^\s*$/;
        if ($t =~ /^\s*even\b/) { $bytes .= "\0" if length($bytes) & 1; next; }
        if ($t =~ /^([\w.]+):\s*(.*)$/) { $at{$1} = length($bytes); $t = $2; }
        elsif ($t =~ /^\w/) { last; }
        if ($t =~ /dc\.b\s+(.*)$/) {
            for my $v (split /,/, $1) {
                $v =~ s/\s//g;
                next if $v eq '';
                $bytes .= chr(($v =~ /^\$(\w+)$/ ? hex($1) : $v) & 0xFF);
            }
        } elsif ($t =~ /^\s*(dc\.w|dc\.l|include|incbin)/) { last; }
        elsif ($t =~ /\S/ && $t !~ /^\s*$/) { last; }
    }
    my $head = '';
    my $base = 2 * @entries;
    for my $e (@entries) { die "script $e not found" unless exists $at{$e}; $head .= pack('n', $base + $at{$e}); }
    return $head . $bytes;
}

# An inline mappings table of s2.asm: `Map_label: dc.w word_X-Map_label ...`, its frames the `word_X:` blocks (anywhere in the file); written as an asm table in the local-label form
sub inline_mappings {
    my ($name, $table) = @_;
    my $text = slurp("$na/s2.asm"); $text =~ s/\r//g;
    my @l = split /\n/, $text;
    my %block; my $cur;
    for my $line (@l) {
        my $t = $line; $t =~ s/;.*//;
        if ($t =~ /^(word_[0-9A-Fa-f]+):\s*(dc\.w.*)$/) { $cur = $1; $block{$cur} = [$2]; }
        elsif ($cur && $t =~ /^\s+dc\.w/) { push @{$block{$cur}}, $t; }
        else { $cur = undef; }
    }
    my $i = 0;
    $i++ while $i < @l && $l[$i] !~ /^\Q$table\E:/;
    die "no table $table" if $i >= @l;
    my @frames;
    for (; $i < @l; $i++) {
        my $t = $l[$i]; $t =~ s/;.*//;
        if ($t =~ /dc\.w\s+(word_[0-9A-Fa-f]+)-\Q$table\E/) { push @frames, $1; next; }
        last if @frames && $t =~ /\S/;
    }
    my $out = "; ---------------------------------------------------------------------------\n; $table of Nick Arcade (s2.asm), Sonic 2 mappings format\n; ---------------------------------------------------------------------------\n";
    my $n = 0;
    for my $f (@frames) { $out .= ($n == 0 ? ".int:" : "") . "\t\tdc.w .f$n-.int\n"; $n++; }
    $n = 0;
    for my $f (@frames) {
        die "no frame $f" unless $block{$f};
        my @b = @{$block{$f}};
        my $first = shift @b;
        $out .= ".f$n:\t\t$first\n" . join("", map { "$_\n" } @b);
        $n++;
    }
    return $out;
}

for my $it (@items) {
    my ($kind, $rest) = split /:/, $it, 2;
    my ($name, $src) = split /=/, $rest, 2;
    if ($kind eq 'art') {
        spit("$root/res/Art/$name", slurp("$na/$src"));
        push @resources, "Art/$name";
    } elsif ($kind eq 'map') {
        if ($src =~ /\.asm$/) {
            my $d = slurp("$na/$src"); $d =~ s/\r//g;
            spit("$root/asm/Mappings/$name.asm", $d);
            push @assemble, "Mappings/$name";
        } else {
            spit("$root/res/Mappings/$name", slurp("$na/$src"));
        }
        push @resources, "Mappings/$name";
    } elsif ($kind eq 'mapi') {
        spit("$root/asm/Mappings/$name.asm", inline_mappings($name, $src));
        push @assemble, "Mappings/$name";
        push @resources, "Mappings/$name";
    } elsif ($kind eq 'anim') {
        my ($file, $table) = split /:/, $src, 2;
        spit("$root/res/Animation/$name", animation($file, $table));
        push @resources, "Animation/$name";
    } else { die "unknown item $it"; }
}

# list them in the project file
my $yml = "$root/paradoxmakefile.yml";
my $y = slurp($yml);
for my $r (@resources) {
    next if $y =~ /^  - "\Q$r\E"$/m;
    $y =~ s/^(sources:)$/  - "$r"\n$1/m;
}
for my $a (@assemble) {
    next if $y =~ /^  - "\Q$a\E"$/m && $y =~ /assemble:[^\n]*\n(?:  - [^\n]*\n)*  - "\Q$a\E"/;
    $y =~ s/^(cmake:)/  - "$a"\n$1/m;
}
spit($yml, $y);
print "imported: @resources\n";
