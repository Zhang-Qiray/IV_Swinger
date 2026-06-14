from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_DIR = REPO_ROOT / "IV_Curve_Tracer_2026" / "firmware" / "IV_Swinger2_R4"


def read_firmware_file(name: str) -> str:
    return (FIRMWARE_DIR / name).read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace_start = source.index("{", start)
    depth = 0
    for index in range(brace_start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace_start + 1 : index]
    raise AssertionError(f"Function body not found for {signature}")


def assert_in_order(test_case: unittest.TestCase, text: str, snippets: list[str]) -> None:
    position = -1
    for snippet in snippets:
        next_position = text.find(snippet, position + 1)
        test_case.assertNotEqual(next_position, -1, f"Missing snippet: {snippet}")
        position = next_position


class R4SweepReadabilityTests(unittest.TestCase):
    def test_teaching_sweep_reads_as_one_straight_line_flow(self):
        source = read_firmware_file("5c_Sweep_manually.ino")

        self.assertNotIn("captureTeachingSweep", source)
        self.assertNotIn("captureFixedTeachingPoints", source)

        body = function_body(source, "void runTeachingSweep(")
        assert_in_order(
            self,
            body,
            [
                "measureVocForSweep();",
                "measureStableIscForSweep()",
                "startSweepPath();",
                "for (int index = 0; index < targetPoints; ++index)",
                "endSweepPath();",
                "printCleanSweepData(out);",
            ],
        )

    def test_automatic_sweep_uses_one_formal_capture_loop(self):
        source = read_firmware_file("5b_AutoSweep.ino")

        self.assertNotIn("saveEveryRawPointDuringFormalSweep", source)
        self.assertNotIn("savePointsByTimeDuringFormalSweep", source)
        self.assertIn("captureFormalSweepPoints", source)
        self.assertIn("shouldSaveFormalSweepPoint", source)


if __name__ == "__main__":
    unittest.main()
