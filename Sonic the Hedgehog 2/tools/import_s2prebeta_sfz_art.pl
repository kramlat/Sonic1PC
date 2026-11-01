#!/usr/bin/perl
# The object art of Sky Fortress, Sky Chase and Death Egg in the Sonic 2 pre-beta (Nemesis files of its Data/SFz, SCz, DEz and CPz: S2PREBETA gives the disassembly's folder, default ~/Projects/s2prebeta-disasm) into
# res/Art, byte for byte (SFZ<name> for Sky Fortress's). The zones' own tiles are tools/import_s2prebeta_zone.pl's. Run it in the Sonic 2 folder.
use strict;
use warnings;
my $base = ($ENV{S2PREBETA} // "$ENV{HOME}/Projects/s2prebeta-disasm") . "/Data";
my $src = "$base/SFz";
my %files = (
    'RbtnkShp.nem' => 'RobotnikShip', 'Tornado.nem' => 'Tornado', 'Clouds.nem' => 'Clouds', 'VerHelix.nem' => 'VerticalHelix', 'HorHelix.nem' => 'HorizontalHelix', 'Balkiry.nem' => 'Balkiry',
    'Brk_Wall.nem' => 'BreakableWall', 'Clucker.nem' => 'Clucker', 'SmallHlx.nem' => 'SmallHelix', 'VerLaser.nem' => 'VerticalLaser', 'Cannon.nem' => 'Cannon', 'HorLaser.nem' => 'HorizontalLaser',
    'CB_Wheels.nem' => 'ConveyorWheels', 'ChainHook.nem' => 'ChainHook', 'RktFlame.nem' => 'RocketFlame', 'Elevator.nem' => 'Elevators', 'Hatchways.nem' => 'Hatchways', 'EngnPart.nem' => 'EnginePart',
    'SpdBoost.nem' => 'SpeedBoost', 'B_Switch.nem' => 'BossSwitch', 'Platform.nem' => 'Platform',
);
# the other zones' (file under Data => resource)
my %others = (
    'SCz/Turtloid.nem' => 'Turtloid', 'SCz/Nebula.nem' => 'Nebula', 'DEz/MetalSnc.Nem' => 'DEZMetalSonic', 'DEz/RbtkBody.nem' => 'DEZRobotnikBody', 'DEz/RbtkFace.nem' => 'DEZRobotnikFaces',
    'DEz/RobotnkW.nem' => 'DEZRobotnikWindow', 'DEz/Robotnik.nem' => 'DEZRobotnik', 'DEz/RbtnkFnl.nem' => 'DEZFinalBoss', 'CPz/AutoDoor.nem' => 'CPZAutomaticDoor',
);
for my $file (sort keys %others) {
    open my $in, '<:raw', "$base/$file" or die "$base/$file: $!\n";
    local $/;
    my $data = <$in>;
    close $in;
    open my $out, '>:raw', "res/Art/$others{$file}" or die "res/Art/$others{$file}: $!\n";
    print $out $data;
    close $out;
}
for my $file (sort keys %files) {
    open my $in, '<:raw', "$src/$file" or die "$src/$file: $!\n";
    local $/;
    my $data = <$in>;
    close $in;
    open my $out, '>:raw', "res/Art/SFZ$files{$file}" or die "res/Art/SFZ$files{$file}: $!\n";
    print $out $data;
    close $out;
}
print scalar(keys %files) + scalar(keys %others), " art files written\n";
