from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
opt = (root / 'src/xwa/assets/opt_model.c').read_text(encoding='utf-8')
fed = (root / 'src/xwa/flight/fediskio.c').read_text(encoding='utf-8')
failures = []

width, height = 1024, 512
texels = 0
for _ in range(6):
    texels += width * height
    width >>= 1
    height >>= 1
if texels != 698_880:
    failures.append(f'test bug: expected 698880 texels, got {texels}')

checks_opt = {
    'fixed 87360-byte lightmap scratch still present': 'g_d3dLightmapScratchPixels[87360]' not in opt,
    'missing overflow-safe mip texel count helper': 'D3DInfo_ComputeMipTexelCount' in opt,
    'mip-level selection still uses signed width*height multiplication': '(uint64_t)(uint32_t)textureData->width' in opt,
    'lightmap palette remap index is not range-guarded': 'lightmapRemapIndex >= 4096' in opt,
    'lightmap scratch is not sized from actual mip chain': 'malloc(mipTexelCount)' in opt,
    'mip offset is not size_t': 'size_t offset = 0;' in opt,
    'dynamic lightmap scratch is not released': 'free(lightmapScratchPixels);' in opt,
    'texture upload capacity is not guarded': 'textureCount >= maxTextureIds' in opt,
    'full 0x2000-byte OPT palette range is not validated': 'textureData->paletteAddress, 0x2000' in opt,
    'texture payload address is not validated': 'Invalid OPT texture payload address' in opt,
    'texture alpha address is not validated': 'Invalid OPT texture alpha address' in opt,
    'texture nodes are not counted dynamically': 'OptModel_CountTextureNodesRecursive' in opt,
    'legacy fixed 200-entry texture id array still present': 'intptr_t textureIds[200]' not in opt,
    'dynamic texture id storage is missing': 'calloc((size_t)textureCapacity' in opt,
    'dynamic texture id storage is not released': 'free(textureIds);' in opt,
}
for msg, ok in checks_opt.items():
    if not ok:
        failures.append(msg)

checks_fed = {
    'missing XWAU CockpitPov loader': 'FeDiskIo_ApplyCockpitPov' in fed,
    'missing disabled [CockpitPov;] compatibility fallback': '"CockpitPov;"' in fed,
    'CockpitPov X is not applied to model hardpoint': 'primaryHardpointX = (int16_t)selected->values[0]' in fed,
    'CockpitPov Y is not applied to model hardpoint': 'primaryHardpointY = (int16_t)selected->values[1]' in fed,
    'CockpitPov Z is not applied to model hardpoint': 'primaryHardpointZ = (int16_t)selected->values[2]' in fed,
    'cockpit model load does not apply authored CockpitPov': 'FeDiskIo_ApplyCockpitPov(modelName' in fed,
}
for msg, ok in checks_fed.items():
    if not ok:
        failures.append(msg)

if failures:
    for item in failures:
        print('FAIL:', item, file=sys.stderr)
    raise SystemExit(1)
print('PASS: XWAU HD texture + cockpit POV source contracts')
