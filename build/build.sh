#!/bin/bash
# Build the meshcore-m2 OpenWrt image. Runs inside the build container:
#   /src  = this repo (read-only bind mount)
#   /work = case-sensitive build volume
# Usage: build.sh [prepare|toolchain|image|all]   (default: all)
set -euo pipefail
OPENWRT_TAG=${OPENWRT_TAG:-v24.10.7}
JOBS=${JOBS:-$(nproc)}
SRC=/src
SITE=${SITE:-/site}
OW=/work/openwrt
step=${1:-all}

prepare() {
	if [ ! -d "$OW/.git" ]; then
		git clone --depth 1 --branch "$OPENWRT_TAG" https://github.com/openwrt/openwrt.git "$OW"
	fi
	cd /work
	# M2 device support (from ChirpStack Gateway OS conf/seeed_sensecap_m2; paths are 'openwrt/...')
	if [ ! -f "$OW/.m2-patched" ]; then
		while read -r p; do
			[ -n "$p" ] && patch -p0 -N < "$SRC/target/sensecap-m2/patches/$p"
		done < "$SRC/target/sensecap-m2/patches/series"
		touch "$OW/.m2-patched"
	fi
	cd "$OW"
	cp feeds.conf.default feeds.conf
	grep -q '^src-link meshcore ' feeds.conf || echo "src-link meshcore $SRC/feed" >> feeds.conf
	./scripts/feeds update -a
	./scripts/feeds install -a
	# image overlay files
	rm -rf files && mkdir -p files/etc/hotplug.d/net
	cp "$SRC/target/sensecap-m2/files/10-set-wifi-mac" files/etc/hotplug.d/net/
	# optional private site config (never commit these): /site/meshcore, /site/keys/identity.key
	if [ -f "$SITE/meshcore" ]; then
		mkdir -p files/etc/config && cp "$SITE/meshcore" files/etc/config/meshcore
		echo "including site config"
	fi
	if [ -f "$SITE/keys/companion.key.hex" ]; then
		mkdir -p files/etc/openhop_repeater && cp "$SITE/keys/companion.key.hex" files/etc/openhop_repeater/companion.key
		chmod 600 files/etc/openhop_repeater/companion.key
		echo "including site companion key"
	fi
	if [ -f "$SITE/keys/identity.key" ]; then
		mkdir -p files/etc/openhop_repeater && cp "$SITE/keys/identity.key" files/etc/openhop_repeater/
		chmod 600 files/etc/openhop_repeater/identity.key
		echo "including site identity key"
	fi
	cp "$SRC/target/sensecap-m2/config.seed" .config
	make defconfig
}

dl_cache() {
	# optional persistent download cache (CI): DL_CACHE=/path
	if [ -n "${DL_CACHE:-}" ] && [ -d "$DL_CACHE" ] && [ ! -L "$OW/dl" ]; then
		rm -rf "$OW/dl"; ln -s "$DL_CACHE" "$OW/dl"
	fi
}
toolchain() { cd "$OW"; dl_cache; make -j"$JOBS" download; make -j"$JOBS" toolchain/install; }
image()     { cd "$OW"; make -j"$JOBS" || make -j1 V=s; ls -l bin/targets/ramips/mt76x8/; }

case "$step" in
	prepare) prepare ;;
	toolchain) toolchain ;;
	image) image ;;
	all) prepare; toolchain; image ;;
esac
