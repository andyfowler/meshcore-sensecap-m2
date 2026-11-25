# m2modem on a Raspberry Pi 4 + RAK2287

A port of `feed/m2modem/src/m2modem.c` to the RAK2287 concentrator (SX1302 + two SX1250, the same chip set as the WM1302) on a Raspberry Pi 4 running Raspberry Pi OS. This is the hardware inside RAK7248-class gateways and several retired Helium hotspots (for example the MNTD). It is a normal Linux daemon, not part of the OpenWrt image in this repository.

It serves the same openHop modem protocol on TCP port 5055 (localhost only), so openHop Repeater runs on top with `radio_type: modem_tcp`.

Tested with openHop Repeater 1.1.4 and openhop_core 1.1.3 on the US MeshCore preset (910.525 MHz, 62.5 kHz, SF7, CR 4/5): receives, transmits, and repeats, on four units, two of them working rooftop repeaters.

## What differs from m2modem 0.1

- **Board:** SPI device is an option (`-d`, default `/dev/spidev0.0`); reset through a script (`-R`), supplied here for the RAK Pi HAT (GPIO 17 and 25, active high); only radio A is driven.
- **Transmit power:** RAK's RAK2287 US915 gain table, with a calibration table that maps 12, 15, 18, 21, 24, and 27 dBm to SX1250 drive levels (matched against the HAL at a second receiver, within about 2 dB). The cap is 12 dBm unless `-P` raises it.
- **Fails loud:** exits on a wrong chip version, a failed HAL call or SPI open, a front-end readback mismatch, or a radio that will not reach standby, and holds the concentrator in reset. After every transmit the PA enable line is read back low.
- **Receive fixes** (both also proposed for `m2modem.c`, see issues 1 and 2):
  - A CAD request that arrives while a packet is being received is answered "busy" without leaving receive.
  - Bit 0 of register 0x08B5 is set at every radio config, against the SX126x gain-control lockup. A gain watch logs the idle level and gain registers and re-enters receive if the level stays high.
- **Housekeeping:** the listener binds to 127.0.0.1; the noise floor is the mean of the last 20 idle samples; a latched preamble flag is cleared after the longest possible packet time; SPI failures are counted and five in a row are fatal; the default preamble follows MeshCore firmware (32 symbols at SF8 and below).
- **Logging:** each received packet shows RSSI, SNR, signal RSSI, and frequency error; an `rx health` line appears every 5 minutes.

The full list is in the comment at the top of `m2modem_rak2287.c`.

## Build

On the Pi, as an ordinary user (needs `gcc`, `make`, `curl`, `tar`, and `python3`):

```sh
mkdir -p ~/m2modem && cp m2modem_rak2287.c build.sh reset_concentrator.sh ~/m2modem/
bash ~/m2modem/build.sh
```

`build.sh` downloads Semtech's sx1302_hal V2.1.0, checks it against a pinned SHA-256 and commit, and links `libloragw` and `libtools` statically. Nothing is installed system-wide.

## Run

Enable SPI (`dtparam=spi=on`), put the user in groups `spi` and `gpio`, and **attach the antenna before any transmit**.

```sh
~/m2modem/m2modem_rak2287 -R ~/m2modem/reset_concentrator.sh -P 27
```

Options: `-d` SPI device, `-R` reset script, `-P` power cap in dBm (12 to 27), `-p` TCP port, `-n` receive only, and `-v` verbose. Under systemd, run the reset script with `--hold` in `ExecStopPost` so the concentrator is held in reset while the daemon is down.

Optional knob files, read while running: `/etc/m2modem/rx_knobs` (`agcfix=0|1`, `watch=0|1|2`, `inflight=0|1`) and `/etc/m2modem/tx_knobs` (`mq=0|1`, `cap=N`). With no file the defaults apply.

## Limits

- Absolute transmit power was matched against the HAL's table at one receiver, not measured with a power meter.
- Raw RSSI has no front-end offset applied; compare SNR, or levels on the same unit.
- CAD thresholds are Semtech's SF7 example values; they were not tuned for 62.5 kHz.
- Only the US 902 to 928 MHz table is included.

## License

MIT, as the rest of this repository. The sx1302_hal it links is Semtech's (revised BSD; `build.sh` keeps its license file next to the build).
