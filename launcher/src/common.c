#include "handlers.h"
#include "init.h"
#include <debug.h>
#include <fcntl.h>
#include <kernel.h>
#include <ps2sdkapi.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int isScreenInited = 0;
char pathbuffer[PATH_MAX];

void initScreen() {
  if (isScreenInited)
    return;

  init_scr();
  scr_setCursor(0);
  scr_printf(".\n\n\n\n"); // To avoid messages being hidden by overscan
  isScreenInited = 1;
}

// Prints a message to the screen and console
void msg(const char *str, ...) {
  va_list args;
  va_start(args, str);

  initScreen();

  scr_vprintf(str, args);

  va_end(args);
}

// Prints a message to the screen and console and exits
void fail(const char *str, ...) {
  va_list args;
  va_start(args, str);

  initScreen();

  scr_vprintf(str, args);

  va_end(args);

  sleep(10);
  rebootPS2();
}

// Tests if file exists by opening it
int tryFile(char *filepath) {
  int fd = open(filepath, O_RDONLY);
  if (fd < 0) {
    return fd;
  }
  close(fd);
  return 0;
}

// Attempts to launch ELF from device and path in path
int launchPath(int argc, char *argv[]) {
  int ret = 0;
  switch (guessDeviceType(argv[0])) {
  case Device_MemoryCard:
    ret = handleMC(argc, argv);
    break;
#ifdef MMCE
  case Device_MMCE:
    ret = handleMMCE(argc, argv);
    break;
#endif
#ifdef USB
  case Device_USB:
    ret = handleBDM(Device_USB, argc, argv);
    break;
#endif
#ifdef ATA
  case Device_ATA:
    ret = handleBDM(Device_ATA, argc, argv);
    break;
#endif
#ifdef MX4SIO
  case Device_MX4SIO:
    ret = handleBDM(Device_MX4SIO, argc, argv);
    break;
#endif
#ifdef ILINK
  case Device_iLink:
    ret = handleBDM(Device_iLink, argc, argv);
    break;
#endif
#ifdef UDPBD
  case Device_UDPBD:
    ret = handleBDM(Device_UDPBD, argc, argv);
    break;
#endif
#ifdef APA
  case Device_PFS:
    ret = handlePFS(argc, argv);
    break;
#endif
#ifdef CDROM
  case Device_CDROM:
    ret = handleCDROM(argc, argv);
    break;
#endif
  default:
    return -ENODEV;
  }

  return ret;
}

// Adds a new string to linkedStr and returns
linkedStr *addStr(linkedStr *lstr, char *str) {
  linkedStr *newLstr = malloc(sizeof(linkedStr));
  newLstr->str = strdup(str);
  newLstr->next = NULL;

  if (lstr) {
    linkedStr *tLstr = lstr;
    // If lstr is not null, go to the last element and
    // link the new element
    while (tLstr->next)
      tLstr = tLstr->next;

    tLstr->next = newLstr;
    return lstr;
  }

  // Else, return the new element as the first one
  return newLstr;
}

// Frees all elements of linkedStr
void freeLinkedStr(linkedStr *lstr) {
  if (!lstr)
    return;

  linkedStr *tPtr = lstr;
  while (lstr->next) {
    free(lstr->str);
    tPtr = lstr->next;
    free(lstr);
    lstr = tPtr;
  }
  free(lstr->str);
  free(lstr);
}

// Attempts to guess device type from path
DeviceType guessDeviceType(char *path) {
  if (!strncmp("mc", path, 2)) {
    return Device_MemoryCard;
#ifdef MMCE
  } else if (!strncmp("mmce", path, 4)) {
    return Device_MMCE;
#endif
#ifdef USB
  } else if (!strncmp("mass", path, 4) || !strncmp("usb", path, 3)) {
    return Device_USB;
#endif
#ifdef ATA
  } else if (!strncmp("ata", path, 3)) {
    return Device_ATA;
#endif
#ifdef MX4SIO
  } else if (!strncmp("mx4sio", path, 6)) {
    return Device_MX4SIO;
#endif
#ifdef ILINK
  } else if (!strncmp("ilink", path, 5)) {
    return Device_iLink;
#endif
#ifdef UDPBD
  } else if (!strncmp("udpbd", path, 5)) {
    return Device_UDPBD;
#endif
#ifdef APA
  } else if (!strncmp("hdd", path, 3)) {
    return Device_PFS;
#endif
#ifdef CDROM
  } else if (!strncmp("cdrom", path, 5)) {
    return Device_CDROM;
  }
#endif
  return Device_None;
}

// Attempts to convert launcher-specific path into a valid device path
char *normalizePath(char *path, DeviceType type) {
  pathbuffer[0] = '\0';
  switch (type) {
  case Device_PFS:
    strcat(pathbuffer, PFS_MOUNTPOINT "/");
  case Device_MemoryCard:
  case Device_MMCE:
  case Device_CDROM:
    strncat(pathbuffer, path, PATH_MAX - 6);
    break;
  // BDM
  case Device_USB:
  case Device_ATA:
  case Device_MX4SIO:
  case Device_iLink:
  case Device_UDPBD:
    // Get relative ELF path from argv[0]
    path = strchr(path, ':');
    if (!path)
      return NULL;

    path++;

    strcpy(pathbuffer, BDM_MOUNTPOINT);
    strncat(pathbuffer, path, PATH_MAX - sizeof(BDM_MOUNTPOINT));
  default:
    return NULL;
  }
  return pathbuffer;
}
