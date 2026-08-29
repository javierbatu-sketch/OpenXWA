#ifndef XWA_RUNTIME_INPUT_RUMBLE_PROVIDER_H
#define XWA_RUNTIME_INPUT_RUMBLE_PROVIDER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Registers XWA's controller rumble with the Aeron DirectInput force-feedback
 * shim (AeronCompat_SetRumbleProvider), routing to XwaControllerMapping. Call
 * once at port startup. */
void XwaRumble_RegisterProvider(void);

#ifdef __cplusplus
}
#endif

#endif
