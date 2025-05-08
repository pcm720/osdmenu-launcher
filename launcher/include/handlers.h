#ifndef _HANDLERS_H_
#define _HANDLERS_H_

#include "common.h"

// handler_bdm.c
//
// Launches ELF from BDM device
int handleBDM(DeviceType device, int argc, char *argv[]);

// handler_cdrom.c
//
// Launches the disc while displaying the visual game ID and writing to the history file
int handleCDROM(int argc, char *argv[]);
int startCDROM(int displayGameID, int skipPS2LOGO, char *dkwdrvPath);

// handler_fmcb.c
//
// Loads ELF specified in OSDMENU.CNF on the memory card or on the APA partition specified in HOSD_CONF_PARTITION
// APA-formatted HDD handling requires the path to start with pfs...
int handleFMCB(int argc, char *argv[]);

// handler_mc.c
//
// Launches ELF from memory cards
int handleMC(int argc, char *argv[]);
// Launches ELF from MMCE devices
int handleMMCE(int argc, char *argv[]);

// handler_pfs.c
//
// Loads ELF from APA-formatted HDD
int handlePFS(int argc, char *argv[]);

// handler_quickboot.c
//
// Loads ELF from the .CNF file in CWD
int handleQuickboot(char *cnfPath);

#endif
