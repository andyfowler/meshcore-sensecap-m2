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

# Sidebar: show this image's build as a badge on its own line under the
# R:/Core: version badges (there's no room for a third one beside them).
idx = bundle("index", "gr={class:`flex items-center gap-2`}")
edit(idx, "gr={class:`flex items-center gap-2`}", "gr={class:`flex items-center gap-2`,style:{flexWrap:`wrap`}}")
old = "M19 9l-7 7-7-7`},null,-1)]],2))])"
edit(idx, old,
     old[:-2] + ",D(`div`,{style:{flexBasis:`100%`}},[D(`span`,{class:`glass-card px-2 py-1 text-xs "
     "font-medium rounded border text-content-secondary dark:text-content-muted border-stroke-subtle "
     "dark:border-stroke`,title:`meshcore-sensecap-m2 build`},`M2:" + build_id + "`)])])")
