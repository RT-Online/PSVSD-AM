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

Commit the matching source before creating the GitHub release tag. Publish a
stable release with one VPK named `PSVSD_AM_<version>.vpk`, such as
`PSVSD_AM_1.2.10.vpk`. The app reads the version and SHA-256 from GitHub's
release API and downloads the VPK over HTTPS. The VPK includes all three
kernel payloads.


## Version metadata

`src/common/paths.h` sets the semantic version and Vita package revision.
CMake derives the app, helper and kernel module metadata from it. Increment
both versions at release time. The Vita revision increments from 01.99 to 02.00.
Semantic versions such as 1.2.30 and 1.10.0 are supported. The updater compares
numeric semantic components, verifies the release digest and matches AM_VERSION
in app/helper metadata. APP_VER must match between both packages.
The settings schema and LiveArea XML format versions are independent.


## Installer cleanup

On return from the installer, startup calls `sceAppMgrDestroyOtherApp` before
removing the installer bubble and drawing the first app frame. This cleanup
requires a recent installer return and runs once when there is no staged
installation waiting. Exit and Save and Exit can remove a leftover installer
by title. A change that copies apps between mounts removes the helper before
copying. The embedded installer is used for future updates. See
[the Vita test guide](UPDATE_TEST.md).
