from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = REPO_ROOT / "IV_Curve_Tracer_2026" / "firmware" / "IV_Swinger2_R4"


class R4FirmwareCleanupTests(unittest.TestCase):
    def test_removed_unused_state_names_do_not_reappear(self):
        removed_names = [
            "VOC_COUNT_BUCKETS",
            "vocCounts",
            "lastIscStablePoints",
            "lastIscLoops",
            "PIN_ONE_WIRE_BUS",
            "DS18B20",
            "temperature sensor",
        ]

        firmware_text = "\n".join(
            path.read_text(encoding="utf-8") for path in FIRMWARE_DIR.glob("*.ino")
        )

        for name in removed_names:
            with self.subTest(name=name):
                self.assertNotIn(name, firmware_text)


if __name__ == "__main__":
    unittest.main()
