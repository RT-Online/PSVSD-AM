# Source and build

## Build the app and all three kernel payloads

Install VitaSDK, then run from the repository root:

```sh
export VITASDK=/path/to/vitasdk
export PATH="$VITASDK/bin:$PATH"
python3 tools/install_update_deps.py
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

`src/common/paths.h` is the source of truth for semantic release 1.2.3 and
Vita package revision 01.23. CMake derives both app/helper SFO fields and kernel
module metadata from it. Increment the NN.NN package revision for each release;
do not calculate it from semantic version digits. After 01.99 use 02.00.
Semantic versions such as 1.2.30 and 1.10.0 are supported. The updater compares
numeric semantic components, verifies the release digest and matches AM_VERSION
in app/helper metadata. APP_VER must match between both packages.
The settings schema and LiveArea XML format versions are independent.

## Maintenance changes in 1.2.3

- Installer exits after an accepted main-app launch; negative results retain retry.
- Diagnostic receipts no longer delay helper exit. Confirmed display logic is unchanged.
- Semantic versions support components above nine during installation, not just checking.
- Shared package validation removes duplicated version arithmetic and unused polling.
- Confirmation/cancellation colors are swapped; control actions remain unchanged.
- Removed superseded release narratives from this guide. Vendor source/licenses retained.

Host tests and native builds cannot establish firmware focus/launch behavior.
Confirm the return transition on hardware, including the previous-app prompt,
then test Save and Reboot after updating. Arbitrary/default-date TLS compatibility
remains unresolved; certificate verification has not been disabled.

## 1.2.3 regression correction

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

## 1.2.3 temporary installer cleanup

Before Save/storage preparation, and independently on Exit and Reboot (including
without-save choices), remove the separately installed RTPSVUPD1 helper using
its exact title ID. Save/copy stops if this cleanup fails; explicit Exit/Reboot
log failures and continue their requested action. No startup cleanup is added.
After verifying the destination mount and before staging app copies, remove only
its stale uma0:app/RTPSVUPD1 folder; failure blocks copying. The embedded installer
inside RTPSV0001 is retained for future updates. Other app folders are untouched.

Helper cleanup is separated from artwork refresh. Reboot retains file-only cache
preparation and always reaches the reset request after cleanup returns, regardless
of cleanup status. Full LiveArea refresh remains on Exit. Console validation is
required for reboot/focus after promoter deletion; this change does not claim to
fix Shell image caching or the close-installer launch prompt.
