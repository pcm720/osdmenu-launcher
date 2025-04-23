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

// OSDSYS deinit function
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

  // Replace function calls with no-ops?
  // Not sure what it does, but leaving it here just in case
  if (_lw(0x202d78) == 0x0c080898 && _lw(0x202b40) == 0x0c080934 && _lw(0x20ffa0) == 0x0c080934) {
    _sw(0x00000000, 0x202d78); // replace jal 0x0080898 with nop
    _sw(0x24020000, 0x202b40); // replace jal 0x0080934 with addiu 0, v0, 0
    _sw(0x24020000, 0x20ffa0); // replace jal 0x0080934 with addiu 0, v0, 0
  }

  int n = 0;
  char *args[5];
  args[n++] = "hdd0:__system:pfs:/osd100/hosdsys.elf";
  // if (settings.patcherFlags & FLAG_BOOT_BROWSER)
  //   args[n++] = "BootBrowser"; // Pass BootBrowser to launch internal mc browser
  // else if ((settings.patcherFlags & FLAG_SKIP_DISC) || (settings.patcherFlags & FLAG_SKIP_SCE_LOGO))
  //   args[n++] = "BootClock"; // Pass BootClock to skip OSDSYS intro

  // if (findString("SkipMc", (char *)epc, 0x100000)) // Pass SkipMc argument
  //   args[n++] = "SkipMc";                          // Skip mc?:/BREXEC-SYSTEM/osdxxx.elf update on v5 and above

  // if (findString("SkipHdd", (char *)epc, 0x100000)) // Pass SkipHdd argument if the ROM supports it
  //   args[n++] = "SkipHdd";                          // Skip HDDLOAD on v5 and above
  // else
  //   patchSkipHDD((uint8_t *)epc); // Skip HDD patch for earlier ROMs

  // Apply disc launch patch to forward disc launch to the launcher
  patchDiscLaunch((uint8_t *)epc);

  // Mangle system update paths to prevent OSDSYS from loading system updates (for ROMs not supporting SkipMc)
  uint8_t *ptr;
  while ((ptr = (uint8_t *)findString("EXEC-SYSTEM", (char *)epc, 0x100000)))
    ptr[2] = '\0';

  // Find OSDSYS deinit function
  ptr =
      findPatternWithMask((uint8_t *)epc, 0x100000, (uint8_t *)patternOSDSYSDeinit, (uint8_t *)patternOSDSYSDeinit_mask, sizeof(patternOSDSYSDeinit));
  if (ptr)
    osdsysDeinit = (void *)ptr;

  FlushCache(0);
  FlushCache(2);
  ExecPS2(epc, gp, n, args);
  Exit(-1);
}

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
// Loads OSDSYS from ROM and handles the patching
void launchOSDSYS() {
  uint8_t *ptr;
  t_ExecData exec;

  if (SifLoadElfEncrypted("pfs0:/osd100/hosdsys.elf", &exec) || (exec.epc < 0))
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
  argv[argc++] = "hdd0:__system:pfs:/osd100/hosdsys.elf";

  // Execute the OSD unpacker. If the above patching was successful it will
  // call the patchExecuteOSDSYS() function after unpacking.
  ExecPS2((void *)exec.epc, (void *)exec.gp, argc, argv);
  Exit(-1);
}

// Calls OSDSYS deinit function
void deinitOSDSYS() {
  if (osdsysDeinit)
    osdsysDeinit(1);
}
