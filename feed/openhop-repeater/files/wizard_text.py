"""Point SenseCAP M2 owners at the right card in openHop's setup wizard.

Run at build time: python3 wizard_text.py <dir containing repeater/>.
The text lives in the compiled web app, so the edit must match exactly once;
an openHop update that changes it fails the build rather than silently
dropping the hint.
"""
import sys
from pathlib import Path

old = "description:`Direct SPI/GPIO connected HATs and board-integrated radios.`"
new = "description:`Choose this for your SenseCAP M2. Also for SPI/GPIO HATs and other board-integrated radios.`"

hits = [p for p in Path(sys.argv[1], "repeater/web/html/assets").glob("Setup-*.js") if old in p.read_text()]
if len(hits) != 1 or hits[0].read_text().count(old) != 1:
    sys.exit("wizard_text: setup wizard text not found exactly once")
hits[0].write_text(hits[0].read_text().replace(old, new))
