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
  "$test_dir/../src/app/controller.c" "$test_dir/../src/common/settings.c" -o "$test_out/test_ui"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_ui" ${PSVSD_AM_PREVIEW_DIR:+"$PSVSD_AM_PREVIEW_DIR"}
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -I"$test_dir" "$test_dir/test_touch.c" "$test_dir/../src/app/touch.c" \
  "$test_dir/../src/app/ui.c" "$test_dir/../src/app/controller.c" \
  "$test_dir/../src/common/settings.c" -o "$test_out/test_touch"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_touch"
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -I"$test_dir" "$test_dir/test_updates.c" "$test_dir/../src/app/updates.c" -o "$test_out/test_updates"
ASAN_OPTIONS=detect_leaks=0 "$test_out/test_updates" ${PSVSD_AM_RELEASE_FIXTURE:+"$PSVSD_AM_RELEASE_FIXTURE"}
