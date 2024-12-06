#include "Animal.h"

const uint8_t AnimalVarIndex[][2] = {
	{0, 5},  // Green Hill Zone
	{2, 3},  // Labyrinth Zone
	{6, 3},  // Marble Zone
	{4, 5},  // Star Light Zone
	{4, 1},  // Spring Yard Zone
	{0, 1}   // Scrap Brain Zone
};

const animalvar_t AnimalVariables[] = {
	{-0x200, -0x400, Mappings_Animals1},
	{-0x200, -0x300, Mappings_Animals2},
	{-0x180, -0x300, Mappings_Animals1},
	{-0x140, -0x180, Mappings_Animals2},
	{-0x1C0, -0x300, Mappings_Animals3},
	{-0x300, -0x400, Mappings_Animals2},
	{-0x280, -0x380, Mappings_Animals3}
};

// Animal End VRAM
const uint16_t AnimalEndVram[] = {
    TILE_MAP(0, 0, 0, 0, 0x5A5), // End Flicky
    TILE_MAP(0, 0, 0, 0, 0x5A5), // End Flicky
    TILE_MAP(0, 0, 0, 0, 0x5A5), // End Flicky
    TILE_MAP(0, 0, 0, 0, 0x553), // End Rabbit
    TILE_MAP(0, 0, 0, 0, 0x553), // End Rabbit
    TILE_MAP(0, 0, 0, 0, 0x573), // End Penguin
    TILE_MAP(0, 0, 0, 0, 0x573), // End Penguin
    TILE_MAP(0, 0, 0, 0, 0x585), // End Seal
    TILE_MAP(0, 0, 0, 0, 0x593), // End Pig
    TILE_MAP(0, 0, 0, 0, 0x565), // End Chicken
    TILE_MAP(0, 0, 0, 0, 0x5B3)  // End Squirrel
};

// Animal End Map
const void* AnimalEndMap[] = {
	Mappings_Animals2,
	Mappings_Animals2,
	Mappings_Animals2,
	Mappings_Animals1,
	Mappings_Animals1,
	Mappings_Animals1,
	Mappings_Animals1,
	Mappings_Animals2,
	Mappings_Animals3,
	Mappings_Animals2,
	Mappings_Animals3
};

// Animal End Speed
const int16_t AnimalEndSpeed[][2] = {
	{-0x440, -0x400},
	{-0x440, -0x400},
	{-0x440, -0x400},
	{-0x300, -0x400},
	{-0x300, -0x400},
	{-0x180, -0x300},
	{-0x180, -0x300},
	{-0x140, -0x180},
	{-0x1C0, -0x300},
	{-0x200, -0x300},
	{-0x280, -0x380}
};

bool Obj_Animals_CheckInRange(Object* obj) {
    return ((player->pos.l.x.v - obj->pos.l.x.v - 0xB8) >= 0 && (player->pos.l.x.v - obj->pos.l.x.v - 0xB8) <= 0x100);
}

void Obj_Animals_UpdateFrame(Object *obj, Scratch_Animals* scratch) {
    obj->frame = 1;
    if (obj->ysp >= 0) {
        obj->frame = 0;
        int16_t floordist = ObjFloorDist(obj,obj->pos.l.x.f.u);
        if (floordist < 0) {
            obj->pos.l.y.v += floordist;
            obj->ysp = scratch->ysp;
        }
    }
}

void Obj_Animals_UpdateRender(Object *obj) {
    obj->render.b |= 1;
    if (obj->pos.l.x.v <= player->pos.l.x.v) {
        obj->render.b &= ~1;
    }
}

void Obj_Animals_FlickyJump(Object *obj, Scratch_Animals* scratch) {
    if (!Obj_Animals_CheckInRange(obj)) {
        obj->xsp = 0;
        scratch->xsp = 0;
        SpeedToPos(obj);
        obj->ysp += 0x18;
        Obj_Animals_UpdateFrame(obj,scratch);
        Obj_Animals_UpdateRender(obj);

        if (--(obj->frame_time.b) < 0) {
            obj->frame_time.b = 1;
            obj->frame = (obj->frame + 1) & 1;
        }
    }

    int16_t dx = obj->pos.l.x.v - player->pos.l.x.v;
    if (dx < 0) {
        dx = -dx;
    }
    if (dx > 0x180 || (obj->render.b & 0x80) == 0) {
        ObjectDelete(obj);
    }

    DisplaySprite(obj);
}

void Obj_Animals_Walk(Object* obj, Scratch_Animals* scratch) {
    ObjectFall(obj);
    obj->frame = 1;

    if (obj->ysp >= 0) {
        obj->frame = 0;
        int16_t floordist = ObjFloorDist(obj,obj->pos.l.x.f.u);
        if (floordist <= 0) {
            obj->pos.l.y.v += floordist;
            obj->ysp = scratch->ysp;
        }
    }

    if (scratch->subtype == 0) {
        if ((int8_t)(obj->render.b) < 0) {
            DisplaySprite(obj);
        } else {
            ObjectDelete(obj);
        }
    }
}

void Obj_Animals_Fly(Object* obj, Scratch_Animals* scratch) {
    SpeedToPos(obj);
    obj->ysp += 0x18;

    if (obj->ysp >= 0) {
        int16_t floordist = ObjFloorDist(obj,obj->pos.l.x.f.u);
        if (floordist < 0) {
            obj->pos.l.y.v += floordist;
            obj->ysp = scratch->ysp;
            if (scratch->subtype != 0 && scratch->subtype != 0xA) {
                obj->xsp = -obj->xsp;
                obj->render.b ^= 1;
            }
        }
    }

    (obj->frame_time.b)--;
    if ((obj->frame_time.b) < 0) {
        obj->frame_time.b = 1;
        obj->frame++;
        obj->frame &= 1;
    }

    if (scratch->subtype == 0) {
        if ((int8_t)(obj->render.b) >= 0) {
            ObjectDelete(obj);
            return;
        }
    } else {
        if ((obj->pos.l.y.v - player->pos.l.y.v) < 0 || (obj->pos.l.y.v - player->pos.l.y.v) > 0x180) {
            if ((int8_t)(obj->render.b) >= 0) {
                ObjectDelete(obj);
                return;
            }
        }
    }
    DisplaySprite(obj);
}

void Obj_Animals_FlickyWait(Object* obj, Scratch_Animals* scratch) {
    if (Obj_Animals_CheckInRange(obj)) {
        obj->xsp = scratch->xsp;
        obj->ysp = scratch->ysp;
        obj->routine = 0xE;
        Obj_Animals_Fly(obj,scratch);
    } else {
        if ((obj->pos.l.x.v - player->pos.l.x.v) >= 0 || (obj->pos.l.x.v - player->pos.l.x.v) > -0x180) {
            if (!(obj->render.b & 0x80)) {
                ObjectDelete(obj);
                return;
            }
        }
        DisplaySprite(obj);
    }
}

void Obj_Animals_FromEnemy(Object* obj, Scratch_Animals* scratch) {
    obj->routine += 2;

    scratch->routine = AnimalVarIndex[level_id][RandomNumber() & 1];

    scratch->xsp = AnimalVariables[scratch->routine].xsp;
    scratch->ysp = AnimalVariables[scratch->routine].ysp;
    obj->mappings = AnimalVariables[scratch->routine].mappings;

    obj->tile = TILE_MAP(0, 0, 0, 0, 0x580);
    if (scratch->routine & 1) {
        obj->tile = TILE_MAP(0, 0, 0, 0, 0x592);
    }

    obj->x_rad = 0xC;
    obj->render.b = 4;
    obj->render.b |= 1;
    obj->priority = 6;
    obj->width_pixels = 8;
    obj->frame_time.b = 7;
    obj->frame = 2;
    obj->ysp = -0x400;

    if (boss_status == 0) {
        Object* newObj = FindFreeObj();
        if (newObj != NULL) {
            newObj->type = ObjId_Points;
            newObj->pos.l.x.v = obj->pos.l.x.v;
            newObj->pos.l.y.v = obj->pos.l.y.v;
            newObj->frame = (scratch->points) >> 1;
        }
    } else {
        obj->routine = 0x12;
        obj->xsp = 0;
    }

    DisplaySprite(obj);
}

void Obj_Animals_Construct(Object* obj, Scratch_Animals* scratch) {
    if (scratch->subtype == 0) {
        Obj_Animals_FromEnemy(obj,scratch);
        return;
    }

    scratch->subtype *= 2;
    obj->routine = scratch->subtype;
    scratch->subtype -= 0x14;

    obj->tile =     AnimalEndVram[scratch->subtype / 2];
    obj->mappings = AnimalEndMap[scratch->subtype / 2];
    scratch->xsp =  AnimalEndSpeed[scratch->subtype / 2][0];
    obj->xsp =      AnimalEndSpeed[scratch->subtype / 2][0];
    scratch->ysp = AnimalEndSpeed[scratch->subtype / 2][1];
    obj->ysp =     AnimalEndSpeed[scratch->subtype / 2][1];

    obj->x_rad = 0xC;
    obj->render.b = 4;
    obj->render.b |= 1;
    obj->priority = 6;
    obj->width_pixels = 8;
    obj->frame_time.b = 7;

    DisplaySprite(obj);
}

void Obj_Animals_Main(Object* obj, Scratch_Animals* scratch) {
    if (!(obj->render.b & 0x80)) {
        ObjectDelete(obj);
        return;
    }

    ObjectFall(obj);

    if (obj->ysp >= 0) {
        int16_t floorDist = ObjFloorDist(obj,obj->pos.l.x.f.u);
        if (floorDist < 0) {
            obj->pos.l.y.v += floorDist;
            obj->xsp = scratch->xsp;
            obj->ysp = scratch->ysp;
            obj->frame = 1;
            uint8_t routineOffset = scratch->routine;
            routineOffset = (routineOffset << 1) + 4;
            obj->routine = routineOffset;

            if (boss_status != 0 && (vbla_routine & 0x10)) {
                obj->xsp = -obj->xsp;
                obj->render.b ^= 0x01;
            }
        }
    }

    DisplaySprite(obj);
}

void Obj_Animals_Prison(Object* obj, Scratch_Animals* scratch) {
    if (!(obj->render.b & 0x80)) {
        ObjectDelete(obj);
        return;
    }

    scratch->timer--;
    if (scratch->timer == 0) {
        obj->routine = 2;
        obj->priority = 3;
    }

    DisplaySprite(obj);
}

void Obj_Animals_RabbitWait(Object* obj, Scratch_Animals* scratch) {
    if (!Obj_Animals_CheckInRange(obj)) {
        int16_t dx = obj->pos.l.x.v - player->pos.l.x.v;
        if (dx >= 0) {
            dx -= 0x180;
            if (dx < 0) {
                if (!(obj->render.b & 0x80)) {
                    ObjectDelete(obj);
                    return;
                }
            }
        }
        DisplaySprite(obj);
    } else {
        obj->xsp = scratch->xsp;
        obj->ysp = scratch->ysp;
        obj->routine = 4;
        Obj_Animals_Walk(obj,scratch);
    }
}

void Obj_Animals_LandJump(Object *obj, Scratch_Animals* scratch) {
    if (Obj_Animals_CheckInRange(obj)) {
        obj->xsp = 0;
        scratch->xsp = 0;
        ObjectFall(obj);

        obj->frame = 1;
        if (obj->ysp >= 0) {
            obj->frame = 0;
            int floorDist = ObjFloorDist(obj,obj->pos.l.x.f.u);
            if (floorDist <= 0) {
                obj->pos.l.y.v += floorDist;
                obj->ysp = scratch->ysp;
            }
        }

        obj->render.b |= 1;
        int dx = obj->pos.l.x.v - player->pos.l.x.v;
        if (dx < 0) {
            obj->render.b &= ~1;
        }
    }

    int d = obj->pos.l.x.v - player->pos.l.x.v;
    if (d < 0) {
        d -= 0x180;
        if (d >= 0) {
            if ((obj->render.b & 0x80) == 0) {
                ObjectDelete(obj);
                return;
            }
        }
    }

    DisplaySprite(obj);
}

void Obj_Animals_SingleBounce(Object *obj, Scratch_Animals* scratch) {
    int floorDist;

    if (Obj_Animals_CheckInRange(obj)) {
        ObjectFall(obj);
        obj->frame = 1;

        if (obj->ysp >= 0) {
            obj->frame = 0;
            floorDist = ObjFloorDist(obj,obj->pos.l.x.f.u);

            if (floorDist <= 0) {
                obj->xsp = -obj->xsp;
                obj->render.b ^= 1;
                obj->pos.l.y.v += floorDist;
                obj->ysp = scratch->ysp;
            }
        }
    }

    int dx = obj->pos.l.x.v - player->pos.l.x.v;
    if (dx < 0) {
        dx = -dx;
    }
    dx -= 0x180;
    if (dx <= 0 && !(obj->render.b & 0x80)) {
        ObjectDelete(obj);
    }

    DisplaySprite(obj);
}

void Obj_Animals_FlyBounce(Object *obj, Scratch_Animals* scratch) {
    int floorDist;

    if (Obj_Animals_CheckInRange(obj)) {
        SpeedToPos(obj);
        obj->ysp += 0x18;

        if (obj->ysp >= 0) {
            floorDist = ObjFloorDist(obj,obj->pos.l.x.f.u);
            if (floorDist < 0) {
                scratch->reverse = ~scratch->reverse;
                if (scratch->reverse == 0) {
                    obj->xsp = -obj->xsp;
                    obj->render.b ^= 1;
                }
                obj->pos.l.y.v += floorDist;
                obj->ysp = scratch->ysp;
            }
        }

        (obj->frame_time.b)--;
        if ((obj->frame_time.b) < 0) {
            obj->frame_time.b = 1;
            obj->frame++;
            obj->frame &= 1;
        }

        int dx = obj->pos.l.x.v - player->pos.l.x.v;
        if (dx >= 0) {
            dx -= 0x180;
            if (dx < 0 && !(obj->render.b & 0x80)) {
                ObjectDelete(obj);
                return;
            }
        }

        DisplaySprite(obj);
    }
}

void Obj_Animals_DoubleBounce(Object* obj, Scratch_Animals* scratch) {
    ObjectFall(obj);
    obj->frame = 1;

    if (obj->ysp >= 0) {
        obj->frame = 0;
        int16_t floorDist = ObjFloorDist(obj,obj->pos.l.x.f.u);

        if (floorDist <= 0) {
            scratch->reverse = !scratch->reverse;
            if (scratch->reverse == 0) {
                obj->xsp = -obj->xsp;
                obj->render.b ^= 1;
            }
            obj->pos.l.y.v += floorDist;
            obj->ysp = scratch->ysp;
        }
    }

    int distToPlayer = obj->pos.l.x.v - player->pos.l.x.v;
    if (distToPlayer < 0) {
        distToPlayer = -distToPlayer;
    }

    if (distToPlayer > 0x180 || (obj->render.b & 0x80)) {
        ObjectDelete(obj);
    } else {
        DisplaySprite(obj);
    }
}

void Obj_Animals(Object* obj) {
Scratch_Animals* scratch = (Scratch_Animals*)&obj->scratch;

	switch (obj->routine) {
		case 0:
			Obj_Animals_Construct(obj,scratch);
			break;
		case 2:
			Obj_Animals_Main(obj,scratch);
			break;
		case 4:
		case 8:
		case 10:
		case 12:
		case 16:
			Obj_Animals_Walk(obj,scratch);
			break;
		case 6:
		case 14:
			Obj_Animals_Fly(obj,scratch);
			break;
		case 18:
			Obj_Animals_Prison(obj,scratch);
			break;
		case 20:
		case 22:
			Obj_Animals_FlickyWait(obj,scratch);
			break;
		case 24:
			Obj_Animals_FlickyJump(obj,scratch);
			break;
		case 26:
			Obj_Animals_RabbitWait(obj,scratch);
			break;
		case 28:
		case 32:
		case 36:
			Obj_Animals_LandJump(obj,scratch);
			break;
		case 30:
		case 34:
			Obj_Animals_SingleBounce(obj,scratch);
			break;
		case 38:
			Obj_Animals_FlyBounce(obj,scratch);
			break;
		case 40:
			Obj_Animals_DoubleBounce(obj,scratch);
			break;
		default:
			ObjectDelete(obj);
			break;
	}
}
