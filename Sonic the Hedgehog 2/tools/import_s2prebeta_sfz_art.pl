#!/usr/bin/perl
# The object art of Sky Fortress in the Sonic 2 pre-beta (Nemesis files of its Data/SFz: S2PREBETA gives the disassembly's folder, default ~/Projects/s2prebeta-disasm) into res/Art/SFZ<name>, byte for byte.
# The zone's own tiles are tools/import_s2prebeta_zone.pl SFZ's. Run it in the Sonic 2 folder.
use strict;
use warnings;
my $src = ($ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm") . "/Data/SFz";
my %files = (
    'RbtnkShp.nem' => 'RobotnikShip', 'Tornado.nem' => 'Tornado', 'Clouds.nem' => 'Clouds', 'VerHelix.nem' => 'VerticalHelix', 'HorHelix.nem' => 'HorizontalHelix', 'Balkiry.nem' => 'Balkiry',
    'Brk_Wall.nem' => 'BreakableWall', 'Clucker.nem' => 'Clucker', 'SmallHlx.nem' => 'SmallHelix', 'VerLaser.nem' => 'VerticalLaser', 'Cannon.nem' => 'Cannon', 'HorLaser.nem' => 'HorizontalLaser',
    'CB_Wheels.nem' => 'ConveyorWheels', 'ChainHook.nem' => 'ChainHook', 'RktFlame.nem' => 'RocketFlame', 'Elevator.nem' => 'Elevators', 'Hatchways.nem' => 'Hatchways', 'EngnPart.nem' => 'EnginePart',
    'SpdBoost.nem' => 'SpeedBoost', 'B_Switch.nem' => 'BossSwitch', 'Platform.nem' => 'Platform',
);
for my $file (sort keys %files) {
    open my $in, '<:raw', "$src/$file" or die "$src/$file: $!\n";
    local $/;
    my $data = <$in>;
    close $in;
    open my $out, '>:raw', "res/Art/SFZ$files{$file}" or die "res/Art/SFZ$files{$file}: $!\n";
    print $out $data;
    close $out;
}
print scalar(keys %files), " art files written\n";
