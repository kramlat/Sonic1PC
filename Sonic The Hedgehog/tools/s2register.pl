#!/usr/bin/perl
# Registers Sonic 2's own versions of objects (src/Object/X.c of "Sonic the Hedgehog 2", converted with objtosst.pl) in its tables (src/Sonic2Objects.c) and project file:
# the prototype goes, the id's row in game_objects[] becomes NULL, a row goes into sst_object_codes[] (the id taken from the row), the header (Sonic 1's, converted already) is included; each mapping is
# taken from resources: and listed in mappings_SonicTheHedgehog2: (binary Sonic 2 mappings) unless Sonic 2's asm for it is there, which is switched to the Sonic 3 macros; the layouts lose the old
# remember bit of the id.
#
# Usage: s2register.pl OBJECT[:Mappings[,Mappings...]]...   run from "Sonic the Hedgehog 2"
use strict;
use warnings;

my $tools = '../Sonic The Hedgehog/tools';
open my $in, '<', 'src/Sonic2Objects.c' or die $!; local $/; my $src = <$in>; close $in;
open $in, '<', 'paradoxmakefile.yml' or die $!; my $yml = <$in>; close $in;

for my $arg (@ARGV) {
    my ($obj, $maps) = split /:/, $arg, 2;
    my @maps = defined $maps ? ($maps eq "-" ? () : split(/,/, $maps)) : ($obj);
    $src =~ s/^void Obj_\Q$obj\E\(Object \*obj\);\n//m;
    $src =~ s/^(#include "Object\/\Q$obj\E\.h"\n)//m;
    if (-f "src/Object/$obj.h" || -f "../Sonic The Hedgehog/src/Object/$obj.h") {
        $src =~ s/^(#include "SstLoad\.h"\n)/$1#include "Object\/$obj.h"\n/m or die "no SstLoad.h include";
    }
    my $id;
    $src =~ s{(/\* ([0-9A-F]{2}) \w+\s*\*/ )Obj_\Q$obj\E,[^\n]*}{$id = hex($2); "$1NULL, // (moved to the status table: sst_object_codes)"}me or die "$obj: no row in game_objects[]";
    my ($name) = $src =~ m{/\* [0-9A-F]{2} (ObjId_\w+)\s*\*/ NULL, // \(moved to the status table: sst_object_codes\)(?!.*\n.*\Q$obj\E)}m;
    # (the name of the row just changed)
    my $row = sprintf('/* %02X ', $id);
    ($name) = $src =~ m{\Q$row\E(ObjId_\w+)};
    $src =~ s/(const SstCode sst_object_codes\[0x100\] = \{\n)/$1\t[$name] = Obj_$obj,\n/ or die "no sst table";
    for my $m (@maps) {
        my $asm = "asm/Mappings/$m.asm";
        if (-f $asm) {
            open my $a, '<', $asm; my $t = <$a>; close $a;
            if ($t =~ /_MapMacros\.asm/) { $t =~ s/_MapMacros\.asm/_MapMacrosS3.asm/; open $a, '>', $asm; print $a $t; close $a; next; }
            next if $t =~ /_MapMacrosS3/;
        }
        # a binary (or dc.w) mapping of Sonic 2's format: the build converts it
        $yml =~ s/(\nresources:\n(?:(?![a-z])[^\n]*\n)*?)  - "Mappings\/\Q$m\E"[^\n]*\n/$1/;
        $yml =~ s/(\nmappings_SonicTheHedgehog2:\n(?:  - "[^\n]*\n)*)/$1  - "Mappings\/$m"\n/ unless $yml =~ /mappings_SonicTheHedgehog2:\n(?:  - "[^\n]*\n)*?  - "Mappings\/\Q$m\E"/;
    }
    system('perl', "$tools/objlayout_clear_remember.pl", $id, map { "res/S2Objects/$_" } do { opendir my $d, 'res/S2Objects'; grep { !/^\./ && $_ ne 'Null' } readdir $d }) == 0 or warn "layout clear failed";
    print "$obj: id " . sprintf('%02X', $id) . " $name\n";
}
open my $out, '>', 'src/Sonic2Objects.c' or die; print $out $src; close $out;
open $out, '>', 'paradoxmakefile.yml' or die; print $out $yml; close $out;
