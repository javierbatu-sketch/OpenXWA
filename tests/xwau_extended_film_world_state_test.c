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
void* Memory_LockHandle(MemoryHandle handle) { assert(handle == 1u); return g_testHandleBlock; }
int Memory_UnlockHandle(MemoryHandle handle) { assert(handle == 1u); return 0; }
void Memory_FreeHandle(const char* tag, MemoryHandle handle) {
    (void)tag; assert(handle == 1u); free(g_testHandleBlock); g_testHandleBlock = NULL;
}

static void seed_low_legacy(CraftData* craft) {
    memset(craft, 0, sizeof(*craft));
    craft->componentState[48] = 3u;
    craft->componentState[49] = 4u;
    craft->meshRotation[49] = 5u;
    craft->componentHp[49] = 6u;
    craft->engineEmitterHealth[15] = 7u;
    craft->warheadData[15].count = 8u;
    CraftExtended_SeedFromLegacy(craft);
}

static void assert_high_defaults(const CraftData* craft) {
    assert(CraftExtended_GetMeshComponentState(craft, 100u) == 0u);
    assert(CraftExtended_GetMeshComponentState(craft, 253u) == 0u);
    assert(CraftExtended_GetMeshRotation(craft, 100u) == 0u);
    assert(CraftExtended_GetComponentHp(craft, 100u) == 0xffu);
    assert(CraftExtended_GetEngineEmitterHealth(craft, 254u) == 0u);
    assert(CraftExtended_GetWeaponEntryConst(craft, 119u)->count == 0u);
}

int main(void) {
    enum { CLASSIC_BYTES = 64 };
    CraftData pool[1];
    uint8_t classic[CLASSIC_BYTES];
    uint8_t* tail;
    uint8_t* film;
    size_t maxTail;
    size_t tailBytes;
    size_t written;
    size_t consumed;
    uint32_t recordedWorldStateSize;

    memset(pool, 0, sizeof(pool));
    memset(classic, 0x5a, sizeof(classic));
    g_craftDataPoolBase = pool;
    assert(CraftExtended_Allocate(1u));
    seed_low_legacy(&pool[0]);

    assert(CraftExtended_SetMeshComponentState(&pool[0], 49u, 20u));
    assert(CraftExtended_SetMeshComponentState(&pool[0], 100u, 21u));
    assert(CraftExtended_SetMeshComponentState(&pool[0], 253u, 22u));
    assert(CraftExtended_SetMeshRotation(&pool[0], 100u, 23u));
    assert(CraftExtended_SetComponentHp(&pool[0], 100u, 24u));
    assert(CraftExtended_SetEngineEmitterHealth(&pool[0], 254u, 25u));
    CraftExtended_GetWeaponEntry(&pool[0], 119u)->count = 26u;
    CraftExtended_GetWeaponEntry(&pool[0], 119u)->weaponType = 9u;

    maxTail = CraftExtended_WorldStateMaxTailSize(1u);
    tail = (uint8_t*)malloc(maxTail);
    assert(tail != NULL);
    assert(CraftExtended_WriteWorldStateTail(tail, maxTail, &written));
    tailBytes = written;
    assert(tailBytes > sizeof(XwaExtendedWorldStateHeader));

    /* Film v5 initial world state is already a uint32 size followed by those
     * exact world-state bytes. Build that envelope and replay it. */
    recordedWorldStateSize = (uint32_t)(CLASSIC_BYTES + tailBytes);
    film = (uint8_t*)malloc(sizeof(recordedWorldStateSize) + recordedWorldStateSize);
    assert(film != NULL);
    memcpy(film, &recordedWorldStateSize, sizeof(recordedWorldStateSize));
    memcpy(film + sizeof(recordedWorldStateSize), classic, CLASSIC_BYTES);
    memcpy(film + sizeof(recordedWorldStateSize) + CLASSIC_BYTES, tail, tailBytes);

    CraftExtended_ResetAll();
    seed_low_legacy(&pool[0]);
    assert_high_defaults(&pool[0]);
    memcpy(&recordedWorldStateSize, film, sizeof(recordedWorldStateSize));
    assert(recordedWorldStateSize == CLASSIC_BYTES + tailBytes);
    assert(CraftExtended_OverlayWorldStateTail(
        film + sizeof(recordedWorldStateSize) + CLASSIC_BYTES,
        recordedWorldStateSize - CLASSIC_BYTES, &consumed));
    assert(consumed == tailBytes);
    assert(CraftExtended_GetMeshComponentState(&pool[0], 49u) == 20u);
    assert(CraftExtended_GetSpecialComponentState(&pool[0]) == 4u);
    assert(CraftExtended_GetMeshComponentState(&pool[0], 100u) == 21u);
    assert(CraftExtended_GetMeshComponentState(&pool[0], 253u) == 22u);
    assert(CraftExtended_GetMeshRotation(&pool[0], 100u) == 23u);
    assert(CraftExtended_GetComponentHp(&pool[0], 100u) == 24u);
    assert(CraftExtended_GetEngineEmitterHealth(&pool[0], 254u) == 25u);
    assert(CraftExtended_GetWeaponEntryConst(&pool[0], 119u)->count == 26u);
    assert(CraftExtended_GetWeaponEntryConst(&pool[0], 119u)->weaponType == 9u);

    /* Replaying a legacy film/world-state with no XWES must reset stale high
     * state and seed only the classic low ranges. Its existing size+bytes
     * envelope has zero bytes remaining after the classic prefix. */
    {
        uint8_t legacyFilm[sizeof(uint32_t) + CLASSIC_BYTES];
        uint32_t legacyWorldStateSize = CLASSIC_BYTES;
        memcpy(legacyFilm, &legacyWorldStateSize, sizeof(legacyWorldStateSize));
        memcpy(legacyFilm + sizeof(legacyWorldStateSize), classic, CLASSIC_BYTES);

        assert(CraftExtended_SetMeshComponentState(&pool[0], 100u, 99u));
        assert(CraftExtended_SetEngineEmitterHealth(&pool[0], 254u, 98u));
        CraftExtended_GetWeaponEntry(&pool[0], 119u)->count = 97u;

        memcpy(&legacyWorldStateSize, legacyFilm, sizeof(legacyWorldStateSize));
        assert(legacyWorldStateSize == CLASSIC_BYTES);
        assert((size_t)legacyWorldStateSize - CLASSIC_BYTES == 0u);
        CraftExtended_ResetAll();
        seed_low_legacy(&pool[0]);
        assert_high_defaults(&pool[0]);
    }

    /* Truncated XWES from a film must be rejected rather than silently applied. */
    assert(!CraftExtended_OverlayWorldStateTail(tail, tailBytes - 1u, &consumed));

    free(film);
    free(tail);
    CraftExtended_Free();
    return 0;
}
