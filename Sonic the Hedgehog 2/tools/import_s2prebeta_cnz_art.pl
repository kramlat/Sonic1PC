#!/usr/bin/perl
# Casino Night's object art of the Sonic 2 pre-beta (S2PREBETA gives the disassembly's folder, default ~/Projects/s2prebeta-disasm; its Data/CNz/*.nem, all Nemesis) into res/Art, under the names the port has:
#   Box CNZBox, GreenPtf CNZGreenPlatform, Spikball CNZSpikeball, Elevator CNZElevator, SlotMach CNZCards (the slot machine's starter), BBumpers CNZBlueBumper, Bumpers CNZBumpers,
#   D_Launch CNZDiagLauncher, V_Launch CNZVertLauncher, GBumpers CNZGreenBumpers, Flippers CNZFlippers, Boss CNZBoss, and Crawl CNZCrawl (new: the Crawl badnik)
# Run it in the Sonic 2 folder: import_s2prebeta_cnz_art.pl
use strict;
use warnings;

my $src = ($ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm") . '/Data/CNz';
my %art = (
    'Box.nem' => 'CNZBox', 'GreenPtf.nem' => 'CNZGreenPlatform', 'Spikball.nem' => 'CNZSpikeball', 'Elevator.nem' => 'CNZElevator', 'SlotMach.nem' => 'CNZCards',
    'BBumpers.nem' => 'CNZBlueBumper', 'Bumpers.nem' => 'CNZBumpers', 'D_Launch.nem' => 'CNZDiagLauncher', 'V_Launch.nem' => 'CNZVertLauncher', 'GBumpers.nem' => 'CNZGreenBumpers',
    'Flippers.nem' => 'CNZFlippers', 'Boss.nem' => 'CNZBoss', 'Crawl.nem' => 'CNZCrawl',
);
for my $f (sort keys %art) {
    system('cp', "$src/$f", "res/Art/$art{$f}") == 0 or die "$f: $!\n";
}
# the green bumper's (D8) and the flippers' (86) mappings and animations (Sonic 2's format: the build converts the mappings)
my $objects = ($ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm") . '/Data/Objects';
for my $job (
    [ 'asm_dc_mappings.pl', 'Obj_0xD8.asm', 'Green_Bumper_Mappings', 'res/Mappings/CNZGreenBumper' ],
    [ 'asm_dc_animation.pl', 'Obj_0xD8.asm', 'Green_Bumper_Animate_Data', 'res/Animation/CNZGreenBumper' ],
    [ 'asm_dc_mappings.pl', 'Obj_0x85.asm', 'Vertical_Pinball_Spring_Mappings', 'res/Mappings/CNZPlungerDiagonal' ], # (the pre-beta's names are the other way round: this one is used with a subtype)
    [ 'asm_dc_mappings.pl', 'Obj_0x85.asm', 'Diagonal_Pinball_Spring_Mappings', 'res/Mappings/CNZPlunger' ],
    [ 'asm_dc_mappings.pl', 'Obj_0xD5.asm', 'CNz_Elevator_Mappings', 'res/Mappings/CNZElevator' ],
    [ 'asm_dc_mappings.pl', 'Obj_0xD7.asm', 'Blue_Bumper_Mappings', 'res/Mappings/CNZBlueBumper' ],
    [ 'asm_dc_animation.pl', 'Obj_0xD7.asm', 'Blue_Bumper_Animate_Data', 'res/Animation/CNZBlueBumper' ],
    [ 'asm_dc_mappings.pl', 'Obj_0xD6.asm', 'Slot_Machine_Starter_Mappings', 'res/Mappings/CNZSlotStarter' ],
    [ 'asm_dc_animation.pl', 'Obj_0xD6.asm', 'Slot_Machine_Starter_Animate_Data', 'res/Animation/CNZSlotStarter' ],
    [ 'asm_dc_mappings.pl', 'Obj_0x86.asm', 'Flippers_Mappings', 'res/Mappings/CNZFlippers' ],
    [ 'asm_dc_animation.pl', 'Obj_0x86.asm', 'Flippers_Animate_Data', 'res/Animation/CNZFlippers' ],
) {
    system('perl', "tools/$job->[0]", "$objects/$job->[1]", $job->[2], $job->[3]) == 0 or die "$job->[3]\n";
}
print "Casino Night's art imported\n";
system('cp', "$src/Symbols.dat", 'res/Art/CNZSymbols') == 0 or die; # (the symbols the zone's animated art draws; raw)
