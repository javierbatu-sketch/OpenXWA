#include "xwa/flight/object/craft_extended_state.h"
#include "xwa/util/memory.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

CraftData* g_craftDataPoolBase;

static void* g_testHandleBlock;

MemoryHandle Memory_AllocHandleZeroed(const char* tag, size_t size) {
    (void)tag;
    assert(g_testHandleBlock == NULL);
    g_testHandleBlock = calloc(1, size);
    return g_testHandleBlock != NULL ? 1u : 0u;
}
void* Memory_LockHandle(MemoryHandle handle) {
    assert(handle == 1u);
    return g_testHandleBlock;
}
int Memory_UnlockHandle(MemoryHandle handle) {
    assert(handle == 1u);
    return 0;
}
void Memory_FreeHandle(const char* tag, MemoryHandle handle) {
    (void)tag;
    assert(handle == 1u);
    free(g_testHandleBlock);
    g_testHandleBlock = NULL;
}

int main(void) {
    CraftData craft;
    CraftData legacyImage;
    XwaCraftExtendedState* ext;
    WarheadInventoryEntry* weapon;

    memset(&craft, 0, sizeof(craft));
    g_craftDataPoolBase = &craft;
    assert(CraftExtended_Allocate(1u));

    CraftExtended_ResetCraft(&craft);
    ext = CraftExtended_Get(&craft);
    assert(ext != NULL);
    assert(ext->componentState[100] == 0u);
    assert(ext->meshRotation[100] == 0u);
    assert(ext->componentHp[100] == 0xffu);
    assert(ext->engineEmitterHealth[100] == 0u);
    assert(ext->warheadData[100].count == 0u);

    craft.componentState[0] = 11u;
    craft.componentState[48] = 12u;
    craft.componentState[49] = 13u;
    craft.meshRotation[49] = 14u;
    craft.componentHp[0] = 15u;
    craft.componentHp[49] = 16u;
    craft.engineEmitterHealth[15] = 17u;
    craft.warheadData[15].count = 18u;

    CraftExtended_SeedFromLegacy(&craft);
    assert(CraftExtended_GetMeshComponentState(&craft, 0u) == 11u);
    assert(CraftExtended_GetMeshComponentState(&craft, 48u) == 12u);
    assert(CraftExtended_GetMeshComponentState(&craft, 49u) == 0u);
    assert(CraftExtended_GetSpecialComponentState(&craft) == 13u);
    assert(CraftExtended_GetMeshRotation(&craft, 49u) == 14u);
    assert(CraftExtended_GetComponentHp(&craft, 0u) == 15u);
    assert(CraftExtended_GetComponentHp(&craft, 49u) == 16u);
    assert(CraftExtended_GetComponentHp(&craft, 50u) == 0xffu);
    assert(CraftExtended_GetEngineEmitterHealth(&craft, 15u) == 17u);
    assert(CraftExtended_GetEngineEmitterHealth(&craft, 16u) == 0u);
    assert(CraftExtended_GetWeaponEntry(&craft, 15u)->count == 18u);
    assert(CraftExtended_GetWeaponEntry(&craft, 16u)->count == 0u);

    assert(CraftExtended_SetMeshComponentState(&craft, 49u, 21u));
    assert(CraftExtended_GetMeshComponentState(&craft, 49u) == 21u);
    assert(CraftExtended_GetSpecialComponentState(&craft) == 13u);

    assert(CraftExtended_SetDetachedPostMeshState(&craft, 50u, 4u));
    assert(CraftExtended_GetMeshComponentState(&craft, 50u) == 4u);
    assert(CraftExtended_GetSpecialComponentState(&craft) == 13u);
    assert(CraftExtended_SetDetachedPostMeshState(&craft, 254u, 5u));
    assert(CraftExtended_GetSpecialComponentState(&craft) == 5u);
    assert(!CraftExtended_SetDetachedPostMeshState(&craft, 255u, 6u));

    assert(CraftExtended_SetMeshComponentState(&craft, 100u, 31u));
    assert(CraftExtended_SetMeshComponentState(&craft, 253u, 32u));
    assert(CraftExtended_SetMeshRotation(&craft, 100u, 33u));
    assert(CraftExtended_SetComponentHp(&craft, 100u, 34u));
    assert(CraftExtended_SetEngineEmitterHealth(&craft, 254u, 35u));
    weapon = CraftExtended_GetWeaponEntry(&craft, 119u);
    assert(weapon != NULL);
    weapon->count = 36u;

    assert(!CraftExtended_SetMeshComponentState(&craft, 254u, 1u));
    assert(CraftExtended_MeshComponentStateRef(&craft, 254u) == NULL);
    assert(CraftExtended_MeshRotationRef(&craft, 254u) == NULL);
    assert(CraftExtended_ComponentHpRef(&craft, 254u) == NULL);
    assert(CraftExtended_EngineEmitterHealthRef(&craft, 255u) == NULL);
    assert(CraftExtended_GetWeaponEntry(&craft, 120u) == NULL);
    assert(CraftExtended_GetWeaponEntry(&craft, 127u) == NULL);

    memset(&legacyImage, 0xcc, sizeof(legacyImage));
    CraftExtended_ProjectToLegacy(&craft, &legacyImage);
    assert(legacyImage.componentState[0] == 11u);
    assert(legacyImage.componentState[48] == 12u);
    assert(legacyImage.componentState[49] == 5u);
    assert(legacyImage.meshRotation[49] == 14u);
    assert(legacyImage.componentHp[49] == 16u);
    assert(legacyImage.engineEmitterHealth[15] == 17u);
    assert(legacyImage.warheadData[15].count == 18u);

    assert(CraftExtended_GetMeshComponentState(&craft, 100u) == 31u);
    assert(CraftExtended_GetMeshComponentState(&craft, 253u) == 32u);
    assert(CraftExtended_GetMeshRotation(&craft, 100u) == 33u);
    assert(CraftExtended_GetComponentHp(&craft, 100u) == 34u);
    assert(CraftExtended_GetEngineEmitterHealth(&craft, 254u) == 35u);
    assert(CraftExtended_GetWeaponEntry(&craft, 119u)->count == 36u);

    CraftExtended_Free();
    return 0;
}
