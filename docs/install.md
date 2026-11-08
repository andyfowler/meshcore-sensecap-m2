# Installing

Installing takes two flashes: first Seeed's open-source firmware, then this image.
The first step is needed because Seeed's stock firmware uses a smaller flash layout and
only accepts Seeed's own images.

> **There's no going back to the factory firmware.** Seeed's own docs say so: once the
> open-source firmware is installed, the stock SenseCAP firmware can't be reinstalled
> through normal means. If you want a way back anyway, make a full flash backup first
> (see [Optional: back up the factory flash](#optional-back-up-the-factory-flash)).

## 1. Find the M2 and log in

1. Plug the M2 into your network with Ethernet and power it on.
2. Find its IP address in your router's list of devices (it shows up as `SenseCAP`).
3. Open `https://<that address>` in a browser and log in with the username and password
   printed on the label under the device.

## 2. Install Seeed's open-source firmware

1. Download the open-source image for your region from
   [Seeed's releases](https://github.com/Seeed-Solution/LoRa_Gateway_OpenWRT/releases)
   (for example `…-sysupgrade-US915-4G.bin` — the 4G build works on units without a modem).
2. In the M2 web interface go to **System → Backup / Flash Firmware → Local upgrade**,
   choose the file and flash it. If you're offered **Keep settings**, untick it.
3. Wait about 3 minutes. The M2 comes back on DHCP; the web interface (LuCI) is at
   `http://<address>`, user `root`, **no password**.

If you're already running [ChirpStack Gateway OS](https://www.chirpstack.io/docs/chirpstack-gateway-os/install/seeed.html)
on your M2, skip this step.

## 3. Install this image

1. Download `meshcore-sensecap-m2-…-sensecap_wm7628n-squashfs-sysupgrade.bin` from the
   [latest release](../../releases/latest).
2. In LuCI go to **System → Backup / Flash Firmware → Flash new firmware image**, choose the
   file, **untick "Keep settings"**, and flash.
3. Wait 4–5 minutes. The first boot prepares the flash and is slower than later ones.

## 4. Find it again

The M2 now calls itself `meshcore-m2-xxxxxx` (the last six characters of its MAC address).
Try `http://meshcore-m2-xxxxxx.local:8000`, or look it up in your router's device list.

Continue with **[first-time setup](setup.md)**.

## Optional: back up the factory flash

Before step 2, the stock firmware's web interface can read the raw flash partitions. A full
32 MB dump lets you restore the factory firmware later with a SPI flash programmer (or the
bootloader's TFTP recovery for the firmware partition). `docs/troubleshooting.md` has the
details; keep the dump private, as it contains the device's keys and passwords.
