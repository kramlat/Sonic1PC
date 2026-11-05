#include "PrisonCapsule.h"
#include "SstLoad.h"
#include "Object/Explosion.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Animal.h"
#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Resource/Animation/PrisonCapsule.h"
#include "Resource/Mappings/PrisonCapsule.h"
#include "Sound.h"

#include <string.h>

// Object 3E - prison capsule after boss fights. Only subtypes 0 (capsule)
// and 1 (switch) are ever actually placed; 2/3 are leftovers from deleted
// prototype subtypes, kept faithfully.

static const struct { uint8_t routine, width, priority, frame; } Pri_Var[4] = {
    { 2, 64 / 2, 4, 0 },
    { 4, 24 / 2, 5, 1 },
    { 6, 32 / 2, 4, 3 },
    { 8, 32 / 2, 3, 5 },
};

static void Pri_Main(Sst *self, Own_PrisonCapsule *own) {
    self->mappings = Mappings_PrisonCapsule;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Prison_Capsule);
    self->render_flags = SST_RENDER_LEVEL;
    own->orig_y = self->y_pos.f.u;

    uint8_t subtype = self->subtype;
    self->routine = Pri_Var[subtype].routine;
    self->width_pixels = Pri_Var[subtype].width;
    self->priority = (uint16_t)(Pri_Var[subtype].priority * 0x80);
    self->mapping_frame = Pri_Var[subtype].frame;

    if (subtype == 2) { // unused leftover
        self->collision_flags = 0x06; // col_32x32 | col_boss
        self->collision_property = 8; // were capsules once supposed to behave like bosses?
    }
}

static void Pri_BodyMain(Sst *self) {
    if (boss_status == 2) { // has the prison been opened from the switch?
        if (((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
            self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
            player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
            player->status |= SST_PSTATUS_IN_AIR;
        }
        self->mapping_frame = 2; // destroyed prison
        return;
    }

    int16_t x_rad = (int16_t)(64 / 2 + 11 /* sonic_solid_width */);
    SstSolidObject(self, (uint16_t)x_rad, 48 / 2, 48 / 2, self->x_pos.f.u, NULL, NULL);
}

// Obj_Animals_Construct branches on the subtype byte, and a leftover nonzero
// value sends a prison animal down the ending-sequence path with the wrong art
// (the "bouncing Eggman cockpit"). Start every animal from a fully cleared
// slot, like the real DeleteObject guarantees.
static Sst *Pri_NewAnimal(void) {
    Sst *animal = Sst_Allocate();
    if (animal != NULL) {
        memset(animal, 0, sizeof(Sst));
        animal->code = sst_object_codes[ObjId_Animal];
    }
    return animal;
}

static void Pri_SpawnAnimals(Sst *self, Own_PrisonCapsule *own) {
    boss_status = 2; // set prison as being opened
    self->routine = 0xC; // advance to Pri_Animals
    self->mapping_frame = 6; // 'delete' switch by turning it invisible
    own->timer = (2 * 60) + 30;
    self->y_pos.f.u = (int16_t)(self->y_pos.f.u + 32); // load all animals 32px below explosions

    // These animals stay in the prison a bit longer to make it seem more crowded
    int16_t delay = (2 * 60) + 34;
    int16_t x_off = -28;
    for (int i = 0; i < 8; i++) {
        Sst *animal = Pri_NewAnimal();
        if (animal == NULL)
            return;
        animal->x_pos.f.u = (int16_t)(self->x_pos.f.u + x_off);
        animal->y_pos.f.u = self->y_pos.f.u;
        ((Own_Animals *)animal->own.u8)->timer = (uint16_t)delay;
        x_off += 7;
        delay -= 8;
    }
}

static void Pri_Switch(Sst *self, Own_PrisonCapsule *own) {
    int16_t x_rad = (int16_t)(24 / 2 + 11 /* sonic_solid_width */);
    SstSolidObject(self, (uint16_t)x_rad, 16 / 2, 16 / 2, self->x_pos.f.u, NULL, NULL);
    Sst_Animate(self, Animation_PrisonCapsule);
    self->y_pos.f.u = own->orig_y;

    if (!((self->status & SST_STATUS_PLAYER_STAND) != 0))
        return;

    self->y_pos.f.u = (int16_t)(self->y_pos.f.u + 8);
    self->routine = 0xA; // advance to Pri_Explosion
    own->timer = 1 * 60;
    time_count = false;
    lock_screen = false;
    lock_ctrl = 1;
    jpad1_hold2 = JPAD_RIGHT; // simulate holding right to move Sonic
    jpad1_press2 = 0;
    self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
    player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    player->status |= SST_PSTATUS_IN_AIR;
}

static void Pri_Explosion(Sst *self, Own_PrisonCapsule *own) {
    if (((uint8_t)frame_count & 7) == 0) { // only spawn an explosion every 8 frames
        Sst *exp = Sst_Allocate();
        if (exp != NULL) {
            exp->code = Obj_ExplosionBomb; // the fiery explosion (Object 3F, with the bomb's sound) a destroyed boss makes: not the badnik's, which lets an animal out
            exp->routine = 0;
            exp->x_pos.f.u = self->x_pos.f.u;
            exp->y_pos.f.u = self->y_pos.f.u;

            uint32_t rand = RandomNumber();
            int16_t rand_x = (int16_t)(((uint8_t)rand >> 2) - 32);
            exp->x_pos.f.u = (int16_t)(exp->x_pos.f.u + rand_x);
            int16_t rand_y = (int16_t)((uint8_t)(rand >> 8) >> 3);
            exp->y_pos.f.u = (int16_t)(exp->y_pos.f.u + rand_y);
        }
    }

    if (--own->timer != 0)
        return;
    Pri_SpawnAnimals(self, own);
}

static void Pri_Animals(Sst *self, Own_PrisonCapsule *own) {
    if (((uint8_t)frame_count & 7) == 0) { // only spawn an animal every 8 frames
        // These animals hop out almost as soon as they are spawned in.
        Sst *animal = Pri_NewAnimal();
        if (animal != NULL) {
            animal->x_pos.f.u = self->x_pos.f.u;
            animal->y_pos.f.u = self->y_pos.f.u;

            uint32_t rand = RandomNumber();
            int16_t rand_x = (int16_t)((rand & 0x1F) - 6);
            if ((int16_t)(rand >> 16) < 0)
                rand_x = (int16_t)-rand_x;
            animal->x_pos.f.u = (int16_t)(animal->x_pos.f.u + rand_x);
            ((Own_Animals *)animal->own.u8)->timer = 12; // hop out after 12 frames
        }
    }

    if (--own->timer != 0)
        return;
    self->routine += 2; // advance to Pri_EndAct
}

// Returns true once every animal has despawned, having already launched the
// end-of-act sequence and deleted itself (caller must not touch it further).
static bool Pri_EndAct(Sst *self) {
    for (int i = 0; i < SST_COUNT; i++) {
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[ObjId_Animal])
            return false; // still animals present -- not there yet
    }

    GotThroughAct(); // all animals have been deleted -- launch end-of-level title cards
    Sst_Delete(self);
    return true;
}

void Obj_PrisonCapsule(Sst *self) {
    Own_PrisonCapsule *own = (Own_PrisonCapsule *)self->own.u8;

    switch (self->routine) {
    case 0: Pri_Main(self, own); break;
    case 2: Pri_BodyMain(self); break;
    case 4: Pri_Switch(self, own); break;
    case 6: case 8: case 0xA: Pri_Explosion(self, own); break; // only 0xA is ever reached in practice
    case 0xC: Pri_Animals(self, own); break;
    case 0xE:
        if (Pri_EndAct(self))
            return; // already deleted itself
        break;
    }

    if (SstOutOfRange(self->x_pos.f.u))
        Sst_Delete(self);
    else
        Sst_Draw(self);
}
