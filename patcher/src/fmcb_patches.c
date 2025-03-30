// FMCB 1.8 OSDSYS patches by Neme
#include "fmcb_patterns.h"
#include "init.h"
#include "loader.h"
#include "osdmenu_patches.h"
#include "settings.h"
#include <kernel.h>
#include <loadfile.h>
#include <stdlib.h>
#include <string.h>

#define OSD_MAGIC 0x39390000 // arbitrary number to identify added menu items

static uint32_t osdMenu[4 + NEWITEMS * 2];

struct OSDMenuInfo {
  uint32_t unknown1;
  uint32_t *menuPtr;
  uint32_t entryCount;
  uint32_t unknown2;
  uint32_t currentEntry;
};

static struct OSDMenuInfo *menuInfo = NULL;

// Searches for byte pattern in memory
uint8_t *findPatternWithMask(uint8_t *buf, uint32_t bufsize, uint8_t *bytes, uint8_t *mask, uint32_t len) {
  uint32_t i, j;

  for (i = 0; i < bufsize - len; i++) {
    for (j = 0; j < len; j++) {
      if ((buf[i + j] & mask[j]) != bytes[j])
        break;
    }
    if (j == len)
      return &buf[i];
  }
  return NULL;
}

// Searches for string in memory
char *findString(const char *string, char *buf, uint32_t bufsize) {
  uint32_t i;
  const char *s, *p;

  for (i = 0; i < bufsize; i++) {
    s = string;
    for (p = buf + i; *s && *s == *p; s++, p++)
      ;
    if (!*s)
      return (buf + i);
  }
  return NULL;
}

// Returns the pointer to OSD string
const char *getStringPointer(const char **strings, uint32_t index) {
  if ((index & 0xffff0000) == OSD_MAGIC)
    return settings.menuItemName[index & 0xffff];

  return strings[index];
}

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

// Patches OSD menu to include custom menu entries
void patchMenu(uint8_t *osd) {
  uint8_t *ptr;
  uint32_t tmp, menuAddr, osdstrAddr, entryAddr, i;

  // Search for all patterns and return if one of them not found
  for (tmp = 0; tmp < 0x100000; tmp = (uint32_t)(ptr - osd + 4)) {
    ptr = findPatternWithMask(osd + tmp, 0x100000 - tmp, (uint8_t *)patternMenuInfo, (uint8_t *)patternMenuInfo_mask, sizeof(patternMenuInfo));
    if (!ptr)
      return;
    if (_lw((uint32_t)ptr + 4) == (uint32_t)ptr - 4 * 4)
      break;
  }
  menuAddr = (uint32_t)ptr;

  menuInfo = (struct OSDMenuInfo *)menuAddr;

  ptr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternOSDString, (uint8_t *)patternOSDString_mask, sizeof(patternOSDString));
  if (!ptr)
    return;
  osdstrAddr = (uint32_t)ptr;

  ptr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternUserInputHandler, (uint8_t *)patternUserInputHandler_mask,
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
static int dx = 0;
static int vel, acc;
static int offsY = 0;
static int fontHeight = 16;

// Draws selected items
void drawMenuItemSelected(int X, int Y, uint32_t *color, int alpha, const char *string, int num) {
  int i;

  for (i = 0; i < 4; i++)
    colorSelected[i] = settings.colorSelected[i];

  if (!settings.scrollMenu) { // Old style menu
    (*DrawMenuItem)(settings.menuX, Y - settings.menuItemCount * 10, colorSelected, alpha, string);
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
      (*DrawMenuItem)(settings.menuX, settings.menuY + Y, colorSelected, alpha, string);
      (*DrawMenuItem)(settings.menuX - 220 + (dx >> 8), settings.menuY + Y, colorSelected, alpha, settings.leftCursor);
      (*DrawMenuItem)(settings.menuX + 220 - (dx >> 8), settings.menuY + Y, colorSelected, alpha, settings.rightCursor);
    }
    (*DrawMenuItem)(settings.menuX, settings.menuY - ((settings.displayedItems + 1) * (fontHeight / 2)), colorSelected, 128,
                    settings.menuDelimiterTop);
    (*DrawMenuItem)(settings.menuX, settings.menuY + ((settings.displayedItems + 1) * (fontHeight / 2)), colorSelected, 128,
                    settings.menuDelimiterBottom);
  }
}

// Draws unselected items
void drawMenuItemUnselected(int X, int Y, uint32_t *color, int alpha, const char *string, int num) {
  int i;

  for (i = 0; i < 4; i++)
    colorUnselected[i] = settings.colorUnselected[i];

  if (!settings.scrollMenu) { // Old style menu
    (*DrawMenuItem)(settings.menuX, Y - settings.menuItemCount * 10, colorUnselected, alpha, string);
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

      (*DrawMenuItem)(settings.menuX, settings.menuY + Y, colorUnselected, alpha, string);
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

  ptr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternDrawMenuItem, (uint8_t *)patternDrawMenuItem_mask, sizeof(patternDrawMenuItem));
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

  // Call original function that was overwritted
  (*DrawButtonPanel_1stfunc)();
}

// drawNonselectableEntryLeft() is called for all items less the last
void drawNonselectableEntryLeft(int X, int Y, uint32_t *color, int alpha, const char *string) {
  if (ButtonsPanel_Type == MAINMENU_PANEL) {
    if (settings.enterX == -1)
      settings.enterX = X - 28;

    if (settings.enterY == -1)
      settings.enterY = Y;

    (*DrawNonSelectableItem)(settings.enterX + 28, settings.enterY, color, alpha, string);
  } else
    (*DrawNonSelectableItem)(X, Y, color, alpha, string);
}

// drawNonselectableEntryRight() is called only for last item (Options or Version)
void drawNonselectableEntryRight(int X, int Y, uint32_t *color, int alpha, const char *string) {
  if (ButtonsPanel_Type == MAINMENU_PANEL) {
    if (settings.versionX == -1)
      settings.versionX = X - 28;

    if (settings.versionY == -1)
      settings.versionY = Y;

    (*DrawNonSelectableItem)(settings.versionX + 28, settings.versionY, color, alpha, string);
  } else
    (*DrawNonSelectableItem)(X, Y, color, alpha, string);
}

// drawIconLeft() is called for all button icons less the last
void drawIconLeft(int type, int X, int Y, int alpha) {
  if (ButtonsPanel_Type == MAINMENU_PANEL) {
    if (settings.enterX == -1)
      settings.enterX = X;

    if (settings.enterY == -1)
      settings.enterY = Y;

    (*DrawIcon)(type, settings.enterX, settings.enterY, alpha);
  } else
    (*DrawIcon)(type, X, Y, alpha);
}

// drawIconRight() is called for only for last button icon (Options or Version)
void drawIconRight(int type, int X, int Y, int alpha) {
  if (ButtonsPanel_Type == MAINMENU_PANEL) {
    if (settings.versionX == -1)
      settings.versionX = X;

    if (settings.versionY == -1)
      settings.versionY = Y;

    (*DrawIcon)(type, settings.versionX, settings.versionY, alpha);
  } else
    (*DrawIcon)(type, X, Y, alpha);
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
  firstPtr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternDrawButtonPanel_1, (uint8_t *)patternDrawButtonPanel_1_mask,
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
// Later PS2 ROMs:
// uint32_t *discLaunchHandlers[7] = {
//    exec_ps2_game_disc,		// PS2 game DVD
//    exec_ps2_game_disc,		// PS2 game CD
//    exec_ps1_game_disc,		// PS1 game CD
//    exec_dvdv_disc,			  // DVD Video
//    do_nothing,				    // none (return 1)
//    do_nothing,				    // none (return 1)
//    exec_hdd_stuff			  // HDDLOAD
//}
//
// Protokernels:
// uint32_t *discLaunchHandlers[6] = {
//    reboot,					      // perform LoadExecPS2(NULL, 0, NULL)
//    do_nothing,				    // none (return 1)
//    exec_ps2_game_disc,		// PS2 game DVD
//    exec_ps2_game_disc,		// PS2 game CD
//    exec_ps1_game_disc,		// PS1 game CD
//    exec_dvdv_disc			  // DVD Video
//}
static uint32_t *discLaunchHandlers = NULL;

// Patches the disc launch handlers to load discs with the launcher
void patchDiscLaunch(uint8_t *osd) {
  uint8_t *ptr;
  uint32_t tmp, pFn;

  ptr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternExecuteDisc, (uint8_t *)patternExecuteDisc_mask, sizeof(patternExecuteDisc));
  if (ptr) {
    pFn = (uint32_t)ptr; // address of the ExecuteDisc function

    tmp = _lw(pFn + 40) << 16;
    tmp += (signed short)(_lw(pFn + 44) & 0xffff);
    discLaunchHandlers = (uint32_t *)tmp;

    discLaunchHandlers[0] = (uint32_t)launchDisc; // Overwrite PS2 DVD function pointer
    discLaunchHandlers[1] = (uint32_t)launchDisc; // Overwrite PS2 CD function pointer
    discLaunchHandlers[2] = (uint32_t)launchDisc; // Overwrite PS1 function pointer
  } else {
    // Handler protokernel disc launchers
    ptr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternExecuteDiscProto, (uint8_t *)patternExecuteDiscProto_mask,
                              sizeof(patternExecuteDiscProto));
    if (!ptr)
      return;

    pFn = (uint32_t)ptr;

    tmp = _lw(pFn + 28) << 16;
    tmp += (signed short)(_lw(pFn + 36) & 0xffff);
    discLaunchHandlers = (uint32_t *)tmp;

    discLaunchHandlers[2] = (uint32_t)launchDisc; // Overwrite protokernel PS2 DVD function pointer
    discLaunchHandlers[3] = (uint32_t)launchDisc; // Overwrite protokernel PS2 CD function pointer
    discLaunchHandlers[4] = (uint32_t)launchDisc; // Overwrite protokernel PS1 function pointer
  }
}

// Patches automatic disc launch
void patchSkipDisc(uint8_t *osd) {
  uint8_t *ptr;
  uint32_t tmp, addr2, addr3, dist;

  ptr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternDetectDisc_1, (uint8_t *)patternDetectDisc_1_mask, sizeof(patternDetectDisc_1));
  if (!ptr)
    return;
  addr2 = (uint32_t)ptr;

  ptr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternDetectDisc_2, (uint8_t *)patternDetectDisc_2_mask, sizeof(patternDetectDisc_2));
  if (!ptr)
    return;
  addr3 = (uint32_t)ptr;

  tmp = addr2 + 48; // patch start
  dist = ((addr3 - tmp) >> 2) - 1;
  if (dist > 0x40)
    return;

  _sw(0x10000000 + dist, tmp); // branch to addr3
  _sw(0, tmp + 4);             // nop
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

  ptr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternMenuLoop, (uint8_t *)patternMenuLoop_mask, sizeof(patternMenuLoop));
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
void patchVideoMode(uint8_t *osd) {
  uint8_t *ptr;

  ptr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternVideoMode, (uint8_t *)patternVideoMode_mask, sizeof(patternVideoMode));
  if (!ptr)
    return;

  if (!strcmp(settings.videoMode, "NTSC"))     // NTSC:
    _sw(0x0000102d, (uint32_t)ptr + 20);       // set return value to 0
  else if (!strcmp(settings.videoMode, "PAL")) // PAL:
    _sw(0x24020001, (uint32_t)ptr + 20);       // set return value to 1
}

// Patches HDD update code for ROMs not supporting "SkipHdd" arg
void patchSkipHDD(uint8_t *osd) {
  uint8_t *ptr;
  uint32_t addr;

  // Search code near MC Update & HDD load
  ptr = findPatternWithMask(osd, 0x00100000, (uint8_t *)patternHDDLoad, (uint8_t *)patternHDDLoad_mask, sizeof(patternHDDLoad));
  if (!ptr)
    return;
  addr = (uint32_t)ptr;

  // Place "beq zero, zero, Exit_HddLoad" just after CheckMcUpdate() call
  _sw(0x10000000 + ((signed short)(_lw(addr + 28) & 0xffff) + 5), addr + 8);
}

// Applies patches and executes OSDSYS
void patchExecuteOSDSYS(void *epc, void *gp) {
  int n = 0;
  char *args[5], *ptr;

  args[n++] = "rom0:";

  if (settings.hackedOSDSYS) {
    // If hacked OSDSYS is enabled, apply menu patch
    patchMenu((uint8_t *)epc);
    patchMenuDraw((uint8_t *)epc);
    patchMenuInfiniteScrolling((uint8_t *)epc);
    patchMenuButtonPanel((uint8_t *)epc);
  }

  // Apply version menu patch
  patchVersionInfo((uint8_t *)epc);

  // Patch the video mode only if different from AUTO
  if (strcmp(settings.videoMode, "AUTO"))
    patchVideoMode((uint8_t *)epc);

  // Apply skip disc patch
  if (settings.skipDisc)
    patchSkipDisc((uint8_t *)epc);

  // Replace function calls with no-ops?
  if (_lw(0x202d78) == 0x0c080898 && _lw(0x202b40) == 0x0c080934 && _lw(0x20ffa0) == 0x0c080934) {
    _sw(0x00000000, 0x202d78); // replace jal 0x0080898 with nop
    _sw(0x24020000, 0x202b40); // replace jal 0x0080934 with addiu 0, v0, 0
    _sw(0x24020000, 0x20ffa0); // replace jal 0x0080934 with addiu 0, v0, 0
  }

  if (settings.goToInnerBrowser)
    args[n++] = "BootBrowser"; // Pass BootBrowser to launch internal mc browser
  else if ((settings.skipDisc) || (settings.skipLogo))
    args[n++] = "BootClock"; // Pass BootClock to skip OSDSYS intro

  if (findString("SkipMc", (char *)epc, 0x100000)) // Pass SkipMc argument
    args[n++] = "SkipMc";                          // Skip mc?:/BREXEC-SYSTEM/osdxxx.elf update on v5 and above

  if (findString("SkipHdd", (char *)epc, 0x100000)) // Pass SkipHdd argument if the ROM supports it
    args[n++] = "SkipHdd";                          // Skip HDDLOAD on v5 and above
  else
    patchSkipHDD((uint8_t *)epc); // Skip HDD patch for earlier ROMs

  // Apply disc launch patch to forward disc launch to the launcher
  patchDiscLaunch((uint8_t *)epc);

  // To avoid loop in OSDSYS (Handle those models not supporting SkipMc arg) :
  while ((ptr = findString("EXEC-SYSTEM", (char *)epc, 0x100000)))
    strncpy(ptr, "EXEC-OSDSYS", 12);

  FlushCache(0);
  FlushCache(2);
  ExecPS2(epc, gp, n, args);
  Exit(-1);
}

// Loads OSDSYS from ROM and handles the patching
void launchOSDSYS(void) {
  uint8_t *ptr;
  t_ExecData exec;

  SifLoadElf("rom0:OSDSYS", &exec);

  if (exec.epc > 0) {
    // If it loaded to 0x200000 it's probably not packed (protokernels).
    // In this case just patch and execute it.
    if ((exec.epc & 0xfff00000) == 0x00200000) {
      resetModules();
      patchExecuteOSDSYS((void *)exec.epc, (void *)exec.gp);
    }

    // Find the ExecPS2 function in the unpacker starting from 0x100000.
    ptr = findPatternWithMask((uint8_t *)0x100000, 0x1000, (uint8_t *)patternExecPS2, (uint8_t *)patternExecPS2_mask, sizeof(patternExecPS2));

    // If found, patch it to call patchExecuteOSDSYS() function.
    if (ptr) {
      uint32_t instr = 0x0c000000;
      instr |= ((uint32_t)patchExecuteOSDSYS >> 2);
      *(uint32_t *)ptr = instr;
      *(uint32_t *)&ptr[4] = 0;
    }

    resetModules();

    // Execute the OSD unpacker. If the above patching was successful it will
    // call the patchExecuteOSDSYS() function after unpacking.
    ExecPS2((void *)exec.epc, (void *)exec.gp, 0, NULL);
    Exit(-1);
  }
}
