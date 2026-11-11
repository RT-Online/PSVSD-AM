# PSVSD/AM Vita tests

## 1.2.7 to 1.2.8 and immediate reboot

1. Start in PSVSD/AM **1.2.7** with Wi-Fi connected and an accurate Vita date and time.
2. With **1.2.8** available as the latest stable GitHub release, open **Updates**, choose **Update to 1.2.8**, and confirm **Install Update** once.
3. Watch both transitions. Record any close prompt or stop at LiveArea and whether it occurs before the installer opens or after it reports completion.
4. Check that the reopened app reports **1.2.8** and stays open. Keep the existing mount and immediately choose **Reboot → Save and Reboot**.
5. Verify that the Vita cold boots from that selection. Check the Home screen for the **PSVSD/AM Installer** bubble before opening PSVSD/AM again.
6. Open PSVSD/AM and check the saved mount. Export `ux0:data/PSVSD_AM/test.log` from **Log** with Square. Check for **Removed temporary installer on startup** and the cold reset request. If the installer bubble remains or the app goes to Home during the update, capture the log and note when it happened.

For a storage change that requires copying, verify that installer cleanup runs
before copying. Repeat **Reboot without Save** and **Save and Exit** separately
to check their actions.

## Clock notice

1. Install 1.2.8 and connect to Wi-Fi. Set the Vita to an old date, such as January 1, 2012.
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

Run `sh tests/run_tests.sh` for host checks of the benchmark, input and installer cleanup. Use the Vita tests above to check actual throughput and the system reboot after update.
