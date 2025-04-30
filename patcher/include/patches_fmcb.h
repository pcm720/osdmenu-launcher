#ifndef _PATCHES_FMCB_H_
#define _PATCHES_FMCB_H_
#include "gs.h"
#include <stdint.h>

// Patches OSD menu to include custom menu entries
void patchMenu(uint8_t *osd);

// Patches menu drawing functions
void patchMenuDraw(uint8_t *osd);

// Patches OSDSYS button prompts
void patchMenuButtonPanel(uint8_t *osd);

// Patches menu scrolling
void patchMenuInfiniteScrolling(uint8_t *osd);

// Working patches
// Patches the disc launch handlers to load discs with the launcher
void patchDiscLaunch(uint8_t *osd);

// Patches automatic disc launch
void patchSkipDisc(uint8_t *osd);

// Forces the video mode
void patchVideoMode(uint8_t *osd, GSVideoMode mode);

#endif
