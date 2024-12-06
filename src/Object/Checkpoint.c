#include "Checkpoint.h"
#include "Game.h"

void Obj_Checkpoint_StoreInfo(Object* obj, Scratch_Checkpoint* scratch) {
    last_lamp = scratch->subtype;
    prev_lamp = last_lamp;
    lamp_state.spawn.x = obj->pos.l.x.v;
    lamp_state.spawn.y = obj->pos.l.y.v;
    lamp_state.rings = rings;
    lamp_state.lives = life_count;
    lamp_state.time.pad = time.pad;
    lamp_state.time.min = time.min;
    lamp_state.time.sec = time.sec;
    lamp_state.time.frame = time.frame;
    lamp_state.dle = dle_routine;
    lamp_state.limitbtm = limit_btm2;
    lamp_state.foreground.x = scrpos_x.v;
    lamp_state.foreground.y = scrpos_y.v;
    lamp_state.background.x = bg_scrpos_x.v;
    lamp_state.background.y = bg_scrpos_y.v;
    lamp_state.background2.x = bg2_scrpos_x.v;
    lamp_state.background2.y = bg2_scrpos_y.v;
    lamp_state.background3.x = bg3_scrpos_x.v;
    lamp_state.background3.y = bg3_scrpos_y.v;
    lamp_state.water_level.pos = wtr_pos2;
    lamp_state.water_level.routine = wtr_routine;
    lamp_state.water_level.state = wtr_state;
}

void Obj_Checkpoint_Construct(Object* obj, Scratch_Checkpoint* scratch) {
    obj->routine += 2;
    obj->mappings = Mappings_Checkpoint;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x7A0);
    obj->render.b = 4;
    obj->width_pixels = 8;
    obj->priority = 5;

    uint8_t respawn = obj->respawn_index;
    objstate[respawn / 2] &= ~(1 << 7);

    if (objstate[respawn / 2] & 1) {
        objstate[respawn / 2] |= 1;
        obj->routine = 4;
        obj->frame = 3;
    } else {
        uint8_t sonic_last_lamp = last_lamp & 0x7F;
        uint8_t current_lamp = scratch->subtype & 0x7F;
        if (current_lamp > sonic_last_lamp) {
            Obj_Checkpoint_Blue(obj,scratch);
        }
    }
}

void Obj_Checkpoint_Blue(Object* obj, Scratch_Checkpoint* scratch) {
    if (debug_use || (jpad1_press2 & 0x80)) {
        return;
    }

    uint8_t sonic_last_lamp = last_lamp & 0x7F;
    uint8_t current_lamp = scratch->subtype & 0x7F;
    if (current_lamp <= sonic_last_lamp) {
        uint8_t respawn = obj->respawn_index;
        objstate[respawn / 2] |= 1;
        obj->routine = 4;
        obj->frame = 3;
        return;
    }

    int16_t dx = player->pos.l.x.v - obj->pos.l.x.v + 8;
    if (dx < 0 || dx >= 16) {
        return;
    }

    int16_t dy = player->pos.l.y.v - obj->pos.l.y.v + 0x40;
    if (dy < 0 || dy >= 0x68) {
        return;
    }

    // FIXME: We have no sound yet!
    // PlaySound_Special(sfx_Lamppost);
    obj->routine += 2;

    Object* newObj = FindFreeObj();
    if (newObj) {
		Scratch_Checkpoint* scratch2 = (Scratch_Checkpoint*)&newObj->scratch;
        newObj->type = ObjId_Checkpoint;
        newObj->routine = 6;
        scratch2->pos.x.v = obj->pos.l.x.v;
        scratch2->pos.y.v = obj->pos.l.y.v - 0x18;
        newObj->mappings = Mappings_Checkpoint;
        newObj->tile = TILE_MAP(0, 0, 0, 0, 0x7A0);
        newObj->render.b = 4;
        newObj->width_pixels = 8;
        newObj->priority = 4;
        newObj->frame = 2;
        scratch2->time = 0x20;
    }

    obj->frame = 1;
    Obj_Checkpoint_StoreInfo(obj,scratch);

    uint8_t respawn = obj->respawn_index;
    objstate[respawn / 2] |= 1;
}

void Obj_Checkpoint_Twirl(Object* obj, Scratch_Checkpoint* scratch) {
    (int16_t)(scratch->time)--;
    if ((int16_t)(scratch->time) < 0) {
        obj->routine = 4; // goto Lamp_Finish next
        return;
    }

    uint8_t angle = obj->angle;
    obj->angle -= 0x10;
    angle -= 0x40;

    int16_t sine, cosine;
    CalcSine(angle, &sine, &cosine);

    obj->pos.l.x.v = scratch->pos.x.v + ((sine * 0xC00) >> 16);
    obj->pos.l.y.v = scratch->pos.y.v + ((cosine * 0xC00) >> 16);
}

void Obj_Checkpoint(Object* obj) {
	Scratch_Checkpoint* scratch = (Scratch_Checkpoint*)&obj->scratch;

	switch (obj->routine) {
		case 0: // constructor
			Obj_Checkpoint_Construct(obj,scratch);
			break;
		case 2: // initial state
			Obj_Checkpoint_Blue(obj,scratch);
			break;
		case 4: // finished state (yes it is supposed to be empty here)
			break;
		case 6: // twirl state
			Obj_Checkpoint_Twirl(obj,scratch);
			break;
		default:
			ObjectDelete(obj);
			break;
	}
	RememberState(obj);
}
