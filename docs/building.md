# Building the image

The image is built with the standard OpenWrt build system inside Docker. You need Docker,
about 20 GB of free disk space and an hour or two for the first build (the OpenWrt
toolchain is compiled from source; later builds are much faster).

```sh
docker build -t meshcore-sensecap-m2-build build/
docker volume create meshcore-sensecap-m2-build      # OpenWrt needs a case-sensitive filesystem

docker run --rm \
  -v meshcore-sensecap-m2-build:/work \
  -v "$PWD":/src:ro \
  meshcore-sensecap-m2-build /src/build/build.sh all
```

`build.sh` steps can also be run one at a time: `prepare` (clone OpenWrt, apply the M2
device patches, set up the package feeds), `toolchain`, `image`.

Copy the result out of the volume:

```sh
docker run --rm -v meshcore-sensecap-m2-build:/work meshcore-sensecap-m2-build \
  sh -c 'cat /work/openwrt/bin/targets/ramips/mt76x8/*sysupgrade.bin' > meshcore-sensecap-m2-sysupgrade.bin
```

GitHub Actions runs the same build (`.github/workflows/build.yml`) and attaches the image to
tagged releases.

## Building an image with your settings built in

If you manage several M2s, or just want a device that comes up fully configured, put a site
configuration in a folder **outside this repository** and mount it at `/site`:

```
my-site/
  meshcore              # copy of feed/meshcore-m2-base/files/meshcore.config, filled in
  keys/identity.key     # optional: existing repeater key (base64, openHop format)
  keys/companion.key.hex   # optional: existing companion key (hex)
```

```sh
docker run --rm \
  -v meshcore-sensecap-m2-build:/work \
  -v "$PWD":/src:ro \
  -v /path/to/my-site:/site:ro \
  meshcore-sensecap-m2-build sh -c '/src/build/build.sh prepare && /src/build/build.sh image'
```

The file documents every option: name, location, owner info, passwords, radio preset,
repeater behaviour, regions, Wi-Fi, observer uploads and the companion. With an admin
password set, the setup wizard is skipped. It's applied once on first boot; after that the
dashboard and the MeshCore app own the settings.

**Never commit a site folder** – it can contain passwords and private keys. An image built
with one contains them too, so don't share that image.

## Layout

```
build/                    Dockerfile and build.sh
target/sensecap-m2/       OpenWrt image config and M2 device patches
feed/                     OpenWrt package feed
  m2modem/                the M2 radio driver (C)
  openhop-repeater/       openHop packaged for OpenWrt (+ a small setup-wizard patch)
  meshcore-m2-base/       first-boot setup, site configuration, helper scripts
docs/
```

## Updating openHop

`feed/openhop-repeater/Makefile` pins openHop Repeater and openHop Core to specific commits
(with SHA-256 hashes), along with the few pure-Python libraries OpenWrt doesn't package.
To move to a newer openHop, update the commit hashes and `PKG_HASH` values, rebuild, test on
a device, and tag a release.
