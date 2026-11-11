# PSVSD/AM updater test

## 1.2.4 to 1.2.5

1. Start in PSVSD/AM **1.2.4** with Wi-Fi connected and an accurate Vita date and time.
2. With **1.2.5** available as the latest stable GitHub release, open **Updates**, choose **Update to 1.2.5**, and confirm **Install Update** once.
3. Watch both transitions. Record any close prompt or stop at LiveArea and whether it occurs before the installer opens or after it reports completion.
4. Check that the reopened app reports **1.2.5** and shows the saved mount. This upgrade uses the 1.2.4 app's outgoing launch request and the 1.2.5 helper's new return sequence.
5. Open **Log** and press Square to export `ux0:data/PSVSD_AM/test.log`. If the app stops before reopening, capture `ux0:data/PSVSD_AM/update/handoff.log` with VitaShell.

The helper return passes when PSVSD/AM reopens after the one install confirmation. A subsequent upgrade starting in 1.2.5 exercises both URI handoffs.

## Clock notice

1. Install 1.2.5 and connect to Wi-Fi. Set the Vita to an old date, such as January 1, 2012.
2. Launch PSVSD/AM. Verify that the **Check the Vita's date and time** notice appears. Dismiss it with **OK**, Cross or Circle. Open **Updates** and manually check to verify the notice appears again.
3. Go to **Settings → Date & Time → Date & Time Settings**. Choose **Set via Internet → Set Now** or **Set Manually**. Return to the app and select **Check for Updates**. You should be able to retry immediately.
4. With a newer release available, check for it with the correct clock. Change the date to an old date and return to the app. Confirm **Install Update**. The download should stop and show the same notice. Correct the date and try the update again.
5. Test with Wi-Fi off. The launch check should skip silently and **Updates** should show **Updates not checked**.

## Host verification

Run `sh tests/run_tests.sh` for regression tests, including button and touch dismissal of the clock notice. Use the console tests above to check the on-device notice and installer handoffs.
