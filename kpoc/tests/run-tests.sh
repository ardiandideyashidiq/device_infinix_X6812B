#!/bin/sh
# Builds and runs the kpoc host tests. The core is platform-free behind
# KpocOps, so it links against plain gcc with no bionic and no device.
set -e

here=$(dirname "$0")
root="$here/.."
out="${OUT:-/tmp}/kpoc_test"

gcc -std=c11 -D_GNU_SOURCE \
    -I"$root/include" -I"$here" \
    -Wall -Wextra -Werror \
    -o "$out" \
    "$here/test_kpoc.c" "$here/harness.c" \
    "$root/src/ctx.c" "$root/src/lights.c" "$root/src/util.c" \
    -lpthread

"$out"
