// Defines paths used by both the patcher and the launcher
#ifndef _DEFAULTS_H_
#define _DEFAULTS_H_

// All files must be placed on the memory card.
// Relative path
// Only pfs0: and mc?: paths are supported
#ifndef CONF_PATH
#define CONF_PATH "OSDMENU/OSDMENU.CNF"
#endif

#ifndef LAUNCHER_PATH
#define LAUNCHER_PATH "mc?:/OSDMENU/launcher.elf"
#endif

#ifndef DKWDRV_PATH
#define DKWDRV_PATH "mc?:/OSDMENU/DKWDRV.ELF"
#endif

// Partition containing OSDMENU.CNF
#define HOSD_CONF_PARTITION "hdd0:__sysconf"

// Partition containing launcher.elf and DKWDRV
#define HOSD_SYS_PARTITION "hdd0:__system"

// Path to HDD-OSD ELF relative to __system partition root
#define HOSD_HDDOSD_PATH "/osd100/hosdsys.elf"

// Path to launcher.elf relative to __system partition root
#ifndef HOSD_LAUNCHER_PATH
#define HOSD_LAUNCHER_PATH "/osdmenu/launcher.elf"
#endif

// Full path to DKWDRV, including the partition
#ifndef HOSD_DKWDRV_PATH
#define HOSD_DKWDRV_PATH HOSD_SYS_PARTITION ":pfs:/osdmenu/DKWDRV.ELF"
#endif

#endif
