#include "common.h"
#include "init.h"
#include "loader.h"
#include <ps2sdkapi.h>
#include <stdio.h>
#include <string.h>

// Loads ELF from APA-formatted HDD
int handlePFS(int argc, char *argv[]) {
  if ((argv[0] == 0) || (strlen(argv[0]) < 4)) {
    msg("PFS: invalid argument\n");
    return -EINVAL;
  }

  // Check if the path starts with "hdd?:/" and reject it
  char *path = &argv[0][5];
  if (path[0] == '/') {
    msg("PFS: invalid path format\n");
    return -EINVAL;
  }

  int res = initPFS(argv[0]);
  if (res)
    return res;

  // Build PFS path
  char *elfPath = normalizePath(argv[0], Device_PFS);
  if (!elfPath)
    return -ENODEV;

  // Make sure file exists
  DPRINTF("Checking for %s\n", elfPath);
  if (tryFile(elfPath)) {
    deinitPFS();
    return -ENOENT;
  }

  // Build the path as 'hdd0:<partition name>:pfs:/<path to ELF>'
  if ((path = strstr(argv[0], ":pfs:"))) {
    path[0] = '\0';
    path += 5;
  } else if ((path = strchr(argv[0], '/'))) {
    path[0] = '\0';
    path++;
  }

  snprintf(elfPath, PATH_MAX - 1, "%s:pfs:/%s", argv[0], path);
  argv[0] = elfPath;

  return LoadELFFromFile(argc, argv);
}
