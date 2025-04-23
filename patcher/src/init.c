#include <fcntl.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifrpc.h>
#include <stdlib.h>
#include <string.h>
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

// Macros for loading embedded IOP modules
#define IRX_DEFINE(mod)                                                                                                                              \
  extern unsigned char mod##_irx[] __attribute__((aligned(16)));                                                                                     \
  extern uint32_t size_##mod##_irx

IRX_DEFINE(iomanX);
IRX_DEFINE(fileXio);
IRX_DEFINE(ps2dev9);
IRX_DEFINE(ps2atad);
IRX_DEFINE(ps2hdd);
IRX_DEFINE(ps2fs);

char *initPS2HDDArguments(uint32_t *argLength);
char *initPS2FSArguments(uint32_t *argLength);

// Function used to initialize module arguments.
// Must set argLength and return non-null pointer to a argument string if successful.
// Returned pointer must point to dynamically allocated memory
typedef char *(*moduleArgFunc)(uint32_t *argLength);

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
  // Apply patches required to load executables from memory cards
  if ((ret = sbv_patch_enable_lmb()))
    return ret;
  if ((ret = sbv_patch_disable_prefix_check()))
    return ret;
  if ((ret = sbv_patch_fileio()))
    return ret;

  ret = SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, &iopret);
  if (ret >= 0)
    ret = 0;
  if (iopret == 1)
    ret = iopret;
  if (ret)
    return -1;

  ret = SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, &iopret);
  if (ret >= 0)
    ret = 0;
  if (iopret == 1)
    ret = iopret;
  if (ret)
    return -1;

  ret = SifExecModuleBuffer(ps2dev9_irx, size_ps2dev9_irx, 0, NULL, &iopret);
  if (ret >= 0)
    ret = 0;
  if (iopret == 1)
    ret = iopret;
  if (ret)
    return -1;

  ret = SifExecModuleBuffer(ps2atad_irx, size_ps2atad_irx, 0, NULL, &iopret);
  if (ret >= 0)
    ret = 0;
  if (iopret == 1)
    ret = iopret;
  if (ret)
    return -1;

  // up to 4 descriptors, 20 buffers
  static char ps2hddArguments[] = "-o"
                                  "\0"
                                  "4"
                                  "\0"
                                  "-n"
                                  "\0"
                                  "20";
  ret = SifExecModuleBuffer(ps2hdd_irx, size_ps2hdd_irx, sizeof(ps2hddArguments), ps2hddArguments, &iopret);
  if (ret >= 0)
    ret = 0;
  if (iopret == 1)
    ret = iopret;
  if (ret)
    return -1;

  // up to 2 mounts, 10 descriptors, 40 buffers
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
  ret = SifExecModuleBuffer(ps2fs_irx, size_ps2fs_irx, sizeof(ps2fsArguments), ps2fsArguments, &iopret);
  if (ret >= 0)
    ret = 0;
  if (iopret == 1)
    ret = iopret;
  if (ret)
    return -1;

  fioInit();
  return 0;
}

// Resets IOP before loading OSDSYS
void resetModules(void) {
  while (!SifIopReset("", 0)) {
  };
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
