#ifndef XWA_RUNTIME_INPUT_WINMM_JOYSTICK_PROVIDER_H
#define XWA_RUNTIME_INPUT_WINMM_JOYSTICK_PROVIDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* XWA's controller source for the Aeron WinMM joystick shim: fills the shim's
 * logical state from the controller selected and mapped by the modern YAML
 * input options, and captures the diagnostic trace sample below. */

/* Latest successful Aeron-to-WinMM axis conversion, for modern diagnostics. */
typedef struct WinmmJoystickTraceSample {
	uint32_t deviceId;
	int8_t sourceAxisX;
	int8_t sourceAxisY;
	int8_t sourceAxisR;
	int16_t sourceValueX;
	int16_t sourceValueY;
	int16_t sourceValueR;
	uint32_t winmmX;
	uint32_t winmmY;
	uint32_t winmmR;
} WinmmJoystickTraceSample;

/* Registers the source with AeronCompat_SetJoystickSource. Call once at port
 * startup, before recovered code polls the joystick API. */
void XwaWinmmJoystick_RegisterSource(void);

/* Copies the most recent successful conversion. */
int WinmmJoystick_GetLastTraceSample(WinmmJoystickTraceSample* sample);
/* Clears the diagnostic sample after controller selection or mapping changes. */
void WinmmJoystick_ResetTrace(void);

#ifdef __cplusplus
}
#endif

#endif
