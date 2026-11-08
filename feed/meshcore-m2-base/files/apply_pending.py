#!/usr/bin/env python3
"""Replay one-shot repeater CLI commands (e.g. region setup) via the local openHop API.

Usage: apply-pending [commands-file]   (default /etc/openhop_repeater/pending-cli.txt)
       apply-pending -c "get name"     (run a single command)
Exit 0 only if every command succeeded, so the caller can delete the file.
"""
import json
import sys
import time
import urllib.request

import yaml

CONF = "/etc/openhop_repeater/config.yaml"


def api(base, path, body, token=None):
    hdr = {"Content-Type": "application/json"}
    if token:
        hdr["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(base + path, data=json.dumps(body).encode(), headers=hdr, method="POST")
    with urllib.request.urlopen(req, timeout=15) as r:  # nosec B310 - localhost only
        return json.loads(r.read())


def main(argv):
    cfg = yaml.safe_load(open(CONF)) or {}
    port = (cfg.get("http") or {}).get("port", 8000)
    pw = ((cfg.get("repeater") or {}).get("security") or {}).get("admin_password", "")
    base = f"http://127.0.0.1:{port}"

    if len(argv) > 2 and argv[1] == "-c":
        cmds = [" ".join(argv[2:])]
    else:
        path = argv[1] if len(argv) > 1 else "/etc/openhop_repeater/boot-cli.txt"
        cmds = [c.strip() for c in open(path) if c.strip() and not c.startswith("#")]

    token = None
    for _ in range(30):  # wait for the daemon's HTTP server
        try:
            res = api(base, "/auth/login", {"username": "admin", "password": pw, "client_id": "meshcore-m2"})
            token = res.get("token") or (res.get("data") or {}).get("token")
            if token:
                break
        except Exception:
            pass
        time.sleep(5)
    if not token:
        print("could not authenticate to openHop API", file=sys.stderr)
        return 1

    ok = True
    for c in cmds:
        res = api(base, "/api/cli", {"command": c}, token)
        reply = (res.get("data") or {}).get("reply", res)
        print(f">> {c}\n{reply}")
        if not res.get("success") or str(reply).startswith("Err"):
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
