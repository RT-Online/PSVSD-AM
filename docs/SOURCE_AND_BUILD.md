# Source and build

## Build the app and all three kernel payloads

Install VitaSDK, then run from the repository root:

```sh
export VITASDK=/path/to/vitasdk
export PATH="$VITASDK/bin:$PATH"
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Outputs:

- `build/PSVSD_AM.vpk`
- `build/psvsd_am.skprx`
- `build/usbmc_accessory_probe.skprx`
- `build/psvsd_am_status.skprx`

The VPK contains all three payloads. The internal app target is `psvsd_am_app`. SELF-generation flags are isolated between targets. The private-API auth ID follows upstream USBMC.

The checked-in generated font atlas and launcher images are sufficient for a normal build. To regenerate assets, install Pillow and run `python3 tools/make_assets.py`. Generated files use the bundled DejaVu fonts and version in `src/common/paths.h`.

## Host verification

```sh
sh tests/run_tests.sh
```

Requires a C99 compiler supporting AddressSanitizer and UndefinedBehaviorSanitizer. No VitaSDK is needed for these mocked tests. The suite covers boot/manual mounting, loader/log handling, configuration conflicts and save rollback, verified copying, mapping queries, rendering, input and touch. It does not execute Sony firmware.

Optional VELF import audit (PyYAML required):

```sh
python3 tests/audit_imports.py build/psvsd_am.velf "$VITASDK/share/vita-headers/db"
```

Repeat for other VELFs as needed. This compares database models for the 3.60 base and 3.63 overlay; it is not a running-console ABI test.

## Recorded dependencies

These pins are retained from the original release record, not downloaded or revalidated during this cleanup:

- USBMC: https://github.com/yifanlu/usbmc , revision `fe38cb16a19df856dc8b732949a350f36d6b5ee2`.
- Recorded VitaSDK release: `sdk-snapshot-20261015.850.1`, GCC 15.2.0.
- Recorded SDK archive SHA-256: `801f483928523fe63547faf58a7dfdaca7482b26c33af0064acea96a906f7662`.
- Public taiHEN developer SDK: https://github.com/yifanlu/taiHEN/releases/download/v0.11/taihen.tar.gz .
- Recorded taiHEN archive SHA-256: `6a7d4592b983292e8b8c53c624fa800e33ff69fe06c9d267cafc33d0ded4d2a9`.

The bundled taiHEN `.a` files are SDK import stubs needed by the linker, not the taiHEN runtime. Preserve their MIT license and the font license. No Sony firmware or mass-storage driver is distributed.

## Firmware-specific implementation

`src/plugin/main.c` holds the inherited mount structures, recognized I/O/AppMgr module NIDs and private offsets. Accessory activation resolves `SceUsbServ` function NID `0xAA6D4409` at runtime. USB mass-driver startup retains upstream retail-check patches at `0x1546` and `0x154C`.

The mass-driver check bounds the segment but does not authenticate the expected instructions. Recognizing an I/O module NID does not prove compatibility with every possible mass-driver build. Keep this limitation visible when extending firmware support.

The resume callback signals a worker; the worker waits for the owned USB device and remounts only that mapping. It does not activate the accessory supply during resume. Primary mounting is boot-only and refuses a live switch after SceShell starts.

## Publication

Keep generated VPK/SKPRX binaries and their checksums in GitHub Releases. Commit source, licensed build inputs, tests and concise documentation to the repository. The separate source-audit report records this cleanup and its validation limits.


## Version metadata

The app and diagnostic reports identify semantic version **1.0.3**. Vita's
two-part package APP_VER is **01.03**; kernel module metadata is major 1,
minor 3. These two-part fields do not express a semantic patch separately.
The settings schema remains `version=1`, and LiveArea XML remains format
`01.00`; neither is the software release number.

## Inherited 1.0.1 cleanup

Removed the obsolete fixed-width pagination header, test-only manager API
wrappers, unused definitions, unused kernel-info mock and unbuilt Sony-card
priority policy. Boot mounting follows saved settings; manual mounting keeps
ux0 unchanged. Exactly one kernel role must be selected when compiling.
Test adapters remain within tests. The app build target is now named
`psvsd_am_app`; installed paths and title ID remain compatible.


## 1.0.3 native build validation

Built on 31 October 2026 with the recorded VitaSDK snapshot (archive SHA-256
matched), GCC 15.2.0 and CMake. All four native targets compiled and linked
with warnings treated as errors. All four VELFs passed the import audit's
3.60 and 3.63-overlay database models. The VPK archive passed its CRC check;
its app, three kernel payloads, licenses and assets were verified, as were
TITLE_ID RTPSV0001 and APP_VER 01.03. Runtime source is unchanged from the
host-tested 1.0.3 tree. Console testing remains pending.

Install PSVSD_AM_1.0.3.vpk over the existing app in VitaShell. Open PSVSD/AM,
retain the intended settings, and use Save and Reboot to refresh the installed
boot plugin. Installing the VPK alone does not replace ur0:tai/psvsd_am.skprx.


## Inherited 1.0.2 complete folder replacement

When preparing USB main storage from a verified uma0 mount, the app now
replaces both complete destination folders (RTPSV0001 and VITASHELL), rather
than merging individual files. Destination-only files and directories are
removed from the active app locations. Other apps and the source trees are
not modified.

Both copies are staged and byte-verified first. Whole-folder renames retain
previous destination trees in the operation's unique staging/old directory.
A promotion or verification failure rolls back both roots, in reverse order.
Failed rollback or lost mount verification retains recovery data and blocks
Save. Successful verification removes staging; cleanup failures are logged.
This is not a power-loss-atomic transaction across both folders. An interrupted
rename may need manual recovery from the retained staging directory.

Verification checks the complete directory manifest (hidden files and empty
directories included), file sizes, EOF and every byte. Source additions or
unexpected destination files during verification are rejected. Existing source
remnants will still be copied; this is an exact mirror, not an app-file allowlist.

Copy preparation still applies only when switching from the current ux0 to a
verified USB uma0 for a saved ux0 boot target. It skips copying if USB is already
verified as live ux0, and it does not run for Disabled/uma0 settings.


## 1.0.3 update check

`updates.c` implements bounded JSON parsing, strict numeric stable-version
comparison and an injectable Wi-Fi/HTTP transport. `update_network.c` implements
that transport with native Net/NetCtl/SSL/HTTP APIs. A connected NetCtl state
and a nonempty Wi-Fi SSID are required before any HTTP setup/request. The SSID
is neither sent nor logged. No auto-connect API is called.

The only endpoint is the public latest-release API:
https://api.github.com/repos/RT-Online/PSVSD-AM/releases/latest
Primary API reference: https://docs.github.com/en/rest/releases/releases#get-the-latest-release

Requests have stage timeouts and a bounded response-read duration/size (64 KiB).
Release notes are limited to 4095 displayed bytes and the renderer's ASCII glyph
range; non-ASCII characters are replaced with question marks. JSON nesting is
bounded; duplicate release fields, malformed/truncated responses, unsupported
tags, drafts and prereleases are rejected. Unknown API fields are skipped.
GitHub controls which stable release its latest endpoint returns; this is not
a scan/sort of all tags or a cryptographic release verification.

Certificate chain, hostname and date verification stay enabled. There is no
insecure fallback, redirect following, VPK download or installation. SSL/HTTP
resources are initialized only after Wi-Fi gating; resources owned by this check
are released. The main loop polls the completion flag of a dedicated worker,
then joins/deletes it and displays the result. Saved mount configuration is not
modified. Tests inject the transport to prove zero HTTP calls while offline and
to exercise version/error parsing; native networking remains a console test.

New imports are SceNet, SceNetCtl, SceHttp, SceSsl and SceSysmodule. Existing
kernel mounting/copy behavior is unchanged apart from release metadata.
