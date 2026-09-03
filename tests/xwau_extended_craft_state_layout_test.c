#include "xwa/flight/object/craft_extended_state.h"

#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(WarheadInventoryEntry) == 14, "retail warhead entry layout changed");
_Static_assert(offsetof(CraftData, componentState) == 0x22E, "retail componentState offset changed");
_Static_assert(offsetof(CraftData, meshRotation) == 0x260, "retail meshRotation offset changed");
_Static_assert(offsetof(CraftData, componentHp) == 0x292, "retail componentHp offset changed");
_Static_assert(offsetof(CraftData, effectiveAiObjectLink) == 0x3F5,
               "retail CraftData prefix before pointer changed");

/* Modern CraftData owns a native pointer, so its in-memory size is pointer-width
 * dependent.  The retail/world-state contract remains 1017 bytes: the packed
 * prefix through 0x3F4 plus a 32-bit encoded effectiveAiObjectLink. */
_Static_assert(sizeof(CraftData) == 0x3F5 + sizeof(ObjectRecord*),
               "modern packed CraftData size no longer matches its native pointer width");
_Static_assert(offsetof(CraftData, effectiveAiObjectLink) + sizeof(uint32_t) == 1017,
               "retail serialized CraftData contract changed");

_Static_assert(XWAU_RENDERABLE_MESH_COUNT == 254, "wrong XWAU renderable mesh count");
_Static_assert(XWAU_COMPONENT_STATE_COUNT == 255, "wrong XWAU component backing count");
_Static_assert(XWAU_SPECIAL_COMPONENT_STATE_INDEX == 254, "wrong XWAU special component slot");
_Static_assert(XWAU_ENGINE_EMITTER_COUNT == 255, "wrong XWAU engine emitter count");
_Static_assert(XWAU_WEAPON_RACK_LIMIT == 120, "wrong XWAU weapon rack gameplay limit");
_Static_assert(XWAU_WEAPON_STATE_CAPACITY == 128, "wrong XWAU weapon backing capacity");
_Static_assert(sizeof(XwaCraftExtendedState) == 2812,
               "extended craft-state POD layout changed");

int main(void) {
    return 0;
}
