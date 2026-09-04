#include "xwa_runtime/snapshot/snapshot.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    XwaFlightObject object;
    memset(&object, 0, sizeof object);

    assert(XWA_SNAP_MAX_MESH_SLOTS == 254);
    assert(sizeof object.component_state == 254u);
    assert(sizeof object.mesh_rotation == 254u);
    assert(sizeof object.component_hp == 254u);

    object.component_state[49] = 1;
    object.damage_flame_frame = 11;
    object.component_state[100] = 2;
    object.component_state[253] = 3;
    object.mesh_rotation[49] = 4;
    object.mesh_rotation[100] = 5;
    object.mesh_rotation[253] = 6;
    object.component_hp[49] = 7;
    object.component_hp[100] = 8;
    object.component_hp[253] = 9;

    assert(object.component_state[49] == 1);
    assert(object.damage_flame_frame == 11);
    assert(object.component_state[100] == 2);
    assert(object.component_state[253] == 3);
    assert(object.mesh_rotation[253] == 6);
    assert(object.component_hp[253] == 9);

    assert(XWA_SNAP_ENGINE_KNOCKOUT_WORDS == 8);
    assert(sizeof object.eg_knockout_mask == 8u * sizeof(uint32_t));
    XwaSnapshot_EngineKnockoutSet(object.eg_knockout_mask, 31);
    XwaSnapshot_EngineKnockoutSet(object.eg_knockout_mask, 32);
    XwaSnapshot_EngineKnockoutSet(object.eg_knockout_mask, 254);
    assert(XwaSnapshot_EngineKnockoutIsSet(object.eg_knockout_mask, 31));
    assert(XwaSnapshot_EngineKnockoutIsSet(object.eg_knockout_mask, 32));
    assert(XwaSnapshot_EngineKnockoutIsSet(object.eg_knockout_mask, 254));
    assert(!XwaSnapshot_EngineKnockoutIsSet(object.eg_knockout_mask, 30));
    assert(!XwaSnapshot_EngineKnockoutIsSet(object.eg_knockout_mask, 255));

    /* Mesh slot 254 is intentionally absent: simulation special state 254 is not a render mesh. */
    assert((sizeof object.component_state / sizeof object.component_state[0]) == 254u);

    return 0;
}
