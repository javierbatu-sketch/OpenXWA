#ifndef XWA_REMASTER_XWAU_MATERIAL_H
#define XWA_REMASTER_XWAU_MATERIAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XWA_XWAU_MATERIAL_NAME_MAX 96
#define XWA_XWAU_MATERIAL_NORMAL_MAP_MAX 256

typedef struct XwaXwauMaterialResolved {
    int has_glossiness;
    float glossiness;
    int has_intensity;
    float intensity;
    int has_metallic;
    float metallic;
    int has_nm_intensity;
    float nm_intensity;
    int has_ambient;
    float ambient;
    int has_normal_map;
    char normal_map[XWA_XWAU_MATERIAL_NORMAL_MAP_MAX];
    int has_no_bloom;
    int no_bloom;
    int has_shadeless;
    int shadeless;
} XwaXwauMaterialResolved;

typedef struct XwaXwauMaterialSection {
    char** names;
    size_t name_count;
    XwaXwauMaterialResolved values;
} XwaXwauMaterialSection;

typedef struct XwaXwauMaterialFile {
    XwaXwauMaterialResolved defaults;
    XwaXwauMaterialSection* sections;
    size_t section_count;
    size_t section_capacity;
} XwaXwauMaterialFile;

int XwaXwauMaterial_ParseText(const char* text, size_t size,
                              XwaXwauMaterialFile* out,
                              char* error, size_t error_size);

int XwaXwauMaterial_Resolve(const XwaXwauMaterialFile* file,
                            const char* material_name,
                            XwaXwauMaterialResolved* out,
                            char* error, size_t error_size);

void XwaXwauMaterial_Free(XwaXwauMaterialFile* file);

#ifdef __cplusplus
}
#endif

#endif
