#include "defaults.h"
#include "init.h"
#include "loader.h"
#include "patches_fmcb.h"
#include "patches_osdmenu.h"
#include "patterns_common.h"
#include "settings.h"
#include <kernel.h>
#include <loadfile.h>
#include <stdlib.h>
#include <string.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

// OSDSYS deinit functions
static void (*sceUmount)(char *mountpoint) = NULL;
static void (*sceRemove)(char *mountpoint) = NULL;
static void (*osdsysDeinit)(uint32_t flags) = NULL;

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

// Applies patches and executes OSDSYS
void patchExecuteOSDSYS(void *epc, void *gp) {
  if (settings.patcherFlags & FLAG_CUSTOM_MENU) {
    // If hacked OSDSYS is enabled, apply menu patch
    patchMenu((uint8_t *)epc);
    patchMenuDraw((uint8_t *)epc);
    patchMenuInfiniteScrolling((uint8_t *)epc);
    patchMenuButtonPanel((uint8_t *)epc);
  }

  // Apply browser application launch patch
  if (settings.patcherFlags & FLAG_BROWSER_LAUNCHER)
    patchBrowserApplicationLaunch((uint8_t *)epc);

  // Apply version menu patch
  patchVersionInfo((uint8_t *)epc);

  switch (settings.videoMode) {
  case GS_MODE_PAL:
    patchVideoMode((uint8_t *)epc, settings.videoMode);
    break;
  case GS_MODE_DTV_480P:
  case GS_MODE_DTV_1080I:
    patchGSVideoMode((uint8_t *)epc, settings.videoMode); // Apply 480p or 1080i patch
  case GS_MODE_NTSC:
    patchVideoMode((uint8_t *)epc, GS_MODE_NTSC); // Force NTSC
  }

  // Apply skip disc patch
  if (settings.patcherFlags & FLAG_SKIP_DISC)
    patchSkipDisc((uint8_t *)epc);

  int n = 0;
  char *args[3];
  args[n++] = "hdd0:__system:pfs:" HOSD_HDDOSD_PATH;
  if (settings.patcherFlags & FLAG_BOOT_BROWSER)
    args[n++] = "BootBrowser"; // Pass BootBrowser to launch internal mc browser
  else if ((settings.patcherFlags & FLAG_SKIP_DISC) || (settings.patcherFlags & FLAG_SKIP_SCE_LOGO))
    args[n++] = "BootClock"; // Pass BootClock to skip OSDSYS intro

  // Apply disc launch patch to forward disc launch to the launcher
  patchDiscLaunch((uint8_t *)epc);

  // Find OSDSYS deinit function
  uint8_t *ptr =
      findPatternWithMask((uint8_t *)epc, 0x100000, (uint8_t *)patternOSDSYSDeinit, (uint8_t *)patternOSDSYSDeinit_mask, sizeof(patternOSDSYSDeinit));
  if (ptr)
    osdsysDeinit = (void *)ptr;
  // Find sceRemove function
  ptr = findPatternWithMask((uint8_t *)epc, 0x100000, (uint8_t *)patternSCERemove, (uint8_t *)patternSCERemove_mask, sizeof(patternSCERemove));
  if (ptr)
    sceRemove = (void *)ptr;
  // Find sceUmount function
  ptr = findPatternWithMask((uint8_t *)epc, 0x100000, (uint8_t *)patternSCEUmount, (uint8_t *)patternSCEUmount_mask, sizeof(patternSCEUmount));
  if (ptr)
    sceUmount = (void *)ptr;

  FlushCache(0);
  FlushCache(2);
  ExecPS2(epc, gp, n, args);
  Exit(-1);
}

// Loads OSDSYS from ROM and handles the patching
void launchOSDSYS() {
  uint8_t *ptr;
  t_ExecData exec;

  if (SifLoadElfEncrypted("pfs0:" HOSD_HDDOSD_PATH, &exec) || (exec.epc < 0))
    return;

  fileXioUmount("pfs0:");

  // Find the ExecPS2 function in the unpacker starting from 0x100000.
  ptr = findPatternWithMask((uint8_t *)0x100000, 0x1000, (uint8_t *)patternExecPS2, (uint8_t *)patternExecPS2_mask, sizeof(patternExecPS2));
  if (ptr) {
    // If found, patch it to call patchExecuteOSDSYS() function.
    uint32_t instr = 0x0c000000;
    instr |= ((uint32_t)patchExecuteOSDSYS >> 2);
    *(uint32_t *)ptr = instr;
    *(uint32_t *)&ptr[4] = 0;
  }

  resetModules();

  int argc = 0;
  char *argv[1];
  argv[argc++] = "hdd0:__system:pfs:" HOSD_HDDOSD_PATH;

  // Execute the OSD unpacker. If the above patching was successful it will
  // call the patchExecuteOSDSYS() function after unpacking.
  ExecPS2((void *)exec.epc, (void *)exec.gp, argc, argv);
}

// Calls OSDSYS deinit function
void deinitOSDSYS() {
  if (sceRemove)
    sceRemove("hdd0:_tmp");
  if (sceUmount)
    sceUmount("pfs1:");

  if (osdsysDeinit)
    osdsysDeinit(1);
}
