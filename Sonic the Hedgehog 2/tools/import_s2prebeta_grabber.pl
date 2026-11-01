#!/usr/bin/perl
# The Grabber of the Sonic 2 pre-beta (Chemical Plant's spider, objects $A7 to $AB: S2PREBETA gives the disassembly's folder, default ~/Projects/s2prebeta-disasm) into res/, from its Data/Objects/Obj_0xAB.asm: the two
# mapping tables in Sonic 2's format (the build converts them) and the animation script. The art (Data/CPz/Grabber.nem) is the alpha's, byte for byte.
#   Mappings/Grabber        the body, the claw and the head: seven frames
#   Mappings/GrabberStrand  the thread the body hangs from: frames 0-7, one more link of it for each (the ninth is the eighth again)
#   Animation/Grabber       its legs: frames 0 and 1 every 8 frames
# Run it in the Sonic 2 folder: import_s2prebeta_grabber.pl
use strict;
use warnings;

my $src = ($ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm") . "/Data/Objects/Obj_0xAB.asm";
open my $f, '<', $src or die "$src: $!\n";
my @lines = <$f>;
close $f;
s/;.*// for @lines;

my (%frames, %tables, $table, $frame, @order);
for (@lines) {
    if (/^(Grabber_Mappings\w*):/) { $table = $1; $tables{$table} = []; undef $frame; next }
    if (/^\s*dc\.w\s+(\w+)-(\w+)\s*$/ && defined $table && $2 eq $table) { push @{ $tables{$table} }, $1; next }
    if (/^(Offset_0x\w+):/) { $frame = $1; $frames{$frame} = { count => undef, pieces => [] }; next }
    next unless defined $frame;
    if (/^\s*dc\.w\s+\$([0-9A-Fa-f]+)\s*$/) {  next if defined $frames{$frame}{count}; $frames{$frame}{count} = hex $1; next }
    if (/^\s*dc\.l\s+\$([0-9A-Fa-f]{8})\s*,\s*\$([0-9A-Fa-f]{8})/) { push @{ $frames{$frame}{pieces} }, pack('NN', hex $1, hex $2) if @{ $frames{$frame}{pieces} } < $frames{$frame}{count}; next }
}

sub table_bytes {
    my ($name) = @_;
    my @names = @{ $tables{$name} or die "no table $name\n" };
    my (@bodies, %seen);
    for my $n (@names) {
        my $fr = $frames{$n} or die "no frame $n\n";
        die "$n: $fr->{count} pieces, found " . scalar @{ $fr->{pieces} } . "\n" unless $fr->{count} == @{ $fr->{pieces} };
        push @bodies, pack('n', $fr->{count}) . join('', @{ $fr->{pieces} });
    }
    # the frames' offsets are from the table's start; the two frames that share a body keep one copy
    my (@offsets, $body, %at);
    my $pos = 2 * @names;
    $body = '';
    for my $i (0 .. $#names) {
        if (exists $at{ $names[$i] }) { push @offsets, $at{ $names[$i] }; next }
        $at{ $names[$i] } = $pos;
        push @offsets, $pos;
        $body .= $bodies[$i];
        $pos += length $bodies[$i];
    }
    return pack('n*', @offsets) . $body;
}

sub spit { my ($p, $d) = @_; open my $o, '>:raw', $p or die "$p: $!\n"; print $o $d; close $o }
spit('res/Mappings/Grabber', table_bytes('Grabber_Mappings'));
spit('res/Mappings/GrabberStrand', table_bytes('Grabber_Mappings_01'));
spit('res/Animation/Grabber', pack('n', 2) . pack('C*', 0x07, 0x00, 0x01, 0xFF));
print "Grabber: mappings and animation written\n";
