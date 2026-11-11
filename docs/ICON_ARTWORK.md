# Launcher artwork

The flat teal microSD icon uses navy `#102137` and teal `#46dcc7`. It shows
PSVSD/AM, the major and minor version, and Accessory Storage Mount. The bubble,
LiveArea background and startup art display `1.2` for all `1.2.x` releases.
The app and package metadata retain the exact release version.

`tools/make_assets.py` generates the editable SVG, indexed 128 px icon and
matching LiveArea images. The SVG and raster drawing share the same geometry.
The version comes from `src/common/paths.h`.

The stylized rear/contact-side card profile uses 11:15 proportions, a stepped
left edge and eight contacts. Its mechanical reference is Kingston's microSDHC
technical specification, page 24:
https://www.kingston.com/datasheets/SDCIT-specsheet-8gb-32gb_en.pdf

Vita package APP_VER comes from `src/common/paths.h`. LiveArea uses
`format-ver="01.00"` for its XML format.

The user confirmed a restart displays the new artwork. In 1.1.7, startup only
checks whether refresh is needed. Cache writes and LiveArea replacement run on
explicit Exit/Reboot to avoid changing Shell metadata during an active session.
Choose Reboot to clear the home screen's retained rendered images.

From 1.1.9, reboot prepares cached files without LiveArea module/API calls. Full
LiveArea refresh is reserved for Exit; the revised reboot reload needs Vita testing.
