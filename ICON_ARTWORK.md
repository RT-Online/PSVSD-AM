# Launcher artwork

The flat teal microSD icon uses navy `#102137` and teal `#46dcc7`. It shows
PSVSD/AM, the current semantic version and Accessory Storage Mount.

`tools/make_assets.py` generates the editable SVG, indexed 128 px icon and
matching LiveArea images. The SVG and raster drawing share the same geometry.
The version comes from `src/common/paths.h`.

The stylized rear/contact-side card profile uses 11:15 proportions, a stepped
left edge and eight contacts. Its mechanical reference is Kingston's microSDHC
technical specification, page 24:
https://www.kingston.com/datasheets/SDCIT-specsheet-8gb-32gb_en.pdf

Version 1.0.2 uses Vita package APP_VER 01.02. The `format-ver="01.00"` in
LiveArea XML describes the XML format and is intentionally unchanged.
