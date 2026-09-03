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

static void assert_default_high_state(const CraftData* craft) {
    uint16_t i;

    for (i = 49u; i < XWAU_RENDERABLE_MESH_COUNT; ++i) {
        assert(CraftExtended_GetMeshComponentState(craft, i) == 0u);
    }
    for (i = 50u; i < XWAU_RENDERABLE_MESH_COUNT; ++i) {
        assert(CraftExtended_GetMeshRotation(craft, i) == 0u);
        assert(CraftExtended_GetComponentHp(craft, i) == 0xffu);
    }
    for (i = 16u; i < XWAU_ENGINE_EMITTER_COUNT; ++i) {
        assert(CraftExtended_GetEngineEmitterHealth(craft, i) == 0u);
    }
    for (i = 16u; i < XWAU_WEAPON_RACK_LIMIT; ++i) {
        const WarheadInventoryEntry* weapon = CraftExtended_GetWeaponEntryConst(craft, i);
        WarheadInventoryEntry zeroWeapon;
        memset(&zeroWeapon, 0, sizeof(zeroWeapon));
        assert(weapon != NULL);
        assert(memcmp(weapon, &zeroWeapon, sizeof(zeroWeapon)) == 0);
    }
}

int main(void) {
    CraftData craftPool[3];
    uint8_t* tail;
    size_t maxTailSize;
    size_t tailSize;
    size_t written;
    size_t consumed;
    XwaExtendedWorldStateHeader header;

    memset(craftPool, 0, sizeof(craftPool));
    g_craftDataPoolBase = craftPool;
    assert(CraftExtended_Allocate(3u));

    assert(sizeof(XwaExtendedWorldStateHeader) == 12u);
    assert(XWAU_EXTENDED_WORLD_STATE_MAGIC == 0x53455758u);
    assert(XWAU_EXTENDED_WORLD_STATE_VERSION == 1u);

    {
        size_t expandedSize = 0u;
        assert(CraftExtended_WorldStateBufferSizeWithTail(1000u, 3u, &expandedSize));
        assert(expandedSize == 1000u + CraftExtended_WorldStateMaxTailSize(3u));
        assert(!CraftExtended_WorldStateBufferSizeWithTail(SIZE_MAX, 3u, &expandedSize));
        assert(!CraftExtended_WorldStateBufferSizeWithTail(1000u, (uint32_t)UINT16_MAX + 1u, &expandedSize));
    }

    maxTailSize = CraftExtended_WorldStateMaxTailSize(3u);
    assert(maxTailSize >= sizeof(XwaExtendedWorldStateHeader));
    assert(CraftExtended_WorldStateMaxTailSize((uint32_t)UINT16_MAX + 1u) == 0u);
    tail = (uint8_t*)malloc(maxTailSize);
    assert(tail != NULL);

    /* No high state still emits a valid empty XWES tail. */
    tailSize = CraftExtended_WorldStateTailSize();
    assert(tailSize == sizeof(XwaExtendedWorldStateHeader));
    assert(CraftExtended_WriteWorldStateTail(tail, maxTailSize, &written));
    assert(written == tailSize);
    memcpy(&header, tail, sizeof(header));
    assert(header.magic == XWAU_EXTENDED_WORLD_STATE_MAGIC);
    assert(header.version == XWAU_EXTENDED_WORLD_STATE_VERSION);
    assert(header.recordCount == 0u);
    assert(header.payloadBytes == 0u);

    /* Non-gameplay backing 120..127 is never serialized as XWES gameplay state. */
    CraftExtended_Get(&craftPool[0])->warheadData[127].count = 99u;
    assert(CraftExtended_WorldStateTailSize() == sizeof(XwaExtendedWorldStateHeader));
    CraftExtended_Get(&craftPool[0])->warheadData[127].count = 0u;

    /* A pure legacy state projects byte-for-byte through the classic CraftData prefix. */
    {
        CraftData legacyImage;
        size_t legacyPrefixBytes = offsetof(CraftData, effectiveAiObjectLink);
        memset(&craftPool[0], 0, sizeof(craftPool[0]));
        craftPool[0].componentState[0] = 1u;
        craftPool[0].componentState[48] = 2u;
        craftPool[0].componentState[49] = 3u;
        craftPool[0].meshRotation[49] = 4u;
        craftPool[0].componentHp[49] = 5u;
        craftPool[0].engineEmitterHealth[15] = 6u;
        craftPool[0].warheadData[15].count = 7u;
        CraftExtended_SeedFromLegacy(&craftPool[0]);
        memset(&legacyImage, 0xcc, sizeof(legacyImage));
        CraftExtended_ProjectToLegacy(&craftPool[0], &legacyImage);
        assert(memcmp(&legacyImage, &craftPool[0], legacyPrefixBytes) == 0);
    }

    /* Distinct low/special/high values. Only high overflow belongs in XWES. */
    craftPool[1].componentState[48] = 10u;
    craftPool[1].componentState[49] = 11u;
    craftPool[1].meshRotation[49] = 12u;
    craftPool[1].componentHp[49] = 13u;
    craftPool[1].engineEmitterHealth[15] = 14u;
    craftPool[1].warheadData[15].count = 15u;
    CraftExtended_SeedFromLegacy(&craftPool[1]);

    assert(CraftExtended_SetMeshComponentState(&craftPool[1], 49u, 21u));
    assert(CraftExtended_SetMeshComponentState(&craftPool[1], 100u, 22u));
    assert(CraftExtended_SetMeshComponentState(&craftPool[1], 253u, 23u));
    assert(CraftExtended_SetSpecialComponentState(&craftPool[1], 24u));
    assert(CraftExtended_SetMeshRotation(&craftPool[1], 100u, 25u));
    assert(CraftExtended_SetComponentHp(&craftPool[1], 100u, 26u));
    assert(CraftExtended_SetEngineEmitterHealth(&craftPool[1], 254u, 27u));
    assert(CraftExtended_GetWeaponEntry(&craftPool[1], 119u) != NULL);
    CraftExtended_GetWeaponEntry(&craftPool[1], 119u)->count = 28u;
    CraftExtended_GetWeaponEntry(&craftPool[1], 119u)->weaponType = 7u;

    tailSize = CraftExtended_WorldStateTailSize();
    assert(tailSize > sizeof(XwaExtendedWorldStateHeader));
    assert(CraftExtended_WriteWorldStateTail(tail, maxTailSize, &written));
    assert(written == tailSize);
    memcpy(&header, tail, sizeof(header));
    assert(header.recordCount == 1u);
    assert(header.payloadBytes == tailSize - sizeof(header));

    /* Simulate legacy restore first: low/special state survives, high resets. */
    CraftExtended_ResetAll();
    CraftExtended_SeedFromLegacy(&craftPool[1]);
    assert(CraftExtended_GetMeshComponentState(&craftPool[1], 48u) == 10u);
    assert(CraftExtended_GetSpecialComponentState(&craftPool[1]) == 11u);
    assert(CraftExtended_GetMeshRotation(&craftPool[1], 49u) == 12u);
    assert(CraftExtended_GetComponentHp(&craftPool[1], 49u) == 13u);
    assert(CraftExtended_GetEngineEmitterHealth(&craftPool[1], 15u) == 14u);
    assert(CraftExtended_GetWeaponEntryConst(&craftPool[1], 15u)->count == 15u);
    assert_default_high_state(&craftPool[1]);

    assert(CraftExtended_OverlayWorldStateTail(tail, tailSize, &consumed));
    assert(consumed == tailSize);
    assert(CraftExtended_GetMeshComponentState(&craftPool[1], 49u) == 21u);
    assert(CraftExtended_GetMeshComponentState(&craftPool[1], 100u) == 22u);
    assert(CraftExtended_GetMeshComponentState(&craftPool[1], 253u) == 23u);
    /* special comes from legacy projection, not duplicated in XWES */
    assert(CraftExtended_GetSpecialComponentState(&craftPool[1]) == 11u);
    assert(CraftExtended_GetMeshRotation(&craftPool[1], 100u) == 25u);
    assert(CraftExtended_GetComponentHp(&craftPool[1], 100u) == 26u);
    assert(CraftExtended_GetEngineEmitterHealth(&craftPool[1], 254u) == 27u);
    assert(CraftExtended_GetWeaponEntryConst(&craftPool[1], 119u)->count == 28u);
    assert(CraftExtended_GetWeaponEntryConst(&craftPool[1], 119u)->weaponType == 7u);

    /* Truncation must fail explicitly. */
    assert(!CraftExtended_OverlayWorldStateTail(tail, tailSize - 1u, &consumed));

    /* Unsupported version must fail explicitly. */
    {
        uint8_t* malformed = (uint8_t*)malloc(tailSize);
        assert(malformed != NULL);
        memcpy(malformed, tail, tailSize);
        ((XwaExtendedWorldStateHeader*)malformed)->version = 2u;
        assert(!CraftExtended_OverlayWorldStateTail(malformed, tailSize, &consumed));
        free(malformed);
    }

    /* Impossible payload count/size mismatch must fail. */
    {
        uint8_t* malformed = (uint8_t*)malloc(tailSize);
        assert(malformed != NULL);
        memcpy(malformed, tail, tailSize);
        ((XwaExtendedWorldStateHeader*)malformed)->recordCount = 2u;
        assert(!CraftExtended_OverlayWorldStateTail(malformed, tailSize, &consumed));
        free(malformed);
    }

    /* Duplicate craft ordinal must fail rather than apply twice. */
    {
        size_t oneRecordBytes = header.payloadBytes;
        size_t dupSize = sizeof(header) + oneRecordBytes * 2u;
        uint8_t* duplicate = (uint8_t*)malloc(dupSize);
        XwaExtendedWorldStateHeader* dupHeader;
        assert(duplicate != NULL);
        memcpy(duplicate + sizeof(header), tail + sizeof(header), oneRecordBytes);
        memcpy(duplicate + sizeof(header) + oneRecordBytes, tail + sizeof(header), oneRecordBytes);
        dupHeader = (XwaExtendedWorldStateHeader*)duplicate;
        dupHeader->magic = XWAU_EXTENDED_WORLD_STATE_MAGIC;
        dupHeader->version = XWAU_EXTENDED_WORLD_STATE_VERSION;
        dupHeader->recordCount = 2u;
        dupHeader->payloadBytes = (uint32_t)(oneRecordBytes * 2u);
        assert(!CraftExtended_OverlayWorldStateTail(duplicate, dupSize, &consumed));
        free(duplicate);
    }

    free(tail);
    CraftExtended_Free();
    return 0;
}
