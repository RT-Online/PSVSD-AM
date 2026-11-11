# PSVSD/AM

**Accessory Storage Mount** — developed by RefurbedTech with AI assistance. A PS Vita PCH-1000 app and kernel plugin that simplifies setup and management of internal USB microSD storage through the accessory port. Features include automatic ux0/uma0 mounting, storage conflict checks, read/write verification, and diagnostic logging.

AI tools assisted with code development, documentation, and testing. RefurbedTech directed development and performed hardware testing.

Version **1.2.9** · Vita title ID **RTPSV0001** · Package version **01.29**

The launcher displays the `1.2` series. The app and update metadata show `1.2.9`.
See [the Vita test guide](docs/UPDATE_TEST.md) for update, reboot and speed checks.

## Features

- Automatic mounting as `uma0:` extra storage or `ux0:` main storage.
- Manual secondary mounting and a 4 MiB read/write verification test.
- Sequential and 4 KiB random read/write benchmarks on the verified PSVSD mount.
- Event-driven standby recovery for the owned USB mapping.
- Touch and controller navigation, diagnostic export, and confirmed log clearing.
- On-demand GitHub release checks, with Wi-Fi detection and paged release notes.
- Storage-plugin conflict checks and backed-up configuration changes.
- Verified copying of PSVSD/AM and VitaShell before switching to USB main storage.

This uses the accessory interface; a reader connected only through the 3G slot needs the corresponding 3G storage plugin. Use one USB storage connection at a time.

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

Popup confirmation buttons use teal backgrounds and Cancel buttons use dark
backgrounds, with original text colors. An underline indicates selection;
actions and default Cancel choices are unchanged.

Use the sidebar or D-pad to navigate. Cross activates choices; Circle/Back returns to the previous menu and never exits the app. Reboot and Exit each offer a save option and a proceed-without-save option. Square exports the report. On Log, Triangle opens Clear Log with Cancel selected by default.

## Read / write test

Choose **Verify** or press Cross to write and check a 4 MiB file. Choose
**Speed Test** or press Triangle to measure the mounted PSVSD. Speed Test uses
a temporary 32 MiB file, reports sequential MiB/s and measures 512 aligned
4 KiB operations per random pass. Random results show MiB/s and IOPS. Each
write pass includes one final flush. Read rates may include system and card
caching. Results are shown on the page and saved in Log.

## Configuration and recovery

- Settings: `ur0:data/PSVSD_AM/settings.ini`
- Boot plugin: `ur0:tai/psvsd_am.skprx`
- Kernel/application logs: `ur0:data/PSVSD_AM/`
- Exported report: `ux0:data/PSVSD_AM/test.log`

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

Run host checks with `sh tests/run_tests.sh`. The Vita test guide covers
the installer handoff, automatic return and clock notice. Earlier
hardware testing confirmed secondary file transfers, standby recovery and
primary mounting.

GPL-3.0-or-later; see [COPYING](COPYING) and [third-party notices](docs/THIRD_PARTY_NOTICES.md). Derived from USBMC by Yifan Lu and VitaShell storage code by TheFloW. Font and taiHEN SDK components retain their own licenses.


## Check for updates

The app checks the latest stable GitHub release over HTTPS once at launch when
Wi-Fi is connected. You can keep using the app during the check. A newer release
is noted on Overview and on the Updates page.

If an HTTPS certificate fails its date check, PSVSD/AM opens a clock notice. Open
**Settings → Date & Time → Date & Time Settings** on the Vita and choose
**Set via Internet → Set Now** or **Set Manually**. Then return to the app and
select **Check for Updates**.

Open **Updates** to read release notes. Select **Update to [version]** and confirm
**Install Update** to download and install; Cancel is selected initially. Pending
mount edits must be saved or discarded first. A manual **Check for Updates**
button remains when no installable newer version is available. Checks usually
have a one-minute cooldown. You can retry immediately after a date error. Use
Left/Right or swipe to browse notes.

The updater checks the VPK SHA-256 before extracting into its staging directory.
It verifies ZIP CRCs and app and installer metadata before handing off to the
temporary installer. The helper stops the previous PSVSD/AM process, promotes the
staged package and requests that PSVSD/AM reopen. On the return from the
installer, PSVSD/AM removes the installer bubble before drawing its first
screen. Update and cleanup failures appear in the log.

Downloads show transferred MiB and percentage when the total is known; unknown
totals show an animated bar. Verification and extraction show their own progress.
The temporary installer reuses PSVSD/AM's actual interface and branding,
animates during system promotion, then shows completion and returns automatically;
it does not invent an installation percentage.

The updater preserves mount settings and does not change other app folders.
It updates the app on the currently mounted ux0 only. If a release changes the
boot plugin, use Save and Reboot afterward to refresh the managed boot plugin.
The separate main-storage preparation limitation below still applies. Release
checks compare numeric versions, optionally prefixed by `v` or `v.`. Semantic version components may exceed 9. Vita APP_VER is an independent,
monotonically increasing NN.NN package revision. Both app and helper carry
AM_VERSION so installation validates the exact release tag and matching package.

The installed version controls the launch of the temporary installer. The helper
from the downloaded VPK controls the return. An in-app upgrade from 1.2.8
exercises the new return sequence.

### Switching storage in either direction

When USB is being changed to ux0, the app copies PSVSD/AM and VitaShell from the
current ux0 to verified USB uma0. When leaving USB ux0 (selecting USB uma0 or
disabling automatic mounting), it exposes the exact pre-USB original storage at
uma0 and copies those same two complete app folders back. Both directions stage,
verify every file byte for byte, replace the full destination folders (including
removal of stale destination-only files), verify again and block Save on errors.
Other app folders and gamecard storage are not touched.

The resident boot plugin captures the original storage's block paths before
redirecting ux0. The app requests that specific storage; it does not guess from
whatever volume happens to be mounted. An occupied or unverified uma0 is refused.
The live USB ux0 mapping is retained throughout preparation; reboot applies the
saved target. Missing original storage blocks Save instead of allowing an unsafe
switch. Keep the original memory card inserted during this process.

**One-time upgrade step for a system already using USB ux0:** install this revised
1.1.7 build, leave the target at ux0 and use Save and Reboot once. That loads the
updated boot plugin so it can capture the original storage. Then select uma0
(or disable mounting) and Save and Reboot to perform the reverse copy. An old
resident plugin cannot provide the new mapping information until rebooted.

The version checker does not offer same-version replacements. Native/hardware
return-storage mounting still requires Vita testing.

Update-check failures now show the failed step, hexadecimal error, HTTP status
and available TLS details directly on the Updates page. The Log refreshes after
a check and when opened, and shows the latest page. After a manual check, a current diagnostic report
is automatically exported to `ux0:data/PSVSD_AM/test.log`; export failures
are reported on screen. Offline skips remain silent.


### Bundled TLS

The VPK includes libcurl, Mbed TLS and a Mozilla CA bundle. Both the version
check and VPK download require TLS 1.2 or newer with certificate and hostname
validation. The app verifies the VPK against GitHub's release-asset SHA-256.

### Installer handoff

The handoff uses paired play-URI requests. The installer tries a direct return
to PSVSD/AM and retries with the previous launch flags if both direct requests
fail. A successful request lets the installer exit. The launch flags and
results go into the log. A boot-relative timestamp identifies a recent return
from the installer and enables one cleanup attempt before the first frame.

### Save and Reboot after an update

When you keep the current storage mount, Save and Reboot commits settings,
prepares the artwork cache through file I/O, and requests a cold reset.
On return from an update, PSVSD/AM closes the installer title and removes its
bubble before displaying the app. A staged update awaiting installation keeps
its installer. Exit and app-folder copying can remove a leftover installer.
Full LiveArea artwork refresh runs on Exit.

### Artwork and data folders

Startup performs a read-only artwork check. Before reboot, changed cache images
are verified and copied without loading/notifying LiveArea. A full restart clears
the Shell's retained images; updated artwork after this revised sequence needs
Vita confirmation. The app does not force a restart or rebuild the database.

| Purpose | Path |
| --- | --- |
| Exported report | `ux0:data/PSVSD_AM/test.log` |
| Updater/staging/receipts | `ux0:data/PSVSD_AM/update/` |
| Settings | `ur0:data/PSVSD_AM/settings.ini` |
| Application and kernel logs | `ur0:data/PSVSD_AM/` |

The 1.1.8 migration remains: known legacy settings/logs are copied only when the
current destination is absent, streamed within 16 MiB per file, byte checked and
renamed into place. Existing current files win. Legacy originals remain backups;
completion markers prevent Clear Log from reimporting old records. Failed groups
remain retryable. Unknown files are untouched.

The first helper started by an old caller bridges its staged update folder before
reading ready/package data. Matching helper-version readiness is required; an
inactive current folder is archived without overwriting update.pre-migration.
Failed moves attempt rollback. Settings readers can fall back to old settings
when current settings are missing. The boot-plugin filename/tai entry stays stable.

After upgrading, use Reboot → Save and Reboot once to stage the current boot
plugin, copy the current app if switching storage, and apply the saved target.
An older resident/boot plugin can still use legacy paths until that reboot.
