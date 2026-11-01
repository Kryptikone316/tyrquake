#!/bin/sh
# Builds tyrquake_libretro.so for the SF3500 (MIPS32r2 74Kc, hard float, no GPU).
#
#   MIPS_PREFIX=/path/to/mips-mti-linux-gnu- [SYSROOT=/path/to/sysroot] ./build_sf3500.sh
#
# The QuakeC interpreter files (common/pr_*.c) are built at -O1 on purpose: at -O2 the device
# sometimes returns a value with one high bit cleared when QuakeC reads an entity field
# (string references lose bit 29, entity references bit 8).  It was never reproduced in QEMU or
# on x86, and no instruction-set, scheduling or aliasing flag changed it; see README-SF3500.md.
set -eu

: "${MIPS_PREFIX:?set MIPS_PREFIX to the toolchain prefix, including the trailing dash}"
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC_REAL="${MIPS_PREFIX}gcc -mips32r2 -mtune=74kc -mhard-float"
[ -n "${SYSROOT:-}" ] && CC_REAL="$CC_REAL --sysroot=$SYSROOT"
STRIP=${STRIP:-${MIPS_PREFIX}strip}

WRAP=$(mktemp)
trap 'rm -f "$WRAP"' EXIT HUP INT TERM
cat > "$WRAP" <<WRAPPER
#!/bin/sh
case "\$*" in
  *common/pr_*) exec $CC_REAL "\$@" -O1 ;;
  *)            exec $CC_REAL "\$@" ;;
esac
WRAPPER
chmod +x "$WRAP"

cd "$HERE"
find . -name '*.o' -delete
make -j"$(nproc 2>/dev/null || echo 4)" platform=unix CC="$WRAP"
"$STRIP" -o tyrquake_libretro.stripped.so tyrquake_libretro.so
echo "built $HERE/tyrquake_libretro.stripped.so"
