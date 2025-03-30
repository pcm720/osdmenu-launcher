## Latest Release includes the full SAS packaages ready to use by themselves or with exploits featuring autoboot elf settings.
**I'l be off computer soon, I've been glued to getting this adapted.**

## ORIGINAL README:
# OSDSYS Menu Launcher

This is an attempt at porting Free McBoot 1.8 OSDSYS patches to modern PS2SDK.  
Tested on SCPH-39004 and SCPH-70000.

Will not work on "protokernel" systems (SCPH-10000, SCPH-15000) and possibly SCPH-18000 and SCPH-50009.

## Usage

1. Copy `patcher.elf` and `launcher.elf` to `mc?:/BOOT/`  
   Copy DKWDRV to `mc?:/BOOT/DKWDRV.ELF` _(optional)_ 
2. Edit `mc?:/SYS-CONF/OSDMENU.CNF` [as you see fit](#fmcb-handler)
3. Configure PS2BBL to launch `mc?:/BOOT/patcher.elf` or launch it manually from LaunchELF

## Key differences from FMCB 1.8:
- All initialization code is removed in favor of using a separate bootloader to start the patcher (e.g. [PS2BBL](https://github.com/israpps/PlayStation2-Basic-BootLoader))
- USB support is dropped from the patcher, so only memory cards are checked for `OSDMENU.CNF`
- No ESR support
- No support for launching ELFs by holding a gamepad button
- ELF paths are not checked by the patcher, so every named entry from FMCB config file is displayed in hacked OSDSYS menu
- A separate launcher is used to launch menu entries
- CD/DVD support was extended to support skipping PS2LOGO, mounting VMCs on MMCE devices, showing visual GameID for PixelFX devices and booting DKWDRV for PS1 discs

Due to memory limitations and the need to support more devices, the original FMCB launcher was split into two parts: patcher and launcher.

## Patcher

This is a slimmed-down and refactored version of OSDSYS patches from FMCB 1.8 for modern PS2SDK.  
It reads settings from `mc?:/SYS-CONF/OSDMENU.CNF` and patches the `rom0:OSDSYS` binary with the following patches:
- Custom OSDSYS menu entries
- Infinite scrolling
- Custom button prompts and menu header
- Automatic disc launch bypass
- Force PAL/NTSC video mode
- HDD update check bypass
- Override PS1 and PS2 disc launch functions with custom code that starts the launcher

See the list for supported `OSDMENU.CNF` options [here](#freemcbcnf).  
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


## OSDMENU.CNF

Most of `OSDMENU.CNF` settings are directly compatible with those from FMCB 1.8 `FREEMCB.CNF`.

1. `OSDSYS_video_mode` — force OSDSYS mode. Valid values are `AUTO`, `PAL` or `NTSC`
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

## Credits

- Everyone involved in developing the original Free MC Boot and OSDSYS patches, especially Neme and jimmikaelkael
- [TonyHax International](https://github.com/alex-free/tonyhax) developers for PS1 game ID detection for generic executables.
- Maximus32 for creating the [`smap_udpbd` module](
https://github.com/rickgaiser/neutrino)
- Matías Israelson for making [PS2BBL](https://github.com/israpps/PlayStation2-Basic-BootLoader)
- CosmicScale for [RetroGEM Disc Launcher](https://github.com/CosmicScale/Retro-GEM-PS2-Disc-Launcher)
