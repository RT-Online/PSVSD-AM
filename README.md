# PSVSD/AM

**Accessory Storage Mount** — developed by RefurbedTech with AI assistance. A PS Vita PCH-1000 app and kernel plugin that simplifies setup and management of internal USB microSD storage through the accessory port. Features include automatic ux0/uma0 mounting, storage conflict checks, read/write verification, and diagnostic logging.

AI tools assisted with code development, documentation, and testing. RefurbedTech directed development and performed hardware testing.

Version **1.0.3** · Vita title ID **RTPSV0001** · Package version **01.03**

## Features

- Automatic mounting as `uma0:` extra storage or `ux0:` main storage.
- Manual secondary mounting and a 4 MiB read/write verification test.
- Event-driven standby recovery for the owned USB mapping.
- Touch and controller navigation, diagnostic export, and confirmed log clearing.
- On-demand GitHub release checks, with Wi-Fi detection and paged release notes.
- Storage-plugin conflict checks and backed-up configuration changes.
- Verified copying of PSVSD/AM and VitaShell before switching to USB main storage.

This uses the accessory interface; a reader connected only through the 3G slot needs the corresponding 3G storage software. Use one USB storage connection at a time.

## Installation

1. Build `PSVSD_AM.vpk` using [the build guide](docs/SOURCE_AND_BUILD.md), or obtain a separately published release VPK.
2. Install it with VitaShell. Existing installations use the same title ID.
3. Enable Unsafe Homebrew in HENkaku Settings. Keep a working recovery storage setup.
4. Initially leave automatic mounting **Disabled**, select `uma0:`, then run **Read / write test**.
5. To enable automatic mounting, choose the target in **Mount setup**, then **Reboot → Save and Reboot**.

Automatic cold-boot mounting requires persistent taiHEN/Ensō. Otherwise loading occurs when taiHEN is activated. Unknown I/O module builds are refused; recognized module cases are inherited from 3.60, 3.65, 3.67 and 3.68. A spoofed firmware display does not establish support for that native firmware.

## Using main storage

Prepare other games, savedata, licenses and storage content separately before selecting `ux0:`. The app copies **only** `ux0:app/RTPSV0001` and `ux0:app/VITASHELL` to the verified USB `uma0:app` folders. Both complete destination app folders are replaced automatically, including removal of destination-only files and directories. The app stages and verifies the copies, moves the previous folders into rollback storage, promotes the new folders, then verifies their full contents and directory structure before Save continues. Unrelated app folders are untouched.

This mirrors the source folders exactly; it does not remove obsolete files already present in the source. There is no live `ux0:` switch. Save and Reboot applies the new boot setting; Save and Exit applies it at the next reboot. Saving while USB is already the verified live `ux0:` skips copying. A loaded mount plugin remains resident until reboot.

## Controls

Use the sidebar or D-pad to navigate. Cross activates choices; Circle/Back returns to the previous menu and never exits the app. Reboot and Exit each offer a save option and a proceed-without-save option. Square exports the report. On Log, Triangle opens Clear Log with Cancel selected by default.

## Configuration and recovery

- Settings: `ur0:data/PSVSD_Accessory/settings.ini`
- Boot plugin: `ur0:tai/psvsd_am.skprx`
- Kernel/application logs: `ur0:data/PSVSD_Accessory/`
- Exported report: `ux0:data/PSVSD_Accessory/test.log`

Save manages its marked block in `ur0:tai/config.txt` and retains first backups as `.psvsdam.bak`. Unfinished `.psvsdam.swap` files stop further replacement for recovery. Existing YAMT/StorageMgr assignments are inspected, not changed automatically. Unknown configuration formats stop enabling rather than being guessed.

To disable: select Disabled and Save and Reboot. For boot recovery on Ensō, hold L to bypass plugins, then use recovery storage/VitaShell to remove the PSVSD/AM entry or disable its settings. Review backups before restoring a whole configuration; the first backup may predate other edits.

Interrupted copies may retain recovery data in `uma0:data/PSVSD_AM/prepare-XXXXXXXX`. The app does not format cards or migrate all main-storage content.

## Source layout

| Path | Purpose |
| --- | --- |
| `src/app/` | UI, input, settings/configuration manager, verified copying and module loading |
| `src/plugin/` | Boot mount, manual mount and read-only query roles |
| `src/common/` | Shared paths and strict settings parser |
| `src/vendor/taihen/` | Licensed public SDK header/import stubs |
| `assets/`, `tools/` | Launcher art, fonts and asset generator |
| `tests/` | Host mocks and regression checks |

The three kernel payloads are intentional: `psvsd_am.skprx` is the boot plugin, `usbmc_accessory_probe.skprx` performs manual secondary mounting, and `psvsd_am_status.skprx` verifies the current USB mapping. The latter two are used by the released app.

## Validation and license

Host checks: `sh tests/run_tests.sh`. These exercise control flow with mocked Vita APIs; they cannot prove hardware behavior, private firmware offsets or power-loss recovery. Original hardware reports confirmed secondary file transfers/standby recovery and primary mounting. Version 1.0.3 has passed the host suite, a native VitaSDK build and static import checks; console testing is pending.

GPL-3.0-or-later; see [COPYING](COPYING) and [third-party notices](docs/THIRD_PARTY_NOTICES.md). Derived from USBMC by Yifan Lu and VitaShell storage code by TheFloW. Font and taiHEN SDK components retain their own licenses.


## Check for updates

Open **Updates → Check for Updates**. The app first checks for a connected Wi-Fi
network. When offline it displays **Connect to Wi-Fi to check for updates** and
makes no HTTP request. It does not enable Wi-Fi or start a connection for you.

When connected, it checks the latest stable public release of
https://github.com/RT-Online/PSVSD-AM and compares numeric `vMAJOR.MINOR.PATCH`
tags. It distinguishes an available update, the current public version, and an
installed build newer than the public release. Release notes are displayed as
bounded plain text with Prev/Next controls. Use Cross or tap the Check button;
Left/Right or swipes browse notes.

Checks are manual, run on a worker thread, and are limited to one GitHub attempt
per minute. There is no startup polling, download, installation, login, token,
telemetry or storage-setting change. A disconnected network, HTTPS failure,
missing release, invalid response or rate limit does not change your saved
mount settings. TLS verification remains enabled; network/TLS operation needs
console testing. An API check cannot prove that a release is hardware-safe.

### Returning to other main storage

Switching away from USB ux0 currently does not automatically verify or update
PSVSD/AM and VitaShell on the storage that will become ux0. Keep those recovery
tools installed and updated on that storage before changing the boot target.
The new update checker does not resolve this separate preparation gap.
