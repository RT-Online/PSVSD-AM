# PSVSD/AM Vita tests

## Local smoke test

Install `PSVSD_AM_1.3.1.vpk` with VitaShell. Open PSVSD/AM and verify the
version and existing mount settings. Keep the existing mount and choose
**Reboot → Save and Reboot**. Confirm that the Vita reboots from that action.

## 1.3.0 to 1.3.1 and immediate reboot

1. Start in PSVSD/AM **1.3.0** with **Auto-update Off**, Wi-Fi connected and an accurate Vita date and time.
2. With **1.3.1** available as the latest stable GitHub release and its VPK attached, open **Updates**, choose **Update to 1.3.1**, and confirm **Install Update** once.
3. Watch the entire update. Check that the progress screen carries into the return and that the Vita stays in the app after **Opening PSVSD/AM**.
4. Check that the reopened app reports **1.3.1**. Keep the existing mount and immediately choose **Reboot → Save and Reboot**.
5. Verify that the Vita cold boots from that selection. Check the **1.3** launcher artwork and confirm the temporary installer bubble is gone.
6. Open PSVSD/AM and check the saved mount. Export `ux0:data/PSVSD_AM/test.log` from **Log** with Square. Check for **Removed temporary installer before first app frame** and the cold reset request. If the installer bubble remains or the app goes to Home during the update, capture the log and note when it happened.

For a storage change that requires copying, verify that installer cleanup runs
before copying. Repeat **Reboot without Save** and **Save and Exit** separately
to check their actions.

## Updates controls and auto-update

1. Open **Updates** in 1.3.0 and verify that **Auto-update** starts **Off** on a fresh install.
2. With 1.3.1 available on GitHub, wait for **Update Available** beside the version badge. Tap it from Overview and verify it opens **Updates**.
3. Swipe the release notes vertically. Use **Left / Right** to move a line at a time or hold to keep scrolling.
4. Choose **Update to 1.3.1**. Check the bright labels and focus outline. Cancel, then confirm that the release stays available.
5. Tap **Auto-update** or press Triangle to enable it. Exit and reopen 1.3.0. After the startup check, verify that the idle app opens the download progress screen and installs 1.3.1.
6. Open **Updates** in 1.3.1 and verify that **Auto-update** still shows **On**. Set it back to **Off** if you prefer manual installation.

## Clock notice

1. Install 1.3.1 and connect to Wi-Fi. Set the Vita to an old date, such as January 1, 2012.
2. Launch PSVSD/AM. Verify that the **Check the Vita's date and time** notice appears. Dismiss it with **OK**, Cross or Circle. Open **Updates** and manually check to verify the notice appears again.
3. Go to **Settings → Date & Time → Date & Time Settings**. Choose **Set via Internet → Set Now** or **Set Manually**. Return to the app and select **Check for Updates**. You should be able to retry immediately.
4. With a newer release available, check for it with the correct clock. Change the date to an old date and return to the app. Confirm **Install Update**. The download should stop and show the same notice. Correct the date and try the update again.
5. Test with Wi-Fi off. The launch check should skip silently and **Updates** should show **Updates not checked**.

## Speed Test

1. Open **Read / write test**, select **Verify**, and confirm that the 4 MiB integrity test passes.
2. Select **Speed Test** or press Triangle. Check the four progress phases and the displayed mount. Verify that sequential read/write show MiB/s and that 4 KiB random read/write show both MiB/s and IOPS.
3. Open **Log** to check the same results. The temporary `data/PSVSD_AM/speed-*.bin` file for the completed run should be removed from the tested mount.
4. Repeat on each verified mount you use. Compare tests on the same mount and treat cached read results as app-level throughput.

## Host verification

Run `sh tests/run_tests.sh` for host checks of update eligibility, rejected
package cleanup and retry, preference persistence, input, the benchmark and
installer cleanup. Use the Vita tests above to check touch scrolling,
auto-update, throughput and reboot after an update.
