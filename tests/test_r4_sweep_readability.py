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
    def test_serial_commands_are_declared_as_a_readable_command_table(self):
        source = read_firmware_file("7_Protocol_Commands.ino")

        self.assertIn("struct SerialCommand", source)
        self.assertIn("const SerialCommand COMMANDS[]", source)
        self.assertIn("for (const SerialCommand &cmd : COMMANDS)", source)
        self.assertNotIn("else if (commandName ==", source)

        for command in [
            "HELP",
            "PING",
            "STATE",
            "IDLE",
            "RELAY",
            "ADC",
            "ADC_BOTH",
            "ADC_SPEED",
            "SPI_HZ",
            "CAL",
            "SET_CAL",
            "VOC",
            "ISC",
            "VOC_TEST",
            "ISC_TEST",
            "SWEEP",
            "SWEEP_T",
            "SWEEP_ALL",
            "SWEEP_POINTS",
            "STOP",
        ]:
            self.assertIn(f'{{"{command}",', source)

    def test_sweep_state_uses_short_domain_names_for_common_state(self):
        firmware_text = "\n".join(
            path.read_text(encoding="utf-8") for path in FIRMWARE_DIR.glob("*.ino")
        )

        for new_name in [
            "saveLimit",
            "rawPointsRead",
            "pointsSaved",
            "tailCurrentAdc",
            "prevCurrentAdc",
            "reachedTail",
            "latestPoint",
        ]:
            self.assertIn(new_name, firmware_text)

        for old_name in [
            "sweepOutputPointLimit",
            "sweepRawPointCount",
            "sweepOutputPointCount",
            "sweepEndCurrentAdcThreshold",
            "sweepPreviousCurrentAdc",
            "sweepCurrentReachedTail",
            "latestSweepPoint",
        ]:
            self.assertNotIn(old_name, firmware_text)

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

    def test_automatic_sweep_uses_original_noise_floor_tail_detection(self):
        sweep_config = read_firmware_file("5_SWEEP.ino")
        auto_sweep = read_firmware_file("5b_AutoSweep.ino")
        firmware_text = "\n".join(
            path.read_text(encoding="utf-8") for path in FIRMWARE_DIR.glob("*.ino")
        )

        self.assertIn("const int MIN_SWEEP_DONE_CURRENT_ADC = 20;", sweep_config)
        self.assertIn("const int SWEEP_DONE_CURRENT_DELTA_ADC = 3;", sweep_config)
        self.assertIn("int tailCurrentAdc = 0;", sweep_config)
        self.assertNotIn("SWEEP_END_VOC_PERCENT", firmware_text)
        self.assertNotIn("sweepVoltageReachedVocPercent", firmware_text)
        self.assertNotIn("ZERO_CURRENT_CONFIRM_POINTS", firmware_text)

        body = function_body(auto_sweep, "bool sweepOutputCurrentReachedTail(")
        self.assertIn("latestPoint.i < tailCurrentAdc", body)
        self.assertIn("currentDelta < SWEEP_DONE_CURRENT_DELTA_ADC", body)
        self.assertNotIn("latestPoint.i != 0", body)

    def test_isc_stability_uses_original_ssr_three_equal_samples(self):
        source = read_firmware_file("5a_Measure.ino")
        body = function_body(source, "void measureIscAverage(")

        self.assertIn("voltage == voltagePrev", body)
        self.assertIn("voltagePrev == voltagePrevPrev", body)
        self.assertIn("current == currentPrev", body)
        self.assertIn("currentPrev == currentPrevPrev", body)
        self.assertIn("lastIscAdc = currentPrevPrev;", body)
        self.assertIn("lastIscStable = true;", body)

    def test_formal_sweep_keeps_voltage_order_when_saving(self):
        source = read_firmware_file("5b_AutoSweep.ino")

        self.assertIn(
            "latestPoint.v < scratch.rawPoints[pointsSaved - 1].v",
            source,
        )
        self.assertIn("pointsSaved--;", source)
        self.assertIn(
            "scratch.rawPoints[pointsSaved - 1] = latestPoint;",
            source,
        )


if __name__ == "__main__":
    unittest.main()
