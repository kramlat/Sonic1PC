#!/usr/bin/perl
# Rewrites an object source file from the old `Object` to the `Sst` of Sonic 3 & Knuckles (engine/Sst.h), the mechanical part: the names of the fields, the calls of the handler, the scratch overlays.
# What it cannot know is left alone and is found by the compiler: the render and status bits written as bit fields, other objects reached through pointers, the screen-position test, anything that is
# not this object. It never changes the meaning of a line it does not recognise.
#
# Usage: objtosst.pl FILE.c [FILE.h]      (in place; the header's Scratch_X structs become Own_X without their leading pad)
use strict;
use warnings;

sub slurp { open my $h, '<', $_[0] or die "$_[0]: $!"; local $/; my $d = <$h>; close $h; $d }
sub spit  { open my $h, '>', $_[0] or die "$_[0]: $!"; print $h $_[1]; close $h }

for my $path (@ARGV) {
    my $s = slurp($path);
    if ($path =~ /\.h$/) {
        $s =~ s/\bScratch_(\w+)\b/Own_$1/g;
        # only the pad at the top of a struct (down to 0x2F) and the subtype go: a pad in the middle keeps the fields after it where objects that read each other's memory (Boss Final) expect them
        $s =~ s/(typedef struct\s*\{\n)((?:[ \t]*(?:u?int\d+_t\s+pad\d*(?:\[\w+\])?|uint8_t\s+subtype);[^\n]*\n)+)/$1/g;
        $s =~ s/\(Object\s*\*\s*(\w+)/(Sst *self/g;
        $s =~ s/#include "Object.h"/#include "Sst.h"/;
        # the resource includes (they define the arrays) go to the .c: a header the object table includes must not
        my @res = $s =~ /^(#include "Resource\/[^"]+"\n)/mg;
        $s =~ s/^(?:\/\/[^\n]*assets[^\n]*\n)?#include "Resource\/[^"]+"\n//mg;
        (my $cpath = $path) =~ s/\.h$/.c/;
        if (@res && -f $cpath) {
            my $c = slurp($cpath);
            for my $r (@res) { $c =~ s/^(#include "[^"]+"\n)/$1$r/ unless index($c, $r) >= 0; }
            spit($cpath, $c);
        }
        spit($path, $s);
        next;
    }
    # the characters are still old objects: their fields stay as they are
    $s =~ s/\b(player|sidekick)->/\x01$1\x02/g;
    # scratch overlays
    $s =~ s/Scratch_((?!Sonic\b|Tails\b)\w+)\s*\*\s*(\w+)\s*=\s*\(Scratch_\w+\s*\*\)\s*&\s*(\w+)->scratch;/Own_$1 *$2 = (Own_$1 *)$3->own.u8;/g;
    $s =~ s/\bScratch_((?!Sonic\b|Tails\b)\w+)\b/Own_$1/g;
    my $own_var = ($s =~ /Own_\w+\s*\*\s*scratch\b/) ? 1 : 0;
    # signatures
    $s =~ s/\bObject\s*\*\s*obj\b/Sst *self/g;
    # positions
    $s =~ s/\b(\w+)->pos\.l\.x\.(f\.u|f\.l|v)\b/$1->x_pos.$2/g;
    $s =~ s/\b(\w+)->pos\.l\.y\.(f\.u|f\.l|v)\b/$1->y_pos.$2/g;
    $s =~ s/\b(\w+)->pos\.s\.x\b/$1->x_pos.f.u/g;
    $s =~ s/\b(\w+)->pos\.s\.y\b/$1->y_pos.f.u/g;
    # motion, radii, tile, frames
    $s =~ s/->xsp\b/->x_vel/g;
    $s =~ s/->ysp\b/->y_vel/g;
    $s =~ s/->inertia\b/->ground_vel/g;
    $s =~ s/->x_rad\b/->x_radius/g;
    $s =~ s/->y_rad\b/->y_radius/g;
    $s =~ s/->tile\b/->art_tile/g;
    $s =~ s/->frame_time\.b\b/->anim_frame_timer/g;
    $s =~ s/->frame\b/->mapping_frame/g;
    $s =~ s/->col_type\b/->collision_flags/g;
    $s =~ s/->col_property\b/->collision_property/g;
    $s =~ s/->routine_sec\b/->routine_secondary/g;
    # the subtype is the first scratch byte of the old object
    $s =~ s/\b(\w+)->scratch\.u8\[0\]/$1->subtype/g;
    $s =~ s/\bscratch->subtype\b/self->subtype/g;
    $s =~ s/\b(\w+)->scratch\.(u8|s8|u16|s16|u32|s32)\b/$1->own.$2/g;
    # render flags the way the objects write them
    $s =~ s/(\w+)->render\.b\s*\|=\s*SPRITE_CAM_FIELD;/$1->render_flags |= SST_RENDER_LEVEL;/g;
    $s =~ s/(\w+)->render\.b\s*=\s*SPRITE_CAM_FIELD;/$1->render_flags = SST_RENDER_LEVEL;/g;
    $s =~ s/(\w+)->render\.b\s*=\s*0;\s*\n\s*\1->render\.f\.level_fg\s*=\s*true;/$1->render_flags = SST_RENDER_LEVEL;/g;
    $s =~ s/(\w+)->render\.b\b/$1->render_flags/g;
    $s =~ s/(\w+)->render\.f\.on_screen\b/($1->render_flags & SST_RENDER_ON_SCREEN)/g;
    $s =~ s/([\w\[\]]+)->render\.f\.level_fg\s*=\s*true;/$1->render_flags |= SST_RENDER_LEVEL;/g;
    # render flips and the explicit height
    $s =~ s/(\w+)->render\.f\.([xy])_flip\s*=\s*([^;]+);/"SST_RENDER_SET($1, SST_RENDER_".uc($2)."_FLIP, $3);"/ge;
    $s =~ s/(\w+)->render\.f\.([xy])_flip\b/"(($1->render_flags & SST_RENDER_".uc($2)."_FLIP) != 0)"/ge;
    $s =~ s/(\w+)->render\.f\.explicit_height\s*=\s*true;/$1->height_pixels = $1->y_radius;/g;
    $s =~ s/(\w+)->status\.b\b/$1->status/g;
    $s =~ s/(\w+)->status\.o\.b\b/$1->status/g;
    $s =~ s/(\w+)->status\.o\.f\.player_push\s*=\s*true;/$1->status |= SST_STATUS_PLAYER_PUSH;/g;
    # status flips
    $s =~ s/(\w+)->status\.o\.f\.([xy])_flip\s*=\s*([^;]+);/"SST_STATUS_SET($1, SST_STATUS_".uc($2)."_FLIP, $3);"/ge;
    $s =~ s/(\w+)->status\.o\.f\.x_flip\s*\^=\s*1;/$1->status ^= SST_STATUS_X_FLIP;/g;
    $s =~ s/(\w+)->status\.o\.f\.x_flip\s*=\s*true;/$1->status |= SST_STATUS_X_FLIP;/g;
    $s =~ s/(\w+)->status\.o\.f\.y_flip\s*=\s*true;/$1->status |= SST_STATUS_Y_FLIP;/g;
    $s =~ s/(\w+)->status\.o\.f\.x_flip\b/(($1->status & SST_STATUS_X_FLIP) != 0)/g;
    $s =~ s/(\w+)->status\.o\.f\.y_flip\b/(($1->status & SST_STATUS_Y_FLIP) != 0)/g;
    $s =~ s/(\w+)->status\.o\.f\.player_stand\s*=\s*false;/$1->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;/g;
    $s =~ s/(\w+)->status\.o\.f\.player_stand\s*=\s*true;/$1->status |= SST_STATUS_PLAYER_STAND;/g;
    $s =~ s/(\w+)->status\.o\.f\.player_push\s*=\s*false;/$1->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;/g;
    $s =~ s/(\w+)->status\.o\.f\.player_stand\b/(($1->status & SST_STATUS_PLAYER_STAND) != 0)/g;
    $s =~ s/(\w+)->status\.o\.f\.player_push\b/(($1->status & SST_STATUS_PLAYER_PUSH) != 0)/g;
    $s =~ s/(\w+)->status\.o\.f\.flag7\s*=\s*true;/$1->status |= 0x80;/g;
    $s =~ s/(\w+)->status\.o\.f\.flag7\b/(($1->status & 0x80) != 0)/g;
    # priority: 0-7 in the old, units of $80 in the new
    $s =~ s/(\w+)->priority\s*=\s*(\d+);/"$1->priority = $2 * 0x80;"/ge;
    $s =~ s/\(Own_(\w+)\s*\*\)\s*&\s*(\w+)->scratch\b/(Own_$1 *)$2->own.u8/g;
    $s =~ s/\(void\)\s*scratch;/(void)own;/g;
    $s =~ s/KillSonic\(player,\s*self\)/KillSonicFrom(player, false)/g;
    # pointers to other objects of the table (the ones that start as this object or a parent, or in an array)
    $s =~ s/\bObject\s*\*\s*(\w+)\s*=\s*(self|parent|NULL)\b/Sst *$1 = $2/g;
    $s =~ s/\bObject\s*\*\s*(\w+)\[/Sst *$1\[/g;
    $s =~ s/\bFindNextFreeObj\(\s*(\w+)\s*\)/Sst_AllocateAfter($1)/g;
    $s =~ s/(\w+)->type\s*=\s*(ObjId_\w+);/$1->code = sst_object_codes[$2];/g;
    $s =~ s/\bobjects\[([^\]]+)\]\.(anim|routine|mapping_frame|subtype)\b/sst[$1].$2/g;
    $s =~ s/\bObject(\s*\*\s*\w+\s*=\s*Sst_Allocate)/Sst$1/g;
    # the persistent state byte
    $s =~ s/objstate\[(\w+)->respawn_index\]/(*SstLoad_State($1))/g;
    $s =~ s/(\w+)->respawn_index\b/$1->respawn/g;
    $s =~ s/\b[A-Za-z]+_OutOfRange\(/SstOutOfRange(/g;
    # other objects of the table, by index
    $s =~ s/\bObject\s*\*\s*(\w+)\s*=\s*&objects\[/Sst *$1 = &sst[/g;
    $s =~ s/\bObjectDelete\(\s*&objects\[([^\]]+)\]\s*\)/Sst_Delete(&sst[$1])/g;
    $s =~ s/\(uint8_t\)\(\s*(\w+)\s*-\s*objects\s*\)/(uint8_t)($1 - sst)/g;
    # leaving range: the object forgets its place in the respawn table (SstLoad.h) so that it comes back
    $s =~ s/if\s*\(IS_OFFSCREEN\(([^()]*(?:\([^()]*\))?[^()]*)\)\)\s*\{\s*ObjectDelete\(\s*(\w+)\s*\);\s*return;\s*\}\s*DisplaySprite\(\s*\2\s*\);/SstOnScreenTest2($2, $1);/g;
    $s =~ s/if\s*\(IS_OFFSCREEN\(([^()]*(?:\([^()]*\))?[^()]*)\)\)\s*\{?\s*ObjectDelete\(\s*(\w+)\s*\);\s*\}?\s*else\s*\{?\s*DisplaySprite\(\s*\2\s*\);\s*\}?/SstOnScreenTest2($2, $1);/g;
    $s =~ s/if\s*\(IS_OFFSCREEN\(([^()]*(?:\([^()]*\))?[^()]*)\)\)\s*\n?\s*ObjectDelete\(\s*(\w+)\s*\);/SstDeleteIfNotInRange2($2, $1);/g;
    $s =~ s/\bIS_OFFSCREEN\(/SstOutOfRange(/g;
    $s =~ s/static bool \w+_OutOfRange\(int16_t x\) \{.*?\n\}\n\n?//gs;
    $s =~ s/if\s*\(\w+_OutOfRange\(([^()]*(?:\([^()]*\))?[^()]*)\)\)\s*Sst_Delete\(\s*(\w+)\s*\);\s*else\s*Sst_Draw\(\s*\2\s*\);/SstOnScreenTest2($2, $1);/g;
    $s =~ s/if\s*\(\w+_OutOfRange\(([^()]*(?:\([^()]*\))?[^()]*)\)\)\s*Sst_Delete\(\s*(\w+)\s*\);/SstDeleteIfNotInRange2($2, $1);/g;
    # the handler's calls
    $s =~ s/\bAnimateSprite\(\s*(\w+)\s*,/Sst_Animate($1,/g;
    $s =~ s/\bDisplaySprite\(\s*(\w+)\s*\)/Sst_Draw($1)/g;
    $s =~ s/\bObjectDelete\(\s*(\w+)\s*\)/Sst_Delete($1)/g;
    $s =~ s/\bRememberState\(\s*(\w+)\s*\)/SstOnScreenTest($1)/g;
    $s =~ s/\bSpeedToPos\(\s*(\w+)\s*\)/Sst_MoveSprite2($1)/g;
    $s =~ s/\bObjectFall\(\s*(\w+)\s*\)/Sst_MoveSprite($1)/g;
    $s =~ s/\bObjFloorDist\(/SstFloorDist(/g;
    $s =~ s/\bObjHitWallRight\(/SstHitWallRight(/g;
    $s =~ s/\bObjHitWallLeft\(/SstHitWallLeft(/g;
    $s =~ s/\bObjHitCeiling\(/SstHitCeiling(/g;
    $s =~ s/\bChkObjectVisible\(/SstChkObjectVisible(/g;
    $s =~ s/\bChkPartiallyVisible\(/SstChkPartiallyVisible(/g;
    $s =~ s/\bSmashObject\(/SstSmashObject(/g;
    $s =~ s/\bFragmentatePlatform\(/SstFragmentatePlatform(/g;
    $s =~ s/\bFindNearestTile\(\s*\w+\s*,/FindNearestTile(NULL,/g;
    $s =~ s/\bSolidObject_Heightmap\(/SstSolidObject_Heightmap(/g;
    $s =~ s/\bSolidObject\(/SstSolidObject(/g;
    $s =~ s/\bSolid_ChkEnter\(/SstSolid_ChkEnter(/g;
    $s =~ s/\bPlatformObject_CustomHeight\(/SstPlatformObject_CustomHeight(/g;
    $s =~ s/\bPlatformObject\(/SstPlatformObject(/g;
    $s =~ s/\bPlatform3\(/SstPlatform3(/g;
    $s =~ s/\bSlopeObject_AssumeStoodOn\(/SstSlopeObject_AssumeStoodOn(/g;
    $s =~ s/\bSlopeObject\(/SstSlopeObject(/g;
    $s =~ s/\bExitPlatform\(/SstExitPlatform(/g;
    $s =~ s/\bMvSonicOnPtfm\(/SstMvSonicOnPtfm(/g;
    if ($own_var && $s !~ /Scratch_(Sonic|Tails)\s*\*\s*scratch\b/) {
        $s =~ s/\b(Own_\w+)\s*\*\s*scratch\b/$1 *own/g;
        $s =~ s/\bscratch->/own->/g;
        $s =~ s/([(,]\s*)scratch(\s*[,)])/$1own$2/g;
    }
    $s =~ s/\bObject\s*\*\s*(\w+)\s*=\s*FindNextFreeObj\(\s*(\w+)\s*\);/Sst *$1 = Sst_AllocateAfter($2);/g;
    $s =~ s/\bObject\s*\*\s*(\w+)\s*=\s*FindFreeObj\(\s*\);/Sst *$1 = Sst_Allocate();/g;
    # the object's own name for itself in the bodies
    $s =~ s/\bobj\b/self/g;
    # includes
    $s =~ s/#include "Object.h"\n//;
    $s =~ s/(#include "[^"]+\.h"\n)/$1/;
    for my $inc ('Object.h', 'ObjectSst.h', 'SstLoad.h') {
        $s =~ s/(#include "[^"]+"\n)/$1#include "$inc"\n/ unless $s =~ /#include "\Q$inc\E"/;
    }
    # leaving range: forget the place in the respawn table before deleting
    { my @l = split /\n/, $s, -1;
      for my $i (0 .. $#l) {
          next unless $l[$i] =~ /^\s*(?:\}\s*else\s*)?if\s*\(!?SstOutOfRange\(/ && $l[$i] !~ /^\s*if\s*\(!/;
          next if $l[$i] !~ /\{\s*$/;   # (an unbraced `if` is a one-line delete: SstDeleteIfNotInRange2 does it)
          for my $j ($i + 1 .. ($i + 8 > $#l ? $#l : $i + 8)) {
              if ($l[$j] =~ /^(\s*)Sst_Delete\(self\);/ && $l[$j - 1] !~ /Forget/) { $l[$j] = "$1SstLoad_Forget(self);\n$l[$j]"; last }
          }
      }
      $s = join "\n", @l; }
    $s =~ s/\x01(\w+)\x02/$1->/g;
    spit($path, $s);
}
