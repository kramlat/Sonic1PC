#include "PrisonCapsule.h"

#include "Animal.h"
#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Resource/Animation/PrisonCapsule.h"
#include "Resource/Mappings/PrisonCapsule.h"
#include "Sound.h"

// Object 3E - prison capsule after boss fights. Only subtypes 0 (capsule)
// and 1 (switch) are ever actually placed; 2/3 are leftovers from deleted
// prototype subtypes, kept faithfully.

static const struct { uint8_t routine, width, priority, frame; } Pri_Var[4] = {
    { 2, 64 / 2, 4, 0 },
    { 4, 24 / 2, 5, 1 },
    { 6, 32 / 2, 4, 3 },
    { 8, 32 / 2, 3, 5 },
};

static bool Pri_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

static void Pri_Main(Object *obj, Scratch_PrisonCapsule *scratch) {
    obj->mappings = Mappings_PrisonCapsule;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Prison_Capsule);
    obj->render.f.align_fg = true;
    scratch->orig_y = obj->pos.l.y.f.u;

    uint8_t subtype = obj->scratch.u8[0];
    obj->routine = Pri_Var[subtype].routine;
    obj->width_pixels = Pri_Var[subtype].width;
    obj->priority = Pri_Var[subtype].priority;
    obj->frame = Pri_Var[subtype].frame;

    if (subtype == 2) { // unused leftover
        obj->col_type = 0x06; // col_32x32 | col_boss
        obj->col_property = 8; // were capsules once supposed to behave like bosses?
    }
}

static void Pri_BodyMain(Object *obj) {
    if (boss_status == 2) { // has the prison been opened from the switch?
        if (obj->status.o.f.player_stand) {
            obj->status.o.f.player_stand = false;
            player->status.p.f.object_stand = false;
            player->status.p.f.in_air = true;
        }
        obj->frame = 2; // destroyed prison
        return;
    }

    int16_t x_rad = (int16_t)(64 / 2 + 11 /* sonic_solid_width */);
    SolidObject(obj, (uint16_t)x_rad, 48 / 2, 48 / 2, obj->pos.l.x.f.u, NULL, NULL);
}

static void Pri_SpawnAnimals(Object *obj) {
    boss_status = 2; // set prison as being opened
    obj->routine = 0xC; // advance to Pri_Animals
    obj->frame = 6; // 'delete' switch by turning it invisible
    obj->frame_time.w = (2 * 60) + 30;
    obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 32); // load all animals 32px below explosions

    // These animals stay in the prison a bit longer to make it seem more crowded
    int16_t delay = (2 * 60) + 34;
    int16_t x_off = -28;
    for (int i = 0; i < 8; i++) {
        Object *animal = FindFreeObj();
        if (animal == NULL)
            return;
        animal->type = ObjId_Animal;
        animal->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + x_off);
        animal->pos.l.y.f.u = obj->pos.l.y.f.u;
        ((Scratch_Animals *)&animal->scratch)->timer = (uint16_t)delay;
        x_off += 7;
        delay -= 8;
    }
}

static void Pri_Switch(Object *obj, Scratch_PrisonCapsule *scratch) {
    int16_t x_rad = (int16_t)(24 / 2 + 11 /* sonic_solid_width */);
    SolidObject(obj, (uint16_t)x_rad, 16 / 2, 16 / 2, obj->pos.l.x.f.u, NULL, NULL);
    AnimateSprite(obj, Animation_PrisonCapsule);
    obj->pos.l.y.f.u = scratch->orig_y;

    if (!obj->status.o.f.player_stand)
        return;

    obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 8);
    obj->routine = 0xA; // advance to Pri_Explosion
    obj->frame_time.w = 1 * 60;
    time_count = false;
    lock_screen = false;
    lock_ctrl = 1;
    jpad1_hold2 = JPAD_RIGHT; // simulate holding right to move Sonic
    jpad1_press2 = 0;
    obj->status.o.f.player_stand = false;
    player->status.p.f.object_stand = false;
    player->status.p.f.in_air = true;
}

static void Pri_Explosion(Object *obj) {
    if (((uint8_t)frame_count & 7) == 0) { // only spawn an explosion every 8 frames
        Object *exp = FindFreeObj();
        if (exp != NULL) {
            exp->type = ObjId_Explosion;
            exp->routine = 2; // plain explosion -- no animal, no points
            exp->pos.l.x.f.u = obj->pos.l.x.f.u;
            exp->pos.l.y.f.u = obj->pos.l.y.f.u;

            uint32_t rand = RandomNumber();
            int16_t rand_x = (int16_t)(((uint8_t)rand >> 2) - 32);
            exp->pos.l.x.f.u = (int16_t)(exp->pos.l.x.f.u + rand_x);
            int16_t rand_y = (int16_t)((uint8_t)(rand >> 8) >> 3);
            exp->pos.l.y.f.u = (int16_t)(exp->pos.l.y.f.u + rand_y);
        }
    }

    if (--obj->frame_time.w != 0)
        return;
    Pri_SpawnAnimals(obj);
}

static void Pri_Animals(Object *obj) {
    if (((uint8_t)frame_count & 7) == 0) { // only spawn an animal every 8 frames
        // These animals hop out almost as soon as they are spawned in.
        Object *animal = FindFreeObj();
        if (animal != NULL) {
            animal->type = ObjId_Animal;
            animal->pos.l.x.f.u = obj->pos.l.x.f.u;
            animal->pos.l.y.f.u = obj->pos.l.y.f.u;

            uint32_t rand = RandomNumber();
            int16_t rand_x = (int16_t)((rand & 0x1F) - 6);
            if ((int16_t)(rand >> 16) < 0)
                rand_x = (int16_t)-rand_x;
            animal->pos.l.x.f.u = (int16_t)(animal->pos.l.x.f.u + rand_x);
            ((Scratch_Animals *)&animal->scratch)->timer = 12; // hop out after 12 frames
        }
    }

    if (--obj->frame_time.w != 0)
        return;
    obj->routine += 2; // advance to Pri_EndAct
}

// Returns true once every animal has despawned, having already launched the
// end-of-act sequence and deleted itself (caller must not touch it further).
static bool Pri_EndAct(Object *obj) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        if (level_objects[i].type == ObjId_Animal)
            return false; // still animals present -- not there yet
    }

    GotThroughAct(); // all animals have been deleted -- launch end-of-level title cards
    ObjectDelete(obj);
    return true;
}

void Obj_PrisonCapsule(Object *obj) {
    Scratch_PrisonCapsule *scratch = (Scratch_PrisonCapsule *)&obj->scratch;

    switch (obj->routine) {
    case 0: Pri_Main(obj, scratch); break;
    case 2: Pri_BodyMain(obj); break;
    case 4: Pri_Switch(obj, scratch); break;
    case 6: case 8: case 0xA: Pri_Explosion(obj); break; // only 0xA is ever reached in practice
    case 0xC: Pri_Animals(obj); break;
    case 0xE:
        if (Pri_EndAct(obj))
            return; // already deleted itself
        break;
    }

    if (Pri_OutOfRange(obj->pos.l.x.f.u))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}
