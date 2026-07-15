# Delegates to the real formatter, so the extension tests exercise it.
import sys

from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / 'scripts'))

import zscript_formatter

zscript_formatter.main()
