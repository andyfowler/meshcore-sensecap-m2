# First-time setup

## The setup wizard

Open the dashboard at `http://meshcore-m2-xxxxxx.local:8000` (or `http://<ip address>:8000`).
On a fresh install, openHop's setup wizard appears:

1. **Repeater name** – what other MeshCore users will see.
2. **Hardware** – choose **SenseCAP M2 (built-in radio)**.
3. **Radio preset** – pick the preset your local mesh uses. (US/Canada default:
   910.525 MHz, 62.5 kHz, SF7, CR 4/5.)
4. **Admin password** – at least 8 characters. This is the password for the dashboard and
   for remote admin from the MeshCore app.

After the wizard the repeater restarts and starts repeating.

## Passwords and keys

| What | How it's set up |
|---|---|
| **Repeater identity** | A MeshCore key pair is generated **on the M2** the first time it starts. The private key never leaves the device unless you export it. Back it up from the dashboard. You can also import an existing key there. |
| **Admin password** | Chosen in the wizard. Change it later from the dashboard or with the MeshCore CLI `password` command. |
| **Root password** (SSH and the LuCI web interface at `http://<address>/`) | Set automatically to your admin password once the wizard is finished. Until then, SSH password logins are switched off, so a fresh M2 isn't an open root shell on your network. Change it any time in **LuCI → System → Administration**. |
| **Guest password** | Blank by default, so anyone can query the repeater's status. |

## Repeater settings

Everything else – location, owner info, transmit power, advert intervals, path hash mode,
delays, regions, duty cycle – is set from the dashboard or from the MeshCore app's remote
admin, using the same CLI commands as any MeshCore repeater (`set lat 42.28`,
`set tx 22`, `region put …`). Settings are saved on the device's flash and survive reboots
and firmware updates.

Your local mesh group may publish recommended settings; apply them the same way.

## Wi-Fi instead of Ethernet

In LuCI (`http://<address>/`, user `root`): **Network → Wireless → Scan**, join your network,
and assign it to the **wan** firewall zone. Leave Ethernet plugged in until the M2 shows up
on Wi-Fi.

## Observer uploads (maps and analyzers)

To share what your repeater hears with networks such as **LetsMesh** or **MeshMapper**, open
the dashboard's MQTT settings, add the network's preset and set your region code (usually the
nearest airport code, or whatever your local network uses). No passwords are needed – uploads
are signed with the repeater's own key.

## Companion: use the M2 from the MeshCore app

The M2 can also host a **companion** node: a second, separate MeshCore identity that the
MeshCore app talks to over your network instead of Bluetooth. Enable it in the dashboard's
identities section, then in the MeshCore app add a device by **TCP** at `<address>:5000`.

## RAM

The M2 has 128 MB of RAM. A repeater, dashboard and one companion fit comfortably; the image
also enables compressed swap (zram). Plugins and extra companions each add a Python process,
so add them one at a time and watch memory on the dashboard.
