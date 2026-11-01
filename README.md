# PSVSD/AM

**Accessory Storage Mount** — developed by RefurbedTech with AI assistance. A PS Vita PCH-1000 app and kernel plugin that simplifies setup and management of internal USB microSD storage through the accessory port. Features include automatic ux0/uma0 mounting, storage conflict checks, read/write verification, and diagnostic logging.

AI tools assisted with code development, documentation, and testing. RefurbedTech directed development and performed hardware testing.

Version **1.2.2** · Vita title ID **RTPSV0001** · Package version **01.22**

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

Popup confirmation buttons use teal backgrounds and Cancel buttons use dark
backgrounds, with original text colors. An underline indicates selection;
actions and default Cancel choices are unchanged.

Use the sidebar or D-pad to navigate. Cross activates choices; Circle/Back returns to the previous menu and never exits the app. Reboot and Exit each offer a save option and a proceed-without-save option. Square exports the report. On Log, Triangle opens Clear Log with Cancel selected by default.

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

Host checks: `sh tests/run_tests.sh`. These exercise control flow with mocked Vita APIs; they cannot prove hardware behavior, private firmware offsets or power-loss recovery. Original hardware reports confirmed secondary file transfers/standby recovery and primary mounting. Version 1.2.2 has passed the host suite, a native VitaSDK build and static import checks; console testing is pending.

GPL-3.0-or-later; see [COPYING](COPYING) and [third-party notices](docs/THIRD_PARTY_NOTICES.md). Derived from USBMC by Yifan Lu and VitaShell storage code by TheFloW. Font and taiHEN SDK components retain their own licenses.


## Check for updates

The app quietly checks the latest stable GitHub release once at launch when
Wi-Fi is already connected. It does not block menu navigation, enable Wi-Fi,
prompt for a connection or force an update. Offline/error results do not open a
dialog. A newer release is noted on Overview and on the Updates page.

Open **Updates** to read release notes. Select **Update to [version]** and confirm
**Install Update** to download and install; Cancel is selected initially. Pending
mount edits must be saved or discarded first. A manual **Check for Updates**
button remains when no installable newer version is available. Checks are limited
to one GitHub attempt per minute; Left/Right or swipes browse notes.

Only a named `PSVSD_AM_*.vpk` asset from this repository with a GitHub-provided
SHA-256 digest is installable. The app downloads over verified HTTPS, checks the
digest, extracts into its own staging directory, checks ZIP CRCs and validates
TITLE_ID, CONTENT_ID and APP_VER before handing off to a separate installer.
The helper closes PSVSD/AM, installs the staged package and reopens PSVSD/AM.
Its temporary bubble is removed when you choose Exit, if system promotion
allows removal. Reboot deliberately skips this Shell-facing operation.
Failures leave diagnostics in the app's update data; pre-install validation
failures do not touch the live app. Keep power available during installation.

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

Version 1.1.6 and later select the helper from the verified download. However,
installed versions through 1.1.8 still initiate that first handoff with the older
URI call. If an old version stops at installer LiveArea, press Start once to
finish, or install 1.2.2 manually with VitaShell. Only after the new main app is
running can its corrected installer-launch call be used. Same-version releases
are not offered again.

The default/arbitrary system-date compatibility change is still pending. TLS
certificate and hostname verification remain enabled. Installer handoff and
installation still require console testing; host checks cannot prove hardware
or power-loss behavior. There is no login, token or telemetry.

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


### Bundled TLS — no iTLS-Enso requirement

The current 1.2.2 build includes libcurl, Mbed TLS and a Mozilla CA bundle inside
the VPK. Update checks no longer use the Vita's system HTTPS implementation.
Users do not need to install iTLS-Enso or modify system certificates. TLS 1.2 or
newer, certificate-chain validation and hostname verification remain required.
The network feature still needs testing on the Vita; it does not change storage
settings. If it fails, the Updates page includes a readable libcurl error reason.

### Update handoffs (1.2.2)

Both directions use the 1.2.0 title-launch sequence, with flags 0x70000.
The installer exits after an accepted request; a failed request stays visible
for retry. The Shell may still ask to close the installer. Startup does not
perform package deletion or LiveArea refresh. Explicit Exit removes the helper.
One-shot startup receipts expire after five minutes and are diagnostic only.

### Save and Reboot after an update

The user reports normal boot works, but Save and Reboot after updating minimizes
the app. The previous sequence requested LiveArea refresh and helper removal
before reset. Those Shell-facing operations are now absent from the reboot path.

Save still verifies app copies and commits settings first. On success, reboot
prepares artwork cache files using file I/O only, then calls the existing cold
reset API directly. It logs the returned code and displays an error if reset is
rejected. Save failure still prevents reboot. No storage mapping logic changes.

Actual LiveArea refresh and helper removal remain available on explicit Exit.
The temporary helper bubble can remain through reboot until a later Exit. Startup and idle code do not perform package deletion. This timing tradeoff keeps Shell calls
out of the foreground-dependent reboot sequence.

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

### Testing status

Build, host tests and package checks pass. Both automatic launches, the
post-update Save/Reboot path and artwork reload still require Vita confirmation.
Default-date TLS compatibility remains unresolved. No GitHub changes are made
by preparing these packages.

## 1.2.2 regression correction

The 1.2.1 hardware log showed a successful install, a 33-second delayed startup,
successful automatic cleanup and subsequent main-app activity despite the reported
return to LiveArea. It did not establish a process crash or isolate which of the
two new Shell operations caused lost focus. The generic 0x70000 title launch from
1.2.0 is restored and automatic startup package deletion is removed. Cleanup is
again on explicit Exit. The earlier close-installer prompt may remain. This is a
regression recovery, not a claim that prompt suppression has been solved.

Popup confirmation buttons now use the teal background and Cancel the dark card
background. Original text colors are restored. A selection underline preserves
visible focus; default Cancel selection, actions and positions are unchanged.
The Reboot/Exit choice screens retain their original styling.

One-shot five-minute diagnostic receipts are retained. Installer display, storage
copying and reboot logic are unchanged. Validate this build on Vita.
