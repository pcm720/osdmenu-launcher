#ifndef _OSDMENU_PATCHES_H_
#define _OSDMENU_PATCHES_H_
// Additional patch patterns for osdmenu-launcher
#include "gs.h"
#include <stdint.h>

// Extends version menu with custom entries
void patchVersionInfo(uint8_t *osd);

// Overrides SetGsCrt and sceGsPutDispEnv functions to support 480p and 1080i output modes
// ALWAYS call restoreGSVideoMode before launching apps
void patchGSVideoMode(uint8_t *osd, GSVideoMode outputMode);

// Restores SetGsCrt.
// Can be safely called even if GS video mode patch wasn't applied
void restoreGSVideoMode();

#endif
