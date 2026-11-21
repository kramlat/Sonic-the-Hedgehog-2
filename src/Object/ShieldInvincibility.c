// The shield and the invincibility stars for Sonic 2 (Nick Arcade's object 38): Sonic 1's with Nick Arcade's changes. The shield is wider and Emerald Hill's art is at its own place; the stars are unfinished:
// each one is Sonic's own mapping frame (not a star's) drawn where Nick Arcade's star trail buffer says, and that buffer is never written (the routine meant to fill it is never called), so the stars sit at the
// level's top left corner and, but for the invincibility itself, nothing shows.
#include "Object/ShieldInvincibility.h"
#include "Constants.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Resource/Animation/ShieldInvincibility.h"
#include "Resource/Mappings/ShieldInvincibility.h"
extern const uint8_t Mappings_Sonic[];

#include "Level.h"
#include "Object/Sonic.h"
#include "Object/InvincibilityStars.h"
#include "Object/Tails.h"
#include "VersusStats.h"

static Sst *Owner(const Sst *self) { // (the character it goes with: status_tertiary 1 is Tails)
    return self->status_tertiary == 1 ? TAILS_OBJ : player;
}

void ShieldInvincibility_MakeFor(uint8_t anim, const Sst *owner) {
    const uint8_t who = (owner == TAILS_OBJ) ? 1 : 0;
    if (anim > 1) // (the pre-beta's stars are one object: the monitor asks for four of Nick Arcade's, the others are not made)
        return;
    SstCode code = anim == 1 ? Obj_InvincibilityStars : Obj_ShieldInvincibility;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == code && sst[i].status_tertiary == who && (anim == 1 || sst[i].anim == anim))
            return;
    Sst *self = Sst_SpawnCode(code, 0, 0);
    if (self != NULL) {
        self->status_tertiary = who;
        if (anim == 0)
            self->anim = anim;
    }
}

void ShieldInvincibility_Make(uint8_t anim) {
    ShieldInvincibility_MakeFor(anim, player);
}

void Obj_ShieldInvincibility(Sst *self) {
    switch (self->routine) {
    case 0: // Initialiation
        // Increment routine
        self->routine += 2;

        // Set object drawing information
        self->mappings = Mappings_ShieldInvincibility;
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->width_pixels = 0x18;

        // Check if invincibility or shield
        if (!self->anim) {
            // Shield
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Shield);
        } else {
            // Invincibility: Sonic's own mappings, with the stars' tiles
            self->routine += 2;
            self->mappings = Mappings_Sonic;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Invincibility);
            self->priority = 2 * 0x80;
        }
        break;
    case 2: // Shield
        // Check if shield should exist
        const Sst *owner = Owner(self); // (Beta 4: it goes with its own character and his own flag)
        if (VsStats_Has(owner, VS_INVINCIBLE))
            break;
        if (!VsStats_Has(owner, VS_SHIELD)) {
            Sst_Delete(self);
            break;
        }

        // Update shield position
        self->x_pos.f.u = owner->x_pos.f.u;
        self->y_pos.f.u = owner->y_pos.f.u;
        self->status = owner->status;
        // The pre-beta's: it is in front of the level's tiles when he is
        self->art_tile = (uint16_t)((self->art_tile & 0x7FFF) | (owner->art_tile & 0x8000));

        // Animate and draw shield
        Sst_Animate(self, Animation_ShieldInvincibility);
        Sst_Draw(self);
        break;
    case 4: // Invincibility
        // Check if invincibility should exist
        if (!invincibility) {
            Sst_Delete(self);
            break;
        }

        // The star trail buffer (never filled in): the corner of the level
        self->x_pos.f.u = 0;
        self->y_pos.f.u = 0;
        self->status = player->status;
        self->mapping_frame = player->mapping_frame;
        self->render_flags = SST_RENDER_LEVEL | (player->render_flags & (SST_RENDER_X_FLIP | SST_RENDER_Y_FLIP));
        Sst_Draw(self);
        break;
    }
}
