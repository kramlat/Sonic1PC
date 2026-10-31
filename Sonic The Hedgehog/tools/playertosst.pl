#!/usr/bin/perl
# Rewrites the uses of the players (`player->` and `sidekick->`, the characters' own fields) from the old `Object` to the `Sst` of Sonic 3 & Knuckles (engine/Sst.h), in any source file. The mechanical part only,
# like objtosst.pl: the field names, the character status bits (PlayerStatus's bit fields are the bits of S3&K's character status: SST_PSTATUS_*), and the overlay of Scratch_Sonic on the character's own words.
#
# Usage: playertosst.pl FILE...   (in place)
use strict;
use warnings;
sub slurp { open my $h, '<', $_[0] or die "$_[0]: $!"; local $/; my $d = <$h>; close $h; $d }
sub spit  { open my $h, '>', $_[0] or die "$_[0]: $!"; print $h $_[1]; close $h }

my %bit = (x_flip => 'X_FLIP', in_air => 'IN_AIR', in_ball => 'IN_BALL', object_stand => 'OBJECT_STAND', roll_jump => 'ROLL_JUMP', pushing => 'PUSHING', underwater => 'UNDERWATER', must_roll => 'MUST_ROLL');
my $R = qr/(?:player|sidekick|chr|pla|sonic|tails|self|TAILS_OBJ)/;   # the names the characters go by

for my $path (@ARGV) {
    my $s = slurp($path);
    my $o = $s;
    # the old object status as a character's (the same bits: x_flip 1, y_flip = in the air 2, player_stand = on an object 8, player_push 0x20)
    my %obit = (x_flip => 1, y_flip => 2, flag2 => 4, player_stand => 8, flag4 => 0x10, player_push => 0x20, flag6 => 0x40, flag7 => 0x80);
    $s =~ s/\b(player|sidekick)->status\.o\.f\.(\w+)\s*=\s*(true|false)\s*;/$3 eq 'true' ? "$1->status |= $obit{$2};" : "$1->status &= (uint8_t)~$obit{$2};"/ge;
    $s =~ s/\b(player|sidekick)->status\.o\.f\.(\w+)\b/"(($1->status & $obit{$2}) != 0)"/ge;
    # status bits first
    $s =~ s/\b($R)->status\.p\.f\.(\w+)\s*\^=\s*1\s*;/exists $bit{$2} ? "$1->status ^= SST_PSTATUS_$bit{$2};" : "$1->status.p.f.$2 ^= 1;"/ge;
    $s =~ s/\b($R)->status\.p\.f\.(\w+)\s*=\s*(true|false)\s*;/exists $bit{$2} ? ($3 eq 'true' ? "$1->status |= SST_PSTATUS_$bit{$2};" : "$1->status &= (uint8_t)~SST_PSTATUS_$bit{$2};") : "$1->status.p.f.$2 = $3;"/ge;
    $s =~ s/\b($R)->status\.p\.f\.(\w+)\s*=\s*([^;=][^;]*);/exists $bit{$2} ? "SST_STATUS_SET($1, SST_PSTATUS_$bit{$2}, $3);" : "$1->status.p.f.$2 = $3;"/ge;
    $s =~ s/\b($R)->status\.p\.f\.(\w+)\b/exists $bit{$2} ? "(($1->status & SST_PSTATUS_$bit{$2}) != 0)" : "$1->status.p.f.$2"/ge;
    $s =~ s/\b($R)->status\.b\b/$1->status/g;
    $s =~ s/\bKillSonic\(\s*(\w+)\s*,\s*\1\s*\)/KillSonicFrom($1, false)/g;
    $s =~ s/\b($R)->type\s*(==|!=)\s*(?:0|ObjId_Null)\b/$1->code $2 NULL/g;
    $s =~ s/\b($R)->type\s*=\s*(?:0|ObjId_Null)\s*;/$1->code = NULL;/g;
    # which routine a character runs
    $s =~ s/\b($R)->type\s*=\s*(ObjId_\w+|\d+)\s*;/$1->code = sst_object_codes[$2];/g;
    $s =~ s/\b($R)->type\s*(==|!=)\s*(ObjId_\w+)/$1->code $2 sst_object_codes[$3]/g;
    # fields
    $s =~ s/\b($R)->pos\.l\.x\.(f\.u|f\.l|v)\b/$1->x_pos.$2/g;
    $s =~ s/\b($R)->pos\.l\.y\.(f\.u|f\.l|v)\b/$1->y_pos.$2/g;
    $s =~ s/\b($R)->pos\.l\.([xy])\b/$1->$2_pos/g;
    $s =~ s/\b($R)->xsp\b/$1->x_vel/g;
    $s =~ s/\b($R)->ysp\b/$1->y_vel/g;
    $s =~ s/\b($R)->inertia\b/$1->ground_vel/g;
    $s =~ s/\b($R)->x_rad\b/$1->x_radius/g;
    $s =~ s/\b($R)->y_rad\b/$1->y_radius/g;
    $s =~ s/\b($R)->tile\b/$1->art_tile/g;
    $s =~ s/\b($R)->frame_time\.b\b/$1->anim_frame_timer/g;
    $s =~ s/\b($R)->frame\b/$1->mapping_frame/g;
    $s =~ s/\b($R)->routine_sec\b/$1->routine_secondary/g;
    $s =~ s/\b($R)->col_type\b/$1->collision_flags/g;
    $s =~ s/\b($R)->col_property\b/$1->collision_property/g;
    $s =~ s/\b($R)->render\.b\b/$1->render_flags/g;
    $s =~ s/\b($R)->render\.f\.on_screen\b/(($1->render_flags & SST_RENDER_ON_SCREEN) != 0)/g;
    $s =~ s/\b($R)->render\.f\.level_fg\s*=\s*true;/$1->render_flags |= SST_RENDER_LEVEL;/g;
    $s =~ s/\b($R)->render\.f\.([xy])_flip\s*=\s*([^;]+);/"SST_RENDER_SET($1, SST_RENDER_".uc($2)."_FLIP, $3);"/ge;
    $s =~ s/\b($R)->render\.f\.([xy])_flip\b/"(($1->render_flags & SST_RENDER_".uc($2)."_FLIP) != 0)"/ge;
    $s =~ s/\b($R)->priority\s*=\s*(\d+)\s*;/"$1->priority = $2 * 0x80;"/ge;
    $s =~ s/\b($R)->scratch\.(u8|s8|u16|s16|u32|s32)\b/$1->own.$2/g;
    # the overlay of the character's own words
    $s =~ s/\(Scratch_(Sonic|Tails)\s*\*\)\s*&\s*(\w+)->scratch\b/(Scratch_$1 *)$2->own.u8/g;
    spit($path, $s) if $s ne $o;
}
