// Dust Hill's badniks for Sonic 2, as the alpha has them (objects 9E and A3: the prototype has neither): the Crawlton, a caterpillar that turns to Sonic when he is near and crawls at him and back (its head is the
// object, its body an object of its own that follows), and the Flasher, a firefly that drifts about, and flashes brighter and brighter until it is dangerous to touch.
#include "Object/DHZBadniks.h"
#include "Constants.h"

#include "Level.h"
#include "Macros.h"

#include "Resource/Animation/Flasher.h"
#include "Resource/Animation/FlasherAlert.h"
#include "Resource/Animation/FlasherFade.h"
#include "Resource/Mappings/Crawlton.h"
#include "Resource/Mappings/Flasher.h"

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Crawlton (9E)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0[0x10];  // 0x29-0x38
    uint8_t return_state;// 0x39: the state it goes to when it has crawled
    uint8_t counter;     // 0x3A
    uint8_t state;       // 0x3B
    uint8_t pad1[2];     // 0x3C-0x3D
    uint8_t link;        // 0x3E: (the body's) the slot of the head
} Scratch_Crawlton;

enum { Crawl_Init = 0, Crawl_Wait = 2, Crawl_Gather = 4, Crawl_Go = 6, Crawl_Rest = 8, Crawl_Body = 0xA };

static void Crawlton_MakeBody(Object *head) {
    Object *body = FindNextFreeObj(head + 1);
    if (body == NULL)
        return;
    body->type = head->type;
    body->render.b = head->render.b | 0x40; // (the sub-sprites)
    body->mappings = head->mappings;
    body->tile = head->tile;
    body->width_pixels = head->width_pixels;
    body->frame = 0; // (no sprite of its own)
    body->child_count = 7;
    body->priority = 5;
    ((Scratch_Crawlton *)&body->scratch)->state = Crawl_Body;
    ((Scratch_Crawlton *)&body->scratch)->link = (uint8_t)(head - objects);
    body->pos.l.x.f.u = head->pos.l.x.f.u;
    body->pos.l.y.f.u = head->pos.l.y.f.u;
    for (int i = 0; i < 7; i++) {
        body->children[i].x = head->pos.l.x.f.u;
        body->children[i].y = head->pos.l.y.f.u;
        body->children[i].frame = 2;
    }
}

void Obj_Crawlton(Object *obj) {
    Scratch_Crawlton *scratch = (Scratch_Crawlton *)&obj->scratch;

    switch (scratch->state) {
    case Crawl_Init:
        obj->mappings = Mappings_Crawlton;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x3C0); // ($7800)
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->width_pixels = 0x80;
        obj->col_type = 0x0B;
        scratch->state += 2;
        Crawlton_MakeBody(obj);
        break;

    case Crawl_Wait: { // waits until Sonic is within $80 pixels each way: then turns to him (and takes the speed towards him: a quarter of how far, a frame)
        const int16_t dx = (int16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u);
        const int16_t dy = (int16_t)(obj->pos.l.y.f.u - player->pos.l.y.f.u);
        if ((uint16_t)(dx + 0x80) < 0x100 && (uint16_t)(dy + 0x80) < 0x100) {
            scratch->state += 2;
            scratch->counter = 0x10;
            obj->render.f.x_flip = (uint16_t)obj->pos.l.x.f.u < (uint16_t)player->pos.l.x.f.u;
            obj->xsp = (int16_t)(((-dx) << 3) & 0xFF00);
            obj->ysp = (int16_t)(((-dy) << 3) & 0xFF00);
        }
        RememberState(obj);
        break;
    }
    case Crawl_Gather:
        if ((int8_t)--scratch->counter < 0) {
            scratch->state += 2;
            scratch->return_state = Crawl_Rest;
            scratch->counter = 0x1C;
        }
        RememberState(obj);
        break;
    case Crawl_Go:
        if (--scratch->counter == 0) {
            scratch->state = scratch->return_state;
            scratch->counter = 0x20;
        } else {
            SpeedToPos(obj);
        }
        RememberState(obj);
        break;
    case Crawl_Rest:
        if (--scratch->counter == 0) { // and back
            scratch->state = Crawl_Go;
            scratch->return_state = Crawl_Wait;
            scratch->counter = 0x1C;
            obj->xsp = (int16_t)-obj->xsp;
            obj->ysp = (int16_t)-obj->ysp;
        }
        RememberState(obj);
        break;

    case Crawl_Body: {
        Object *head = &objects[scratch->link];
        if (head->type != obj->type) {
            ObjectDelete(obj);
            return;
        }
        const Scratch_Crawlton *head_state = (const Scratch_Crawlton *)&head->scratch;
        if (head_state->state == Crawl_Go) { // the pieces of the body follow, one after another as the head's crawl goes on
            const int16_t dx = (int16_t)(head->xsp >> 8), dy = (int16_t)(head->ysp >> 8);
            int threshold = 0x18;
            for (int i = 0; i < 7; i++) {
                if (head_state->counter < threshold) {
                    obj->children[i].x = (int16_t)(obj->children[i].x + dx);
                    obj->children[i].y = (int16_t)(obj->children[i].y + dy);
                }
                threshold -= 4;
                if (threshold < 0)
                    break;
            }
        }
        DisplaySprite(obj);
        break;
    }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Flasher (A3)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0;        // 0x29
    int16_t timer;       // 0x2A
    int16_t index;       // 0x2C: how far through its wandering
    int16_t accel;       // 0x2E: its sideways acceleration
    int16_t wait;        // 0x30
} Scratch_Flasher;

enum { Flasher_Init = 0, Flasher_Rest = 2, Flasher_Drift = 4, Flasher_Flash = 6, Flasher_Glow = 8, Flasher_Fade = 0xA, Flasher_Again = 0xC };

// Offset_0x029D06 and Offset_0x029D16: when it turns (the frames it has drifted), and then whether its sideways acceleration and its vertical speed turn
static const uint16_t flasher_turns[8] = { 0x0100, 0x01A0, 0x0208, 0x0285, 0x0300, 0x0340, 0x0390, 0x0440 };
static const uint8_t flasher_flips[9][2] = { { 0xF0, 0x00 }, { 0x01, 0x01 }, { 0x00, 0x01 }, { 0x01, 0x01 }, { 0x00, 0x01 }, { 0x00, 0x01 }, { 0x01, 0x00 }, { 0x00, 0x01 }, { 0x00, 0x01 } };

// The alpha's AnimateSprite, for the end command $FC: it moves the routine on, and also moves the frame on and clears the time to the next (Sonic 1's, which the engine has, does neither: nothing there goes on animating after it).
// The Flasher's three animations all share animation 0, so the place in the script carries over from one to the next, as it did in the alpha: its flashing ends (at its $20th byte) just short of where the alert's
// script has the byte that restarts it, which is what runs it. (Its fade ends at the 7th byte, so that the flashing that follows begins in the middle of its script: after the first time)
static void Flasher_Animate(Object *obj, const uint8_t *script) {
    const uint8_t routine = obj->routine;
    AnimateSprite(obj, script);
    if (obj->routine != routine) {
        obj->anim_frame++;
        obj->frame_time.b = 0;
        if (routine == Flasher_Flash)
            obj->anim_frame = 0; // (the alpha reads the byte after the alert's script, which is $FF, and starts it again)
    }
}

void Obj_Flasher(Object *obj) {
    Scratch_Flasher *scratch = (Scratch_Flasher *)&obj->scratch;

    switch (obj->routine) {
    case Flasher_Init:
        obj->mappings = Mappings_Flasher;
        obj->tile = TILE_MAP(1, 0, 0, 0, 0x3A8); // ($7500)
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->width_pixels = 0x10;
        obj->col_type = 0x06;
        obj->routine += 2;
        scratch->timer = 0x40;
        break;

    case Flasher_Rest:
        if (--scratch->timer < 0) {
            obj->routine += 2;
            obj->xsp = (int16_t)0xFF00;
            obj->ysp = 0x40;
            scratch->accel = 2;
            scratch->timer = 0;
            scratch->wait = 0x80;
        }
        RememberState(obj);
        break;

    case Flasher_Drift:
        if (--scratch->wait < 0) { // it has drifted enough: it stops to flash
            obj->routine += 2;
            scratch->wait = 0x80;
            obj->col_type |= 0x80;
            RememberState(obj);
            break;
        }
        if (scratch->timer < 0) {
            ObjectDelete(obj);
            break;
        }
        obj->render.f.x_flip = false;
        obj->status.o.f.x_flip = false;
        if (obj->xsp >= 0) {
            obj->render.f.x_flip = true;
            obj->status.o.f.x_flip = true;
        }
        scratch->timer++;
        if ((uint16_t)scratch->timer >= flasher_turns[(scratch->index >> 1) & 7] && scratch->index < 0x10) {
            scratch->index += 2;
            const uint8_t *flip = flasher_flips[scratch->index >> 1];
            if (flip[0])
                scratch->accel = (int16_t)-scratch->accel;
            if (flip[1])
                obj->ysp = (int16_t)-obj->ysp;
        }
        obj->xsp = (int16_t)(obj->xsp + scratch->accel);
        SpeedToPos(obj);
        RememberState(obj);
        break;

    case Flasher_Flash:
        Flasher_Animate(obj, Animation_Flasher);
        RememberState(obj);
        break;
    case Flasher_Glow:
        if (--scratch->wait < 0) {
            obj->routine += 2;
            RememberState(obj);
            break;
        }
        obj->anim = 0;
        Flasher_Animate(obj, Animation_FlasherAlert);
        RememberState(obj);
        break;
    case Flasher_Fade:
        Flasher_Animate(obj, Animation_FlasherFade);
        RememberState(obj);
        break;
    case Flasher_Again:
        obj->routine = Flasher_Drift;
        scratch->wait = 0x80;
        obj->col_type &= 0x7F;
        RememberState(obj);
        break;
    }
}
