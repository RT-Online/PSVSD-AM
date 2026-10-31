cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Vita toolchain")
  else()
    message(FATAL_ERROR "Set VITASDK to your VitaSDK installation")
  endif()
endif()
project(psvsd_am VERSION 1.0.1 LANGUAGES C)
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

add_executable(psvsd_am_app app/main.c app/session.c app/manager.c app/prepare.c app/ui.c app/controller.c app/touch.c common/settings.c)
target_compile_options(psvsd_am_app PRIVATE -Wall -Wextra -Werror -O2 -fno-builtin-printf)
target_link_libraries(psvsd_am_app SceDisplay_stub SceCtrl_stub SceProcessmgr_stub
  SceIofilemgr_stub SceLibKernel_stub SceVshBridge_stub ScePower_stub SceTouch_stub taihen_stub)
clean_vita_self(psvsd_am_app.self psvsd_am_app UNSAFE)
vita_create_vpk(PSVSD_AM.vpk RTPSV0001 psvsd_am_app.self
  VERSION 01.01 NAME "PSVSD/AM"
  FILE "${CMAKE_CURRENT_BINARY_DIR}/usbmc_accessory_probe.skprx" usbmc_accessory_probe.skprx
  FILE "${CMAKE_CURRENT_BINARY_DIR}/psvsd_am.skprx" psvsd_am.skprx
  FILE "${CMAKE_CURRENT_BINARY_DIR}/psvsd_am_status.skprx" psvsd_am_status.skprx
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
