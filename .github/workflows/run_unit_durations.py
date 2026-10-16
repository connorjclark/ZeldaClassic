# TEMP: run the unit tests like `python -m unittest discover tests`, but report
# every test's duration. Some test modules parse sys.argv on import, so clear it.
import sys
import unittest

sys.argv = sys.argv[:1]
suite = unittest.defaultTestLoader.discover('tests')
result = unittest.TextTestRunner(verbosity=2, durations=0).run(suite)
sys.exit(0 if result.wasSuccessful() else 1)
