#!/usr/bin/perl
# Clears bit 15 of the Y word (the old "remember state") in the entries of the given object ids, in place.
#
# An object that has moved to the status table of Sonic 3 & Knuckles is loaded by the loader of that game (engine/SstLoad.c), where bit 15 of an entry means "load whatever the camera's Y is":
# the entries of a moved id must not carry the old meaning. Run it once for each id as the object moves over.
#
# Usage: objlayout_clear_remember.pl ID[,ID...] FILE...      (ids in hex or decimal)
use strict;
use warnings;

my $ids = shift or die "usage: $0 ID[,ID...] FILE...\n";
my %ids = map { (/^0x/i ? hex($_) : /^\$/ ? hex(substr($_, 1)) : $_ + 0) => 1 } split /,/, $ids;

for my $path (@ARGV) {
    open my $in, '<:raw', $path or die "$path: $!";
    local $/;
    my $data = <$in>;
    close $in;
    if (length($data) % 6) { warn "$path: size is not a multiple of 6, skipped\n"; next; }
    my $changed = 0;
    for (my $i = 0; $i < length($data); $i += 6) {
        my ($x, $yw, $id, $sub) = unpack('nnCC', substr($data, $i, 6));
        next if $x == 0xFFFF || !$ids{$id} || !($yw & 0x8000);
        substr($data, $i, 6) = pack('nnCC', $x, $yw & 0x7FFF, $id, $sub);
        $changed++;
    }
    next unless $changed;
    open my $o, '>:raw', $path or die "$path: $!";
    print $o $data;
    close $o;
    print "$path: $changed entr", ($changed == 1 ? "y" : "ies"), "\n";
}
