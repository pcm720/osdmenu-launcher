// FMCB 1.8 OSDSYS patches by Neme
// FMCB 1.9 patches by sp193
#include "patches_fmcb.h"
#include "init.h"
#include "loader.h"
#include "patches_common.h"
#include "patches_osdmenu.h"
#include "patterns_fmcb.h"
#include "settings.h"
#include <kernel.h>
#include <loadfile.h>
#include <stdlib.h>
#include <string.h>

static uint32_t osdMenu[4 + CUSTOM_ITEMS * 3];

struct OSDMenuInfo {
  uint32_t unknown1;
  uint32_t *menuPtr;
  uint32_t entryCount;
  uint32_t unknown2;
  uint32_t currentEntry;
};

static struct OSDMenuInfo *menuInfo = NULL;
#define OSD_MAGIC 0x39390000 // arbitrary number to identify added menu items

// Handles custom menu entries
int handleMenuEntry(int selected) {
  if (selected == 1)
    return 1;

  if (selected >= 2 + settings.menuItemCount)
    return 0;

  if (selected - 2 >= 0) {
    // Build 'fmcb<mc slot>:<idx>' string for the launcher
    int idx = settings.menuItemIdx[selected - 2];

    char *item = malloc(10 * sizeof(char));
    item[0] = 'f';
    item[1] = 'm';
    item[2] = 'c';
    item[3] = 'b';
    item[4] = '0' + settings.mcSlot;
    item[5] = ':';

    if (idx < 10) {
      // For single-digit numbers
      item[6] = '0' + idx;
      item[7] = '\0';
    } else if (idx < 100) {
      // For two-digit numbers
      item[6] = '0' + (idx / 10);
      item[7] = '0' + (idx % 10);
      item[8] = '\0';
    } else {
      // For three-digit numbers
      item[6] = '0' + (idx / 100);
      item[7] = '0' + ((idx / 10) % 10);
      item[8] = '0' + (idx % 10);
      item[9] = '\0';
    }

    launchItem(item);
  }
  return 0;
}

// Returns the pointer to OSD string
const char *getStringPointer(const char **strings, uint32_t index) {
  if ((index & 0xffff0000) == OSD_MAGIC)
    return settings.menuItemName[index & 0xffff];

  return strings[index];
}

// Patches OSD menu to include custom menu entries
void patchMenu(uint8_t *osd) {
  uint8_t *ptr;
  uint32_t tmp, menuAddr, osdstrAddr, entryAddr, i;

  // Try to find the menu info struct
  for (tmp = 0; tmp < 0x100000; tmp = (uint32_t)(ptr - osd + 4)) {
    ptr = findPatternWithMask(osd + tmp, 0x100000 - tmp, (uint8_t *)patternMenuInfo, (uint8_t *)patternMenuInfo_mask, sizeof(patternMenuInfo));
    if (!ptr)
      return;

    // Found if the current address points to the pointer to "Browser" string
    if (_lw((uint32_t)ptr + 4) == (uint32_t)ptr - 4 * 4)
      break;
  }
  menuAddr = (uint32_t)ptr;

  menuInfo = (struct OSDMenuInfo *)menuAddr;

  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternOSDString, (uint8_t *)patternOSDString_mask, sizeof(patternOSDString));
  if (!ptr)
    return;
  osdstrAddr = (uint32_t)ptr;

  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternUserInputHandler, (uint8_t *)patternUserInputHandler_mask,
                            sizeof(patternUserInputHandler));
  if (!ptr)
    return;
  entryAddr = (uint32_t)ptr;

  // Patch the OSD string function
  tmp = 0x0c000000;
  tmp |= ((uint32_t)getStringPointer >> 2);
  _sw(0x0200282d, osdstrAddr + 2 * 4); // daddu  a1, s0, zero
                                       // lw     a0, $xxxx(v0)
  _sw(tmp, osdstrAddr + 4 * 4);        // jal    getStringPointer
  _sw(0x00000000, osdstrAddr + 5 * 4); // nop

  // Patch the user input handling function
  tmp = 0x0c000000;
  tmp |= ((uint32_t)handleMenuEntry >> 2);
  _sw(tmp, entryAddr + 2 * 4);           // jal    handleMenuEntry
  tmp = _lw(entryAddr + 3 * 4) & 0xffff; //
  tmp |= 0x8c440000;                     //
  _sw(tmp, entryAddr + 3 * 4);           // lw     a0, $xxxx(v0)
  _sw(0x1040000a, entryAddr + 4 * 4);    // beq    v0, zero, exit

  // Build the OSD menu
  osdMenu[0] = _lw(menuAddr - 4 * 4); // "Browser"
  osdMenu[1] = _lw(menuAddr - 3 * 4);
  osdMenu[2] = _lw(menuAddr - 2 * 4); // "System Configuration"
  osdMenu[3] = _lw(menuAddr - 1 * 4);

  for (i = 0; i < settings.menuItemCount; i++) {
    osdMenu[4 + i * 2] = OSD_MAGIC + i;
    osdMenu[5 + i * 2] = 0;
  }

  menuInfo->menuPtr = osdMenu;                       // store menu pointer
  menuInfo->entryCount = 2 + settings.menuItemCount; // store number of menu items
}

static uint32_t colorSelected[4] __attribute__((aligned(16)));
static uint32_t colorUnselected[4] __attribute__((aligned(16)));

static void (*DrawMenuItem)(int X, int Y, uint32_t *color, int alpha, const char *string);
// Protokernel DrawMenuItem expects a pointer to string address, not the string address
// For later consoles, this function is set to DrawMenuItem
static void (*DrawMenuItemStringPtr)(int X, int Y, uint32_t *color, int alpha, const char *string);
static int dx = 0;
static int vel, acc;
static int offsY = 0;
static int fontHeight = 16;

// Draws selected items
void drawMenuItemSelected(int X, int Y, uint32_t *color, int alpha, const char *string, int num) {
  int i;

  for (i = 0; i < 4; i++)
    colorSelected[i] = settings.colorSelected[i];

  if (alpha > 0x80)
    alpha = 0x80;

  if (!(settings.patcherFlags & FLAG_SCROLL_MENU)) { // Old style menu
    DrawMenuItem(settings.menuX, Y - settings.menuItemCount * 10, colorSelected, alpha, string);
  } else { // New style menu
    if (num == 0) {
      int amount;
      if (offsY < 0) {
        amount = -offsY >> 2;
        offsY += (amount > 0 ? amount : 1);
      } else if (offsY > 0) {
        amount = offsY >> 2;
        offsY -= (amount > 0 ? amount : 1);
      }
    }
    Y = (num << 1) - offsY;
    if ((Y < ((settings.displayedItems + 1) * (fontHeight / 2))) && (Y > -((settings.displayedItems + 1) * (fontHeight / 2)))) {
      vel -= acc;
      if (vel < -settings.cursorMaxVelocity || vel > settings.cursorMaxVelocity)
        acc = -acc;
      dx += vel;
      DrawMenuItem(settings.menuX, settings.menuY + Y, colorSelected, alpha, string);
      DrawMenuItemStringPtr(settings.menuX - 220 + (dx >> 8), settings.menuY + Y, colorSelected, alpha, settings.leftCursor);
      DrawMenuItemStringPtr(settings.menuX + 220 - (dx >> 8), settings.menuY + Y, colorSelected, alpha, settings.rightCursor);
    }
    DrawMenuItemStringPtr(settings.menuX, settings.menuY - (settings.displayedItems * (fontHeight / 2) + (fontHeight / 2)), colorSelected, alpha,
                          settings.menuDelimiterTop);
    DrawMenuItemStringPtr(settings.menuX, settings.menuY + (settings.displayedItems * (fontHeight / 2) + (fontHeight / 2)), colorSelected, alpha,
                          settings.menuDelimiterBottom);
  }
}

// Draws unselected items
void drawMenuItemUnselected(int X, int Y, uint32_t *color, int alpha, const char *string, int num) {
  int i;

  for (i = 0; i < 4; i++)
    colorUnselected[i] = settings.colorUnselected[i];

  if (!(settings.patcherFlags & FLAG_SCROLL_MENU)) { // Old style menu
    DrawMenuItem(settings.menuX, Y - settings.menuItemCount * 10, colorUnselected, alpha, string);
  } else { // New style menu
    if (num == 0) {
      int amount, destY = menuInfo->currentEntry << 4;
      if (offsY < destY) {
        amount = (destY - offsY) >> 2;
        offsY += (amount > 0 ? amount : 1);
      } else if (offsY > destY) {
        amount = (offsY - destY) >> 2;
        offsY -= (amount > 0 ? amount : 1);
      }
    }
    Y = (num << 1) - offsY;
    if ((Y < ((settings.displayedItems + 1) * (fontHeight / 2))) && (Y > -((settings.displayedItems + 1) * (fontHeight / 2)))) {
      if (Y < 0)
        alpha = 128 + (Y * (128 / ((settings.displayedItems + 1) * (fontHeight / 2))));
      else
        alpha = 128 - (Y * (128 / ((settings.displayedItems + 1) * (fontHeight / 2))));
      if (alpha < 0)
        alpha = 0;

      DrawMenuItem(settings.menuX, settings.menuY + Y, colorUnselected, alpha, string);
    }
  }
}

// Patches menu drawing functions
// You can change menu items' color and position for selected and unselected
// items separately in
// drawMenuItemSelected()
// drawMenuItemUnselected()
//
// Be careful what you write in these functions as they get called every
// frame for every menu item!  For positioning the menu, update both
// functions with the same calculations, using the X/Y variables.
//
// Default values of the variables in V12 OSDSYS:
// X = 430 (this is the center of the menu)
// Y = 110 (this is the Y of the first item)
// alpha = 128 (smaller value is more transparency)
void patchMenuDraw(uint8_t *osd) {
  uint8_t *ptr;
  uint32_t tmp, pSelItem, pUnselItem;

  vel = settings.cursorMaxVelocity;
  acc = settings.cursorAcceleration;

  settings.displayedItems |= 1; // must be odd value
  if (settings.displayedItems < 1)
    settings.displayedItems = 1;
  if (settings.displayedItems > 15)
    settings.displayedItems = 15;

  if (!menuInfo)
    return;

  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternDrawMenuItem, (uint8_t *)patternDrawMenuItem_mask, sizeof(patternDrawMenuItem));
  if (!ptr)
    return;
  pSelItem = (uint32_t)ptr; // code for selected menu item

  ptr = findPatternWithMask(ptr + 4, 256, (uint8_t *)patternDrawMenuItem, (uint8_t *)patternDrawMenuItem_mask, sizeof(patternDrawMenuItem));
  if (ptr != (uint8_t *)(pSelItem + 48))
    return;
  pUnselItem = (uint32_t)ptr; // code for unselected menu item

  tmp = _lw(pSelItem + 32); // get the OSD's DrawMenuItem function pointer
  tmp &= 0x03ffffff;
  tmp <<= 2;
  DrawMenuItem = (void *)tmp;
  DrawMenuItemStringPtr = DrawMenuItem;

  tmp = 0x0c000000;
  tmp |= ((uint32_t)drawMenuItemSelected >> 2);
  _sw(tmp, pSelItem + 32); // overwrite the function call for selected item

  tmp = 0x0c000000;
  tmp |= ((uint32_t)drawMenuItemUnselected >> 2);
  _sw(tmp, pUnselItem + 32); // overwrite the function call for unselected item

  _sw(0x001048c0, pSelItem);       // make menu item's number the sixth param
  _sw(0x01231021, pSelItem + 4);   // by loading it into t1 (multiplied by 8):
  _sw(0x001048c0, pUnselItem);     // sll   t1, s0, 3
  _sw(0x01231021, pUnselItem + 4); // addu  v0, t1, v1
}

static void (*DrawNonSelectableItem)(int X, int Y, uint32_t *color, int alpha, const char *string);
static void (*DrawIcon)(int type, int X, int Y, int alpha);
static void (*DrawButtonPanel_1stfunc)(void);

int ButtonsPanel_Type = 0;

#define MAINMENU_PANEL 1
#define SYSCONF_PANEL 8

// getButtonsPanelType() is called prior to functions above and determine panel type :
// 	- Main menu type : 1
// 	- Sys conf screen type : 8
void getButtonsPanelType(int type) {
  // Get Buttons Panel type (catch it in a0)
  ButtonsPanel_Type = type;

  // Call original function that was overridden
  DrawButtonPanel_1stfunc();
}

// drawNonselectableEntryLeft() is called for all items less the last
void drawNonselectableEntryLeft(int X, int Y, uint32_t *color, int alpha, const char *string) {
  if (ButtonsPanel_Type == MAINMENU_PANEL) {
    if (settings.enterX == -1)
      settings.enterX = X - 28;

    if (settings.enterY == -1)
      settings.enterY = Y;

    DrawNonSelectableItem(settings.enterX + 28, settings.enterY, color, alpha, string);
  } else
    DrawNonSelectableItem(X, Y, color, alpha, string);
}

// drawNonselectableEntryRight() is called only for last item (Options or Version)
void drawNonselectableEntryRight(int X, int Y, uint32_t *color, int alpha, const char *string) {
  if (ButtonsPanel_Type == MAINMENU_PANEL) {
    if (settings.versionX == -1)
      settings.versionX = X - 28;

    if (settings.versionY == -1)
      settings.versionY = Y;

    DrawNonSelectableItem(settings.versionX + 28, settings.versionY, color, alpha, string);
  } else
    DrawNonSelectableItem(X, Y, color, alpha, string);
}

// drawIconLeft() is called for all button icons less the last
void drawIconLeft(int type, int X, int Y, int alpha) {
  if (ButtonsPanel_Type == MAINMENU_PANEL) {
    if (settings.enterX == -1)
      settings.enterX = X;

    if (settings.enterY == -1)
      settings.enterY = Y;

    DrawIcon(type, settings.enterX, settings.enterY, alpha);
  } else
    DrawIcon(type, X, Y, alpha);
}

// drawIconRight() is called for only for last button icon (Options or Version)
void drawIconRight(int type, int X, int Y, int alpha) {
  if (ButtonsPanel_Type == MAINMENU_PANEL) {
    if (settings.versionX == -1)
      settings.versionX = X;

    if (settings.versionY == -1)
      settings.versionY = Y;

    DrawIcon(type, settings.versionX, settings.versionY, alpha);
  } else
    DrawIcon(type, X, Y, alpha);
}

// Patches OSDSYS button prompts
//
// OSDSYS defaults are
// 	- Enter icon X : 188
// 	- Enter icon Y : 230 on PAL
// 	- Enter text X : 216
// 	- Enter text Y : 230 on PAL
// 	- Version icon X : 501
// 	- Version icon Y : 230 on PAL
// 	- Version text X : 529
// 	- Version text Y : 230 on PAL
void patchMenuButtonPanel(uint8_t *osd) {
  uint8_t *ptr;
  uint8_t *firstPtr;
  uint32_t tmp;
  uint32_t addr, pButtonsPanelType, pBottomRightIcon, pBottomLeftIcon, pBottomRightItem, pBottomLeftItem;
  uint32_t pattern[1];
  uint32_t mask[1];

  // Search and overwrite 1st function call in DrawButtonPanel function
  firstPtr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternDrawButtonPanel_1, (uint8_t *)patternDrawButtonPanel_1_mask,
                                 sizeof(patternDrawButtonPanel_1));
  if (!firstPtr)
    return;
  pButtonsPanelType = (uint32_t)firstPtr;

  tmp = _lw(pButtonsPanelType + 32);
  tmp &= 0x03ffffff;
  tmp <<= 2;
  DrawButtonPanel_1stfunc = (void *)tmp; // get original function call

  tmp = 0x0c000000;
  tmp |= ((uint32_t)getButtonsPanelType >> 2);
  _sw(tmp, pButtonsPanelType + 32); // overwrite the function call

  // Search and overwrite 1st DrawIcon function call in DrawButtonPanel function
  ptr = findPatternWithMask(firstPtr, 0x1000, (uint8_t *)patternDrawButtonPanel_2, (uint8_t *)patternDrawButtonPanel_2_mask,
                            sizeof(patternDrawButtonPanel_2));
  if (!ptr)
    return;
  pBottomRightIcon = (uint32_t)ptr; // code for bottom right icon

  tmp = _lw(pBottomRightIcon + 24);
  addr = tmp; // save function call code
  tmp &= 0x03ffffff;
  tmp <<= 2;
  DrawIcon = (void *)tmp;

  tmp = 0x0c000000;
  tmp |= ((uint32_t)drawIconRight >> 2);
  _sw(tmp, pBottomRightIcon + 24); // overwrite the function call for bottom right icon

  // Make pattern with function call code saved above
  pattern[0] = addr;
  mask[0] = 0xffffffff;

  // Search and overwrite 2nd DrawIcon function call in DrawButtonPanel function
  ptr = findPatternWithMask(ptr + 28, 0x1000, (uint8_t *)pattern, (uint8_t *)mask, sizeof(pattern));
  if (!ptr)
    return;
  pBottomLeftIcon = (uint32_t)ptr; // code for bottom left icons

  tmp = 0x0c000000;
  tmp |= ((uint32_t)drawIconLeft >> 2);
  _sw(tmp, pBottomLeftIcon); // overwrite the function call for bottom left icons

  // Search and overwrite 1st DrawNonSelectableItem function call in DrawButtonPanel function
  ptr = findPatternWithMask(firstPtr, 0x1000, (uint8_t *)patternDrawButtonPanel_3, (uint8_t *)patternDrawButtonPanel_3_mask,
                            sizeof(patternDrawButtonPanel_3));
  if (!ptr)
    return;
  pBottomRightItem = (uint32_t)ptr; // code for bottom right item

  tmp = _lw(pBottomRightItem + 20); // get the OSD's DrawNonSelectableItem function pointer
  addr = tmp;                       // save function call code
  tmp &= 0x03ffffff;
  tmp <<= 2;
  DrawNonSelectableItem = (void *)tmp;

  tmp = 0x0c000000;
  tmp |= ((uint32_t)drawNonselectableEntryRight >> 2);
  _sw(tmp, pBottomRightItem + 20); // overwrite the function call for bottom right item

  // Make pattern with function call code saved above
  pattern[0] = addr;
  mask[0] = 0xffffffff;

  // Search and overwrite 2nd DrawNonSelectableItem function call in DrawButtonPanel function
  ptr = findPatternWithMask(ptr + 24, 0x1000, (uint8_t *)pattern, (uint8_t *)mask, sizeof(pattern));
  if (!ptr)
    return;
  pBottomLeftItem = (uint32_t)ptr; // code for bottom left item

  tmp = 0x0c000000;
  tmp |= ((uint32_t)drawNonselectableEntryLeft >> 2);
  _sw(tmp, pBottomLeftItem); // overwrite the function call for bottom left item
}

// An array that stores what function to call for each disc type.
// HDD-OSD:
// uint32_t *discLaunchHandlers[8] = {
//    exec_ps2_game_disc,		// PS2 game
//    exec_ps2_game_disc,		// PS2 game
//    exec_ps1_game_disc,		// PS1 game CD
//    exec_dvdv_disc,			  // DVD Video
//    do_nothing,				    // none (return 1)
//    do_nothing,				    // none (return 1)
//    exec_hdd_stuff			  // HDDLOAD
//    exec_hdd_app          // Execute HDD application
//}
// Patches the disc launch handlers to load discs with the launcher
void patchDiscLaunch(uint8_t *osd) {
  uint8_t *ptr;
  uint32_t tmp, pFn;
  static uint32_t *discLaunchHandlers = NULL;

  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternExecuteDisc, (uint8_t *)patternExecuteDisc_mask, sizeof(patternExecuteDisc));
  if (!ptr)
    return;

  pFn = (uint32_t)ptr; // address of the ExecuteDisc function

  tmp = _lw(pFn + 40) << 16;
  tmp += (int16_t)(_lw(pFn + 44) & 0xffff);
  discLaunchHandlers = (uint32_t *)tmp;

  discLaunchHandlers[0] = (uint32_t)launchDisc; // Overwrite PS2 DVD function pointer
  discLaunchHandlers[1] = (uint32_t)launchDisc; // Overwrite PS2 CD function pointer
  discLaunchHandlers[2] = (uint32_t)launchDisc; // Overwrite PS1 function pointer
}

// Patches automatic disc launch
void patchSkipDisc(uint8_t *osd) {
  uint8_t *ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternDetectDisc, (uint8_t *)patternDetectDisc_mask, sizeof(patternDetectDisc));
  if (!ptr)
    return;

  // NOP out the jump table
  for (uint32_t i = 0; i < (sizeof(patternDetectDisc) / sizeof(uint32_t)); i++)
    _sw(0, (uint32_t)ptr + i * 4);

  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternDetectDisc_Clock, (uint8_t *)patternDetectDisc_Clock_mask,
                                     sizeof(patternDetectDisc_Clock));
  if (!ptr)
    return;

  // NOP out the jump table
  for (uint32_t i = 0; i < (sizeof(patternDetectDisc_Clock) / sizeof(uint32_t)); i++)
    _sw(0, (uint32_t)ptr + i * 4);
}

static uint32_t menuLoopPatch_1[7] = {0x8e04fff8, 0x8e05fff0, 0x0004102a, 0x0082280b, 0x20a3ffff, 0x1000000c, 0xae03fff8};
static uint32_t menuLoopPatch_2[9] = {
    0x1040000e, 0x30620020, 0x8e05fff8, 0x8e02fff0, 0x24a30001, 0x0062102a, 0x0002180a, 0x00000000, 0xae03fff8,
};

// Patches menu scrolling
void patchMenuInfiniteScrolling(uint8_t *osd) {
  int i;
  uint8_t *ptr;
  uint32_t *addr, *src, *dst;

  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternMenuLoop, (uint8_t *)patternMenuLoop_mask, sizeof(patternMenuLoop));
  if (!ptr)
    return;

  addr = (uint32_t *)ptr;

  if (addr[9] == 0x30624000 && addr[20] == 0x24045200) {
    src = menuLoopPatch_1;
    dst = addr + 2;
    for (i = 0; i < 7; i++)
      *(dst++) = *(src++);

    src = menuLoopPatch_2;
    dst = addr + 11;
    for (i = 0; i < 9; i++)
      *(dst++) = *(src++);

    addr[10] = addr[-1]; // reload normal pad variable
    addr[-1] += 8;       // get key repeat variable
  }
}

// Forces the video mode
void patchVideoMode(uint8_t *osd, GSVideoMode mode) {
  uint8_t *ptr;

  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternVideoMode, (uint8_t *)patternVideoMode_mask, sizeof(patternVideoMode));
  if (!ptr)
    return;

  if (mode == GS_MODE_PAL)
    _sw(0x24020001, (uint32_t)ptr + 20); // set return value to 1
  else
    _sw(0x0000102d, (uint32_t)ptr + 20); // set return value to 0 to force NTSC
}
