#include "xwa/flight/object/craft_extended_state.h"
#include "xwa/util/memory.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

CraftData* g_craftDataPoolBase;

static void* g_testHandleBlock;
static size_t g_testAllocatedBytes;
static unsigned int g_testFreeCount;

MemoryHandle Memory_AllocHandleZeroed(const char* tag, size_t size) {
    (void)tag;
    assert(g_testHandleBlock == NULL);
    g_testHandleBlock = calloc(1, size);
    g_testAllocatedBytes = size;
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
    ++g_testFreeCount;
}

int main(void) {
    CraftData craftPool[5];
    CraftData unrelatedCraft;
    XwaCraftExtendedState* first;
    XwaCraftExtendedState* middle;
    XwaCraftExtendedState* last;

    memset(craftPool, 0, sizeof(craftPool));
    memset(&unrelatedCraft, 0, sizeof(unrelatedCraft));
    g_craftDataPoolBase = craftPool;

    assert(CraftExtended_Allocate(5u));
    assert(g_testAllocatedBytes == sizeof(XwaCraftExtendedState) * 5u);

    first = CraftExtended_Get(&craftPool[0]);
    middle = CraftExtended_Get(&craftPool[2]);
    last = CraftExtended_Get(&craftPool[4]);
    assert(first != NULL);
    assert(middle == first + 2);
    assert(last == first + 4);
    assert(CraftExtended_GetConst(&craftPool[2]) == middle);

    assert(CraftExtended_Get(&craftPool[5]) == NULL);
    assert(CraftExtended_Get(&unrelatedCraft) == NULL);
    assert(CraftExtended_Get((CraftData*)((uint8_t*)craftPool + 1u)) == NULL);
    assert(CraftExtended_Get(NULL) == NULL);

    middle->componentState[100] = 9u;
    middle->meshRotation[100] = 10u;
    middle->componentHp[100] = 11u;
    middle->engineEmitterHealth[100] = 12u;
    middle->warheadData[100].count = 13u;
    CraftExtended_Copy(&craftPool[4], &craftPool[2]);
    assert(last->componentState[100] == 9u);
    assert(last->meshRotation[100] == 10u);
    assert(last->componentHp[100] == 11u);
    assert(last->engineEmitterHealth[100] == 12u);
    assert(last->warheadData[100].count == 13u);

    CraftExtended_ResetComponentArrays(&craftPool[4]);
    assert(last->componentState[100] == 0u);
    assert(last->meshRotation[100] == 0u);
    assert(last->componentHp[100] == 0xffu);
    assert(last->engineEmitterHealth[100] == 12u);
    assert(last->warheadData[100].count == 13u);

    craftPool[4].warheadData[0].count = 21u;
    craftPool[4].warheadData[15].count = 22u;
    last->warheadData[0].count = 23u;
    last->warheadData[100].count = 24u;
    last->warheadData[127].count = 25u;
    CraftExtended_ResetWeaponState(&craftPool[4]);
    assert(craftPool[4].warheadData[0].count == 0u);
    assert(craftPool[4].warheadData[15].count == 0u);
    assert(last->warheadData[0].count == 0u);
    assert(last->warheadData[100].count == 0u);
    assert(last->warheadData[127].count == 0u);
    assert(last->engineEmitterHealth[100] == 12u);

    {
        unsigned char* bytes = (unsigned char*)&craftPool[4];
        const size_t clearBytes =
            offsetof(CraftData, warheadData[14].count) + sizeof(craftPool[4].warheadData[14].count);
        size_t byteIndex;

        memset(&craftPool[4], 0xa5, sizeof(craftPool[4]));
        last->componentState[100] = 1u;
        last->meshRotation[100] = 2u;
        last->componentHp[100] = 3u;
        last->engineEmitterHealth[100] = 4u;
        last->warheadData[100].count = 5u;

        CraftExtended_ClearLegacyReusablePrefix(&craftPool[4]);

        for (byteIndex = 0; byteIndex < clearBytes; ++byteIndex) {
            assert(bytes[byteIndex] == 0u);
        }
        assert(clearBytes < sizeof(craftPool[4]));
        assert(bytes[clearBytes] == 0xa5u);
        assert(last->componentState[100] == 0u);
        assert(last->meshRotation[100] == 0u);
        assert(last->componentHp[100] == 0xffu);
        assert(last->engineEmitterHealth[100] == 0u);
        assert(last->warheadData[100].count == 0u);
    }

    CraftExtended_ResetAll();
    assert(middle->componentState[100] == 0u);

    CraftExtended_Free();
    assert(g_testFreeCount == 1u);
    assert(CraftExtended_Get(&craftPool[0]) == NULL);

    return 0;
}
