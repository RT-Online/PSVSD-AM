#!/bin/sh
set -eu
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
test_out=$(mktemp -d)
trap 'rm -rf "$test_out"' EXIT HUP INT TERM
for mode in 0 1; do
  cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
    -DUSBMC_HOST_TEST=1 -DUSBMC_ACCESSORY_PROBE="$mode" -DUSBMC_MANAGED_BOOT="$((1-mode))" \
    -I"$test_dir" "$test_dir/test_plugin.c" "$test_dir/../src/common/settings.c" -o "$test_out/test_$mode"
  # No heap allocation is used. LeakSanitizer needs /proc access unavailable
  # in some containers; AddressSanitizer and UndefinedBehaviorSanitizer remain on.
  ASAN_OPTIONS=detect_leaks=0 "$test_out/test_$mode"
done
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -DACCESSORY_APP_TEST=1 -I"$test_dir" "$test_dir/test_app.c" -o "$test_out/test_app"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_app"
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -DPSVSD_AM_HOST_TEST=1 -I"$test_dir" "$test_dir/test_manager.c" \
  "$test_dir/../src/common/settings.c" -o "$test_out/test_manager"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_manager"
for suite in managed status; do
  cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
    -DUSBMC_HOST_TEST=1 -DUSBMC_ACCESSORY_PROBE=0 -I"$test_dir" \
    "$test_dir/test_$suite.c" "$test_dir/../src/common/settings.c" -o "$test_out/test_$suite"
  ASAN_OPTIONS=detect_leaks=0 "$test_out/test_$suite"
done
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -I"$test_dir" "$test_dir/test_ui.c" "$test_dir/../src/app/ui.c" \
  "$test_dir/../src/app/controller.c" "$test_dir/../src/app/updates.c" "$test_dir/../src/common/settings.c" -o "$test_out/test_ui"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_ui" ${PSVSD_AM_PREVIEW_DIR:+"$PSVSD_AM_PREVIEW_DIR"}
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -I"$test_dir" "$test_dir/test_touch.c" "$test_dir/../src/app/touch.c" \
  "$test_dir/../src/app/ui.c" "$test_dir/../src/app/controller.c" "$test_dir/../src/app/updates.c" \
  "$test_dir/../src/common/settings.c" -o "$test_out/test_touch"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_touch"
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -I"$test_dir" "$test_dir/test_updates.c" "$test_dir/../src/app/updates.c" -o "$test_out/test_updates"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_updates" ${PSVSD_AM_RELEASE_FIXTURE:+"$PSVSD_AM_RELEASE_FIXTURE"}
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -DPSVSD_AM_HOST_TEST=1 -D_POSIX_C_SOURCE=200809L -DIOAPI_NO_64 -I"$test_dir" \
  "$test_dir/test_update_install.c" "$test_dir/../src/app/update_install.c" \
  "$test_dir/../src/app/package_metadata.c" "$test_dir/../src/vendor/minizip/unzip.c" \
  "$test_dir/../src/vendor/minizip/ioapi.c" -lz -o "$test_out/test_update_install"
python3 "$test_dir/make_update_fixtures.py" "$test_out/install"
for fixture in good traversal duplicate wrong_title wrong_version oversize truncated crc symlink missing_helper wrong_helper_title wrong_helper_version multi_digit wrong_semantic wrong_helper_semantic; do
  ASAN_OPTIONS=detect_leaks=0 "$test_out/test_update_install" "$test_out/install" "$fixture"
done
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -DPSVSD_AM_HOST_TEST=1 -I"$test_dir" "$test_dir/test_artwork.c" \
  "$test_dir/../src/app/artwork.c" -o "$test_out/test_artwork"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_artwork"

cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -DPSVSD_AM_HOST_TEST=1 -I"$test_dir" "$test_dir/test_relaunch.c" \
  "$test_dir/../src/app/relaunch.c" "$test_dir/../src/app/launch.c" -o "$test_out/test_relaunch"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_relaunch"
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -DPSVSD_AM_HOST_TEST=1 -I"$test_dir" "$test_dir/test_installer.c" -o "$test_out/test_installer"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_installer"
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -DPSVSD_AM_HOST_TEST=1 -I"$test_dir" "$test_dir/test_migration.c" \
  "$test_dir/../src/app/migration.c" -o "$test_out/test_migration"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_migration"
