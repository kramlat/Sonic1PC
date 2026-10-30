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
my @ids;
for my $arg (@ARGV) {
    my ($name, $maps) = split /:/, $arg, 2;
    my @maps = defined $maps ? split(/,/, $maps) : ($name);
    $table =~ s/^void Obj_\Q$name\E\(Object \*obj\);\n//m;
    $table =~ s{^(\t/\* ([0-9A-F]{2}) (ObjId_\w+)\s*\*/ )Obj_\Q$name\E,}{$1NULL, // (moved to the status table: sst_object_codes)}m
        or die "$name: not in the table\n";
    my ($id, $idname) = ($2, $3);
    push @ids, hex($id);
    if (-f '../Sonic the Hedgehog 2/src/Sonic2Objects.c' && slurp('../Sonic the Hedgehog 2/src/Sonic2Objects.c') =~ /^\t\/\* [0-9A-F]{2} ObjId_\w+\s*\*\/ Obj_\Q$name\E,/m) {
        warn "$name: Sonic 2's table runs it too (shared): move it there as well (sst_object_codes, NULL in game_objects, its layouts in res/S2Objects)\n";
    }
    $table =~ s/(const SstCode sst_object_codes\[0x100\] = \{\n)/$1\t[$idname] = Obj_$name,\n/ or die "no sst table\n";
    $table =~ s/(#include "Object\/Points.h"\n)/$1#include "Object\/$name.h"\n/ unless $table =~ /#include "Object\/\Q$name\E\.h"/;
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
