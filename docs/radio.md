# The radio

## The problem

The M2's LoRa module (Seeed WM1302) is built around Semtech's **SX1302** gateway chip and two
**SX1250** radio front-ends. The SX1302's demodulators only handle 125, 250 and 500 kHz channels,
while most US MeshCore meshes use **62.5 kHz** – which is where earlier attempts to reuse gateway
hardware for MeshCore stopped ([background](https://housedillon.com/blog/meshcore-and-lorawan/)).

## Using the SX1250s directly

Normally the SX1250s stream raw I/Q samples to the SX1302, which does all the demodulation. But the
SX1250 belongs to Semtech's SX126x family (the same as the SX1262 in most MeshCore nodes) and still
contains a complete LoRa modem. Taking it out of streaming mode (register `0x0587`) and configuring it
like an SX1262 gives an ordinary LoRa transceiver at any SX126x bandwidth, including 62.5 kHz. The
SX1302 is only used as an SPI pass-through.

```
 MeshCore app / dashboard
          │
 openhop-repeater ──tcp 127.0.0.1:5055──▶ m2modem ──SPI──▶ SX1302 ──▶ SX1250 A ──▶ front-end ──▶ antenna
                                                                └──▶ SX1250 B (clock source)
```

## The front-end module

Between radio A and the antenna is a front-end module (FEM) with a power amplifier and RX/TX switch,
controlled by the SX1302. Those controls only respond while the SX1302 is clocked, and its clock comes
from a radio in I/Q streaming mode. So `m2modem` keeps **radio B** streaming purely as a clock source,
which lets it switch the FEM to transmit or receive around every packet and select the amplifier gain.

Transmit power follows Seeed's calibrated table for the M2: up to 15 dBm with the amplifier bypassed,
16–27 dBm with it on. openHop's default is 22 dBm; to use the M2's full 27 dBm, set the transmit power in the dashboard or send `set tx 27` from the MeshCore app, then restart the repeater.

## m2modem

[`feed/m2modem/src/m2modem.c`](../feed/m2modem/src/m2modem.c) is a small C daemon that owns the radio
and speaks the [openHop modem protocol](https://github.com/openhop-dev/openhop_core) over TCP:
configuration, transmit, receive, channel-activity detection (listen-before-talk), noise floor and
status. openHop already supports network-attached modems (`radio_type: modem_tcp`), so it runs on top
unchanged.

## Limits

- Radio B has no transmit path and is busy being the clock, so the M2 is a single-channel MeshCore
  radio, like any other repeater.
- RAM (128 MB) is the main constraint. openHop lives in the compressed read-only part of the firmware.

## Troubleshooting

`logread -e m2modem` should show, shortly after boot:

```
SX1302 clocked from radio B; FEM control OK
radio config: 910.525 MHz BW62500 SF7 CR4/5 22 dBm (FEM pa_gain 1, SX1250 5 dBm) …
m2modem … listening on tcp/5055 (radio A)
```

and then a line for every packet received or sent, with RSSI and SNR.

- **`FEM control registers not responding`** – transmit power will be very low (other nodes hear the
  M2 only up close). Run `/etc/init.d/m2modem restart`; if it persists, power-cycle the M2.
- **Nothing received** – check the antenna is on the RP-SMA connector, and that the radio preset in
  the dashboard matches your mesh.
- **Range** – the bundled antenna is a 3 dBi indoor whip. An outdoor antenna on low-loss coax makes
  more difference than any setting.
