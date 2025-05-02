#include "init.h"
#include "loader.h"
#include <hdd-ioctl.h>
#include <ps2sdkapi.h>
#include <stdio.h>
#include <string.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>

#define DELAY_ATTEMPTS 20

// Loads ELF from APA-formatted HDD
int handlePFS(int argc, char *argv[]) {
  if ((argv[0] == 0) || (strlen(argv[0]) < 4)) {
    msg("PFS: invalid argument\n");
    return -EINVAL;
  }

  // Extract partition from hdd?:<partition>/<path to ELF>
  // Check if the path starts with "hdd?:/" and reject it
  char *path = &argv[0][5];
  if (path[0] == '/') {
    msg("PFS: invalid path format\n");
    return -EINVAL;
  }

  path = strstr(argv[0], ":pfs:");
  if (path) {
    path[0] = '\0';
    path += 5;
  } else if ((path = strchr(argv[0], '/'))) {
    path[0] = '\0';
    path++;
  } else {
    msg("PFS: no file path found\n");
    return -EINVAL;
  }

  // Terminate the path to get partition name in argv[0]
  // and increment to point to the PFS ELF path
  int res = initModules(Device_PFS);
  if (res)
    return res;

  // Wait for IOP to initialize device driver
  DPRINTF("Waiting for HDD to become available\n");
  for (int attempts = 0; attempts < DELAY_ATTEMPTS; attempts++) {
    res = open("hdd0:", O_DIRECTORY | O_RDONLY);
    if (res >= 0) {
      close(res);
      break;
    }
    sleep(1);
  }
  if (res < 0)
    return -ENODEV;

  // Build PFS path
  char *elfPath = normalizePath(path, Device_PFS);
  if (!elfPath)
    return -ENODEV;

  // Mount the partition
  DPRINTF("Mounting %s to %s\n", argv[0], PFS_MOUNTPOINT);
  if (fileXioMount(PFS_MOUNTPOINT, argv[0], FIO_MT_RDONLY))
    return -ENODEV;

  // Make sure file exists
  DPRINTF("Checking for %s\n", elfPath);
  if (tryFile(elfPath)) {
    fileXioDevctl(PFS_MOUNTPOINT, PDIOC_CLOSEALL, NULL, 0, NULL, 0);
    fileXioSync(PFS_MOUNTPOINT, FXIO_WAIT);
    fileXioUmount(PFS_MOUNTPOINT);
    return -ENOENT;
  }

  // Build the path as 'hdd0:<partition name>:pfs:/<path to ELF>'
  if (path[0] == '/')
    path++;

  snprintf(elfPath, PATH_MAX - 1, "%s:pfs:/%s", argv[0], path);
  argv[0] = elfPath;

  return LoadELFFromFile(argc, argv);
}
