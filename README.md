# PSVSD/AM

**Accessory Storage Mount** — developed by RefurbedTech with AI assistance. A PS Vita PCH-1000 app and kernel plugin that simplifies setup and management of internal USB microSD storage through the accessory port. Features include automatic ux0/uma0 mounting, storage conflict checks, read/write verification, and diagnostic logging.

AI tools assisted with code development, documentation, and testing. RefurbedTech directed development and performed hardware testing.

Version **1.1.7** · Vita title ID **RTPSV0001** · Package version **01.17**

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

Host checks: `sh tests/run_tests.sh`. These exercise control flow with mocked Vita APIs; they cannot prove hardware behavior, private firmware offsets or power-loss recovery. Original hardware reports confirmed secondary file transfers/standby recovery and primary mounting. Version 1.1.7 has passed the host suite, a native VitaSDK build and static import checks; console testing is pending.

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
Its temporary bubble is removed when you choose Exit or Reboot, if system
promotion allows removal. It can remain visible while this app stays open.
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
checks compare numeric versions, optionally prefixed by `v` or `v.`. Supported
Vita APP_VER mapping is MAJOR.(MINOR*10+PATCH), with minor/patch each 0–9.

Version 1.1.6 and later select the installer from the verified download, so they
can install 1.1.7 through the app after the release is published. Older versions
that reuse their own helper may need a manual VitaShell installation once.
Same-version releases are not offered again.

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
is automatically exported to `ux0:data/PSVSD_Accessory/test.log`; export failures
are reported on screen. Offline skips remain silent.


### Bundled TLS — no iTLS-Enso requirement

The current 1.1.7 build includes libcurl, Mbed TLS and a Mozilla CA bundle inside
the VPK. Update checks no longer use the Vita's system HTTPS implementation.
Users do not need to install iTLS-Enso or modify system certificates. TLS 1.2 or
newer, certificate-chain validation and hostname verification remain required.
The network feature still needs testing on the Vita; it does not change storage
settings. If it fails, the Updates page includes a readable libcurl error reason.

### Focus and installer cleanup (1.1.7)

The user confirmed that 1.1.6 opens the updated app but then sends it to the home
screen while it remains running; reopening it manually stays in the app. This is
a focus interruption, not a confirmed crash. The new app no longer destroys the
helper at startup or schedules package removal after a two-second delay.

Shell-facing cleanup runs only when the user chooses Exit or Reboot, after any
requested Save succeeds. It targets only the temporary installer, records errors,
and clears its receipt/result files after successful removal. If removal fails,
a later explicit Exit/Reboot can retry. The helper may remain visible until then.
There is no idle cleanup scheduler or automatic retry loop.

### Artwork refresh (1.1.7)

Startup only compares the four artwork files and version marker. That check does
not write caches, load the LiveArea module or request system metadata changes.
It shows a pending-artwork notice when necessary. Actual verified cache repair
and LiveArea replacement are deferred to Exit/Reboot along with helper cleanup.

The user confirmed that a full restart displays the current artwork. Choose
Reboot to apply pending metadata and clear the home screen's retained images.
The app never forces a restart, rebuilds the database or changes bubble layout.

### Updater display and return

The helper keeps its two framebuffers. It now waits using the framebuffer-specific
API and verifies the immediately presented buffer before allowing another render.
A delayed flip keeps its buffers untouched; failed display calls are logged and
retried without advancing the buffer state. A spare vertical blank paces drawing.
These checks replace the assumption that one generic vblank completed each flip.

The confirmed-working title-ID return launch and per-attempt startup receipt are
retained. The main app draws and acknowledges startup without closing the helper.
The helper exits when it observes the receipt; if suspended, it can remain until
explicit user Exit/Reboot cleanup. It still times out visibly when startup is not
acknowledged and offers an explicit retry. Accepted launches are not resubmitted
automatically. The next app launch imports update/handoff.log into Log.

The focus and display fixes still require Vita confirmation. If focus is taken
again, note the timing and export Log; the cleanup/LiveArea operations are no
longer performed while you are using the app.
