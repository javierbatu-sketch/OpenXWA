#include "xwa/flight/object/craft_extended_state.h"
#include "xwa/util/memory.h"

#include <stdint.h>
#include <string.h>

static MemoryHandle g_craftExtendedStatePoolHandle;
static XwaCraftExtendedState* g_craftExtendedStatePool;
static uint32_t g_craftExtendedStateCount;

void CraftExtended_Free(void) {
    if (g_craftExtendedStatePoolHandle != 0u) {
        if (g_craftExtendedStatePool != NULL) {
            Memory_UnlockHandle(g_craftExtendedStatePoolHandle);
        }
        Memory_FreeHandle("XWAUCRAFTSTATE", g_craftExtendedStatePoolHandle);
    }

    g_craftExtendedStatePoolHandle = 0u;
    g_craftExtendedStatePool = NULL;
    g_craftExtendedStateCount = 0u;
}

int CraftExtended_Allocate(uint32_t craftCount) {
    size_t allocationSize;

    CraftExtended_Free();
    if (craftCount == 0u) {
        return 1;
    }
    if (sizeof(XwaCraftExtendedState) > SIZE_MAX / (size_t)craftCount ||
        sizeof(CraftData) > SIZE_MAX / (size_t)craftCount) {
        return 0;
    }

    allocationSize = sizeof(XwaCraftExtendedState) * (size_t)craftCount;
    g_craftExtendedStatePoolHandle = Memory_AllocHandleZeroed("XWAUCRAFTSTATE", allocationSize);
    if (g_craftExtendedStatePoolHandle == 0u) {
        return 0;
    }

    g_craftExtendedStatePool =
        (XwaCraftExtendedState*)Memory_LockHandle(g_craftExtendedStatePoolHandle);
    if (g_craftExtendedStatePool == NULL) {
        Memory_FreeHandle("XWAUCRAFTSTATE", g_craftExtendedStatePoolHandle);
        g_craftExtendedStatePoolHandle = 0u;
        return 0;
    }

    g_craftExtendedStateCount = craftCount;
    CraftExtended_ResetAll();
    return 1;
}

void CraftExtended_ResetAll(void) {
    uint32_t i;
    if (g_craftExtendedStatePool == NULL) {
        return;
    }
    for (i = 0; i < g_craftExtendedStateCount; ++i) {
        memset(&g_craftExtendedStatePool[i], 0, sizeof(g_craftExtendedStatePool[i]));
        memset(g_craftExtendedStatePool[i].componentHp, 0xff,
               sizeof(g_craftExtendedStatePool[i].componentHp));
    }
}

XwaCraftExtendedState* CraftExtended_Get(CraftData* craft) {
    uintptr_t base;
    uintptr_t address;
    size_t span;
    size_t delta;
    size_t index;

    if (craft == NULL || g_craftDataPoolBase == NULL || g_craftExtendedStatePool == NULL ||
        g_craftExtendedStateCount == 0u) {
        return NULL;
    }

    span = sizeof(CraftData) * (size_t)g_craftExtendedStateCount;
    base = (uintptr_t)g_craftDataPoolBase;
    address = (uintptr_t)craft;
    if (span > UINTPTR_MAX - base || address < base || address >= base + span) {
        return NULL;
    }

    delta = (size_t)(address - base);
    if (delta % sizeof(CraftData) != 0u) {
        return NULL;
    }

    index = delta / sizeof(CraftData);
    if (index >= (size_t)g_craftExtendedStateCount) {
        return NULL;
    }
    return &g_craftExtendedStatePool[index];
}

const XwaCraftExtendedState* CraftExtended_GetConst(const CraftData* craft) {
    return CraftExtended_Get((CraftData*)craft);
}

static void CraftExtended_ResetState(XwaCraftExtendedState* state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    memset(state->componentHp, 0xff, sizeof(state->componentHp));
}

void CraftExtended_ResetCraft(CraftData* craft) {
    CraftExtended_ResetState(CraftExtended_Get(craft));
}

void CraftExtended_ClearLegacyReusablePrefix(CraftData* craft) {
    if (craft == NULL) {
        return;
    }

    memset(craft, 0, offsetof(CraftData, warheadData[14].count) +
                         sizeof(craft->warheadData[14].count));
    CraftExtended_ResetCraft(craft);
}

void CraftExtended_ResetComponentArrays(CraftData* craft) {
    XwaCraftExtendedState* state = CraftExtended_Get(craft);
    if (state == NULL) {
        return;
    }

    memset(state->componentState, 0, sizeof(state->componentState));
    memset(state->meshRotation, 0, sizeof(state->meshRotation));
    memset(state->componentHp, 0xff, sizeof(state->componentHp));
}

void CraftExtended_ResetWeaponState(CraftData* craft) {
    XwaCraftExtendedState* state = CraftExtended_Get(craft);
    if (state == NULL) {
        return;
    }

    memset(state->warheadData, 0, sizeof(state->warheadData));
    memset(craft->warheadData, 0, sizeof(craft->warheadData));
}

void CraftExtended_Copy(CraftData* dst, const CraftData* src) {
    XwaCraftExtendedState* dstState = CraftExtended_Get(dst);
    const XwaCraftExtendedState* srcState = CraftExtended_GetConst(src);

    if (dstState == NULL || srcState == NULL) {
        return;
    }

    memcpy(dstState, srcState, sizeof(*dstState));
}

void CraftExtended_SeedFromLegacy(CraftData* craft) {
    XwaCraftExtendedState* state;

    if (craft == NULL) {
        return;
    }
    state = CraftExtended_Get(craft);
    if (state == NULL) {
        return;
    }

    CraftExtended_ResetState(state);
    memcpy(&state->componentState[0], &craft->componentState[0], 49u);
    state->componentState[XWAU_SPECIAL_COMPONENT_STATE_INDEX] = craft->componentState[49];
    memcpy(&state->meshRotation[0], &craft->meshRotation[0], sizeof(craft->meshRotation));
    memcpy(&state->componentHp[0], &craft->componentHp[0], sizeof(craft->componentHp));
    memcpy(&state->engineEmitterHealth[0], &craft->engineEmitterHealth[0],
           sizeof(craft->engineEmitterHealth));
    memcpy(&state->warheadData[0], &craft->warheadData[0], sizeof(craft->warheadData));
}

void CraftExtended_ProjectToLegacy(const CraftData* craft, CraftData* legacyImage) {
    const XwaCraftExtendedState* state;

    if (craft == NULL || legacyImage == NULL) {
        return;
    }

    memcpy(legacyImage, craft, sizeof(*legacyImage));
    state = CraftExtended_GetConst(craft);
    if (state == NULL) {
        return;
    }

    memcpy(&legacyImage->componentState[0], &state->componentState[0], 49u);
    legacyImage->componentState[49] = state->componentState[XWAU_SPECIAL_COMPONENT_STATE_INDEX];
    memcpy(&legacyImage->meshRotation[0], &state->meshRotation[0], sizeof(legacyImage->meshRotation));
    memcpy(&legacyImage->componentHp[0], &state->componentHp[0], sizeof(legacyImage->componentHp));
    memcpy(&legacyImage->engineEmitterHealth[0], &state->engineEmitterHealth[0],
           sizeof(legacyImage->engineEmitterHealth));
    memcpy(&legacyImage->warheadData[0], &state->warheadData[0], sizeof(legacyImage->warheadData));
}

uint8_t* CraftExtended_MeshComponentStateRef(CraftData* craft, uint16_t meshIndex) {
    XwaCraftExtendedState* state;
    if (meshIndex >= XWAU_RENDERABLE_MESH_COUNT) {
        return NULL;
    }
    state = CraftExtended_Get(craft);
    return state != NULL ? &state->componentState[meshIndex] : NULL;
}

const uint8_t* CraftExtended_MeshComponentStateRefConst(const CraftData* craft, uint16_t meshIndex) {
    return CraftExtended_MeshComponentStateRef((CraftData*)craft, meshIndex);
}

uint8_t CraftExtended_GetMeshComponentState(const CraftData* craft, uint16_t meshIndex) {
    const uint8_t* value = CraftExtended_MeshComponentStateRefConst(craft, meshIndex);
    return value != NULL ? *value : 0u;
}

int CraftExtended_SetMeshComponentState(CraftData* craft, uint16_t meshIndex, uint8_t value) {
    uint8_t* slot = CraftExtended_MeshComponentStateRef(craft, meshIndex);
    if (slot == NULL) {
        return 0;
    }
    *slot = value;
    return 1;
}

uint8_t CraftExtended_GetSpecialComponentState(const CraftData* craft) {
    const XwaCraftExtendedState* state = CraftExtended_GetConst(craft);
    return state != NULL ? state->componentState[XWAU_SPECIAL_COMPONENT_STATE_INDEX] : 0u;
}

int CraftExtended_SetSpecialComponentState(CraftData* craft, uint8_t value) {
    XwaCraftExtendedState* state = CraftExtended_Get(craft);
    if (state == NULL) {
        return 0;
    }
    state->componentState[XWAU_SPECIAL_COMPONENT_STATE_INDEX] = value;
    return 1;
}

int CraftExtended_SetDetachedPostMeshState(CraftData* craft, uint16_t meshCount, uint8_t value) {
    XwaCraftExtendedState* state;
    if (meshCount >= XWAU_COMPONENT_STATE_COUNT) {
        return 0;
    }
    state = CraftExtended_Get(craft);
    if (state == NULL) {
        return 0;
    }
    state->componentState[meshCount] = value;
    return 1;
}

uint8_t* CraftExtended_MeshRotationRef(CraftData* craft, uint16_t meshIndex) {
    XwaCraftExtendedState* state;
    if (meshIndex >= XWAU_RENDERABLE_MESH_COUNT) {
        return NULL;
    }
    state = CraftExtended_Get(craft);
    return state != NULL ? &state->meshRotation[meshIndex] : NULL;
}

const uint8_t* CraftExtended_MeshRotationRefConst(const CraftData* craft, uint16_t meshIndex) {
    return CraftExtended_MeshRotationRef((CraftData*)craft, meshIndex);
}

uint8_t CraftExtended_GetMeshRotation(const CraftData* craft, uint16_t meshIndex) {
    const uint8_t* value = CraftExtended_MeshRotationRefConst(craft, meshIndex);
    return value != NULL ? *value : 0u;
}

int CraftExtended_SetMeshRotation(CraftData* craft, uint16_t meshIndex, uint8_t value) {
    uint8_t* slot = CraftExtended_MeshRotationRef(craft, meshIndex);
    if (slot == NULL) {
        return 0;
    }
    *slot = value;
    return 1;
}

uint8_t* CraftExtended_ComponentHpRef(CraftData* craft, uint16_t meshIndex) {
    XwaCraftExtendedState* state;
    if (meshIndex >= XWAU_RENDERABLE_MESH_COUNT) {
        return NULL;
    }
    state = CraftExtended_Get(craft);
    return state != NULL ? &state->componentHp[meshIndex] : NULL;
}

const uint8_t* CraftExtended_ComponentHpRefConst(const CraftData* craft, uint16_t meshIndex) {
    return CraftExtended_ComponentHpRef((CraftData*)craft, meshIndex);
}

uint8_t CraftExtended_GetComponentHp(const CraftData* craft, uint16_t meshIndex) {
    const uint8_t* value = CraftExtended_ComponentHpRefConst(craft, meshIndex);
    return value != NULL ? *value : 0u;
}

int CraftExtended_SetComponentHp(CraftData* craft, uint16_t meshIndex, uint8_t value) {
    uint8_t* slot = CraftExtended_ComponentHpRef(craft, meshIndex);
    if (slot == NULL) {
        return 0;
    }
    *slot = value;
    return 1;
}

uint8_t* CraftExtended_EngineEmitterHealthRef(CraftData* craft, uint16_t emitterIndex) {
    XwaCraftExtendedState* state;
    if (emitterIndex >= XWAU_ENGINE_EMITTER_COUNT) {
        return NULL;
    }
    state = CraftExtended_Get(craft);
    return state != NULL ? &state->engineEmitterHealth[emitterIndex] : NULL;
}

const uint8_t* CraftExtended_EngineEmitterHealthRefConst(const CraftData* craft, uint16_t emitterIndex) {
    return CraftExtended_EngineEmitterHealthRef((CraftData*)craft, emitterIndex);
}

uint8_t CraftExtended_GetEngineEmitterHealth(const CraftData* craft, uint16_t emitterIndex) {
    const uint8_t* value = CraftExtended_EngineEmitterHealthRefConst(craft, emitterIndex);
    return value != NULL ? *value : 0u;
}

int CraftExtended_SetEngineEmitterHealth(CraftData* craft, uint16_t emitterIndex, uint8_t value) {
    uint8_t* slot = CraftExtended_EngineEmitterHealthRef(craft, emitterIndex);
    if (slot == NULL) {
        return 0;
    }
    *slot = value;
    return 1;
}

WarheadInventoryEntry* CraftExtended_GetWeaponEntry(CraftData* craft, uint16_t weaponIndex) {
    XwaCraftExtendedState* state;
    if (weaponIndex >= XWAU_WEAPON_RACK_LIMIT) {
        return NULL;
    }
    state = CraftExtended_Get(craft);
    return state != NULL ? &state->warheadData[weaponIndex] : NULL;
}

const WarheadInventoryEntry* CraftExtended_GetWeaponEntryConst(const CraftData* craft,
                                                                uint16_t weaponIndex) {
    return CraftExtended_GetWeaponEntry((CraftData*)craft, weaponIndex);
}

enum {
    XWAU_XWES_COMPONENT_STATE_FIRST = 49,
    XWAU_XWES_COMPONENT_STATE_COUNT = XWAU_RENDERABLE_MESH_COUNT - XWAU_XWES_COMPONENT_STATE_FIRST,
    XWAU_XWES_MESH_ROTATION_FIRST = 50,
    XWAU_XWES_MESH_ROTATION_COUNT = XWAU_RENDERABLE_MESH_COUNT - XWAU_XWES_MESH_ROTATION_FIRST,
    XWAU_XWES_COMPONENT_HP_FIRST = 50,
    XWAU_XWES_COMPONENT_HP_COUNT = XWAU_RENDERABLE_MESH_COUNT - XWAU_XWES_COMPONENT_HP_FIRST,
    XWAU_XWES_ENGINE_FIRST = 16,
    XWAU_XWES_ENGINE_COUNT = XWAU_ENGINE_EMITTER_COUNT - XWAU_XWES_ENGINE_FIRST,
    XWAU_XWES_WEAPON_FIRST = 16,
    XWAU_XWES_WEAPON_COUNT = XWAU_WEAPON_RACK_LIMIT - XWAU_XWES_WEAPON_FIRST,
};

#pragma pack(push, 1)
typedef struct XwaExtendedWorldStateRecordV1 {
    uint32_t craftOrdinal;
    uint8_t componentState[XWAU_XWES_COMPONENT_STATE_COUNT];
    uint8_t meshRotation[XWAU_XWES_MESH_ROTATION_COUNT];
    uint8_t componentHp[XWAU_XWES_COMPONENT_HP_COUNT];
    uint8_t engineEmitterHealth[XWAU_XWES_ENGINE_COUNT];
    WarheadInventoryEntry warheadData[XWAU_XWES_WEAPON_COUNT];
} XwaExtendedWorldStateRecordV1;
#pragma pack(pop)

typedef char xwau_extended_world_state_header_size
    [(sizeof(XwaExtendedWorldStateHeader) == 12u) ? 1 : -1];
typedef char xwau_extended_world_state_weapon_entry_size
    [(sizeof(WarheadInventoryEntry) == 14u) ? 1 : -1];

static int CraftExtended_AllBytesEqual(const void* data, size_t size, uint8_t value) {
    const uint8_t* bytes = (const uint8_t*)data;
    size_t i;

    for (i = 0; i < size; ++i) {
        if (bytes[i] != value) {
            return 0;
        }
    }
    return 1;
}

static int CraftExtended_HasWorldStateOverflow(const XwaCraftExtendedState* state) {
    if (state == NULL) {
        return 0;
    }

    if (!CraftExtended_AllBytesEqual(
            &state->componentState[XWAU_XWES_COMPONENT_STATE_FIRST],
            XWAU_XWES_COMPONENT_STATE_COUNT, 0u)) {
        return 1;
    }
    if (!CraftExtended_AllBytesEqual(
            &state->meshRotation[XWAU_XWES_MESH_ROTATION_FIRST],
            XWAU_XWES_MESH_ROTATION_COUNT, 0u)) {
        return 1;
    }
    if (!CraftExtended_AllBytesEqual(
            &state->componentHp[XWAU_XWES_COMPONENT_HP_FIRST],
            XWAU_XWES_COMPONENT_HP_COUNT, 0xffu)) {
        return 1;
    }
    if (!CraftExtended_AllBytesEqual(
            &state->engineEmitterHealth[XWAU_XWES_ENGINE_FIRST],
            XWAU_XWES_ENGINE_COUNT, 0u)) {
        return 1;
    }
    if (!CraftExtended_AllBytesEqual(
            &state->warheadData[XWAU_XWES_WEAPON_FIRST],
            sizeof(WarheadInventoryEntry) * (size_t)XWAU_XWES_WEAPON_COUNT, 0u)) {
        return 1;
    }
    return 0;
}

static uint32_t CraftExtended_CountWorldStateOverflowRecords(void) {
    uint32_t count = 0u;
    uint32_t i;

    if (g_craftExtendedStatePool == NULL) {
        return 0u;
    }
    for (i = 0u; i < g_craftExtendedStateCount; ++i) {
        if (CraftExtended_HasWorldStateOverflow(&g_craftExtendedStatePool[i])) {
            ++count;
        }
    }
    return count;
}

size_t CraftExtended_WorldStateMaxTailSize(uint32_t craftCount) {
    size_t recordsBytes;

    if (craftCount > UINT16_MAX) {
        return 0u;
    }
    if (craftCount != 0u &&
        sizeof(XwaExtendedWorldStateRecordV1) > SIZE_MAX / (size_t)craftCount) {
        return 0u;
    }
    recordsBytes = sizeof(XwaExtendedWorldStateRecordV1) * (size_t)craftCount;
    if (recordsBytes > UINT32_MAX ||
        recordsBytes > SIZE_MAX - sizeof(XwaExtendedWorldStateHeader)) {
        return 0u;
    }
    return sizeof(XwaExtendedWorldStateHeader) + recordsBytes;
}

size_t CraftExtended_WorldStateTailSize(void) {
    uint32_t recordCount = CraftExtended_CountWorldStateOverflowRecords();

    if (recordCount > UINT16_MAX) {
        return 0u;
    }
    return CraftExtended_WorldStateMaxTailSize(recordCount);
}

int CraftExtended_WorldStateBufferSizeWithTail(size_t classicCapacity, uint32_t craftCount,
                                                size_t* outCapacity) {
    size_t tailCapacity;

    if (outCapacity == NULL) {
        return 0;
    }
    *outCapacity = 0u;

    tailCapacity = CraftExtended_WorldStateMaxTailSize(craftCount);
    if (tailCapacity == 0u || classicCapacity > SIZE_MAX - tailCapacity) {
        return 0;
    }

    *outCapacity = classicCapacity + tailCapacity;
    return 1;
}

static void CraftExtended_WriteWorldStateRecord(XwaExtendedWorldStateRecordV1* record,
                                                uint32_t craftOrdinal,
                                                const XwaCraftExtendedState* state) {
    record->craftOrdinal = craftOrdinal;
    memcpy(record->componentState,
           &state->componentState[XWAU_XWES_COMPONENT_STATE_FIRST],
           sizeof(record->componentState));
    memcpy(record->meshRotation,
           &state->meshRotation[XWAU_XWES_MESH_ROTATION_FIRST],
           sizeof(record->meshRotation));
    memcpy(record->componentHp,
           &state->componentHp[XWAU_XWES_COMPONENT_HP_FIRST],
           sizeof(record->componentHp));
    memcpy(record->engineEmitterHealth,
           &state->engineEmitterHealth[XWAU_XWES_ENGINE_FIRST],
           sizeof(record->engineEmitterHealth));
    memcpy(record->warheadData,
           &state->warheadData[XWAU_XWES_WEAPON_FIRST],
           sizeof(record->warheadData));
}

int CraftExtended_WriteWorldStateTail(uint8_t* dst, size_t capacity, size_t* outSize) {
    XwaExtendedWorldStateHeader header;
    uint8_t* cursor;
    uint32_t recordCount;
    uint32_t i;
    size_t requiredSize;

    if (outSize != NULL) {
        *outSize = 0u;
    }
    if (dst == NULL || outSize == NULL) {
        return 0;
    }

    recordCount = CraftExtended_CountWorldStateOverflowRecords();
    if (recordCount > UINT16_MAX) {
        return 0;
    }
    requiredSize = CraftExtended_WorldStateMaxTailSize(recordCount);
    if (requiredSize == 0u || requiredSize > capacity) {
        return 0;
    }

    header.magic = XWAU_EXTENDED_WORLD_STATE_MAGIC;
    header.version = XWAU_EXTENDED_WORLD_STATE_VERSION;
    header.recordCount = (uint16_t)recordCount;
    header.payloadBytes = (uint32_t)(requiredSize - sizeof(header));
    memcpy(dst, &header, sizeof(header));
    cursor = dst + sizeof(header);

    for (i = 0u; i < g_craftExtendedStateCount; ++i) {
        XwaExtendedWorldStateRecordV1 record;
        if (!CraftExtended_HasWorldStateOverflow(&g_craftExtendedStatePool[i])) {
            continue;
        }
        CraftExtended_WriteWorldStateRecord(&record, i, &g_craftExtendedStatePool[i]);
        memcpy(cursor, &record, sizeof(record));
        cursor += sizeof(record);
    }

    *outSize = requiredSize;
    return 1;
}

static uint32_t CraftExtended_ReadWorldStateRecordOrdinal(const uint8_t* recordBytes) {
    uint32_t ordinal;
    memcpy(&ordinal, recordBytes, sizeof(ordinal));
    return ordinal;
}

static int CraftExtended_ValidateWorldStateRecords(const uint8_t* records,
                                                   uint16_t recordCount) {
    uint16_t i;
    uint16_t j;

    for (i = 0u; i < recordCount; ++i) {
        uint32_t currentOrdinal = CraftExtended_ReadWorldStateRecordOrdinal(
            records + sizeof(XwaExtendedWorldStateRecordV1) * (size_t)i);
        if (currentOrdinal >= g_craftExtendedStateCount) {
            return 0;
        }
        for (j = 0u; j < i; ++j) {
            uint32_t previousOrdinal = CraftExtended_ReadWorldStateRecordOrdinal(
                records + sizeof(XwaExtendedWorldStateRecordV1) * (size_t)j);
            if (previousOrdinal == currentOrdinal) {
                return 0;
            }
        }
    }
    return 1;
}

static void CraftExtended_OverlayWorldStateRecord(const XwaExtendedWorldStateRecordV1* record) {
    XwaCraftExtendedState* state = &g_craftExtendedStatePool[record->craftOrdinal];

    memcpy(&state->componentState[XWAU_XWES_COMPONENT_STATE_FIRST],
           record->componentState, sizeof(record->componentState));
    memcpy(&state->meshRotation[XWAU_XWES_MESH_ROTATION_FIRST],
           record->meshRotation, sizeof(record->meshRotation));
    memcpy(&state->componentHp[XWAU_XWES_COMPONENT_HP_FIRST],
           record->componentHp, sizeof(record->componentHp));
    memcpy(&state->engineEmitterHealth[XWAU_XWES_ENGINE_FIRST],
           record->engineEmitterHealth, sizeof(record->engineEmitterHealth));
    memcpy(&state->warheadData[XWAU_XWES_WEAPON_FIRST],
           record->warheadData, sizeof(record->warheadData));
}

int CraftExtended_OverlayWorldStateTail(const uint8_t* src, size_t size, size_t* outConsumed) {
    XwaExtendedWorldStateHeader header;
    const uint8_t* records;
    size_t expectedPayloadBytes;
    size_t expectedSize;
    uint16_t i;

    if (outConsumed != NULL) {
        *outConsumed = 0u;
    }
    if (src == NULL || outConsumed == NULL || size < sizeof(header) ||
        g_craftExtendedStatePool == NULL) {
        return 0;
    }

    memcpy(&header, src, sizeof(header));
    if (header.magic != XWAU_EXTENDED_WORLD_STATE_MAGIC ||
        header.version != XWAU_EXTENDED_WORLD_STATE_VERSION ||
        header.recordCount > g_craftExtendedStateCount) {
        return 0;
    }
    if (header.recordCount != 0u &&
        sizeof(XwaExtendedWorldStateRecordV1) > SIZE_MAX / (size_t)header.recordCount) {
        return 0;
    }
    expectedPayloadBytes = sizeof(XwaExtendedWorldStateRecordV1) * (size_t)header.recordCount;
    if (expectedPayloadBytes > UINT32_MAX || header.payloadBytes != (uint32_t)expectedPayloadBytes ||
        expectedPayloadBytes > SIZE_MAX - sizeof(header)) {
        return 0;
    }
    expectedSize = sizeof(header) + expectedPayloadBytes;
    if (expectedSize != size) {
        return 0;
    }

    records = src + sizeof(header);
    if (!CraftExtended_ValidateWorldStateRecords(records, header.recordCount)) {
        return 0;
    }

    for (i = 0u; i < header.recordCount; ++i) {
        XwaExtendedWorldStateRecordV1 record;
        memcpy(&record, records + sizeof(record) * (size_t)i, sizeof(record));
        CraftExtended_OverlayWorldStateRecord(&record);
    }

    *outConsumed = expectedSize;
    return 1;
}
