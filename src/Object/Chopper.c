#include "Chopper.h"

// Helper function for initialization
void Obj_Chopper_Construct(Object* obj, Scratch_Chopper* scratch) {
    obj->mappings = Mappings_Chopper;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x47B);
    obj->render.b = 0;
    obj->render.f.align_fg = true; obj->priority = 4;
    obj->col_type = 0x09;
    obj->width_pixels = 16;
    obj->ysp = -0x700;
    scratch->orig_y = obj->pos.l.y.f.u;
}

// Helper function for movement logic
void Obj_Chopper_Move(Object* obj, Scratch_Chopper* scratch) {
    SpeedToPos(obj);
    obj->ysp += 0x18;
    Obj_Chopper_CheckAndResetPosition(obj, scratch);
}

// Helper function for animation
void Obj_Chopper_Animate(Object* obj, Scratch_Chopper* scratch) {
    obj->anim = 1;
    if ((scratch->orig_y - 0xC0) < obj->pos.l.y.f.u) {
        obj->anim = 0;
        if (obj->ysp >= 0)
            obj->anim = 2;
    }
    AnimateSprite(obj, Animation_Chopper);
}

// Helper function to check position and reset if necessary
void Obj_Chopper_CheckAndResetPosition(Object* obj, Scratch_Chopper* scratch) {
    if (scratch->orig_y < obj->pos.l.y.f.u) {
        obj->pos.l.y.f.u = scratch->orig_y;
        obj->ysp = -0x700;
    }
}

// Main Chopper function using helpers
void Obj_Chopper(Object* obj) {
    Scratch_Chopper* scratch = (Scratch_Chopper*)&obj->scratch;
    switch (obj->routine) {
        case 0: // Initialization
            obj->routine += 2;
            Obj_Chopper_Construct(obj, scratch);

            // Fallthrough to case 2
        case 2: // Moving
            Obj_Chopper_Move(obj, scratch);
             Obj_Chopper_Animate(obj, scratch);
             break;
    }
    // Draw and unload once off-screen
    RememberState(obj);
}
