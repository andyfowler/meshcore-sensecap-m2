#!/usr/bin/env bash
# Build m2modem_rak2287 on the Pi, as an ordinary user, in ~/m2modem. No sudo, nothing installed.
#
# HAL source: the daemon links Semtech's sx1302_hal 2.1.0 libloragw and libtools statically
# (no libloragw.so, nothing installed system wide). It calls only lgw_connect, lgw_disconnect,
# lgw_reg_r, lgw_reg_w, sx1250_setup, and sx1302_radio_*, all unmodified in the upstream
# release, so the build needs no patches to the HAL.
#   - If /opt/sx1302_hal holds version 2.1.0 (an earlier HAL install), its libloragw and
#     libtools are copied from there, read only.
#   - Otherwise the pristine release tag V2.1.0 is downloaded from github.com/Lora-net/sx1302_hal,
#     checked against the pinned archive SHA-256 and the pinned commit ID, and used.
#   HAL_SRC=fetch forces the download; HAL_SRC=<directory> uses that tree.
# The HAL license (Semtech revised BSD, plus MIT for the parson library in libtools) is copied to
# ~/m2modem/hal/LICENSE.TXT.
#
# Optional environment: OUT=<binary name> (default m2modem_rak2287) and EXTRA_CFLAGS=<flags>
# (for example -DM2_TEST_DRIVE for the power-calibration test binary, which is never installed).
# Needs: gcc, make, curl, tar, python3 (all in the Raspberry Pi OS image except possibly make).
#
# Usage (from the PC):
#   scp m2modem_rak2287.c build.sh <user>@<pi>:~/m2modem/
#   ssh <user>@<pi> 'bash ~/m2modem/build.sh'
set -euo pipefail

W="$HOME/m2modem"
OUT="${OUT:-m2modem_rak2287}"
HAL_URL=https://github.com/Lora-net/sx1302_hal/archive/refs/tags/V2.1.0.tar.gz
HAL_SHA256=4b62aa6a83ad449c68fdd844fc35024586c9faabca3c5a90a2544735b380de5b
HAL_COMMIT=4b42025d1751e04632c0b04160e0d29dbbb222a5   # commit that tag V2.1.0 points to
mkdir -p "$W"
cd "$W"

for t in gcc make; do command -v "$t" >/dev/null || { echo "ERROR: $t not installed" >&2; exit 1; }; done

HAL_SRC="${HAL_SRC:-}"
if [ -z "$HAL_SRC" ]; then
    if [ "$(cat /opt/sx1302_hal/VERSION 2>/dev/null)" = "2.1.0" ]; then HAL_SRC=/opt/sx1302_hal; else HAL_SRC=fetch; fi
fi

if [ "$HAL_SRC" = fetch ]; then
    for t in curl tar python3; do command -v "$t" >/dev/null || { echo "ERROR: $t not installed" >&2; exit 1; }; done
    mkdir -p hal-cache
    TGZ=hal-cache/sx1302_hal-V2.1.0.tar.gz
    if ! { [ -f "$TGZ" ] && echo "$HAL_SHA256  $TGZ" | sha256sum -c --quiet - 2>/dev/null; }; then
        echo "downloading $HAL_URL"
        curl -fsSL --retry 3 --connect-timeout 20 --max-time 180 -o "$TGZ.part" "$HAL_URL" || { echo "ERROR: HAL download failed" >&2; rm -f "$TGZ.part"; exit 1; }
        mv "$TGZ.part" "$TGZ"
    fi
    echo "$HAL_SHA256  $TGZ" | sha256sum -c --quiet - || { echo "ERROR: HAL archive checksum differs from the pinned value; not using it" >&2; exit 1; }
    got="$(python3 -I -c 'import sys,tarfile; print(tarfile.open(sys.argv[1]).pax_headers.get("comment",""))' "$TGZ")"
    [ "$got" = "$HAL_COMMIT" ] || { echo "ERROR: HAL archive commit '$got' is not the pinned $HAL_COMMIT" >&2; exit 1; }
    rm -rf hal-src
    mkdir hal-src
    tar -xzf "$TGZ" -C hal-src --strip-components=1
    HAL_SRC="$W/hal-src"
    echo "HAL: Lora-net/sx1302_hal tag V2.1.0, commit $HAL_COMMIT, archive sha256 $HAL_SHA256"
fi

[ -f "$HAL_SRC/VERSION" ] || { echo "ERROR: $HAL_SRC/VERSION not found" >&2; exit 1; }
echo "HAL version: $(cat "$HAL_SRC/VERSION") (from $HAL_SRC)"

rm -rf hal
mkdir -p hal
cp -r "$HAL_SRC/libloragw" "$HAL_SRC/libtools" hal/
cp "$HAL_SRC/VERSION" "$HAL_SRC/target.cfg" hal/
[ -f "$HAL_SRC/LICENSE.TXT" ] && cp "$HAL_SRC/LICENSE.TXT" hal/LICENSE.TXT
rm -f hal/libloragw/libloragw.so hal/libloragw/*.a
# libloragw/inc/config.h is generated from library.cfg, target.cfg, and VERSION by the HAL's own Makefile.
if [ ! -f hal/libloragw/inc/config.h ]; then
    make -s -C hal/libloragw inc/config.h >/dev/null || { echo "ERROR: could not generate config.h" >&2; exit 1; }
fi
[ -f hal/libloragw/inc/config.h ] || { echo "ERROR: hal/libloragw/inc/config.h missing" >&2; exit 1; }

# shellcheck disable=SC2086
gcc -std=gnu99 -O2 -Wall -Wextra ${EXTRA_CFLAGS:-} \
    -Ihal/libloragw/inc -Ihal/libloragw -Ihal/libtools/inc \
    m2modem_rak2287.c hal/libloragw/src/*.c hal/libtools/src/*.c \
    -lm -lrt -lpthread -o "$OUT" 2> build.log || { cat build.log; echo "BUILD FAILED" >&2; exit 1; }

echo "warnings in m2modem_rak2287.c:"
grep "m2modem_rak2287.c" build.log || echo "  none"
echo "other warnings (HAL sources): $(grep -c 'warning:' build.log || true) lines"
ls -l "$OUT"
sha256sum "$OUT"
