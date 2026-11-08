#!/usr/bin/env python3
"""Render /etc/config/meshcore (UCI) into an openHop repeater config.yaml.

Starts from openHop's example config, applies the site settings, and writes the
result. Region definitions can't live in config.yaml (openHop keeps them in its
database), so they are written as CLI commands to a pending file that the
openhop-repeater init script replays once the daemon is up.
"""
import argparse
import os
import subprocess
import sys

import yaml

PENDING_CLI = "/etc/openhop_repeater/boot-cli.txt"  # replayed on every start; regions live in the RAM database


def uci_section(cfg):
    """Return {section: {option: value | [values]}} from `uci show <cfg>`."""
    out = subprocess.run(["uci", "-q", "show", cfg], capture_output=True, text=True).stdout
    data = {}
    for line in out.splitlines():
        key, _, val = line.partition("=")
        parts = key.split(".")
        if len(parts) != 3:
            continue
        _, sec, opt = parts
        # uci quotes values; lists are space separated quoted values
        vals = []
        cur, inq = "", False
        for ch in val:
            if ch == "'":
                inq = not inq
                if not inq:
                    vals.append(cur)
                    cur = ""
            elif inq:
                cur += ch
        data.setdefault(sec, {})[opt] = vals if len(vals) > 1 or opt in ("region", "broker", "disable_broker") else (vals[0] if vals else "")
    return data


def as_list(v):
    if not v:
        return []
    return v if isinstance(v, list) else [v]


def num(v, cast=float):
    try:
        return cast(v)
    except (TypeError, ValueError):
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--template", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    u = uci_section("meshcore")
    node, radio, mesh, mqtt = (u.get(k, {}) for k in ("node", "radio", "mesh", "mqtt"))

    with open(args.template) as f:
        cfg = yaml.safe_load(f) or {}

    rep = cfg.setdefault("repeater", {})
    if node.get("name"):
        rep["node_name"] = node["name"]
    for k_uci, k_oh in (("lat", "latitude"), ("lon", "longitude")):
        if num(node.get(k_uci)) is not None:
            rep[k_oh] = num(node.get(k_uci))
    if node.get("owner_info"):
        rep["owner_info"] = node["owner_info"].replace("|", "\n")
    sec = rep.setdefault("security", {})
    # A blank admin password leaves openHop's first-run setup wizard enabled.
    sec["admin_password"] = node.get("admin_password") or ""
    sec["guest_password"] = node.get("guest_password", "")
    if sec["admin_password"]:
        cfg["setup_completed"] = True
    rep["identity_file"] = "/etc/openhop_repeater/identity.key"

    if num(mesh.get("advert_interval"), int) is not None:
        rep["advert_interval_minutes"] = num(mesh["advert_interval"], int)
    if num(mesh.get("flood_advert_interval"), int) is not None:
        rep["send_advert_interval_hours"] = num(mesh["flood_advert_interval"], int)
    if num(mesh.get("flood_max"), int) is not None:
        rep["max_flood_hops"] = num(mesh["flood_max"], int)
    if num(mesh.get("agc_reset_interval"), int) is not None:
        rep["agc_reset_interval"] = num(mesh["agc_reset_interval"], int)

    m = cfg.setdefault("mesh", {})
    if num(mesh.get("path_hash_mode"), int) is not None:
        m["path_hash_mode"] = num(mesh["path_hash_mode"], int)
    if mesh.get("loop_detect"):
        m["loop_detect"] = mesh["loop_detect"]

    d = cfg.setdefault("delays", {})
    for k_uci, k_oh in (("txdelay", "tx_delay_factor"), ("direct_txdelay", "direct_tx_delay_factor"),
                        ("rxdelay", "rx_delay_base")):
        if num(mesh.get(k_uci)) is not None:
            d[k_oh] = num(mesh[k_uci])

    duty = num(mesh.get("duty_cycle"))
    if duty is not None:
        dc = cfg.setdefault("duty_cycle", {})
        dc["enforcement_enabled"] = duty < 100
        dc["max_airtime_per_minute"] = int(60000 * duty / 100)

    cfg["radio_type"] = "modem_tcp"
    cfg["modem_tcp"] = {"host": "127.0.0.1", "port": 5055, "token": "", "connect_timeout": 5.0,
                        "lbt_enabled": True, "lbt_max_attempts": 5}
    r = cfg.setdefault("radio", {})
    for k_uci, k_oh, cast in (("frequency", "frequency", int), ("bandwidth", "bandwidth", int),
                              ("spreading_factor", "spreading_factor", int), ("coding_rate", "coding_rate", int),
                              ("preamble", "preamble_length", int)):
        if num(radio.get(k_uci), cast) is not None:
            r[k_oh] = num(radio[k_uci], cast)
    if radio.get("sync_word"):
        r["sync_word"] = int(radio["sync_word"], 0)
    if num(node.get("tx_power"), int) is not None:
        r["tx_power"] = num(node["tx_power"], int)

    st = cfg.setdefault("storage", {})
    st["storage_dir"] = "/var/lib/openhop_repeater"   # RAM; snapshotted to flash by meshcore-m2
    st.setdefault("retention", {})["sqlite_cleanup_days"] = 7
    # 128 MB RAM: keep the CherryPy worker pool small
    h = cfg.setdefault("http", {})
    h["thread_pool"] = 4
    h["thread_pool_max"] = 8
    # Plugins need a separate manager service (and usually Docker); off on the M2.
    cfg["plugins"] = {"enabled": False}

    if mqtt.get("enabled") == "1":
        mb = cfg.setdefault("mqtt_brokers", {})
        if mqtt.get("iata"):
            mb["iata_code"] = mqtt["iata"]
        mb["owner"] = mqtt.get("owner", "")      # LetsMesh: owner's companion public key (claims the node)
        mb["email"] = mqtt.get("email", "")
        brokers = [{"preset": b} for b in as_list(mqtt.get("broker"))]
        # switch off individual preset brokers by name, e.g. 'Europe (LetsMesh v1)'
        brokers += [{"name": n, "enabled": False} for n in as_list(mqtt.get("disable_broker"))]
        mb["brokers"] = brokers

    comp = u.get("companion", {})
    if comp.get("enabled") == "1":
        key_file = comp.get("key_file") or "/etc/openhop_repeater/companion.key"
        key_hex = ""
        if os.path.exists(key_file):
            key_hex = open(key_file).read().strip()
        if len(key_hex) not in (64, 128):
            sys.path.insert(0, "/usr/lib/openhop")
            from repeater.keygen import generate_meshcore_keypair  # MeshCore-format 64-byte key
            _, prv = generate_meshcore_keypair()
            key_hex = prv.hex()
            with open(key_file, "w") as f:
                f.write(key_hex + "\n")
            os.chmod(key_file, 0o600)
        name = comp.get("name") or (rep.get("node_name", "SenseCAP M2") + " companion")
        cfg.setdefault("identities", {})["companions"] = [{
            "name": "companion",
            "identity_key": key_hex,
            "settings": {
                "node_name": name,
                "tcp_port": num(comp.get("tcp_port"), int) or 5000,
                "bind_address": "0.0.0.0",
                "tcp_timeout": num(comp.get("tcp_timeout"), int) or 0,
            },
        }]

    tmp = args.out + ".tmp"
    with open(tmp, "w") as f:
        yaml.safe_dump(cfg, f, sort_keys=False, default_flow_style=False)
    os.replace(tmp, args.out)

    regions = mesh.get("region") or []
    if isinstance(regions, str):
        regions = [regions]
    if regions:
        cmds = []
        for entry in regions:
            name, _, parent = entry.partition(":")
            cmds.append(f"region put {name} {parent}".strip())
        cmds.append("region save")
        with open(PENDING_CLI, "w") as f:
            f.write("\n".join(cmds) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
