#ifndef XWA_RUNTIME_COMPAT_DIRECTX_DX_WIN_TYPES_H
#define XWA_RUNTIME_COMPAT_DIRECTX_DX_WIN_TYPES_H

#include "aeron/dx5/win_types.h"

/* DirectInput and DirectSound remain game-owned and retain their established
 * ABI spelling while sharing the same calling convention definition. */
#define XWA_DXAPI AERON_DXAPI

#endif
