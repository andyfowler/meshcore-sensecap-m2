"""Small edits to openHop for the SenseCAP M2.

Run at build time: python3 openhop_tweaks.py <dir containing repeater/> <build id>.
Each edit must match exactly once, so an openHop update that changes the code
fails the build instead of silently dropping the edit.
"""
import sys
from pathlib import Path

lib = Path(sys.argv[1])
build_id = sys.argv[2]
assets = lib / "repeater/web/html/assets"


def bundle(prefix, old):
    """The web app's file names change with every openHop release."""
    hits = [p for p in assets.glob(prefix + "-*.js") if old in p.read_text()]
    if len(hits) != 1:
        sys.exit(f"openhop_tweaks: {old[:60]!r} not found in exactly one {prefix}-*.js")
    return hits[0]


def edit(path, old, new):
    s = path.read_text()
    if s.count(old) != 1:
        sys.exit(f"openhop_tweaks: pattern not found exactly once in {path.name}: {old[:60]!r}")
    path.write_text(s.replace(old, new))


# Setup wizard: point M2 owners at the right connection card.
old = "{key:`gpio`,title:`HAT GPIO Based Device`,"
edit(bundle("Setup", old), old, "{key:`gpio`,title:`SenseCAP M2 / other GPIO device`,")

# Dashboard: don't show the "Plugin manager is unavailable" banner. That service
# isn't part of this image (plugins don't fit in the M2's memory).
old = "a.value=/unavailable|503/i.test(t)"
edit(bundle("Dashboard", old), old, "a.value=!1")

# Sidebar version details (click the R:/Core: badges): add this image's build.
old = "D(`span`,br,`v`+p(Q.value.base),1)])"
edit(bundle("index", old), old,
     old + ",D(`div`,yr,[D(`span`,{class:`text-content-muted font-medium`},`Build:`),"
     "D(`span`,br,`meshcore-sensecap-m2 %s`)])" % build_id)

# MQTT status messages (LetsMesh, MeshMapper, ...): name the hardware.
edit(
    lib / "repeater/data_acquisition/mqtt_handler.py",
    '"model": "openHop-Repeater",',
    '"model": "openHop-Repeater (SenseCAP M2)",',
)

# The over-the-air version reply looks up pip package metadata, which this
# build doesn't install, so it answered "pyMC". Use the package version.
edit(
    lib / "repeater/handler_helpers/protocol_request.py",
    '            from importlib.metadata import version as pkg_version\n\n'
    '            fw_version = pkg_version("openhop-repeater")\n',
    '            from repeater import __version__ as fw_version\n',
)
