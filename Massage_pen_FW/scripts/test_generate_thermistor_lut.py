"""Host checks: python3 -m unittest discover -s scripts -p 'test_*.py'."""
import math
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "scripts/generate_thermistor_lut.py"


class ThermistorTableTests(unittest.TestCase):
    def generate(self, config=None):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "table.h"
            args = [sys.executable, str(GENERATOR), "--output", str(output)]
            if config is not None:
                config_path = Path(directory) / "sensors.h"
                config_path.write_text(config)
                args += ["--config", str(config_path)]
            result = subprocess.run(args, capture_output=True, text=True)
            return result, output.read_text() if output.exists() else ""

    def test_calibration_and_interpolation_accuracy(self):
        result, header = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        entries = [(int(r), int(t)) for r, t in
                   re.findall(r"\{\s*(\d+)U,\s*(-?\d+)L\s*\}", header)]
        self.assertGreater(len(entries), 1)
        # At 25 C, Rntc = 10 kohm: Vinput/Vbattery = 4120 / 14120.
        self.assertIn((round(1_000_000 * 4120 / 14120), 25000), entries)
        self.assertLessEqual(entries[0][1], 40000)
        self.assertGreaterEqual(entries[-1][1], 49000)
        for (r0, t0), (r1, t1) in zip(entries, entries[1:]):
            self.assertGreater(r1, r0)
            self.assertGreater(t1, t0)
            for ratio in (r0, (r0 + r1) // 2, r1):
                resistance = 4120 * (1_000_000 / ratio - 1)
                actual = (1 / (1 / 298.15 + math.log(resistance / 10000) / 3435)
                          - 273.15) * 1000
                interpolated = t0 + (ratio - r0) * (t1 - t0) // (r1 - r0)
                self.assertLess(abs(actual - interpolated), 100)

    def test_reproducible_output(self):
        first, header1 = self.generate()
        second, header2 = self.generate()
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(header1, header2)

    def test_rejects_zero_step_without_writing_table(self):
        config = (ROOT / "User/Inc/sensors.h").read_text()
        config = re.sub(r"(#define THERMISTOR_LUT_STEP_MDEGC\s+)1000L",
                        r"\g<1>0L", config)
        result, header = self.generate(config)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("step must be positive", result.stderr)
        self.assertEqual(header, "")


if __name__ == "__main__":
    unittest.main()
