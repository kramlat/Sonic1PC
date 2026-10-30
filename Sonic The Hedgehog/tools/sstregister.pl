#!/usr/bin/perl
# Registers objects that have been converted with objtosst.pl in Sonic 1's tables (src/Sonic1Objects.c): the prototype goes, the id's row in game_objects[] becomes NULL, a row goes into
# sst_object_codes[], the header is included; the object's mappings file(s) take the Sonic 3 macros; the layouts lose the old remember bit of the id.
#
# Usage: sstregister.pl OBJECT[:Mappings[,Mappings...]]...   (OBJECT as in Obj_OBJECT; the mappings default to OBJECT; tools/ is the cwd's, run from "Sonic The Hedgehog")
use strict;
use warnings;
sub slurp { open my $h, '<', $_[0] or die "$_[0]: $!"; local $/; my $d = <$h>; close $h; $d }
sub spit  { open my $h, '>', $_[0] or die "$_[0]: $!"; print $h $_[1]; close $h }

my $table = slurp('src/Sonic1Objects.c');
sub header_of { my ($n) = @_; for my $h (glob('src/Object/*.h')) { return $h =~ m{([^/]+)\.h$} ? $1 : $n if slurp($h) =~ /\bObj_\Q$n\E\(Sst/; } return $n }
my @ids;
# Sonic 2 has its own version of some objects (its yml lists the source in `except:`): those are not Sonic 1's and stay as they are
sub s2_replaces {
    my ($n) = @_;
    my $yml = slurp('../Sonic the Hedgehog 2/paradoxmakefile.yml');
    for my $c (glob('src/Object/*.c')) {
        next unless slurp($c) =~ /^void Obj_\Q$n\E\(/m;
        return $yml =~ /"\Q$c\E"/ ? 1 : 0;
    }
    return 0;
}
# a header that has no prototype for the object's routine gets one (the table includes the header)
sub ensure_prototype {
    my ($n) = @_;
    for my $h (glob('src/Object/*.h')) { return if slurp($h) =~ /\bObj_\Q$n\E\(Sst/; }
    for my $c (glob('src/Object/*.c')) {
        next unless slurp($c) =~ /^void Obj_\Q$n\E\(Sst/m;
        (my $h = $c) =~ s/\.c$/.h/;
        my $t = slurp($h);
        $t =~ s/#endif/void Obj_$n(Sst *self);\n\n#endif/;
        spit($h, $t);
        return;
    }
}
my @s2ids;
for my $arg (@ARGV) {
    my ($name, $maps) = split /:/, $arg, 2;
    my @maps = defined $maps ? split(/,/, $maps) : ($name);
    $table =~ s/^void Obj_\Q$name\E\(Object \*obj\);\n//m;
    $table =~ s{^(\t/\* ([0-9A-F]{2}) (ObjId_\w+)\s*\*/ )Obj_\Q$name\E,}{$1NULL, // (moved to the status table: sst_object_codes)}m
        or die "$name: not in the table\n";
    my ($id, $idname) = ($2, $3);
    push @ids, hex($id);
    ensure_prototype($name);
    my $s2 = '../Sonic the Hedgehog 2/src/Sonic2Objects.c';
    if (-f $s2 && !s2_replaces($name)) {   # Sonic 2 runs some of Sonic 1's objects as they are: it moves them as well
        my $t2 = slurp($s2);
        if ($t2 =~ s{^(\t/\* [0-9A-F]{2} ObjId_\w+\s*\*/ )Obj_\Q$name\E,}{$1NULL, // (moved to the status table: sst_object_codes)}m) {
            $t2 =~ s/^void Obj_\Q$name\E\(Object \*obj\);\n//m;
            $t2 =~ s/(const SstCode sst_object_codes\[0x100\] = \{\n)/$1\t[$idname] = Obj_$name,\n/ or die "no sst table in Sonic 2\n";
            { my $h = header_of($name); $t2 =~ s/(#include "Object\/Points.h"\n)/$1#include "Object\/$h.h"\n/ unless $t2 =~ /#include "Object\/\Q$h\E\.h"/; }
            spit($s2, $t2);
            push @s2ids, hex($id);
            warn "$name: Sonic 2 runs it too: moved there as well\n";
        }
    }
    $table =~ s/(const SstCode sst_object_codes\[0x100\] = \{\n)/$1\t[$idname] = Obj_$name,\n/ or die "no sst table\n";
    { my $h = header_of($name); $table =~ s/(#include "Object\/Points.h"\n)/$1#include "Object\/$h.h"\n/ unless $table =~ /#include "Object\/\Q$h\E\.h"/; }
    for my $m (@maps) {
        my $p = "asm/Mappings/$m.asm";
        my $t = slurp($p);
        $t =~ s{Mappings/_MapMacros\.asm}{Mappings/_MapMacrosS3.asm} or warn "$p: no macros include (raw data? convert by hand)\n";
        spit($p, $t);
    }
}
spit('src/Sonic1Objects.c', $table);
my $list = join ',', map { sprintf '0x%02X', $_ } @ids;
system("perl tools/objlayout_clear_remember.pl $list res/ObjectLayout/* 2>&1 | grep -v skipped");
if (@s2ids) {
    my $l2 = join ',', map { sprintf '0x%02X', $_ } @s2ids;
    system("perl tools/objlayout_clear_remember.pl $l2 '../Sonic the Hedgehog 2/res/S2Objects'/* 2>&1 | grep -v skipped");
}
