# PSVSD/AM 1.2.4 update test

The bubble and LiveArea artwork show `1.2`. The app and installer report
`1.2.4`, with package revision `01.24`.

## Release details

- Repository `RT-Online/PSVSD-AM`
- Stable tag `1.2.4`
- Release title `PSVSD/AM 1.2.4`
- Asset `PSVSD_AM_1.2.4.vpk`
- SHA-256 `19770cd6c14192576cb8aa826230cd6cb0bbeb8aa86ed07dea6657bd3b201296`

The 1.2.3 updater reads the latest stable GitHub release. Publishing this
asset as that release makes the candidate available to existing installs.
Match the asset name and SHA-256 before starting the console test.

### Release text

> The launcher artwork now displays the 1.2 series. The app shows the full
> 1.2.4 version. This build is prepared for testing the update from 1.2.3.

## Vita test

1. Start on a Vita running PSVSD/AM **1.2.3** with Wi-Fi connected and an
   accurate date and time. This isolates the app handoff from the open
   certificate-date issue.
2. Open **Updates**, choose **Update to 1.2.4**, and confirm the install once.
3. Watch the transfer to the installer and the return to PSVSD/AM. Record
   any close prompt or stop at LiveArea and which transition produced it.
4. Confirm the reopened app reports **1.2.4** and shows the expected mount.
   Check the **1.2** bubble after the normal Exit or Reboot artwork refresh.
5. Open **Log** and press Square to export `ux0:data/PSVSD_AM/test.log`.
   If the app stops before reopening, save
   `ux0:data/PSVSD_AM/update/handoff.log` using VitaShell.

The handoff passes when the app returns automatically after the single
install confirmation and reports 1.2.4.

## Build verification

The native VitaSDK build and the sanitizer-enabled host suite passed. All
five Vita binaries passed the import audits. The VPK CRCs, app and installer
metadata, and packaged assets match the build. The 1.2.3 release parser and
staging code accepted the actual 1.2.4 VPK with a representative release
response and mocked package promotion. The console launch sequence still
requires the Vita test above.
