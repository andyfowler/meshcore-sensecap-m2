# MeshCore for the SenseCAP M2

Firmware that turns a **Seeed SenseCAP M2** LoRaWAN gateway into a **[MeshCore](https://meshcore.co.uk)
repeater**, including the US/Canada **62.5 kHz** preset the M2's gateway chip doesn't support on its own.

It's [OpenWrt](https://openwrt.org) 24.10 with [openHop Repeater](https://github.com/openhop-dev/openhop_repeater):

- manage it from the **MeshCore app** (remote admin) or a **web dashboard**
- up to **27 dBm** through the M2's built-in amplifier and external antenna port
- optional uploads to observer networks such as **LetsMesh** and **MeshMapper**
- optional **companion**: use the MeshCore app over Wi-Fi with the M2 as your radio
- OpenWrt's web interface for Wi-Fi, passwords and firmware updates

> Early but running on real hardware (M2 Multi-Platform, US915). Please report what you find.

## Install

You need a **SenseCAP M2 Multi-Platform** (the *Data-Only* Helium model isn't supported) connected
to your network by Ethernet. Installing is two firmware flashes: Seeed's open-source firmware, then this.

> **No way back.** Once Seeed's open-source firmware is installed, the factory SenseCAP firmware
> can't be reinstalled through the web interface.

1. **Log in to the stock firmware.** Find the M2 in your router's device list, open `https://<address>`
   and log in with the username and password on the label underneath.
2. **Install Seeed's open-source firmware.** Download the image for your region from
   [Seeed's releases](https://github.com/Seeed-Solution/LoRa_Gateway_OpenWRT/releases)
   (e.g. `…-sysupgrade-US915-4G.bin`; the 4G build works without a modem). Flash it under
   **System → Backup / Flash Firmware → Local upgrade**, unticking *Keep settings* if offered.
   Skip this step if you're running ChirpStack Gateway OS.
3. **Install this firmware.** After ~3 minutes the M2 is back with a plain OpenWrt interface at
   `http://<address>` (user `root`, no password). Under **System → Backup / Flash Firmware → Flash
   new firmware image**, flash the `…-squashfs-sysupgrade.bin` from the
   [latest release](../../releases/latest) with *Keep settings* **unticked**.
4. Wait about 5 minutes. The first boot is slow.

## Set up

Open **`http://meshcore-m2-xxxxxx.local:8000`**, where `xxxxxx` is the end of the M2's MAC address,
or use its IP address from your router. The setup wizard asks for:

- **Name** for the repeater
- **Hardware**: choose **SenseCAP M2 (built-in radio)**
- **Radio preset** your local mesh uses (US/Canada: 910.525 MHz, 62.5 kHz, SF7, CR 4/5)
- **Admin password** for the dashboard and MeshCore remote admin

That's it – it starts repeating. Everything else (location, transmit power, regions, path hash mode,
delays, owner info) can be set in the dashboard or from the MeshCore app with the usual repeater
commands, and survives reboots and updates.

**Passwords and keys**

- The repeater's **identity key** is generated on the M2 itself. Back it up (or import an existing one)
  from the dashboard.
- **Root** (SSH, and the OpenWrt interface at `http://<address>/`) gets the same password as the admin
  password once the wizard is done. SSH password logins are off until then.

**Optional extras**

- **Wi-Fi**: in the OpenWrt interface, *Network → Wireless → Scan*, join your network and put it
  in the **wan** zone.
- **Observer uploads**: add a network (LetsMesh, MeshMapper, …) and your region code in the dashboard's
  MQTT settings. No passwords; uploads are signed with the repeater's key.
- **Companion**: enable it in the dashboard's identities section, then add a **TCP** device at
  `<address>:5000` in the MeshCore app.

The M2 has 128 MB of RAM: a repeater, dashboard and one companion fit comfortably. Add plugins or
more companions one at a time.

## Update

Download the new `…-squashfs-sysupgrade.bin` from [releases](../../releases) and flash it in the
OpenWrt interface (**System → Backup / Flash Firmware**) with *Keep settings* **ticked**. Your keys,
settings, regions and companion contacts are kept.

openHop is built into the firmware, so use firmware releases rather than the dashboard's update button.

## Help

- **Logs**: `ssh root@<address>`, then `logread -e m2modem` (radio) or `logread | grep python3`
  (openHop). Also in the OpenWrt interface under *Status → System Log*.
- **Dashboard not loading**: openHop takes 1–2 minutes to start after a reboot.
- **Start over**: `firstboot -y && reboot` wipes all settings and keys (export your identity first).
- **Recovery**: press the reset button repeatedly while booting for OpenWrt failsafe mode
  (`192.168.1.1`); the bootloader also has a TFTP recovery menu on the USB-C serial console (57600 baud).

## More

- [docs/radio.md](docs/radio.md) – how the M2's radio is made to do 62.5 kHz, and radio troubleshooting
- [docs/building.md](docs/building.md) – build the firmware yourself, optionally with your settings built in

## Credits

- William Dillon's [The Unfortunate Unsuitability of using LoRa Gateway Concentrators with MeshCore](https://housedillon.com/blog/meshcore-and-lorawan/)
  and its comments, which mapped out the problem this project works around
- [openHop Repeater and openHop Core](https://github.com/openhop-dev) – the repeater, dashboard and MeshCore stack
- [Semtech SX1302 HAL](https://github.com/Lora-net/sx1302_hal) – concentrator register access
- [Seeed Studio](https://github.com/Seeed-Solution/LoRa_Gateway_OpenWRT) – the M2's open-source firmware and SPI patch
- [ChirpStack Gateway OS](https://github.com/chirpstack/chirpstack-gateway-os) – M2 device support for OpenWrt 24.10
- [MeshCore](https://github.com/meshcore-dev/MeshCore) and [OpenWrt](https://openwrt.org)
