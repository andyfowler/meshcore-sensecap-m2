"""Small edits to vendored libraries that make openHop start faster on the M2.

Run at build time: python3 speedups.py <vendor dir>. Each edit must apply
exactly, so a library update that changes the code fails the build instead of
silently losing the fix.
"""
import sys
from pathlib import Path

vendor = Path(sys.argv[1])


def edit(rel, old, new):
    p = vendor / rel
    s = p.read_text()
    if s.count(old) != 1:
        sys.exit(f"speedups: pattern not found exactly once in {rel}: {old[:60]!r}")
    p.write_text(s.replace(old, new))


# tempora (pulled in by CherryPy) builds a table of time zones at import.
# OpenWrt has no system zoneinfo, so each lookup unpacks dateutil's bundled
# archive: about 12 s on the M2. Build the table on first use instead.
edit("tempora/__init__.py", "\ntzinfos = dict(\n", "\n@functools.cache\ndef _tzinfos():\n  return dict(\n")
edit("tempora/__init__.py", "tzinfos=tzinfos,", "tzinfos=_tzinfos(),")

# CherryPy looks up its own version through importlib.metadata, which imports
# a dozen stdlib modules and scans every package on the path.
edit(
    "cherrypy/__init__.py",
    "try:\n    import importlib.metadata as importlib_metadata\nexcept ImportError:\n"
    "    # fall back for python <= 3.7\n    # This try/except can be removed with py <= 3.7 support\n"
    "    import importlib_metadata\n",
    "",
)
edit(
    "cherrypy/__init__.py",
    "try:\n    __version__ = importlib_metadata.version('cherrypy')\nexcept Exception:\n    __version__ = 'unknown'",
    "__version__ = %r" % next(vendor.glob("CherryPy-*.dist-info")).name.removesuffix(".dist-info").split("-")[1],
)
