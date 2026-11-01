cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Vita toolchain")
  else()
    message(FATAL_ERROR "Set VITASDK to your VitaSDK installation")
  endif()
endif()
project(psvsd_am VERSION 1.1.8 LANGUAGES C)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(CMAKE_C_STANDARD 99)

# vita.cmake uses macros that otherwise retain export flags between targets.
function(clean_vita_self target source)
  set(VITA_ELF_CREATE_FLAGS "")
  # Match the original USBMC installer's private-API auth ID.
  set(VITA_MAKE_FSELF_FLAGS "-a 0x2800000000000001")
  vita_create_self(${target} ${source} ${ARGN})
endfunction()

# Public SDK stubs from taiHEN 0.11; provenance is in docs/SOURCE_AND_BUILD.md.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/taihen/include")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/taihen/lib")

foreach(mode IN ITEMS psvsd_am usbmc_accessory_probe psvsd_am_status)
  add_executable(${mode} plugin/main.c common/settings.c)
  target_compile_options(${mode} PRIVATE -Wall -Wextra -Werror -O2 -fno-builtin)
  target_link_options(${mode} PRIVATE -Wl,-q -nostdlib)
  target_link_libraries(${mode} SceIofilemgrForDriver_stub SceSysclibForDriver_stub
    SceModulemgrForDriver_stub SceThreadmgrForDriver_stub SceKernelSuspendForDriver_stub
    taihenForKernel_stub taihenModuleUtils_stub)
  if(mode STREQUAL "usbmc_accessory_probe")
    target_compile_definitions(${mode} PRIVATE USBMC_ACCESSORY_PROBE=1)
    set(exports "${CMAKE_CURRENT_SOURCE_DIR}/plugin/exports_probe.yml")
  elseif(mode STREQUAL "psvsd_am_status")
    target_compile_definitions(${mode} PRIVATE USBMC_STATUS_ONLY=1)
    set(exports "${CMAKE_CURRENT_SOURCE_DIR}/plugin/exports_status.yml")
  else()
    target_compile_definitions(${mode} PRIVATE USBMC_MANAGED_BOOT=1)
    set(exports "${CMAKE_CURRENT_SOURCE_DIR}/plugin/exports.yml")
  endif()
  clean_vita_self(${mode}.skprx ${mode} CONFIG ${exports} UNSAFE)
endforeach()

add_executable(psvsd_am_app app/main.c app/session.c app/manager.c app/prepare.c app/ui.c app/controller.c app/touch.c app/updates.c app/update_network.c app/update_install.c app/launch.c app/relaunch.c app/migration.c app/artwork.c app/package.c app/package_metadata.c vendor/minizip/unzip.c vendor/minizip/ioapi.c common/settings.c)
target_compile_definitions(psvsd_am_app PRIVATE IOAPI_NO_64)
target_compile_options(psvsd_am_app PRIVATE -Wall -Wextra -Werror -O2 -fno-builtin-printf)
# Reserve an extra data-segment page for vita-elf-create's appended metadata.
execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-file-name=ldscripts/armvita.x
  OUTPUT_VARIABLE vita_link_script OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE vita_link_result)
if(NOT vita_link_result EQUAL 0)
  message(FATAL_ERROR "Could not locate the Vita linker script")
endif()
file(READ "${vita_link_script}" vita_link_contents)
set(vita_data_align ". = DATA_SEGMENT_ALIGN (0x10000, 0x10000);")
string(FIND "${vita_link_contents}" "${vita_data_align}" vita_align_index)
if(vita_align_index LESS 0)
  message(FATAL_ERROR "Vita linker script lacks the expected data-segment alignment")
endif()
string(REPLACE "${vita_data_align}" "${vita_data_align}\n  . += 0x10000;" vita_link_contents "${vita_link_contents}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/app-layout.ld" "${vita_link_contents}")
target_link_options(psvsd_am_app PRIVATE "-Wl,-T,${CMAKE_CURRENT_BINARY_DIR}/app-layout.ld")
target_link_libraries(psvsd_am_app SceDisplay_stub SceCtrl_stub SceProcessmgr_stub
  SceIofilemgr_stub SceLibKernel_stub SceVshBridge_stub ScePower_stub SceTouch_stub SceLiveArea_stub SceNet_stub SceNetCtl_stub SceAppMgr_stub SceRtc_stub ScePromoterUtil_stub SceSysmodule_stub taihen_stub
  -Wl,--start-group curl mbedtls mbedx509 mbedcrypto everest p256m z zstd pthread -Wl,--end-group)
clean_vita_self(psvsd_am_app.self psvsd_am_app UNSAFE)
add_executable(psvsd_am_installer app/installer_main.c app/launch.c app/relaunch.c app/migration.c app/package.c app/package_metadata.c app/ui.c app/updates.c common/settings.c)
target_compile_options(psvsd_am_installer PRIVATE -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections)
target_link_options(psvsd_am_installer PRIVATE -Wl,--gc-sections)
target_link_libraries(psvsd_am_installer SceAppMgr_stub SceRtc_stub ScePromoterUtil_stub SceSysmodule_stub SceDisplay_stub SceCtrl_stub SceLibKernel_stub SceIofilemgr_stub SceProcessmgr_stub)
clean_vita_self(psvsd_am_installer.self psvsd_am_installer UNSAFE)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/installer.sfo"
  COMMAND "${VITASDK}/bin/vita-mksfoex" -s TITLE_ID=RTPSVUPD1 -s APP_VER=01.18
    -s CONTENT_ID=HB0001-RTPSVUPD1_00-0000000000000000 "PSVSD/AM Installer" "${CMAKE_CURRENT_BINARY_DIR}/installer.sfo")
add_custom_target(installer_sfo DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/installer.sfo")
vita_create_vpk(PSVSD_AM.vpk RTPSV0001 psvsd_am_app.self
  VERSION 01.18 NAME "PSVSD/AM"
  FILE "${CMAKE_CURRENT_BINARY_DIR}/psvsd_am_installer.self" installer/eboot.bin
  FILE "${CMAKE_CURRENT_BINARY_DIR}/installer.sfo" installer/param.sfo
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/updater/head.bin" installer/head.bin
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/main-head.bin" main-head.bin
  FILE "${CMAKE_CURRENT_BINARY_DIR}/usbmc_accessory_probe.skprx" usbmc_accessory_probe.skprx
  FILE "${CMAKE_CURRENT_BINARY_DIR}/psvsd_am.skprx" psvsd_am.skprx
  FILE "${CMAKE_CURRENT_BINARY_DIR}/psvsd_am_status.skprx" psvsd_am_status.skprx
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/update-ca-bundle.pem" update-ca-bundle.pem
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../docs/licenses/curl.txt" licenses/curl.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../docs/licenses/mbedtls.txt" licenses/mbedtls.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../docs/licenses/zlib.txt" licenses/zlib.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../docs/licenses/zstd.txt" licenses/zstd.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../docs/licenses/ca-bundle-MPL-2.0.txt" licenses/ca-bundle-MPL-2.0.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon0.png" sce_sys/icon0.png
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/bg.png" sce_sys/livearea/contents/bg.png
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/startup.png" sce_sys/livearea/contents/startup.png
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../COPYING" LICENSE.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/fonts/LICENSE.txt" FONT_LICENSE.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../docs/THIRD_PARTY_NOTICES.md" CREDITS.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/template.xml" sce_sys/livearea/contents/template.xml)
if(TARGET PSVSD_AM.vpk-vpk)
  add_dependencies(PSVSD_AM.vpk-vpk usbmc_accessory_probe.skprx-self psvsd_am.skprx-self psvsd_am_status.skprx-self)
else()
  add_dependencies(PSVSD_AM.vpk usbmc_accessory_probe.skprx psvsd_am.skprx psvsd_am_status.skprx)
endif()

add_dependencies(PSVSD_AM.vpk-vpk psvsd_am_installer.self-self installer_sfo)
