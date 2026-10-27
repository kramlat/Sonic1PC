#include <stdlib.h>
#include <stdio.h>
#include "test.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "Object.h"
#include "Object/DebugList.h"
#include "Object/DebugMarkers.h"
#include "Object/Tails.h"
#include "Object/Sonic.h"
#include "Object/SuperSonic.h"
#include "Object/DustSplash.h"
#include "Object/Countdown.h"
#include "Object/Animals.h"
#include "Object/Coconuts.h"
#include "Object/EHZBoss.h"
#include "LevelScroll.h"
#include "Object/Spiral.h"
#include "Object/MTZBadniks.h"
#include "Object/HTZBadniks.h"
#include "Object/MTZBoss.h"
#include "Object/HTZBoss.h"
#include "Object/HPZBadniks.h"
#include "SoundIds.h"
#include "Backend/YM2612.h"
#include "Sound.h"
#include "Object/BossCommon.h"
#include "PLC.h"
#include "EngineSound.h"
#include "Backend/VDP.h"

void Obj_TailsTails(Object *obj);
void Obj_Signpost(Object *obj);
void ScrollVertical(void);

// The prototype's levels place objects by id; every id a built zone places has to run something (Obj_Null just deletes itself). The zones that are built are checked completely: the ids that are still not
// ported are listed here, so a zone that gains an object (or one that loses it) makes the test say so (remove the id from the list when it is ported).

typedef struct {
    int zone;
    int count;
    uint8_t ids[16];
} Pending;

// The object ids that the layouts of these zones place and that are still the null object (empty list: nothing missing)
static const Pending pending[] = {
    { ZoneId_EHZ, 0, { 0 } },
    { ZoneId_HTZ, 0, { 0 } },
    { ZoneId_HPZ, 0, { 0 } },
    { ZoneId_CPZ, 0, { 0 } },
    { ZoneId_ARZ, 0, { 0 } },
    { ZoneId_MCZ, 0, { 0 } }, // (Dust Hill)
    { ZoneId_OOZ, 0, { 0 } }, // (Oil Ocean)
    { ZoneId_MTZ, 0, { 0 } }, // (Metropolis)
};

static bool IsPending(const Pending *p, uint8_t id) {
    for (int i = 0; i < p->count; i++)
        if (p->ids[i] == id)
            return true;
    return false;
}

static void ObjectCoverage_BuiltZonesPortEveryObjectTheyPlace(void) {
    for (size_t z = 0; z < sizeof(pending) / sizeof(pending[0]); z++) {
        const Pending *p = &pending[z];
        for (int act = 0; act < 3; act++) {
            for (const uint8_t *e = level_obj[p->zone][act]; e != NULL && !(e[0] == 0xFF && e[1] == 0xFF); e += 6) {
                const uint8_t id = e[4];
                const bool ported = id < game_object_count && game_objects[id] != NULL && game_objects[id] != Obj_Null;
                if (!ported && !IsPending(p, id)) {
                    printf("\n    zone %d act %d places object %02X, which is not ported", p->zone, act, id);
                    CHECK(ported);
                    break; // (one report for each zone and act is enough)
                }
            }
        }
    }
}

// The acts that a zone does not have are the null level (no objects, no rings), so that it is clear they are empty; Metropolis' third act is a level of its own
static void Levels_ActsAZoneDoesNotHaveAreEmpty(void) {
    for (int zone = 0; zone < ZoneId_Num; zone++) {
        if (level_obj[zone][0] == NULL)
            continue; // (a slot that is not a zone)
        for (int act = 2; act < 4; act++) {
            const bool own = zone == ZoneId_MTZ && act == 2;
            const uint8_t *objects = level_obj[zone][act], *rings = level_ring[zone][act];
            CHECK(own != (objects[0] == 0xFF && objects[1] == 0xFF));
            CHECK(own != (rings[0] == 0xFF && rings[1] == 0xFF));
        }
    }
}

static void ObjectCoverage_NoPlacedObjectIsFalselyPending(void) {
    for (size_t z = 0; z < sizeof(pending) / sizeof(pending[0]); z++) {
        const Pending *p = &pending[z];
        for (int i = 0; i < p->count; i++) {
            const uint8_t id = p->ids[i];
            CHECK(!(id < game_object_count && game_objects[id] != NULL && game_objects[id] != Obj_Null)); // (it was ported: take it off the list)
        }
    }
}

// The debug lists (Debug_* in the disassemblies: the prototype's, and the alpha's for the zones taken from it): how many entries each has, and that what each one places is an object this port runs (the ones that are not built are placeholders of type null)
static void DebugLists_HaveThePrototypesLengthsAndPlaceRealObjects(void) {
    static const struct { int zone, count; } expect[] = {
        { ZoneId_EHZ, 18 }, { ZoneId_MTZ, 31 }, { ZoneId_HTZ, 28 }, { ZoneId_HPZ, 8 }, { ZoneId_OOZ, 16 },
        { ZoneId_MCZ, 16 }, { ZoneId_CNZ, 2 }, { ZoneId_CPZ, 19 }, { ZoneId_ARZ, 17 }, { ZoneId_WZ, 2 },
    };
    for (size_t i = 0; i < sizeof(expect) / sizeof(expect[0]); i++) {
        level_id = LEVEL_ID(expect[i].zone, 0);
        int count = 0;
        const DebugListEntry *list = DebugList_Get(&count);
        CHECK_EQ(count, expect[i].count);
        for (int e = 0; e < count; e++) {
            if (list[e].type == ObjId_Null)
                continue;
            const bool ported = (int)list[e].type < game_object_count && game_objects[list[e].type] != NULL && game_objects[list[e].type] != Obj_Null;
            if (!ported) {
                printf("\n    zone %d entry %d places object %02X, which is not ported", expect[i].zone, e, (int)list[e].type);
                CHECK(ported);
            }
            CHECK(list[e].mappings != NULL);
        }
    }
}

// The "?" corners of an invisible box (Sonic 1's lava tag has the same with the debug cheat on)
static void DebugMarkers_TheCornersOfABoxAreMarkedOnlyWithTheCheat(void) {
    Object box;
    memset(&box, 0, sizeof(box));
    box.pos.l.x.f.u = 0x1000;
    box.pos.l.y.f.u = 0x400;
    debug_cheat = false;
    DebugMarkers_Show(&box, 0x40, 0x20);
    CHECK_EQ(box.child_count, 0);
    debug_cheat = true;
    DebugMarkers_Show(&box, 0x40, 0x20);
    CHECK_EQ(box.child_count, 4);
    CHECK(box.render.f.multi_sprite);
    CHECK_EQ(box.children[0].x, 0x1000 - 0x40 + 8);
    CHECK_EQ(box.children[0].y, 0x400 - 0x20 + 8);
    CHECK_EQ(box.children[3].x, 0x1000 + 0x40 - 8);
    CHECK_EQ(box.children[3].y, 0x400 + 0x20 - 8);
    int16_t w, h;
    CHECK(DebugMarkers_TouchBox(0x95, &w, &h));
    CHECK_EQ(w, 0x80);
    CHECK_EQ(h, 0x20);
    CHECK(!DebugMarkers_TouchBox(0x00, &w, &h));
    debug_cheat = false;
}


// The tails (object 05) of a Tails who rolls or spin dashes: the prototype draws them (frames $49 to $58 for a roll, $81 to $84 for the dash) from their art window
static bool TailsTailsShows(uint8_t tails_anim, int16_t xsp) {
    memset(&objects[TAILS_SLOT], 0, sizeof(Object));
    memset(&objects[0x1D], 0, sizeof(Object));
    Object *tails = TAILS_OBJ;
    tails->anim = tails_anim;
    tails->xsp = xsp;
    tails->inertia = xsp;
    Object *tail = &objects[0x1D];
    bool shown = false;
    for (int i = 0; i < 12; i++) {
        Obj_TailsTails(tail);
        const uint8_t *tiles = VDP_TileSpace() + 0x7B0 * 0x20;
        bool art = false;
        for (int b = 0; b < 0x40; b++)
            art |= tiles[b] != 0;
        shown |= art && tail->frame >= 0x49;
    }
    return shown;
}

// The alpha's tails of a Tails who pushes: animation 4's (the wag of frames $87-$8A), whatever his own animation says
static void TailsTails_WagWhileHePushes(void) {
    memset(&objects[TAILS_SLOT], 0, sizeof(Object));
    memset(&objects[0x1D], 0, sizeof(Object));
    Object *tails = TAILS_OBJ;
    tails->anim = 0;
    tails->status.p.f.pushing = true;
    Object *tail = &objects[0x1D];
    bool wagging = false;
    for (int i = 0; i < 12; i++) {
        Obj_TailsTails(tail);
        wagging |= tail->frame >= 0x87 && tail->frame <= 0x8A;
    }
    CHECK(wagging);
}

static void TailsTails_ShowWhileRollingAndDashing(void) {
    CHECK(TailsTailsShows(2, 0x600));
    CHECK(TailsTailsShows(3, 0x600));
    CHECK(TailsTailsShows(9, 0));
}


// The music: the final game's 29 tracks at its ids ($81-$9D, here 1-$1D), each with its song, and the level playlist as the alpha's MusicList has it (its bytes less $80), but for Hidden Palace
static void Music_EveryFinalTrackHasASongAndZonesFollowTheAlphasPlaylist(void) {
    for (int id = 1; id <= 0x1F; id++) {
        CHECK(game_sound_bank.songs[id] != NULL);
        CHECK(game_sound_bank.songs_json[id] != NULL);
    }
    CHECK_EQ(game_sound_bank.music_first, 1);
    CHECK_EQ(game_sound_bank.music_last, 0x20);
    CHECK(game_sound_bank.songs[0x1E] == game_sound_bank.songs[0x10]); // $9E is a copy of $90, $9F (the last) an empty song
    static const uint8_t alpha_music_list[16] = { 0x82, 0x82, 0x85, 0x84, 0x85, 0x85, 0x8C, 0x86, 0x83, 0x8D, 0x88, 0x8B, 0x89, 0x8E, 0x8E, 0x87 };
    for (int zone = 0; zone < 16; zone++) {
        if (zone == 8)
            CHECK_EQ(Level_Music(LEVEL_ID(zone, 0)), 0x10); // Hidden Palace stays, with its own track ($90), where the alpha has the Mystic Cave 2P theme
        else if (zone == 14)
            CHECK_EQ(Level_Music(LEVEL_ID(zone, 0)), mus_MzSwap); // Cyber City (in the alpha's Genocide City slot) has the Metropolis theme with major and minor swapped (id $20), where the alpha has Chemical Plant's
        else
            CHECK_EQ(Level_Music(LEVEL_ID(zone, 0)), alpha_music_list[zone] - 0x80);
    }
}


// Super Sonic runs on his own table of animations (the alpha's, which follows Sonic's in the data): his standing and walking frames are not Sonic's
static uint8_t SonicWalkFrame(bool super) {
    super_sonic_flag = super;
    Object sonic;
    memset(&sonic, 0, sizeof(sonic));
    sonic.anim = SonAnimId_Walk;
    sonic.prev_anim = 0xFF;
    sonic.inertia = 0x100;
    Sonic_Animate(&sonic);
    super_sonic_flag = false;
    return sonic.frame;
}

static void Sonic_SuperSonicHasHisOwnAnimations(void) {
    CHECK(SonicWalkFrame(true) != SonicWalkFrame(false));
    SuperSonic_Set(true);
    CHECK_EQ(sonspeed_max, 0xA00);
    SuperSonic_Set(false);
    CHECK_EQ(sonspeed_max, 0x600);
    CHECK(!super_sonic_flag);
}


// The alpha's dust and splash: the dust of a spin dash appears where the player is, with its tiles brought to the window ($49C for Sonic), the splash at the water's height, and neither while he has no air to spare
static void DustSplash_FollowsTheSpinDashAndSplashesAtTheWater(void) {
    memset(&objects[0], 0, sizeof(Object));
    objects[0].pos.l.x.f.u = 0x123;
    objects[0].pos.l.y.f.u = 0x234;
    ((Scratch_Sonic *)&objects[0].scratch)->air = 30;
    Object *dust = &objects[0x1B];
    memset(dust, 0, sizeof(*dust));
    dust->type = ObjId_Splash;
    Obj_DustSplash(dust);
    CHECK_EQ(dust->routine, 2);

    DustSplash_Show(dust, DUST_DASH);
    for (int i = 0; i < 6; i++)
        Obj_DustSplash(dust);
    CHECK_EQ(dust->pos.l.x.f.u, 0x123);
    CHECK_EQ(dust->pos.l.y.f.u, 0x234);
    const uint8_t *tiles = VDP_TileSpace() + 0x49C * 0x20;
    bool art = false;
    for (int b = 0; b < 0x80; b++)
        art |= tiles[b] != 0;
    CHECK(art);

    ((Scratch_Sonic *)&objects[0].scratch)->air = 5; // (about to drown: no dust)
    Obj_DustSplash(dust);
    CHECK_EQ(dust->anim, DUST_NULL);
    ((Scratch_Sonic *)&objects[0].scratch)->air = 30;

    wtr_pos1 = 0x300;
    DustSplash_Show(dust, DUST_SPLASH);
    Obj_DustSplash(dust);
    CHECK_EQ(dust->pos.l.y.f.u, 0x300);
    CHECK_EQ(dust->pos.l.x.f.u, 0x123);
}


// The alpha's counting object: a player underwater loses a unit of air every 60 frames (starting with the first), bubbles come out of him, and at 0 he drowns (his animation, then the death routine 120 frames on)
static int CountdownObjects(void) {
    int n = 0;
    for (int i = 1; i < 0x40; i++)
        if (objects[i].type == ObjId_DrownCount)
            n++;
    return n;
}

static void Countdown_CountsAPlayersAirAndDrownsHim(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    Object *sonic = &objects[0];
    sonic->type = ObjId_Sonic;
    sonic->routine = 2;
    sonic->status.p.f.underwater = true;
    sonic->pos.l.x.f.u = 0x200;
    sonic->pos.l.y.f.u = 0x400;
    wtr_pos1 = 0x100;
    ((Scratch_Sonic *)&sonic->scratch)->air = 30;
    Countdown_Make(13, false);
    Object *counter = &objects[13];

    Obj_Countdown(counter); // the first frame counts at once
    CHECK_EQ(((Scratch_Sonic *)&sonic->scratch)->air, 29);
    for (int f = 0; f < 59; f++)
        Obj_Countdown(counter);
    CHECK_EQ(((Scratch_Sonic *)&sonic->scratch)->air, 29);
    Obj_Countdown(counter);
    CHECK_EQ(((Scratch_Sonic *)&sonic->scratch)->air, 28);
    CHECK(CountdownObjects() > 1); // (bubbles)

    // Out of air: he drowns
    ((Scratch_Sonic *)&sonic->scratch)->air = 0;
    for (int f = 0; f < 60; f++)
        Obj_Countdown(counter);
    CHECK_EQ(sonic->anim, SonAnimId_Drown);
    CHECK_EQ(((Scratch_Sonic *)&sonic->scratch)->air, 30);
    CHECK_EQ(sonic->routine, 2);
    for (int f = 0; f < 0x80; f++)
        Obj_Countdown(counter);
    CHECK_EQ(sonic->routine, 6);
}


// The alpha's animals: each zone has its pair (the cue its title cards load), one of the two comes out of a badnik with the kind and the speeds the pair gives, and those of the end of a level have the kind of
// movement, speeds and art of their subtype
static void Animals_ZonesHaveTheirPairAndEndAnimalsTheirOwnMovement(void) {
    level_id = (uint16_t)(ZoneId_EHZ << 8);
    CHECK_EQ(Level_AnimalsPlc(), PlcId_Flicky32);
    level_id = (uint16_t)(ZoneId_HPZ << 8);
    CHECK_EQ(Level_AnimalsPlc(), PlcId_Flicky36);
    level_id = (uint16_t)(ZoneId_CPZ << 8);
    CHECK_EQ(Level_AnimalsPlc(), PlcId_Flicky3A);

    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    boss_status = 0;
    level_id = (uint16_t)(ZoneId_HPZ << 8); // (Mouse $08 at $580, Seal $03 at $594)
    Object *animal = &objects[0x20];
    animal->type = ObjId_Animal;
    Obj_FlickyAnimals(animal);
    CHECK_EQ(animal->routine, 2);
    CHECK_EQ(animal->frame, 2);
    CHECK_EQ(animal->ysp, -0x400);
    const unsigned tile = animal->tile & 0x7FF;
    CHECK(tile == 0x580 || tile == 0x594);
    const Scratch_Flicky *a = (const Scratch_Flicky *)&animal->scratch;
    CHECK(tile == 0x580 ? a->kind == 8 : a->kind == 3);

    boss_status = 1; // (after a boss they wait for the capsule instead)
    Object *waiting = &objects[0x21];
    waiting->type = ObjId_Animal;
    Obj_FlickyAnimals(waiting);
    CHECK_EQ(waiting->routine, 0x1C);
    CHECK_EQ(waiting->xsp, 0);
    boss_status = 0;

    Object *prison = &objects[0x22];
    prison->type = ObjId_Animal;
    ((Scratch_Flicky *)&prison->scratch)->subtype = 15; // (the Penguin that waits for the player, art at $573)
    Obj_FlickyAnimals(prison);
    CHECK_EQ(prison->routine, 30);
    CHECK_EQ(prison->tile & 0x7FF, 0x573);
    CHECK_EQ(prison->xsp, -0x180);
    CHECK_EQ(prison->ysp, -0x300);
}


// The alpha's Coconuts: it sets itself up from its subtype's look, waits, throws a coconut at Sonic when he is within $60 pixels of it (turning to him), and the coconut falls from its hand
static void Coconuts_ThrowACoconutWhenSonicIsNear(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    objects[0].type = ObjId_Sonic;
    objects[0].pos.l.x.f.u = 0x400;
    objects[0].pos.l.y.f.u = 0x300;
    scrpos_x.f.u = 0x300;
    scrpos_y.f.u = 0x200;
    Object *monkey = &objects[0x20];
    monkey->type = ObjId_Coconuts;
    monkey->pos.l.x.f.u = 0x440;
    monkey->pos.l.y.f.u = 0x300;
    ((Scratch_Flicky *)&monkey->scratch)->subtype = 0x1E;
    Obj_Coconuts(monkey);
    CHECK_EQ(monkey->routine, 2);
    CHECK_EQ(monkey->tile & 0x7FF, 0x3EE);
    CHECK_EQ(monkey->col_type, 9);

    Obj_Coconuts(monkey); // (Sonic is $40 pixels to its left: it faces him and throws)
    CHECK_EQ(monkey->routine, 6);
    CHECK_EQ(monkey->frame, 1);
    CHECK(!monkey->render.f.x_flip);
    for (int f = 0; f < 10; f++)
        Obj_Coconuts(monkey);
    CHECK_EQ(monkey->frame, 2);
    Object *nut = NULL;
    for (int i = 1; i < 0x40; i++)
        if (objects[i].type == ObjId_EnemyWeapon)
            nut = &objects[i];
    CHECK(nut != NULL);
    if (nut != NULL) {
        CHECK_EQ(nut->xsp, -0x100);
        CHECK_EQ(nut->pos.l.x.f.u, 0x440 + 11);
        CHECK_EQ(nut->pos.l.y.f.u, 0x300 - 13);
        Obj_EnemyWeapon(nut);
        CHECK_EQ(nut->col_type, 0x8B);
        Obj_EnemyWeapon(nut);
        CHECK_EQ(nut->ysp, 0x20);
    }
}

// The alpha's boss explosion: seven frames of seven ticks, then it is gone
static void BossExplosion_RunsSevenFramesAndGoes(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    Object *exp = &objects[0x30];
    exp->type = 0x58;
    exp->pos.l.x.f.u = 0x100;
    exp->pos.l.y.f.u = 0x100;
    Obj_BossExplosion(exp);
    CHECK_EQ(exp->routine, 2);
    CHECK_EQ(exp->tile & 0x7FF, 0x580);
    for (int f = 0; f < 7 * 8 + 2 && exp->type != 0; f++)
        Obj_BossExplosion(exp);
    CHECK_EQ(exp->type, 0);
}


// The alpha's Slicer: it walks towards the left, and when Sonic is within $80 pixels in front of it raises its blades and sends two pincers at him
static void Slicer_RaisesItsBladesAndSendsTwoPincers(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    objects[0].type = ObjId_Sonic;
    objects[0].pos.l.x.f.u = 0x400;
    objects[0].pos.l.y.f.u = 0x300;
    scrpos_x.f.u = 0x300;
    scrpos_y.f.u = 0x200;
    Object *slicer = &objects[0x20];
    slicer->type = ObjId_Slicer;
    slicer->pos.l.x.f.u = 0x440;
    slicer->pos.l.y.f.u = 0x300;
    Obj_Slicer(slicer);
    CHECK_EQ(slicer->routine, 2);
    CHECK_EQ(slicer->xsp, -0x40);
    CHECK_EQ(slicer->col_type, 6);
    Obj_Slicer(slicer); // (Sonic is in front of it, $40 away)
    CHECK_EQ(slicer->routine, 6);
    CHECK_EQ(slicer->frame, 3);
    for (int f = 0; f < 10; f++)
        Obj_Slicer(slicer);
    CHECK_EQ(slicer->frame, 4);
    int pincers = 0;
    for (int i = 1; i < 0x40; i++)
        if (objects[i].type == ObjId_SlicerPincers)
            pincers++;
    CHECK_EQ(pincers, 2);
}

// The alpha's unfinished Metropolis boss: its ship makes the object of the balls, which makes seven of them (the object itself the first) that orbit the ship; a hit lets them go
static void MTZBoss_MakesSevenBallsThatOrbitTheShip(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    scrpos_x.f.u = 0x1D00;
    scrpos_y.f.u = 0x100;
    Object *ship = &objects[0x20];
    ship->type = ObjId_MTZBoss;
    Obj_MTZBoss(ship);
    CHECK_EQ(ship->pos.l.x.f.u, 0x1EA0 - 2);
    CHECK_EQ(ship->col_type, 0xF);
    Object *first = NULL;
    for (int i = 1; i < 0x40 && first == NULL; i++)
        if (objects[i].type == ObjId_MTZBossBall)
            first = &objects[i];
    CHECK(first != NULL);
    if (first != NULL)
        Obj_MTZBossBall(first); // (it makes the seven)
    int balls = 0;
    for (int i = 1; i < 0x40; i++)
        if (objects[i].type == ObjId_MTZBossBall && objects[i].routine == 2)
            balls++;
    CHECK_EQ(balls, 7);
}


// The alpha's Emerald Hill boss, from the level's event to its parts: in act 2, as the camera reaches $26E0, the event puts the ship (object 56, subtype $81) in the arena, locks the screen and starts the boss's music; the ship makes
// its face and the car's parts (objects 5B), and the explosions of its wreck are object 58
static void EHZBoss_TheEventPutsTheAlphasShipInTheArena(void) {
    for (int i = 0; i < 0x80; i++)
        memset(&objects[i], 0, sizeof(Object));
    level_id = LEVEL_ID(ZoneId_EHZ, 1);
    dle_routine = 0;
    boss_status = 0;
    lock_screen = false;
    scrpos_x.f.u = 0x2700;
    DynamicLevelEvents();
    CHECK(lock_screen);
    CHECK_EQ(dle_routine, 2);
    Object *ship = NULL;
    for (int i = 1; i < 0x80; i++)
        if (objects[i].type == 0x56)
            ship = &objects[i];
    CHECK(ship != NULL);
    if (ship == NULL)
        return;
    CHECK_EQ(ship->pos.l.x.f.u, 0x29D0);
    Obj_EHZBoss(ship);
    CHECK_EQ(ship->pos.l.x.f.u, 0x2A00); // (the ship is put at $2A00,$2C0, out of the arena, and slides in from the right)
    CHECK_EQ(ship->pos.l.y.f.u, 0x2C0);
    int parts = 0;
    for (int i = 1; i < 0x80; i++)
        if (objects[i].type == 0x5B)
            parts++;
    CHECK_EQ(parts, 6); // (the car's body, its three wheels and spike, and the cockpit's glass, as the alpha makes them)
}

// The rotating cylinder of Metropolis (object 06 with bit 7 in its subtype): a character level with its middle is taken onto it and carried round (his height follows a cosine, his flip angle goes on by 4), and let go at its end
static void Cylinder_TakesACharacterAndLetsGoAtTheEnd(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    objects[0].type = ObjId_Sonic;
    objects[0].y_rad = 0x13;
    objects[0].pos.l.x.f.u = 0x500;
    objects[0].pos.l.y.f.u = 0x429; // (his feet 4 pixels into the cylinder's top)
    objects[0].inertia = 0x700;
    scrpos_x.f.u = 0x400;
    scrpos_y.f.u = 0x300;
    Object *cyl = &objects[0x20];
    cyl->type = 6;
    cyl->scratch.u8[0] = 0x80;
    cyl->pos.l.x.f.u = 0x500;
    cyl->pos.l.y.f.u = 0x400;
    Obj_Spiral(cyl);
    CHECK_EQ(cyl->routine, 4);
    CHECK(cyl->status.b & 8); // (he is on it)
    CHECK_EQ(objects[0].pos.l.y.f.u, 0x428);
    Obj_Spiral(cyl);
    const int16_t y_start = objects[0].pos.l.y.f.u;
    for (int f = 0; f < 4; f++)
        Obj_Spiral(cyl);
    CHECK(objects[0].pos.l.y.f.u != y_start || ((Scratch_Sonic *)&objects[0].scratch)->flip_angle != 0); // (round it)
    objects[0].pos.l.x.f.u = 0x500 + 0x100; // (past its end)
    Obj_Spiral(cyl);
    CHECK(!(cyl->status.b & 8));
    CHECK(objects[0].status.p.f.in_air);
}

// The end of an act: the characters that run on to the right are despawned when they leave the screen (not before), so they do not fall out of the level and die under the results card
static void Signpost_LevelEndDespawnsCharactersThatLeaveTheScreen(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    objects[0].type = ObjId_Sonic;
    scrpos_x.f.u = 0x1000;
    Object *sign = &objects[0x20];
    sign->type = ObjId_Signpost;
    sign->routine = 8;
    sign->pos.l.x.f.u = 0x1000 + 0x80;
    objects[0].pos.l.x.f.u = 0x1000 + SCREEN_WIDTH - 8; // (still on the screen)
    Obj_Signpost(sign);
    CHECK_EQ(objects[0].type, ObjId_Sonic);
    objects[0].pos.l.x.f.u = 0x1000 + SCREEN_WIDTH + 8; // (gone off its right)
    Obj_Signpost(sign);
    CHECK_EQ(objects[0].type, ObjId_Null);
}

// A level that wraps vertically (its top limit is exactly $FF00, its bottom $800: Metropolis): with the camera near the bottom of the level and the character just over the wrap (at the top), the camera goes on (it is
// $60 above him) and does not jump a whole level's height, and one level's height up is the same: the distance is taken modulo $800
static void Scroll_AWrappingLevelKeepsTheCameraWithACharacterOverTheWrap(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    objects[0].type = ObjId_Sonic;
    limit_top2 = 0xFF00;
    limit_btm2 = 0x800;
    look_shift = 0x60;
    bgscrollvert = false;
    scrpos_y.v = 0x7C0 << 16;
    objects[0].pos.l.y.f.u = 0x7C0 + 0x60; // (the character is where the camera wants him: $60 down)
    objects[0].pos.l.y.f.u &= 0x7FF;       // (= $20, over the wrap)
    ScrollVertical();
    CHECK(scrpos_y.f.u == 0x7C0 || scrpos_y.f.u == 0x7C0 - 0x800 + 0x800); // (it stays)
    CHECK(scrshift_y == 0);
    objects[0].pos.l.y.f.u = 0x7E0; // (above where he should be: the camera goes up a little, not a level's height)
    ScrollVertical();
    CHECK(scrpos_y.f.u < 0x7C0 && scrpos_y.f.u > 0x7C0 - 0x20);
    scrpos_y.v = 0x7F0 << 16;
    objects[0].pos.l.y.f.u = 0x10; // (over the wrap, $20 below the camera's top: well above its reference, $40 up)
    ScrollVertical();
    CHECK(scrpos_y.f.u < 0x7F0 && scrpos_y.f.u > 0x7F0 - 0x20);
    limit_top2 = 0;
    limit_btm2 = 0x720;
}

// Hill Top's Spiker: it walks to and fro, and with Sonic within $20 pixels across it stops, and fires its drill (once)
static void Spiker_FiresItsDrillOnceWhenSonicIsAbove(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    objects[0].type = ObjId_Sonic;
    objects[0].pos.l.x.f.u = 0x410;
    objects[0].pos.l.y.f.u = 0x200;
    scrpos_x.f.u = 0x300;
    scrpos_y.f.u = 0x100;
    Object *spiker = &objects[0x20];
    spiker->type = ObjId_Spiker;
    spiker->pos.l.x.f.u = 0x400;
    spiker->pos.l.y.f.u = 0x300;
    Obj_Spiker(spiker);
    CHECK_EQ(spiker->routine, 2);
    CHECK_EQ(spiker->col_type, 0x12);
    for (int f = 0; f < 40; f++)
        Obj_Spiker(spiker);
    int drills = 0;
    for (int i = 1; i < 0x40; i++)
        if (objects[i].type == ObjId_SpikerDrill)
            drills++;
    CHECK_EQ(drills, 1); // (and not another, however long it stays)
    CHECK_EQ(spiker->frame == 2 || spiker->anim == 1, true);
}

// The alpha's Hill Top boss: it rises to the top of its side, opens its cannon (a jet of flame, once, and a second hitbox while it is open), sinks and throws two balls of lava; with its hits out it blows up and the act's boss is done
static void HTZBoss_FiresItsJetThenThrowsTwoLavaBalls(void) {
    for (int i = 0; i < 0x80; i++)
        memset(&objects[i], 0, sizeof(Object));
    memset(&boss_move, 0, sizeof(boss_move));
    boss_status = 0;
    objects[0].type = ObjId_Sonic;
    objects[0].pos.l.x.f.u = 0x3000;
    objects[0].pos.l.y.f.u = 0x500;
    scrpos_x.f.u = 0x2F00;
    scrpos_y.f.u = 0x400;
    Object *boss = &objects[0x20];
    boss->type = ObjId_HTZBoss;
    Obj_HTZBoss(boss);
    CHECK_EQ(boss->col_type, 0xF);
    CHECK_EQ(((Scratch_AlphaBoss *)&boss->scratch)->hits, 8);
    int jets = 0, balls = 0;
    bool box_was_there = false;
    for (int f = 0; f < 1200 && balls < 2; f++) {
        frame_count++;
        Obj_HTZBoss(boss);
        int16_t cx, cy, hw;
        box_was_there |= HTZBoss_CannonBox(boss, &cx, &cy, &hw);
        jets = balls = 0;
        for (int i = 0x21; i < 0x80; i++) {
            if (objects[i].type != ObjId_HTZBoss)
                continue;
            if (((Scratch_AlphaBoss *)&objects[i].scratch)->boss_routine == 6 && objects[i].routine_sec == 0)
                Obj_HTZBoss(&objects[i]); // (the spawner makes its two)
            const uint8_t part = ((Scratch_AlphaBoss *)&objects[i].scratch)->boss_routine;
            jets += part == 4;
            balls += part == 6;
        }
    }
    CHECK_EQ(jets, 1);
    CHECK_EQ(balls, 2);
    CHECK_EQ(box_was_there, true);
    int16_t left = 0, right = 0;
    for (int i = 0x21; i < 0x80; i++)
        if (objects[i].type == ObjId_HTZBoss && ((Scratch_AlphaBoss *)&objects[i].scratch)->boss_routine == 6) {
            left += objects[i].xsp < 0;
            right += objects[i].xsp > 0;
        }
    CHECK_EQ(left, 1);
    CHECK_EQ(right, 1);

    // beaten: its explosions, and then the boss is done
    ((Scratch_AlphaBoss *)&boss->scratch)->hits = 0;
    for (int f = 0; f < 1200 && !boss_status; f++) {
        frame_count++;
        Obj_HTZBoss(boss);
    }
    CHECK_EQ(boss_status, 1);
    CHECK_EQ(boss->type, 0);
}

// The alpha's Rhinobot (4D, unused there): its look from the art list ($3B2, palette line 1), and once it stands on the floor it waits, then walks off to the way it faces turned round
static void Rhinobot_SetsItselfUpAndWalksOff(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    objects[0].type = ObjId_Sonic;
    objects[0].pos.l.x.f.u = 0x400;
    scrpos_x.f.u = 0x300;
    scrpos_y.f.u = 0x280;
    Object *rhino = &objects[0x20];
    rhino->type = 0x4D;
    rhino->pos.l.x.f.u = 0x400;
    rhino->pos.l.y.f.u = 0x300;
    Obj_Rhinobot(rhino);
    CHECK_EQ(rhino->col_type, 0x0A);
    CHECK_EQ(rhino->tile, TILE_MAP(0, 1, 0, 0, 0x3B2));
    CHECK_EQ(rhino->width_pixels, 0x18);
    rhino->routine = 2; // (standing: no floor in the test)
    rhino->routine_sec = 0;
    Obj_Rhinobot(rhino);
    CHECK_EQ(rhino->routine_sec, 2);
    CHECK_EQ(rhino->xsp, 0x80);
    CHECK_EQ(rhino->status.o.f.x_flip, true);
}

// The alpha's new effects ($E1-$E9) are effects like the rest: they play on the effects' channels (they once played into the music's, over its notes)
static void Music_AlphaEffectsPlayOnTheEffectChannelsNotTheMusics(void) {
    Sound_Init();
    memset(sound_music.queue, 0, sizeof(sound_music.queue));
    memset(sound_sfx.queue, 0, sizeof(sound_sfx.queue));
    StopAllSound();
    PlayMusic(mus_EHZ);
    for (int f = 0; f < 20; f++)
        Sound_Frame();
    int music_channels = 0;
    for (int i = 0; i < SOUND_CHANNELS; i++)
        music_channels += sound_music.channels[i].active;
    PlaySound(sfx_DHZRoots2);
    Sound_Frame();
    CHECK(sound_sfx.channels[2].active);      // its noise track (PSG3)
    CHECK(sound_sfx.channels[2].psg_noise);
    int after = 0;
    for (int i = 0; i < SOUND_CHANNELS; i++)
        after += sound_music.channels[i].active;
    CHECK_EQ(after, music_channels);          // and the song's own tracks are all still there
}

// The 1-up jingle (the final's, $98) interrupts the level song and the song comes back, fading in
static void Music_ExtraLifeJingleGivesTheLevelSongBack(void) {
    Sound_Init();
    memset(sound_music.queue, 0, sizeof(sound_music.queue));
    memset(sound_sfx.queue, 0, sizeof(sound_sfx.queue));
    StopAllSound();
    PlayMusic(mus_EHZ);
    for (int f = 0; f < 200; f++)
        Sound_Frame();
    PlayMusic(mus_ExtraLife);
    Sound_Frame();
    CHECK_EQ(sound_music.current_music_id, mus_ExtraLife);
    int back = -1;
    for (int f = 0; f < 1800 && back < 0; f++) {
        Sound_Frame();
        if (sound_music.current_music_id == mus_EHZ)
            back = f;
    }
    CHECK(back > 0);
    CHECK(sound_music.fadein_active);
}

// $9F, an empty song, silences what was playing, and $9E plays Hidden Palace's song as $90 does
static void Music_BlankSongSilencesAndNineEIsHiddenPalace(void) {
    Sound_Init();
    memset(sound_music.queue, 0, sizeof(sound_music.queue));
    memset(sound_sfx.queue, 0, sizeof(sound_sfx.queue));
    StopAllSound();
    PlayMusic(mus_EHZ);
    for (int f = 0; f < 60; f++)
        Sound_Frame();
    PlayMusic(mus_Blank);
    Sound_Frame();
    Sound_Frame();
    CHECK_EQ(sound_music.current_music_id, mus_Blank);
    int active = 0;
    for (int i = 0; i < SOUND_CHANNELS; i++)
        active += sound_music.channels[i].active;
    CHECK_EQ(active, 0);
    PlayMusic(mus_HPZCopy);
    Sound_Frame();
    Sound_Frame();
    CHECK_EQ(sound_music.current_music_id, mus_HPZCopy);
}

// The song that comes back after the 1-up jingle has the voices it had: the operator registers of every FM channel (the voices, not the levels) are what they were when the jingle cut in
static void RestartSound(uint8_t song) {
    Sound_Init();
    memset(sound_music.queue, 0, sizeof(sound_music.queue));
    memset(sound_sfx.queue, 0, sizeof(sound_sfx.queue));
    StopAllSound();
    PlayMusic(song);
}

// What the jingle leaves in the chip must not outlast it: the song brought back has the noise mode it set at its start (its smpsPSGform, which it never writes again), and its FM voices
// A song change leaves nothing of the song before it in the chips: the noise mode and the FM voices are what a fresh start of the new song has
static void Music_SongChangeLeavesNothingOfThePreviousSong(void) {
    static const uint8_t after[] = { mus_EHZ, mus_OOZ, mus_MTZ, mus_HPZ, mus_CPZ, mus_Boss };
    int bad = 0;
    for (size_t k = 0; k < sizeof(after); k++) {
        const uint8_t song = after[k];
        RestartSound(song);
        for (int f = 0; f < 400; f++)
            Sound_Frame();
        const uint8_t rate = sound_music.psg.noise_shift_rate, white = sound_music.psg.noise_fb_white;
        uint8_t want[2][0xA0];
        for (int port = 0; port < 2; port++)
            for (int r = 0x30; r < 0xB8; r++)
                want[port][r - 0x30] = YM2612_PeekReg(sound_music.fm, port, (uint8_t)r);
        for (int before = 1; before <= 0x1D; before++) {
            if (before == song || before == mus_ExtraLife)
                continue;
            RestartSound((uint8_t)before);
            for (int f = 0; f < 400; f++)
                Sound_Frame();
            PlayMusic(song);
            for (int f = 0; f < 400; f++)
                Sound_Frame();
            if (sound_music.psg.noise_shift_rate != rate || sound_music.psg.noise_fb_white != white) {
                fprintf(stderr, "song %d after song %d: noise %d/%d, fresh %d/%d\n", song, before, sound_music.psg.noise_shift_rate, sound_music.psg.noise_fb_white, rate, white);
                bad++;
            }
            for (int port = 0; port < 2; port++)
                for (int r = 0x30; r < 0xB8; r++) {
                    if ((r & 0xF0) == 0x40 || (r & 0xF0) == 0x90 || (r & 0xFC) == 0xA0 || (r & 0xFC) == 0xA4 || (r & 3) == 3 || want[port][r - 0x30] == 0)
                        continue;
                    if (YM2612_PeekReg(sound_music.fm, port, (uint8_t)r) != want[port][r - 0x30]) {
                        fprintf(stderr, "song %d after song %d: port %d reg %02X is %02X, fresh %02X\n", song, before, port, r, YM2612_PeekReg(sound_music.fm, port, (uint8_t)r), want[port][r - 0x30]);
                        bad++;
                    }
                }
        }
    }
    CHECK_EQ(bad, 0);
}

static void Music_SongKeepsItsNoiseAndVoicesAfterTheExtraLifeJingle(void) {
    int songs_with_noise = 0;
    for (int song = 1; song <= 0x1D; song++) {
        if (song == mus_ExtraLife || song == mus_ActClear || song == mus_GameOver || song == mus_Continue || song == mus_Emerald)
            continue;
        RestartSound((uint8_t)song);
        for (int f = 0; f < 400; f++)
            Sound_Frame();
        const uint8_t rate = sound_music.psg.noise_shift_rate, white = sound_music.psg.noise_fb_white;
        uint8_t voices[2][0xA0];
        for (int port = 0; port < 2; port++)
            for (int r = 0x30; r < 0xB8; r++)
                voices[port][r - 0x30] = YM2612_PeekReg(sound_music.fm, port, (uint8_t)r);
        int playing = 0;
        for (int i = 0; i < SOUND_CHANNELS; i++)
            playing += sound_music.channels[i].active;
        if (!playing)
            continue; // (a song that does not loop has ended by now)
        if (rate != 0 || white != 0)
            songs_with_noise++;
        PlayMusic(mus_ExtraLife);
        Sound_Frame();
        CHECK_EQ(sound_music.current_music_id, mus_ExtraLife);
        for (int f = 0; f < 3000 && sound_music.current_music_id != song; f++)
            Sound_Frame();
        CHECK_EQ(sound_music.current_music_id, song);
        Sound_Frame();
        CHECK_EQ(sound_music.psg.noise_shift_rate, rate);
        CHECK_EQ(sound_music.psg.noise_fb_white, white);
        for (int port = 0; port < 2; port++)
            for (int r = 0x30; r < 0xB8; r++) {
                if ((r & 0xF0) == 0x40 || (r & 0xF0) == 0x90 || (r & 0xFC) == 0xA0 || (r & 0xFC) == 0xA4 || (r & 3) == 3)
                    continue; // (levels, SSG-EG and frequencies move on)
                if (voices[port][r - 0x30] == 0)
                    continue; // (a channel the song had not started yet)
                if (YM2612_PeekReg(sound_music.fm, port, (uint8_t)r) != voices[port][r - 0x30])
                    fprintf(stderr, "song %d port %d reg %02X: %02X was %02X\n", song, port, r, YM2612_PeekReg(sound_music.fm, port, (uint8_t)r), voices[port][r - 0x30]);
                CHECK_EQ(YM2612_PeekReg(sound_music.fm, port, (uint8_t)r), voices[port][r - 0x30]);
            }
    }
    CHECK(songs_with_noise > 0); // (some of the final's songs do use the noise channel)
}

// The Sol: it makes four fireballs that circle it, and with Sonic near and its frame 2 it throws them one by one
static void Sol_MakesFourFireballs(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    objects[0].type = ObjId_Sonic;
    objects[0].pos.l.x.f.u = 0x1000;
    scrpos_x.f.u = 0x300;
    scrpos_y.f.u = 0x100;
    Object *sol = &objects[0x20];
    sol->type = ObjId_Sol;
    sol->pos.l.x.f.u = 0x400;
    sol->pos.l.y.f.u = 0x300;
    Obj_Sol(sol);
    CHECK_EQ(sol->routine, 2);
    int balls = 0;
    for (int i = 1; i < 0x40; i++)
        if (objects[i].type == ObjId_Sol && objects[i].routine == 6)
            balls++;
    CHECK_EQ(balls, 4);
    CHECK_EQ(sol->col_type, 0x0B);
}

// The Rexon: the body walks to and fro, and with Sonic within reach it rears up and makes its five heads, the last of which throws fireballs; with the last one gone the rest come apart
static void Rexon_RearsUpIntoFiveHeads(void) {
    for (int i = 0; i < 0x40; i++)
        memset(&objects[i], 0, sizeof(Object));
    objects[0].type = ObjId_Sonic;
    objects[0].pos.l.x.f.u = 0x1000; // (far)
    scrpos_x.f.u = 0x300;
    scrpos_y.f.u = 0x100;
    Object *rexon = &objects[0x20];
    rexon->type = ObjId_RexonBody;
    rexon->pos.l.x.f.u = 0x400;
    rexon->pos.l.y.f.u = 0x300;
    Obj_RexonBody(rexon);
    CHECK_EQ(rexon->routine, 2);
    CHECK_EQ(rexon->frame, 2);
    Obj_RexonBody(rexon);
    CHECK_EQ(rexon->routine, 2); // (nothing yet)
    objects[0].pos.l.x.f.u = 0x420;
    objects[0].pos.l.y.f.u = 0x300;
    Obj_RexonBody(rexon);
    CHECK_EQ(rexon->routine, 6);
    int heads = 0;
    for (int i = 1; i < 0x40; i++)
        if (objects[i].type == ObjId_RexonHead)
            heads++;
    CHECK_EQ(heads, 5);
}

void RegisterObjectCoverageTests(void) {
    RUN_TEST(DebugMarkers_TheCornersOfABoxAreMarkedOnlyWithTheCheat);
    RUN_TEST(TailsTails_ShowWhileRollingAndDashing);
    RUN_TEST(TailsTails_WagWhileHePushes);
    RUN_TEST(Countdown_CountsAPlayersAirAndDrownsHim);
    RUN_TEST(Coconuts_ThrowACoconutWhenSonicIsNear);
    RUN_TEST(BossExplosion_RunsSevenFramesAndGoes);
    RUN_TEST(Scroll_AWrappingLevelKeepsTheCameraWithACharacterOverTheWrap);
    RUN_TEST(Signpost_LevelEndDespawnsCharactersThatLeaveTheScreen);
    RUN_TEST(Cylinder_TakesACharacterAndLetsGoAtTheEnd);
    RUN_TEST(EHZBoss_TheEventPutsTheAlphasShipInTheArena);
    RUN_TEST(Spiker_FiresItsDrillOnceWhenSonicIsAbove);
    RUN_TEST(HTZBoss_FiresItsJetThenThrowsTwoLavaBalls);
    RUN_TEST(Rhinobot_SetsItselfUpAndWalksOff);
    RUN_TEST(Music_AlphaEffectsPlayOnTheEffectChannelsNotTheMusics);
    RUN_TEST(Music_ExtraLifeJingleGivesTheLevelSongBack);
    RUN_TEST(Music_SongKeepsItsNoiseAndVoicesAfterTheExtraLifeJingle);
    RUN_TEST(Music_SongChangeLeavesNothingOfThePreviousSong);
    RUN_TEST(Music_BlankSongSilencesAndNineEIsHiddenPalace);
    RUN_TEST(Rexon_RearsUpIntoFiveHeads);
    RUN_TEST(Sol_MakesFourFireballs);
    RUN_TEST(Slicer_RaisesItsBladesAndSendsTwoPincers);
    RUN_TEST(MTZBoss_MakesSevenBallsThatOrbitTheShip);
    RUN_TEST(Animals_ZonesHaveTheirPairAndEndAnimalsTheirOwnMovement);
    RUN_TEST(DustSplash_FollowsTheSpinDashAndSplashesAtTheWater);
    RUN_TEST(Sonic_SuperSonicHasHisOwnAnimations);
    RUN_TEST(Music_EveryFinalTrackHasASongAndZonesFollowTheAlphasPlaylist);
    RUN_TEST(DebugLists_HaveThePrototypesLengthsAndPlaceRealObjects);
    RUN_TEST(ObjectCoverage_BuiltZonesPortEveryObjectTheyPlace);
    RUN_TEST(Levels_ActsAZoneDoesNotHaveAreEmpty);
    RUN_TEST(ObjectCoverage_NoPlacedObjectIsFalselyPending);
}
