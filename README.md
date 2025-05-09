# OSDMenu

Free McBoot 1.8 OSDSYS patches ported to modern PS2SDK with some useful additions.  

## Usage

1. Copy `patcher.elf` and `launcher.elf` to `mc?:/BOOT/`  
   Copy DKWDRV to `mc?:/BOOT/DKWDRV.ELF` _(optional)_ 
2. Edit `mc?:/SYS-CONF/OSDMENU.CNF` [as you see fit](#osdmenucnf)
3. Configure PS2BBL to launch `mc?:/BOOT/patcher.elf` or launch it manually from LaunchELF

## Key differences from FMCB 1.8:
- All initialization code is removed in favor of using a separate bootloader to start the patcher (e.g. [PS2BBL](https://github.com/israpps/PlayStation2-Basic-BootLoader))
- USB support is dropped from the patcher, so only memory cards are checked for `OSDMENU.CNF`
- No ESR support
- No support for launching ELFs by holding a gamepad button
- ELF paths are not checked by the patcher, so every named entry from FMCB config file is displayed in hacked OSDSYS menu
- A separate launcher is used to launch menu entries
- CD/DVD support was extended to support skipping PS2LOGO, mounting VMCs on MMCE devices, showing visual GameID for PixelFX devices and booting DKWDRV for PS1 discs
- "Unlimited" number of paths for each entry
- Support for 1080i and 480p (as line-doubled 240p) video modes
- Support for "protokernel" systems (SCPH-10000, SCPH-15000) ported from Free McBoot 1.9 by reverse-engineering
- Support for launching applications from the memory card browser

Due to memory limitations and the need to support more devices, the original FMCB launcher was split into two parts: patcher and launcher.

## Patcher

This is a slimmed-down and refactored version of OSDSYS patches from FMCB 1.8 for modern PS2SDK with some new patches sprinkled in.  
It reads settings from `mc?:/SYS-CONF/OSDMENU.CNF` and patches the `rom0:OSDSYS` binary with the following patches:
- Custom OSDSYS menu with up to 255 entries
- Infinite scrolling
- Custom button prompts and menu header
- Automatic disc launch bypass
- Force GS video mode to PAL, NTSC, 1080i or line-doubled 480p (with half the vertical resolution).  
  Due to how to OSDSYS renders everything, "true" 480p can't be implemented easily
- HDD update check bypass
- Override PS1 and PS2 disc launch functions with custom code that starts the launcher
- Additional system information in version submenu (Video mode, ROM version, EE, GS and MechaCon revision)
- Launch SAS-compatible applications from the memory card browser if `title.cfg` exists in the directory (see [config handler](#config-handler))  
  This patch swaps around the "Enter" and "Options" menus and substitutes file properties submenu with the launcher.  
  To launch an app, just press "Enter" after selecting the app icon.  
  To copy or delete the save file, just use the triangle button.

Patches not supported/limited on protokernel systems:
- Automatic disc launch bypass
- Button prompt customization
- PAL video mode

### Configuration

See the list for supported `OSDMENU.CNF` options [here](#osdmenucnf).  
For every menu item and disc launch, it starts the launcher from `mc?:/BOOT/launcher.elf` and passes the menu index to it.

## Launcher

A fully-featured main ELF launcher that handles launching ELFs and CD/DVD discs.  
Supports passing arbitrary arguments to an ELF and can also be used standalone.

Supported paths are:
- `mmce?:` — MMCE devices. Can be `mmce0`, `mmce1` or `mmce?`
- `mc?:` — Memory Cards. Can be `mc0`, `mc1` or `mc?`
- `mass:` and `usb:` — USB devices (supported via BDM)
- `ata:` — internal exFAT-formatted HDD (supported via BDM)
- `mx4sio:` — MX4SIO (supported via BDM)
- `ilink:` — i.Link mass storage (supported via BDM)
- `udpbd:` — UDPBD (supported via BDM)
- `hdd0:` — internal APA-formatted HDD
- `cdrom` — CD/DVD discs
- `fmcb` — special path for the patcher

Device support can be enabled and disabled by changing build-time configuration options (see [Makefile](launcher/Makefile))

### `udpbd` handler

Reads PS2 IP address from `mc?:/SYS-CONF/IPCONFIG.DAT`

### `cdrom` handler

Waits for the disc to be detected and launches it.
Supports the following arguments:
- `-nologo` — launches the game executable directly, bypassing `rom0:PS2LOGO`
- `-nogameid` — disables visual game ID
- `-dkwdrv` — when PS1 disc is detected, launches DKWDRV from `mc?:/BOOT/DKWDRV.ELF` instead of `rom0:PS1DRV`
- `-dkwdrv=mc?:/<path to DKWDRV>` — same as `-dkwdrv`, but with custom DKWDRV path.

For PS1 CDs with generic executable name (e.g. `PSX.EXE`), attempts to guess the game ID using the volume creation date
stored in the Primary Volume Descriptor, based on the table from [TonyHax International](https://github.com/alex-free/tonyhax/blob/master/loader/gameid-psx-exe.c).

### `fmcb` handler
When the launcher receives `fmcb0:<idx>` or `fmcb1:<idx>` path, it reads `OSDMENU.CNF` from the respective memory card,
searches for `path?_OSDSYS_ITEM_<idx>` and `arg_OSDSYS_ITEM_<idx>` entries and attempts to launch the ELF.

Respects `cdrom_skip_ps2logo`, `cdrom_disable_gameid` and `cdrom_use_dkwdrv` for `cdrom` paths.

### Config handler
When the launcher receives a path that ends with `.CNF`, `.cnf`, `.CFG` or `.cfg`,
it will run the [quickboot handler](#quickboot-handler) using this file.

Config file can be located at any device as long as the device mountpoint is one of the listed above.

### Quickboot handler
When the launcher is started without any arguments, it tries to open `<ELF file name>.CNF`
file at the current working directory
and attempts to launch every path in order.

Quickboot file syntax example:
```ini
boot=boot.elf
path=mmce?:/apps/wle.elf
path=mmce?:/apps/wle2.elf
path=ata:/apps/wle.elf
path=mc?:/BOOT/BOOT.ELF
arg=-testarg
arg=-testarg2
```

`boot` — path relative to the config file  
`path` — absolute paths  
`arg` — arguments that will be passed to the ELF file 

## OSDMENU.CNF

Most of `OSDMENU.CNF` settings are directly compatible with those from FMCB 1.8 `FREEMCB.CNF`.

### Character limits

OSDMenu supports up to 250 custom menu entries, each up to 79 characters long.  
Note that left and right cursors are limited to 19 characters and top and bottom delimiters are limited to 79 characters.  
Launcher and DKWDRV paths are limited to 49 characters.

### Configuration options

1. `OSDSYS_video_mode` — force OSDSYS mode. Valid values are `AUTO`, `PAL`, `NTSC`, `480p` or `1080i`
2. `hacked_OSDSYS` — enables or disables OSDSYS patches
3. `OSDSYS_scroll_menu` — enables or disables infinite scrolling
4. `OSDSYS_menu_x` — menu X center coordinate
5. `OSDSYS_menu_y` — menu Y center coordinate
6. `OSDSYS_enter_x` — `Enter` button X coordinate (at main OSDSYS menu)
7. `OSDSYS_enter_y` — `Enter` button Y coordinate (at main OSDSYS menu)
8. `OSDSYS_version_x` — `Version` button X coordinate (at main OSDSYS menu)
9. `OSDSYS_version_y` — `Version` button Y coordinate (at main OSDSYS menu)
10. `OSDSYS_cursor_max_velocity` — max cursor speed
11. `OSDSYS_cursor_acceleration` — cursor speed
12. `OSDSYS_left_cursor` — left cursor text
13. `OSDSYS_right_cursor` — right cursor text
14. `OSDSYS_menu_top_delimiter` — top menu delimiter text
15. `OSDSYS_menu_bottom_delimiter` — bottom menu delimiter text
16. `OSDSYS_num_displayed_items` — the number of menu items displayed
17. `OSDSYS_Skip_Disc` — enables/disables automatic CD/DVD launch
18. `OSDSYS_Skip_Logo` — enables/disables SCE logo
19. `OSDSYS_Inner_Browser` — enables/disables going to the Browser after launching OSDSYS
20. `OSDSYS_selected_color` — color of selected menu entry
21. `OSDSYS_unselected_color` — color of unselected menu entry
22. `name_OSDSYS_ITEM_???` — menu entry name
23. `path?_OSDSYS_ITEM_???` — path to ELF. Also supports the following special paths: `cdrom`, `OSDSYS`, `POWEROFF`

New to this launcher:

24. `arg_OSDSYS_ITEM_???` — custom argument to be passed to the ELF. Each argument needs a separate entry.
25. `cdrom_skip_ps2logo` — enables or disables running discs via `rom0:PS2LOGO`. Useful for MechaPwn-patched consoles.
26. `cdrom_disable_gameid` — disables or enables visual Game ID
27. `cdrom_use_dkwdrv` — enables or disables launching DKWDRV for PS1 discs
28. `path_LAUNCHER_ELF` — custom path to launcher.elf. The path MUST be on the memory card
29. `path_DKWDRV_ELF` — custom path to DKWDRV.ELF. The path MUST be on the memory card
30. `OSDSYS_Browser_Launcher` — enables/disables patch for launching applications from the Browser 

## Credits

- Everyone involved in developing the original Free MC Boot and OSDSYS patches, especially Neme and jimmikaelkael
- [TonyHax International](https://github.com/alex-free/tonyhax) developers for PS1 game ID detection for generic executables.
- Maximus32 for creating the [`smap_udpbd` module](
https://github.com/rickgaiser/neutrino) and Neutrino GSM
- Matías Israelson for making [PS2BBL](https://github.com/israpps/PlayStation2-Basic-BootLoader)
- CosmicScale for [RetroGEM Disc Launcher](https://github.com/CosmicScale/Retro-GEM-PS2-Disc-Launcher)
