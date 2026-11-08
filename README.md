# MeshCore for the SenseCAP M2

Turn a **Seeed SenseCAP M2** LoRaWAN gateway into a **[MeshCore](https://meshcore.co.uk) repeater**,
including the US/Canada **62.5 kHz** preset that the M2's gateway chip was never meant to support.

The image is plain [OpenWrt](https://openwrt.org) 24.10 with
[openHop Repeater](https://github.com/openhop-dev/openhop_repeater) on top, so you get:

- a full MeshCore repeater you can manage from the **MeshCore app** (remote admin) or a **web dashboard**
- up to **27 dBm** transmit power through the M2's built-in amplifier, and its external antenna port
- optional uploads to observer networks (**LetsMesh**, **MeshMapper**, …) for maps and analyzers
- an optional **companion** node, so the MeshCore app can chat through the M2 over Wi-Fi instead of Bluetooth
- the OpenWrt web interface (**LuCI**) for Wi-Fi, network settings and firmware updates

> **Status:** early, but running on real hardware. Tested on an M2 Multi-Platform (US915).
> Expect rough edges, and please report what you find.

## What you need

- A **SenseCAP M2 Multi-Platform** gateway (WM1302 LoRa module). The *M2 Data-Only* (Helium) model is
  more locked down and isn't covered here.
- An Ethernet connection to your network for the first setup (Wi-Fi can be configured afterwards).
- About 20 minutes.

## Quick start

1. **Install** – flash Seeed's open-source firmware, then this image.
   Step by step: **[docs/install.md](docs/install.md)**
2. **Set up** – open `http://meshcore-m2-xxxxxx.local:8000` (or the M2's IP address, port 8000).
   The setup wizard asks for a repeater name, radio preset and admin password. Pick
   **"SenseCAP M2 (built-in radio)"** as the hardware.
   Details, passwords and keys: **[docs/setup.md](docs/setup.md)**
3. **Done** – manage it from the dashboard, or from the MeshCore app like any other repeater.

## Documentation

| | |
|---|---|
| [Install](docs/install.md) | From a stock M2 to this image |
| [First-time setup](docs/setup.md) | Wizard, passwords, identity keys, Wi-Fi, companion, observer uploads |
| [Updating](docs/updating.md) | New firmware versions without losing your settings |
| [Building](docs/building.md) | Build the image yourself, optionally with your settings built in |
| [How it works](docs/how-it-works.md) | The radio trickery that makes 62.5 kHz possible |
| [Troubleshooting](docs/troubleshooting.md) | Recovery, logs, common problems |

## Credits

- [openHop Repeater / openHop Core](https://github.com/openhop-dev) – the repeater, dashboard and MeshCore stack (MIT)
- [Semtech SX1302 HAL](https://github.com/Lora-net/sx1302_hal) – register access to the concentrator (BSD)
- [Seeed Studio](https://github.com/Seeed-Solution/LoRa_Gateway_OpenWRT) – M2 open-source firmware and SPI patch
- [ChirpStack Gateway OS](https://github.com/chirpstack/chirpstack-gateway-os) – M2 device support for OpenWrt 24.10
- [MeshCore](https://github.com/meshcore-dev/MeshCore) and [OpenWrt](https://openwrt.org)
