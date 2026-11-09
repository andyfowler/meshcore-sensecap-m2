"""Small edits to openHop for the SenseCAP M2.

Run at build time: python3 openhop_tweaks.py <dir containing repeater/>.
Each edit must match exactly once, so an openHop update that changes the code
fails the build instead of silently dropping the edit.
"""
import sys
from pathlib import Path

lib = Path(sys.argv[1])


def edit(path, old, new):
    s = path.read_text()
    if s.count(old) != 1:
        sys.exit(f"openhop_tweaks: pattern not found exactly once in {path.name}: {old[:60]!r}")
    path.write_text(s.replace(old, new))


# Setup wizard: point M2 owners at the right connection card. The text lives in
# the compiled web app, whose file name changes with every openHop release.
old = "description:`Direct SPI/GPIO connected HATs and board-integrated radios.`"
hits = [p for p in (lib / "repeater/web/html/assets").glob("Setup-*.js") if old in p.read_text()]
if len(hits) != 1:
    sys.exit("openhop_tweaks: setup wizard text not found in exactly one file")
edit(hits[0], old, "description:`Choose this for your SenseCAP M2. Also for SPI/GPIO HATs and other board-integrated radios.`")

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
