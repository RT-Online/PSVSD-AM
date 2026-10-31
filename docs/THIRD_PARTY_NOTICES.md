# PSVSD/AM — credits and third-party notices

PSVSD/AM is derived from Yifan Lu's USBMC and TheFloW's VitaShell storage implementation. Retained storage patches are Copyright (C) 2015-2017 TheFloW, GPL version 3 or later. The full license is provided as COPYING in the source package and LICENSE.txt in the VPK. Upstream USBMC: https://github.com/yifanlu/usbmc .

RefurbedTech: concept, hardware, console testing and product direction. Implementation assistance: OpenAI Codex. The new app/menu/settings/test code and graphics are supplied with this source package under GPL-3.0-or-later.

The current graphical app uses generated bitmap atlases from bundled DejaVu Sans. Copyright (c) 2003 Bitstream, Inc.; DejaVu changes are public domain. The complete font notices and permission terms are in assets/fonts/LICENSE.txt and FONT_LICENSE.txt in the VPK. The original AM icon and launcher artwork are included as editable source/generator output.

The public taiHEN SDK header/import stubs are from Yifan Lu's official 0.11 developer archive, Copyright (c) 2016 Yifan Lu, MIT licensed. The license is src/vendor/taihen/LICENSE; the taiHEN runtime is not distributed. Upstream: https://github.com/yifanlu/taiHEN .

VitaSDK provides compiler, headers, tools and import libraries. Primary LiveArea guidance comes from the CC0 VitaSDK samples. No Sony firmware, boot image or mass-storage driver is included. Upstream: https://github.com/vitasdk .

The late resume event selection is informed by SKGleba/yamt-vita and TeamFAPS/PSVita-StorageMgr. This fork's callback/worker and tests are newly written. The new app also follows VitaShell's documented eight-byte _vshKernelSearchModuleByName work-buffer usage to detect an existing module, without assuming the returned UID matches a private kernel UID.


The update checker statically links libcurl with Mbed TLS rather than the Vita
system HTTP/SSL implementation. No iTLS-Enso installation or system TLS patch is
required by this backend. libcurl uses the curl license; Mbed TLS is used under
Apache-2.0; zlib uses its zlib license; Zstandard uses its BSD license option.
The bundled Mozilla CA certificate data is distributed under MPL-2.0.
Complete notices are in docs/licenses and the VPK licenses directory.

Known build inputs: VitaSDK packages curl-mbedtls (package header reports
libcurl 8.21.0), Mbed TLS 3.6.5, zlib and Zstandard. Pinned archive hashes and
verified installation are in tools/install_update_deps.py. These are build-time
SDK dependencies; the user installs only the resulting VPK. Upstream sources:
https://github.com/curl/curl/tree/curl-8_21_0
https://github.com/Mbed-TLS/mbedtls/tree/mbedtls-3.6.5
https://github.com/madler/zlib
https://github.com/facebook/zstd
Vita port/build recipes and patches: https://github.com/vitasdk/packages
CA extraction provenance: https://curl.se/docs/caextract.html
