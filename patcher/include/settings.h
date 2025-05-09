#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include "gs.h"
#include <stdint.h>

#define CUSTOM_ITEMS 250 // Max number of items in custom menu
#define NAME_LEN 80      // Max menu item length (incl. the string terminator)

typedef enum {
  FLAG_CUSTOM_MENU = (1 << 0),      // Apply menu patches
  FLAG_SKIP_DISC = (1 << 1),        // Disable disc autolaunch
  FLAG_SKIP_SCE_LOGO = (1 << 2),    // Skip SCE logo on boot
  FLAG_BOOT_BROWSER = (1 << 3),     // Boot directly to MC browser
  FLAG_SCROLL_MENU = (1 << 4),      // Enable infinite scrolling
  FLAG_SKIP_PS2_LOGO = (1 << 5),    // Skip PS2LOGO when booting discs
  FLAG_DISABLE_GAMEID = (1 << 6),   // Disable PixelFX game ID
  FLAG_USE_DKWDRV = (1 << 7),       // Use DKWDRV for PS1 discs
  FLAG_BROWSER_LAUNCHER = (1 << 8), // Apply patches for launching applications from the Browser
} PatcherFlags;

// Patcher settings struct, contains all configurable patch settings and menu items
typedef struct {
  uint32_t colorSelected[4];                 // The menu items color when selected
  uint32_t colorUnselected[4];               // The menu items color when not selected
  int menuX;                                 // Menu X coordinate (menu center)
  int menuY;                                 // Menu Y coordinate (menu center), only for scroll menu
  int enterX;                                // "Enter" button X coordinate (at main OSDSYS menu)
  int enterY;                                // "Enter" button Y coordinate (at main OSDSYS menu)
  int versionX;                              // "Version" button X coordinate (at main OSDSYS menu)
  int versionY;                              // "Version" button Y coordinate (at main OSDSYS menu)
  int cursorMaxVelocity;                     // The cursors movement amplitude, only for scroll menu
  int cursorAcceleration;                    // The cursors speed, only for scroll menu
  int displayedItems;                        // The number of menu items displayed, only for scroll menu
  int menuItemIdx[CUSTOM_ITEMS];             // Item index in the config file
  int menuItemCount;                         // Total number of valid menu items
  uint16_t patcherFlags;                     // Patcher options
  char leftCursor[20];                       // The left cursor text, only for scroll menu
  char rightCursor[20];                      // The right cursor text, only for scroll menu
  char menuDelimiterTop[NAME_LEN];           // The top menu delimiter text, only for scroll menu
  char menuDelimiterBottom[NAME_LEN];        // The bottom menu delimiter text, only for scroll menu
  char menuItemName[CUSTOM_ITEMS][NAME_LEN]; // Menu items text
#ifndef HOSD
  char launcherPath[50]; // Path to launcher ELF
  char dkwdrvPath[50];   // Path to DKWDRV
#endif
  char romver[15];       // ROMVER string, initialized before patching
  uint8_t mcSlot;        // Memory card slot contaning currently loaded OSDMENU.CNF
  GSVideoMode videoMode; // OSDSYS Video mode (0 for auto)
} PatcherSettings;

// Stores patcher settings and OSDSYS menu items
extern PatcherSettings settings;

int loadConfig(void);
void initConfig(void);

#endif
