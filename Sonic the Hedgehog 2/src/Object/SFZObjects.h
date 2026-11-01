#ifndef _SFZOBJECTS_H
#define _SFZOBJECTS_H

#include "Sst.h"

// Sky Fortress's own objects of the pre-beta (Wing Fortress in the final game; its object ids are $B0 and up)

// $BA: the wheels of a conveyor belt (a picture of 32 by 32 pixels that stays: in the pre-beta nothing turns it, and the level's dynamic events are what is to move it when it is used)
void Obj_SFZConveyorWheels(Sst *self);

// $D9: something to hang from (an invisible place: a character that comes into its box in the air takes hold and stays, and a jump lets go; the hang bars of the pre-beta's Sky Fortress are this and a picture of
// the level's own tiles). Each of the two characters has his own state. The pre-beta puts no script in it: the level's dynamic events are what use it when they place it.
void Obj_SFZHang(Sst *self);

// $B2: the Tornado (Tails' biplane: the pre-beta calls it nothing, Sonic Adventure called it that). Its subtype says which: $50 Sonic steers it (Sky Chase: it follows him and goes up and down with Up and Down), $52 it flies at a
// steady speed with the characters on it, and puffs smoke ($C4) once it climbs, $54 it waits for Sonic to come near and then flies off. Tails sits at its front, drawn from his own art.
void Obj_SFZTornado(Sst *self);
// $C4: a puff of smoke that comes off the Tornado
void Obj_SFZTornadoSmoke(Sst *self);

// How fast Sky Chase's level scrolls by itself (the pre-beta's $FFFFF736 and $FFFFF738: its dynamic events set them; the Tornado keeps up with it). They are 0 in a zone that does not scroll
extern int16_t tornado_scroll_x, tornado_scroll_y;

// $8B: the switch of the palette cycle (invisible; the first of the zone's cycles is on, or the second, as a character crosses its line from one side and back from the other: the object's X flip says which). Its subtype's
// low two bits are how far it reaches up and down from it: $20, $40, $80 or $100. It is seen only in debug mode in the pre-beta (a picture of the path swapper's): not here
void Obj_SFZPaletteSwitch(Sst *self);

// $AD: a platform of the Clucker's own picture (its frame $C), solid, that stays
void Obj_SFZCluckerPlatform(Sst *self);

// $BD: a maker of elevators (invisible; subtype $76 sends them up, $78 down): one every $40 frames comes out of it, opens, carries what stands on it for 200 frames at a speed of one a frame, and closes and goes
void Obj_SFZElevator(Sst *self);

// $C0: the speed booster, a platform that throws what stands on it: it speeds up along its way (subtype times 16 pixels, to the left unless it is flipped), takes them along, and at the end throws them with
// its speed and up at $400
void Obj_SFZSpeedBooster(Sst *self);

// $B4: a vertical helix (a spiked screw that turns: it hurts), $B9: the laser that comes from the fortress at the start (it waits until it is on the screen, and then crosses it to the left at 16 a frame),
// $C2: the switch that opens the way into the boss's plane (a character who lands on it rolling destroys it and opens the way: some chunks of the level's layout are put over)
void Obj_SFZVerticalHelix(Sst *self);
void Obj_SFZLaser(Sst *self);
void Obj_SFZBossSwitch(Sst *self);

// $B5: the horizontal helix (a long spiral that turns up to speed and slows again, over and over: while it is at its full speed it takes a character who is over it and throws him spinning), $B6: the helix with a
// platform (four kinds by subtype: a platform that folds away and comes back, one that does when it is stood on, one that fires a laser, and a small one), $B7: the laser that it fires (it flickers for $20 frames)
void Obj_SFZHorizontalHelix(Sst *self);
void Obj_SFZPlatformHelix(Sst *self);
void Obj_SFZLargeLaser(Sst *self);

// (PaletteCycle.c: which of the zone's two sets of cycle colours is on)
extern uint8_t sfz_cycle_second;

#endif //_SFZOBJECTS_H
