# Updating

New versions are published as firmware images on the
[releases page](../../releases). Each release bundles a tested openHop version together with
OpenWrt and the M2 radio driver, so everything is updated in one step.

1. Download the new `…-squashfs-sysupgrade.bin`.
2. In LuCI (`http://<address>/`) go to **System → Backup / Flash Firmware → Flash new
   firmware image** and choose the file.
3. **Leave "Keep settings" ticked.**
4. Wait about 3 minutes.

## What's kept

- your repeater identity key and companion key
- all repeater settings (`/etc/openhop_repeater/`)
- the latest snapshot of openHop's database: regions, companion contacts and messages,
  neighbours (saved every 6 hours and whenever the repeater service stops)
- network, Wi-Fi and password settings

## Why not openHop's own updater?

openHop's dashboard has an update button that upgrades openHop with `pip`. On the M2 openHop
is built into the read-only part of the firmware (which keeps it compressed and frees RAM),
so it's updated with the firmware instead. The dashboard's update check may still say an
update is available; use the next firmware release to get it.
