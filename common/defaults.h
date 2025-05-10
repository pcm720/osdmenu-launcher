// Defines paths used by both the patcher and the launcher
#ifndef _DEFAULTS_H_
#define _DEFAULTS_H_

//
// OSDMenu paths
//

// All files must be placed on the memory card.
// mc? paths are also supported.
#ifndef CONF_PATH
#define CONF_PATH "mc0:/SYS-CONF/OSDMENU.CNF"
#endif

#ifndef LAUNCHER_PATH
#define LAUNCHER_PATH "mc0:/BOOT/launcher.elf"
#endif

#ifndef DKWDRV_PATH
#define DKWDRV_PATH "mc0:/BOOT/DKWDRV.ELF"
#endif

//
// HOSDMenu paths
//

// Partition containing OSDMENU.CNF
#define HOSD_CONF_PARTITION "hdd0:__sysconf"

// Partition containing launcher.elf and DKWDRV
#define HOSD_SYS_PARTITION "hdd0:__system"

// Path relative to HOSD_CONF_PARTITION root
#ifndef HOSD_CONF_PATH
#define HOSD_CONF_PATH "/OSDMENU/OSDMENU.CNF"
#endif

// Path to HDD-OSD ELF relative to HOSD_SYS_PARTITION partition root
#ifndef HOSD_HDDOSD_PATH
#define HOSD_HDDOSD_PATH "/osd100/OSDSYS_A.XLF"
#endif

// Path to launcher.elf relative to HOSD_SYS_PARTITION partition root
#ifndef HOSD_LAUNCHER_PATH
#define HOSD_LAUNCHER_PATH "/osdmenu/launcher.elf"
#endif

// Full path to DKWDRV, including the partition
#ifndef HOSD_DKWDRV_PATH
#define HOSD_DKWDRV_PATH HOSD_SYS_PARTITION ":pfs:/osdmenu/DKWDRV.ELF"
#endif

#endif
