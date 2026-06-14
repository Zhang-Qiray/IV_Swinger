import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import types
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
PLOTTER_PATH = REPO_ROOT / "IV_Curve_Tracer_2026" / "tools" / "r4_sweep_plot.py"


def load_plotter():
    matplotlib = types.ModuleType("matplotlib")
    pyplot = types.ModuleType("matplotlib.pyplot")
    pyplot.show = lambda: None
    pyplot.close = lambda _fig: None
    pyplot.subplots = lambda *_args, **_kwargs: (_DummyFigure(), [_DummyAxes(), _DummyAxes(), _DummyAxes()])
    matplotlib.pyplot = pyplot
    sys.modules.setdefault("matplotlib", matplotlib)
    sys.modules.setdefault("matplotlib.pyplot", pyplot)

    serial = types.ModuleType("serial")
    serial.SerialException = RuntimeError
    serial.Serial = object
    serial_tools = types.ModuleType("serial.tools")
    serial_list_ports = types.ModuleType("serial.tools.list_ports")
    serial_list_ports.comports = lambda: []
    serial_tools.list_ports = serial_list_ports
    serial.tools = serial_tools
    sys.modules.setdefault("serial", serial)
    sys.modules.setdefault("serial.tools", serial_tools)
    sys.modules.setdefault("serial.tools.list_ports", serial_list_ports)

    spec = importlib.util.spec_from_file_location("r4_sweep_plot", PLOTTER_PATH)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class _DummyFigure:
    def suptitle(self, _title):
        return None

    def tight_layout(self):
        return None

    def savefig(self, *_args, **_kwargs):
        return None


class _DummyAxes:
    def plot(self, *_args, **_kwargs):
        return None

    def set_xlabel(self, _label):
        return None

    def set_ylabel(self, _label):
        return None

    def set_title(self, _title):
        return None

    def grid(self, *_args, **_kwargs):
        return None


class R4SweepPlotTests(unittest.TestCase):
    def test_summary_ignores_removed_debug_line_names(self):
        plotter = load_plotter()
        output = io.StringIO()

        lines = [
            "Voc = 40.7603 Isc = 3.6786 Points = 1 us/Point = 30.85",
            "SWEEP_CAPTURE old debug output",
            "SWEEP_ADC old debug output",
            "SWEEP_EDGE old debug output",
            "I = 3.7130 V = 0.3265",
        ]
        points = [plotter.SweepPoint(index=0, x=0.3265, y=3.7130)]

        with contextlib.redirect_stdout(output):
            plotter.print_summary(lines, points, "volts", "amps")

        text = output.getvalue()
        self.assertNotIn("SWEEP_CAPTURE", text)
        self.assertNotIn("SWEEP_ADC", text)
        self.assertNotIn("SWEEP_EDGE", text)

    def test_summary_uses_current_point_format_name_when_no_points(self):
        plotter = load_plotter()
        output = io.StringIO()

        with contextlib.redirect_stdout(output):
            plotter.print_summary([], [], "volts", "amps")

        text = output.getvalue()
        self.assertIn("no I = ... V = ... point lines found", text)
        self.assertNotIn("SWEEP_POINT", text)


if __name__ == "__main__":
    unittest.main()
