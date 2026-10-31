#define Animals_Build
#include "Animal.h"
#include "Object/Points.h"
#include "ObjectSst.h"
#include "Object.h"
#include <stdio.h>
#include "SstLoad.h"

#include "Resource/Mappings/Animals1.h"
#include "Resource/Mappings/Animals2.h"
#include "Resource/Mappings/Animals3.h"

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
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Flicky),
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Flicky),
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Flicky),
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Rabbit),
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Rabbit),
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Penguin),
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Penguin),
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Seal),
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Pig),
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Chicken),
    TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Squirrel)
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

bool Obj_Animals_CheckInRange(Sst *self) {
    return ((player->x_pos.v - self->x_pos.v - 0xB8) >= 0 && (player->x_pos.v - self->x_pos.v - 0xB8) <= 0x100);
}

void Obj_Animals_UpdateFrame(Sst *self, Own_Animals *own) {
    self->mapping_frame = 1;
    if (self->y_vel >= 0) {
        self->mapping_frame = 0;
        int16_t floordist = SstFloorDist(self,self->x_pos.f.u);
        if (floordist < 0) {
            self->y_pos.v += floordist;
            self->y_vel = own->ysp;
        }
    }
}

void Obj_Animals_UpdateRender(Sst *self) {
    self->render_flags |= 1;
    if (self->x_pos.v <= player->x_pos.v) {
        self->render_flags &= ~1;
    }
}

void Obj_Animals_FlickyJump(Sst *self, Own_Animals *own) {
    if (!Obj_Animals_CheckInRange(self)) {
        self->x_vel = 0;
        own->xsp = 0;
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;
        Obj_Animals_UpdateFrame(self,own);
        Obj_Animals_UpdateRender(self);

        if (--(self->anim_frame_timer) < 0) {
            self->anim_frame_timer = 1;
            self->mapping_frame = (self->mapping_frame + 1) & 1;
        }
    }

    int16_t dx = self->x_pos.v - player->x_pos.v;
    if (dx < 0) {
        dx = -dx;
    }
    if (dx > 0x180 || (self->render_flags & 0x80) == 0) {
        Sst_Delete(self);
    }

    Sst_Draw(self);
}

void Obj_Animals_Walk(Sst *self, const Own_Animals *own) {
    Sst_MoveSprite(self);
    self->mapping_frame = 1;

    if (self->y_vel >= 0) {
        self->mapping_frame = 0;
        int16_t floordist = SstFloorDist(self,self->x_pos.f.u);
        if (floordist <= 0) {
            self->y_pos.v += floordist;
            self->y_vel = own->ysp;
        }
    }

    if (self->subtype == 0) {
        if ((int8_t)(self->render_flags) < 0) {
            Sst_Draw(self);
        } else {
            Sst_Delete(self);
        }
    }
}

void Obj_Animals_Fly(Sst *self, const Own_Animals *own) {
    Sst_MoveSprite2(self);
    self->y_vel += 0x18;

    if (self->y_vel >= 0) {
        int16_t floordist = SstFloorDist(self,self->x_pos.f.u);
        if (floordist < 0) {
            self->y_pos.v += floordist;
            self->y_vel = own->ysp;
            if (self->subtype != 0 && self->subtype != 0xA) {
                self->x_vel = -self->x_vel;
                self->render_flags ^= 1;
            }
        }
    }

    (self->anim_frame_timer)--;
    if ((self->anim_frame_timer) < 0) {
        self->anim_frame_timer = 1;
        self->mapping_frame++;
        self->mapping_frame &= 1;
    }

    if (self->subtype == 0) {
        if ((int8_t)(self->render_flags) >= 0) {
            Sst_Delete(self);
            return;
        }
    } else {
        if ((self->y_pos.v - player->y_pos.v) < 0 || (self->y_pos.v - player->y_pos.v) > 0x180) {
            if ((int8_t)(self->render_flags) >= 0) {
                Sst_Delete(self);
                return;
            }
        }
    }
    Sst_Draw(self);
}

void Obj_Animals_FlickyWait(Sst *self, const Own_Animals *own) {
    if (Obj_Animals_CheckInRange(self)) {
        self->x_vel = own->xsp;
        self->y_vel = own->ysp;
        self->routine = 0xE;
        Obj_Animals_Fly(self,own);
    } else {
        if ((self->x_pos.v - player->x_pos.v) >= 0 || (self->x_pos.v - player->x_pos.v) > -0x180) {
            if (!(self->render_flags & 0x80)) {
                Sst_Delete(self);
                return;
            }
        }
        Sst_Draw(self);
    }
}

void Obj_Animals_FromEnemy(Sst *self, Own_Animals *own) {
    self->routine += 2;
    own->routine = AnimalVarIndex[LEVEL_ZONE(level_id)][RandomNumber() & 1];
    own->xsp = AnimalVariables[own->routine].xsp;
    own->ysp = AnimalVariables[own->routine].ysp;
    self->mappings = AnimalVariables[own->routine].mappings;

    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Animal_1);
    if (own->routine & 1) {
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Animal_2);
    }

    self->x_radius = 0xC;
    self->render_flags = 4;
    self->render_flags |= 1;
    self->priority = 6 * 0x80;
    self->width_pixels = 8;
    self->anim_frame_timer = 7;
    self->mapping_frame = 2;
    self->y_vel = -0x400;

    if (boss_status == 0) {
        Sst* newObj = Sst_SpawnCode(Obj_Points, self->x_pos.f.u, self->y_pos.f.u);
        if (newObj != NULL) {
            newObj->x_pos.v = self->x_pos.v;
            newObj->y_pos.v = self->y_pos.v;
            newObj->mapping_frame = (own->points) >> 1;
        }
    } else {
        self->routine = 0x12;
        self->x_vel = 0;
    }

    Sst_Draw(self);
}

void Obj_Animals_Construct(Sst *self, Own_Animals *own) {
    if (self->subtype == 0) {
        Obj_Animals_FromEnemy(self,own);
        return;
    }

    self->subtype *= 2;
    self->routine = self->subtype;
    self->subtype -= 0x14;

    self->art_tile =     AnimalEndVram[self->subtype / 2];
    self->mappings = AnimalEndMap[self->subtype / 2];
    own->xsp =  AnimalEndSpeed[self->subtype / 2][0];
    self->x_vel =      AnimalEndSpeed[self->subtype / 2][0];
    own->ysp = AnimalEndSpeed[self->subtype / 2][1];
    self->y_vel =     AnimalEndSpeed[self->subtype / 2][1];

    self->x_radius = 0xC;
    self->render_flags = 4;
    self->render_flags |= 1;
    self->priority = 6 * 0x80;
    self->width_pixels = 8;
    self->anim_frame_timer = 7;

    Sst_Draw(self);
}

void Obj_Animals_Main(Sst *self, const Own_Animals *own) {
    if (!(self->render_flags & 0x80)) {
        Sst_Delete(self);
        return;
    }

    Sst_MoveSprite(self);

    if (self->y_vel >= 0) {
        int16_t floorDist = SstFloorDist(self,self->x_pos.f.u);
        if (floorDist < 0) {
            self->y_pos.v += floorDist;
            self->x_vel = own->xsp;
            self->y_vel = own->ysp;
            self->mapping_frame = 1;
            uint8_t routineOffset = own->routine;
            routineOffset = (routineOffset << 1) + 4;
            self->routine = routineOffset;

            if (boss_status != 0 && (vbla_routine & 0x10)) {
                self->x_vel = -self->x_vel;
                self->render_flags ^= 0x01;
            }
        }
    }

    Sst_Draw(self);
}

void Obj_Animals_Prison(Sst *self, Own_Animals *own) {
    if (!(self->render_flags & 0x80)) {
        Sst_Delete(self);
        return;
    }

    own->timer--;
    if (own->timer == 0) {
        self->routine = 2;
        self->priority = 3 * 0x80;
    }

    Sst_Draw(self);
}

void Obj_Animals_RabbitWait(Sst *self, const Own_Animals *own) {
    if (!Obj_Animals_CheckInRange(self)) {
        int16_t dx = self->x_pos.v - player->x_pos.v;
        if (dx >= 0) {
            dx -= 0x180;
            if (dx < 0) {
                if (!(self->render_flags & 0x80)) {
                    Sst_Delete(self);
                    return;
                }
            }
        }
        Sst_Draw(self);
    } else {
        self->x_vel = own->xsp;
        self->y_vel = own->ysp;
        self->routine = 4;
        Obj_Animals_Walk(self,own);
    }
}

void Obj_Animals_LandJump(Sst *self, Own_Animals *own) {
    if (Obj_Animals_CheckInRange(self)) {
        self->x_vel = 0;
        own->xsp = 0;
        Sst_MoveSprite(self);

        self->mapping_frame = 1;
        if (self->y_vel >= 0) {
            self->mapping_frame = 0;
            int floorDist = SstFloorDist(self,self->x_pos.f.u);
            if (floorDist <= 0) {
                self->y_pos.v += floorDist;
                self->y_vel = own->ysp;
            }
        }

        self->render_flags |= 1;
        int dx = self->x_pos.v - player->x_pos.v;
        if (dx < 0) {
            self->render_flags &= ~1;
        }
    }

    int d = self->x_pos.v - player->x_pos.v;
    if (d < 0) {
        d -= 0x180;
        if (d >= 0) {
            if ((self->render_flags & 0x80) == 0) {
                Sst_Delete(self);
                return;
            }
        }
    }

    Sst_Draw(self);
}

void Obj_Animals_SingleBounce(Sst *self, const Own_Animals *own) {
    int floorDist;

    if (Obj_Animals_CheckInRange(self)) {
        Sst_MoveSprite(self);
        self->mapping_frame = 1;

        if (self->y_vel >= 0) {
            self->mapping_frame = 0;
            floorDist = SstFloorDist(self,self->x_pos.f.u);

            if (floorDist <= 0) {
                self->x_vel = -self->x_vel;
                self->render_flags ^= 1;
                self->y_pos.v += floorDist;
                self->y_vel = own->ysp;
            }
        }
    }

    int dx = self->x_pos.v - player->x_pos.v;
    if (dx < 0) {
        dx = -dx;
    }
    dx -= 0x180;
    if (dx <= 0 && !(self->render_flags & 0x80)) {
        Sst_Delete(self);
    }

    Sst_Draw(self);
}

void Obj_Animals_FlyBounce(Sst *self, Own_Animals *own) {
    int floorDist;

    if (Obj_Animals_CheckInRange(self)) {
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;

        if (self->y_vel >= 0) {
            floorDist = SstFloorDist(self,self->x_pos.f.u);
            if (floorDist < 0) {
                own->reverse = ~own->reverse;
                if (own->reverse == 0) {
                    self->x_vel = -self->x_vel;
                    self->render_flags ^= 1;
                }
                self->y_pos.v += floorDist;
                self->y_vel = own->ysp;
            }
        }

        (self->anim_frame_timer)--;
        if ((self->anim_frame_timer) < 0) {
            self->anim_frame_timer = 1;
            self->mapping_frame++;
            self->mapping_frame &= 1;
        }

        int dx = self->x_pos.v - player->x_pos.v;
        if (dx >= 0) {
            dx -= 0x180;
            if (dx < 0 && !(self->render_flags & 0x80)) {
                Sst_Delete(self);
                return;
            }
        }

        Sst_Draw(self);
    }
}

void Obj_Animals_DoubleBounce(Sst *self, Own_Animals *own) {
    Sst_MoveSprite(self);
    self->mapping_frame = 1;

    if (self->y_vel >= 0) {
        self->mapping_frame = 0;
        int16_t floorDist = SstFloorDist(self,self->x_pos.f.u);

        if (floorDist <= 0) {
            own->reverse = !own->reverse;
            if (own->reverse == 0) {
                self->x_vel = -self->x_vel;
                self->render_flags ^= 1;
            }
            self->y_pos.v += floorDist;
            self->y_vel = own->ysp;
        }
    }

    int distToPlayer = self->x_pos.v - player->x_pos.v;
    if (distToPlayer < 0) {
        distToPlayer = -distToPlayer;
    }

    if (distToPlayer > 0x180 || (self->render_flags & 0x80)) {
        Sst_Delete(self);
    } else {
        Sst_Draw(self);
    }
}

void Obj_Animals(Sst *self) {
Own_Animals *own = (Own_Animals *)self->own.u8;

	switch (self->routine) {
		case 0:
			Obj_Animals_Construct(self,own);
			break;
		case 2:
			Obj_Animals_Main(self,own);
			break;
		case 4:
		case 8:
		case 10:
		case 12:
		case 16:
			Obj_Animals_Walk(self,own);
			break;
		case 6:
		case 14:
			Obj_Animals_Fly(self,own);
			break;
		case 18:
			Obj_Animals_Prison(self,own);
			break;
		case 20:
		case 22:
			Obj_Animals_FlickyWait(self,own);
			break;
		case 24:
			Obj_Animals_FlickyJump(self,own);
			break;
		case 26:
			Obj_Animals_RabbitWait(self,own);
			break;
		case 28:
		case 32:
		case 36:
			Obj_Animals_LandJump(self,own);
			break;
		case 30:
		case 34:
			Obj_Animals_SingleBounce(self,own);
			break;
		case 38:
			Obj_Animals_FlyBounce(self,own);
			break;
		case 40:
			Obj_Animals_DoubleBounce(self,own);
			break;
		default:
			Sst_Delete(self);
			break;
	}
}
