#ifndef XWA_FLIGHT_OBJECT_CRAFT_EXTENDED_STATE_H
#define XWA_FLIGHT_OBJECT_CRAFT_EXTENDED_STATE_H

#include "xwa/flight/object/object.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    XWAU_RENDERABLE_MESH_COUNT = 254,
    XWAU_COMPONENT_STATE_COUNT = 255,
    XWAU_SPECIAL_COMPONENT_STATE_INDEX = 254,
    XWAU_ENGINE_EMITTER_COUNT = 255,
    XWAU_WEAPON_RACK_LIMIT = 120,
    XWAU_WEAPON_STATE_CAPACITY = 128,
};

typedef struct XwaCraftExtendedState {
    uint8_t componentState[XWAU_COMPONENT_STATE_COUNT];
    uint8_t meshRotation[XWAU_COMPONENT_STATE_COUNT];
    uint8_t componentHp[XWAU_COMPONENT_STATE_COUNT];
    uint8_t engineEmitterHealth[XWAU_ENGINE_EMITTER_COUNT];
    WarheadInventoryEntry warheadData[XWAU_WEAPON_STATE_CAPACITY];
} XwaCraftExtendedState;

enum {
    XWAU_EXTENDED_WORLD_STATE_MAGIC = 0x53455758u, /* 'XWES' little-endian */
    XWAU_EXTENDED_WORLD_STATE_VERSION = 1u,
};

#pragma pack(push, 1)
typedef struct XwaExtendedWorldStateHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t recordCount;
    uint32_t payloadBytes;
} XwaExtendedWorldStateHeader;
#pragma pack(pop)

int CraftExtended_Allocate(uint32_t craftCount);
void CraftExtended_Free(void);
void CraftExtended_ResetAll(void);
XwaCraftExtendedState* CraftExtended_Get(CraftData* craft);
const XwaCraftExtendedState* CraftExtended_GetConst(const CraftData* craft);

void CraftExtended_ResetCraft(CraftData* craft);
void CraftExtended_ClearLegacyReusablePrefix(CraftData* craft);
void CraftExtended_ResetComponentArrays(CraftData* craft);
void CraftExtended_ResetWeaponState(CraftData* craft);
void CraftExtended_Copy(CraftData* dst, const CraftData* src);
void CraftExtended_SeedFromLegacy(CraftData* craft);
void CraftExtended_ProjectToLegacy(const CraftData* craft, CraftData* legacyImage);

size_t CraftExtended_WorldStateMaxTailSize(uint32_t craftCount);
int CraftExtended_WorldStateBufferSizeWithTail(size_t classicCapacity, uint32_t craftCount, size_t* outCapacity);
size_t CraftExtended_WorldStateTailSize(void);
int CraftExtended_WriteWorldStateTail(uint8_t* dst, size_t capacity, size_t* outSize);
int CraftExtended_OverlayWorldStateTail(const uint8_t* src, size_t size, size_t* outConsumed);

uint8_t* CraftExtended_MeshComponentStateRef(CraftData* craft, uint16_t meshIndex);
const uint8_t* CraftExtended_MeshComponentStateRefConst(const CraftData* craft, uint16_t meshIndex);
uint8_t CraftExtended_GetMeshComponentState(const CraftData* craft, uint16_t meshIndex);
int CraftExtended_SetMeshComponentState(CraftData* craft, uint16_t meshIndex, uint8_t value);
uint8_t CraftExtended_GetSpecialComponentState(const CraftData* craft);
int CraftExtended_SetSpecialComponentState(CraftData* craft, uint8_t value);
int CraftExtended_SetDetachedPostMeshState(CraftData* craft, uint16_t meshCount, uint8_t value);

uint8_t* CraftExtended_MeshRotationRef(CraftData* craft, uint16_t meshIndex);
const uint8_t* CraftExtended_MeshRotationRefConst(const CraftData* craft, uint16_t meshIndex);
uint8_t CraftExtended_GetMeshRotation(const CraftData* craft, uint16_t meshIndex);
int CraftExtended_SetMeshRotation(CraftData* craft, uint16_t meshIndex, uint8_t value);

uint8_t* CraftExtended_ComponentHpRef(CraftData* craft, uint16_t meshIndex);
const uint8_t* CraftExtended_ComponentHpRefConst(const CraftData* craft, uint16_t meshIndex);
uint8_t CraftExtended_GetComponentHp(const CraftData* craft, uint16_t meshIndex);
int CraftExtended_SetComponentHp(CraftData* craft, uint16_t meshIndex, uint8_t value);

uint8_t* CraftExtended_EngineEmitterHealthRef(CraftData* craft, uint16_t emitterIndex);
const uint8_t* CraftExtended_EngineEmitterHealthRefConst(const CraftData* craft, uint16_t emitterIndex);
uint8_t CraftExtended_GetEngineEmitterHealth(const CraftData* craft, uint16_t emitterIndex);
int CraftExtended_SetEngineEmitterHealth(CraftData* craft, uint16_t emitterIndex, uint8_t value);

WarheadInventoryEntry* CraftExtended_GetWeaponEntry(CraftData* craft, uint16_t weaponIndex);
const WarheadInventoryEntry* CraftExtended_GetWeaponEntryConst(const CraftData* craft, uint16_t weaponIndex);

#ifdef __cplusplus
}
#endif

#endif
