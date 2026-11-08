# How it works

## The problem

The M2's LoRa module (Seeed WM1302) is built around Semtech's **SX1302** gateway chip and two
**SX1250** radio front-ends. The SX1302's demodulators only handle 125, 250 and 500 kHz
channels, and most US MeshCore meshes use **62.5 kHz**. Earlier attempts to use LoRaWAN gateway
hardware for MeshCore stopped there.

## The trick: use the SX1250s directly

Normally the SX1250s run in an I/Q streaming mode and the SX1302 does all the demodulation.
But the SX1250 is a member of Semtech's SX126x family (the same family as the SX1262 in most
MeshCore nodes) and still contains a complete LoRa modem. Taking it out of streaming mode
(register `0x0587`) and configuring it like an SX1262 gives a normal LoRa transceiver at any
SX126x bandwidth, including 62.5 kHz. The SX1302 is only used as an SPI pass-through.

```
 MeshCore app / dashboard
          │
 openhop-repeater (Python) ──tcp 127.0.0.1:5055──▶ m2modem (C) ──SPI──▶ SX1302 ──▶ SX1250 A ──▶ FEM ──▶ antenna
                                                                              └──▶ SX1250 B (clock source)
```

## The front-end module

Between radio A and the antenna sits a front-end module (FEM) with a power amplifier and an
RX/TX switch, controlled by the SX1302. Those controls only respond while the SX1302 is
clocked, and the SX1302 takes its clock from a radio that's in I/Q streaming mode. So
`m2modem` keeps **radio B** in streaming mode purely as a clock source, which lets it switch the
FEM to transmit (`PA_EN`) or receive (`LNA_EN`) around every packet and select the FEM's
amplifier gain. Transmit power follows Seeed's calibrated table: up to 15 dBm with the
amplifier bypassed, 16–27 dBm with it on.

## m2modem

[`feed/m2modem/src/m2modem.c`](../feed/m2modem/src/m2modem.c) is a small C daemon that owns the
radio and speaks the [openHop modem protocol](https://github.com/openhop-dev/openhop_core) over
TCP: configuration, transmit, receive, channel-activity detection (listen-before-talk), noise
floor and status. Because openHop already supports network-attached modems (`radio_type:
modem_tcp`), it runs unmodified on top.

## Limits

- Radio B can't transmit (it has no transmit path) and is busy being the clock, so the M2
  works as a single-channel MeshCore radio – like any other repeater.
- The 128 MB of RAM is the main constraint. openHop is installed in the compressed read-only
  part of the firmware and OpenWrt's web interface is the lightweight `luci-light`.
