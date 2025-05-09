#include <fcntl.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#define NEWLIB_PORT_AWARE
#include <fileio.h>

// Wipes user memory
void wipeUserMem(void) {
  for (int i = 0x100000; i < 0x02000000; i += 64) {
    asm("\tsq $0, 0(%0) \n"
        "\tsq $0, 16(%0) \n"
        "\tsq $0, 32(%0) \n"
        "\tsq $0, 48(%0) \n" ::"r"(i));
  }
}

// Resets IOP before loading OSDSYS
void resetModules(void) {
#ifndef HOSD
  while (!SifIopReset("rom0:UDNL rom0:EELOADCNF", 0)) {
  };
#else
  while (!SifIopReset("", 0)) {
  };
#endif
  while (!SifIopSync()) {
  };

  SifExitIopHeap();
  SifLoadFileExit();
  sceSifExitRpc();
  sceSifExitCmd();

  FlushCache(0);
  FlushCache(2);
  fioInit();
}

#ifndef HOSD
// OSDMenu

// Loads IOP modules
int initModules(void) {
  sceSifInitRpc(0);
  while (!SifIopReset("", 0)) {
  };
  while (!SifIopSync()) {
  };

  sceSifInitRpc(0);

  int ret;
  // Apply patches required to load executables from memory cards
  if ((ret = sbv_patch_enable_lmb()))
    return ret;
  if ((ret = sbv_patch_disable_prefix_check()))
    return ret;
  if ((ret = sbv_patch_fileio()))
    return ret;

  if ((ret = SifLoadModule("rom0:SIO2MAN", 0, NULL)) < 0)
    return ret;
  if ((ret = SifLoadModule("rom0:MCMAN", 0, NULL)) < 0)
    return ret;
  if ((ret = SifLoadModule("rom0:MCSERV", 0, NULL)) < 0)
    return ret;

  fioInit();
  return 0;
}
#else
// HOSDMenu

#include <fileXio_rpc.h>

// Number of attempts initModules() will wait for hdd0 before returning
#define DELAY_ATTEMPTS 20

// Macros for loading embedded IOP modules
#define IRX_DEFINE(mod)                                                                                                                              \
  extern unsigned char mod##_irx[] __attribute__((aligned(16)));                                                                                     \
  extern uint32_t size_##mod##_irx

#define IRX_LOAD(mod, argLen, argStr)                                                                                                                \
  ret = SifExecModuleBuffer(mod##_irx, size_##mod##_irx, argLen, argStr, &iopret);                                                                   \
  if (ret < 0)                                                                                                                                       \
    return -1;                                                                                                                                       \
  if (iopret == 1)                                                                                                                                   \
    return -1;

IRX_DEFINE(iomanX);
IRX_DEFINE(fileXio);
IRX_DEFINE(ps2dev9);
IRX_DEFINE(ps2atad);
IRX_DEFINE(ps2hdd);
IRX_DEFINE(ps2fs);

// ps2hdd module arguments. Up to 4 descriptors, 20 buffers
static char ps2hddArguments[] = "-o"
                                "\0"
                                "4"
                                "\0"
                                "-n"
                                "\0"
                                "20";
// ps2fs module arguments. Up to 2 mounts, 10 descriptors, 40 buffers
char ps2fsArguments[] = "-m"
                        "\0"
                        "2"
                        "\0"
                        "-o"
                        "\0"
                        "10"
                        "\0"
                        "-n"
                        "\0"
                        "40";

// Loads IOP modules
int initModules(void) {
  sceSifInitRpc(0);
  while (!SifIopReset("", 0)) {
  };
  while (!SifIopSync()) {
  };

  sceSifInitRpc(0);

  int ret = 0;
  int iopret = 0;
  // Apply patches required to load executables from EE RAM
  if ((ret = sbv_patch_enable_lmb()))
    return ret;
  if ((ret = sbv_patch_disable_prefix_check()))
    return ret;
  if ((ret = sbv_patch_fileio()))
    return ret;

  IRX_LOAD(iomanX, 0, NULL)
  IRX_LOAD(fileXio, 0, NULL)
  IRX_LOAD(ps2dev9, 0, NULL)
  IRX_LOAD(ps2atad, 0, NULL)
  IRX_LOAD(ps2hdd, sizeof(ps2hddArguments), ps2hddArguments)
  IRX_LOAD(ps2fs, sizeof(ps2fsArguments), ps2fsArguments)

  // Wait for IOP to initialize device drivers
  for (int attempts = 0; attempts < DELAY_ATTEMPTS; attempts++) {
    ret = open("hdd0:", O_DIRECTORY | O_RDONLY);
    if (ret >= 0) {
      close(ret);
      return 0;
    }

    ret = 0x01000000;
    while (ret--)
      asm("nop\nnop\nnop\nnop");
  }
  return -1;
}
#endif
