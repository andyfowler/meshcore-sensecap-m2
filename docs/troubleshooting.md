# Troubleshooting

## Logs

Over SSH (`ssh root@<address>`):

```sh
logread -e m2modem         # radio driver: every packet received/sent, with RSSI/SNR
logread | grep python3     # openHop
logread -f                 # follow everything live
```

LuCI shows the same under **Status → System Log**.

## The radio

`logread -e m2modem` should show, shortly after boot:

```
SX1302 clocked from radio B; FEM control OK
radio config: 910.525 MHz BW62500 SF7 CR4/5 22 dBm (FEM pa_gain 1, SX1250 5 dBm) …
m2modem … listening on tcp/5055 (radio A)
```

If you see `FEM control registers not responding`, transmit power will be very low. Restart
the driver with `/etc/init.d/m2modem restart`; if it persists, power-cycle the M2.

## The dashboard won't load

openHop takes 1–2 minutes to start after a reboot. Check it's running with
`pgrep -f repeater.main`, and look at `logread | grep python3` for errors.

## Starting over

To redo first-time setup without reflashing, run over SSH:

```sh
firstboot -y && reboot      # wipes all settings and keys, back to a fresh install
```

Export your repeater identity from the dashboard first if you want to keep it.

## Recovery

- **Failsafe mode:** OpenWrt's failsafe is enabled. Press the reset button repeatedly while the
  M2 boots; it comes up at `192.168.1.1` with SSH and no services running.
- **Bootloader:** the M2's U-Boot has a TFTP recovery menu on the USB-C serial console
  (57600 baud). From there a sysupgrade image can be written to the firmware partition.

## Backing up the factory flash (before installing)

On stock firmware, the web API allows reading `/dev/mtdblock0`–`6`. Logged in to the stock web
interface, the partitions can be downloaded through `/cgi-bin/cgi-download` and concatenated
into a 32 MB image. Restoring it needs a SPI flash programmer, or the bootloader's TFTP menu
for the firmware partition only.
