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
#include "EngineSound.h"
#include "Backend/VDP.h"

void Obj_TailsTails(Object *obj);

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
    { ZoneId_MTZ3, 0, { 0 } },
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
                const uint8_t id = e[4] & 0x7F;
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

static void ObjectCoverage_NoPlacedObjectIsFalselyPending(void) {
    for (size_t z = 0; z < sizeof(pending) / sizeof(pending[0]); z++) {
        const Pending *p = &pending[z];
        for (int i = 0; i < p->count; i++) {
            const uint8_t id = p->ids[i];
            CHECK(!(id < game_object_count && game_objects[id] != NULL && game_objects[id] != Obj_Null)); // (it was ported: take it off the list)
        }
    }
}

// The prototype's debug lists (Debug_* in its disassembly): how many entries each has, and that what each one places is an object this port runs (the ones that are not built are placeholders of type null)
static void DebugLists_HaveThePrototypesLengthsAndPlaceRealObjects(void) {
    static const struct { int zone, count; } expect[] = {
        { ZoneId_EHZ, 18 }, { ZoneId_MTZ, 28 }, { ZoneId_MTZ3, 28 }, { ZoneId_HTZ, 25 }, { ZoneId_HPZ, 8 }, { ZoneId_OOZ, 16 },
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
    for (int id = 1; id <= 0x1D; id++) {
        CHECK(game_sound_bank.songs[id] != NULL);
        CHECK(game_sound_bank.songs_json[id] != NULL);
    }
    CHECK_EQ(game_sound_bank.music_first, 1);
    CHECK_EQ(game_sound_bank.music_last, 0x1D);
    static const uint8_t alpha_music_list[16] = { 0x82, 0x82, 0x85, 0x84, 0x85, 0x85, 0x8C, 0x86, 0x83, 0x8D, 0x88, 0x8B, 0x89, 0x8E, 0x8E, 0x87 };
    for (int zone = 0; zone < 16; zone++) {
        if (zone == 8)
            CHECK_EQ(Level_Music(LEVEL_ID(zone, 0)), 0x10); // Hidden Palace stays, with its own track ($90), where the alpha has the Mystic Cave 2P theme
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


void RegisterObjectCoverageTests(void) {
    RUN_TEST(DebugMarkers_TheCornersOfABoxAreMarkedOnlyWithTheCheat);
    RUN_TEST(TailsTails_ShowWhileRollingAndDashing);
    RUN_TEST(TailsTails_WagWhileHePushes);
    RUN_TEST(Countdown_CountsAPlayersAirAndDrownsHim);
    RUN_TEST(DustSplash_FollowsTheSpinDashAndSplashesAtTheWater);
    RUN_TEST(Sonic_SuperSonicHasHisOwnAnimations);
    RUN_TEST(Music_EveryFinalTrackHasASongAndZonesFollowTheAlphasPlaylist);
    RUN_TEST(DebugLists_HaveThePrototypesLengthsAndPlaceRealObjects);
    RUN_TEST(ObjectCoverage_BuiltZonesPortEveryObjectTheyPlace);
    RUN_TEST(ObjectCoverage_NoPlacedObjectIsFalselyPending);
}
