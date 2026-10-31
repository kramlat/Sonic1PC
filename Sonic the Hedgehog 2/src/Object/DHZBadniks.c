// Dust Hill's badniks for Sonic 2, as the alpha has them (objects 9E and A3: the prototype has neither): the Crawlton, a caterpillar that turns to Sonic when he is near and crawls at him and back (its head is the
// object, its body an object of its own that follows), and the Flasher, a firefly that drifts about, and flashes brighter and brighter until it is dangerous to touch.
#include "Object/DHZBadniks.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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
} Own_Crawlton;

enum { Crawl_Init = 0, Crawl_Wait = 2, Crawl_Gather = 4, Crawl_Go = 6, Crawl_Rest = 8, Crawl_Body = 0xA };

static void Crawlton_MakeBody(Sst *head) {
    Sst *body = Sst_AllocateAfter(head);
    if (body == NULL)
        return;
    body->code = head->code;
    body->render_flags = head->render_flags | 0x40; // (the sub-sprites)
    body->mappings = head->mappings;
    body->art_tile = head->art_tile;
    body->width_pixels = head->width_pixels;
    body->mapping_frame = 0; // (no sprite of its own)
    body->child_count = 7;
    body->priority = 5 * 0x80;
    ((Own_Crawlton *)body->own.u8)->state = Crawl_Body;
    ((Own_Crawlton *)body->own.u8)->link = (uint8_t)(head - sst);
    body->x_pos.f.u = head->x_pos.f.u;
    body->y_pos.f.u = head->y_pos.f.u;
    for (int i = 0; i < 7; i++) {
        body->children[i].x = head->x_pos.f.u;
        body->children[i].y = head->y_pos.f.u;
        body->children[i].frame = 2;
    }
}

void Obj_Crawlton(Sst *self) {
    Own_Crawlton *own = (Own_Crawlton *)self->own.u8;

    switch (own->state) {
    case Crawl_Init:
        self->mappings = Mappings_Crawlton;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0x3C0); // ($7800)
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x80;
        self->collision_flags = 0x0B;
        own->state += 2;
        Crawlton_MakeBody(self);
        break;

    case Crawl_Wait: { // waits until Sonic is within $80 pixels each way: then turns to him (and takes the speed towards him: a quarter of how far, a frame)
        const int16_t dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
        const int16_t dy = (int16_t)(self->y_pos.f.u - player->y_pos.f.u);
        if ((uint16_t)(dx + 0x80) < 0x100 && (uint16_t)(dy + 0x80) < 0x100) {
            own->state += 2;
            own->counter = 0x10;
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, (uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u);
            self->x_vel = (int16_t)(((-dx) << 3) & 0xFF00);
            self->y_vel = (int16_t)(((-dy) << 3) & 0xFF00);
        }
        SstOnScreenTest(self);
        break;
    }
    case Crawl_Gather:
        if ((int8_t)--own->counter < 0) {
            own->state += 2;
            own->return_state = Crawl_Rest;
            own->counter = 0x1C;
        }
        SstOnScreenTest(self);
        break;
    case Crawl_Go:
        if (--own->counter == 0) {
            own->state = own->return_state;
            own->counter = 0x20;
        } else {
            Sst_MoveSprite2(self);
        }
        SstOnScreenTest(self);
        break;
    case Crawl_Rest:
        if (--own->counter == 0) { // and back
            own->state = Crawl_Go;
            own->return_state = Crawl_Wait;
            own->counter = 0x1C;
            self->x_vel = (int16_t)-self->x_vel;
            self->y_vel = (int16_t)-self->y_vel;
        }
        SstOnScreenTest(self);
        break;

    case Crawl_Body: {
        Sst *head = &sst[own->link];
        if (head->code != self->code) {
            Sst_Delete(self);
            return;
        }
        const Own_Crawlton *head_state = (const Own_Crawlton *)head->own.u8;
        if (head_state->state == Crawl_Go) { // the pieces of the body follow, one after another as the head's crawl goes on
            const int16_t dx = (int16_t)(head->x_vel >> 8), dy = (int16_t)(head->y_vel >> 8);
            int threshold = 0x18;
            for (int i = 0; i < 7; i++) {
                if (head_state->counter < threshold) {
                    self->children[i].x = (int16_t)(self->children[i].x + dx);
                    self->children[i].y = (int16_t)(self->children[i].y + dy);
                }
                threshold -= 4;
                if (threshold < 0)
                    break;
            }
        }
        Sst_Draw(self);
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
} Own_Flasher;

enum { Flasher_Init = 0, Flasher_Rest = 2, Flasher_Drift = 4, Flasher_Flash = 6, Flasher_Glow = 8, Flasher_Fade = 0xA, Flasher_Again = 0xC };

// Offset_0x029D06 and Offset_0x029D16: when it turns (the frames it has drifted), and then whether its sideways acceleration and its vertical speed turn
static const uint16_t flasher_turns[8] = { 0x0100, 0x01A0, 0x0208, 0x0285, 0x0300, 0x0340, 0x0390, 0x0440 };
static const uint8_t flasher_flips[9][2] = { { 0xF0, 0x00 }, { 0x01, 0x01 }, { 0x00, 0x01 }, { 0x01, 0x01 }, { 0x00, 0x01 }, { 0x00, 0x01 }, { 0x01, 0x00 }, { 0x00, 0x01 }, { 0x00, 0x01 } };

// The alpha's AnimateSprite, for the end command $FC: it moves the routine on, and also moves the frame on and clears the time to the next (Sonic 1's, which the engine has, does neither: nothing there goes on animating after it).
// The Flasher's three animations all share animation 0, so the place in the script carries over from one to the next, as it did in the alpha: its flashing ends (at its $20th byte) just short of where the alert's
// script has the byte that restarts it, which is what runs it. (Its fade ends at the 7th byte, so that the flashing that follows begins in the middle of its script: after the first time)
static void Flasher_Animate(Sst *self, const uint8_t *script) {
    const uint8_t routine = self->routine;
    Sst_Animate(self, script);
    if (self->routine != routine) {
        self->anim_frame++;
        self->anim_frame_timer = 0;
        if (routine == Flasher_Flash)
            self->anim_frame = 0; // (the alpha reads the byte after the alert's script, which is $FF, and starts it again)
    }
}

void Obj_Flasher(Sst *self) {
    Own_Flasher *own = (Own_Flasher *)self->own.u8;

    switch (self->routine) {
    case Flasher_Init:
        self->mappings = Mappings_Flasher;
        self->art_tile = TILE_MAP(1, 0, 0, 0, 0x3A8); // ($7500)
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0x06;
        self->routine += 2;
        own->timer = 0x40;
        break;

    case Flasher_Rest:
        if (--own->timer < 0) {
            self->routine += 2;
            self->x_vel = (int16_t)0xFF00;
            self->y_vel = 0x40;
            own->accel = 2;
            own->timer = 0;
            own->wait = 0x80;
        }
        SstOnScreenTest(self);
        break;

    case Flasher_Drift:
        if (--own->wait < 0) { // it has drifted enough: it stops to flash
            self->routine += 2;
            own->wait = 0x80;
            self->collision_flags |= 0x80;
            SstOnScreenTest(self);
            break;
        }
        if (own->timer < 0) {
            Sst_Delete(self);
            break;
        }
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, false);
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
        if (self->x_vel >= 0) {
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
            SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
        }
        own->timer++;
        if ((uint16_t)own->timer >= flasher_turns[(own->index >> 1) & 7] && own->index < 0x10) {
            own->index += 2;
            const uint8_t *flip = flasher_flips[own->index >> 1];
            if (flip[0])
                own->accel = (int16_t)-own->accel;
            if (flip[1])
                self->y_vel = (int16_t)-self->y_vel;
        }
        self->x_vel = (int16_t)(self->x_vel + own->accel);
        Sst_MoveSprite2(self);
        SstOnScreenTest(self);
        break;

    case Flasher_Flash:
        Flasher_Animate(self, Animation_Flasher);
        SstOnScreenTest(self);
        break;
    case Flasher_Glow:
        if (--own->wait < 0) {
            self->routine += 2;
            SstOnScreenTest(self);
            break;
        }
        self->anim = 0;
        Flasher_Animate(self, Animation_FlasherAlert);
        SstOnScreenTest(self);
        break;
    case Flasher_Fade:
        Flasher_Animate(self, Animation_FlasherFade);
        SstOnScreenTest(self);
        break;
    case Flasher_Again:
        self->routine = Flasher_Drift;
        own->wait = 0x80;
        self->collision_flags &= 0x7F;
        SstOnScreenTest(self);
        break;
    }
}
