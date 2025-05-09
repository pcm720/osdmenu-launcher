#include "errno.h"
#include "init.h"
#include "loader.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

// Launches ELF from memory cards
int handleMC(int argc, char *argv[]) {
  if ((argv[0] == 0) || (strlen(argv[0]) < 4)) {
    msg("MC: invalid argument\n");
    return -EINVAL;
  }

  int res = initModules(Device_MemoryCard);
  if (res)
    return res;

  // If path is mc?, test for both memory cards
  if (argv[0][2] == '?') {
    for (int i = '0'; i < '2'; i++) {
      argv[0][2] = i;
      if (!(res = tryFile(argv[0])))
        break;
    }
  } else
    res = tryFile(argv[0]);

  if (res)
    return -ENOENT;

  return LoadELFFromFile(argc, argv);
}

// Launches ELF from MMCE devices
int handleMMCE(int argc, char *argv[]) {
  if ((argv[0] == 0) || (strlen(argv[0]) < 5)) {
    msg("MMCE: invalid argument\n");
    return -EINVAL;
  }

  int res = initModules(Device_MMCE);
  if (res)
    return res;

  // If path is mmce?, test for both slots
  if (argv[0][4] == '?') {
    for (int i = '0'; i < '2'; i++) {
      argv[0][4] = i;
      if (!(res = tryFile(argv[0])))
        break;
    }
  } else
    res = tryFile(argv[0]);

  if (res)
    return -ENOENT;

  return LoadELFFromFile(argc, argv);
}
