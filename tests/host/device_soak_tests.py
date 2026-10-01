"""Fault injection for the acceptance harness; never opens a physical port."""
import ctypes
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("device_soak", ROOT / "scripts/device-soak.py")
soak = importlib.util.module_from_spec(spec)
spec.loader.exec_module(soak)


class Clock:
    now = 0

    def monotonic(self):
        return self.now


class Link:
    def __init__(self, clock, gap=False, restart=False):
        self.clock, self.gap, self.restart = clock, gap, restart
        self.buffer, self.statuses, self.closed = bytearray(), 0, False

    def open(self):
        pass

    def close(self):
        self.closed = True

    def flush(self):
        pass

    @property
    def in_waiting(self):
        return len(self.buffer)

    def write(self, data):
        if data == b"status\n":
            self.statuses += 1
            uptime = 500 if self.restart and self.statuses > 1 else 1000*self.statuses
            self.buffer.extend(("mode=4 wifi=1 tf=0 request=0 busy=0 games=1 heap=100 min=100\n"
                                f"firmware=0.1.4 reset_reason=1 uptime_ms={uptime}\n").encode())

    def read(self, count):
        self.clock.now += 6 if self.gap else .05
        data = bytes(self.buffer[:count])
        del self.buffer[:count]
        return data


class HarnessTests(unittest.TestCase):
    def run_case(self, gap=False, restart=False, save_error=False):
        clock = Clock()
        link = Link(clock, gap, restart)
        power_calls = []

        def execution_state(flags):
            power_calls.append(flags)
            return 0x80000000

        with tempfile.TemporaryDirectory() as directory:
            arguments = ["device-soak.py", "--hours", "0", "--cycles", "0",
                         "--firmware-sha256", "injected-test", "--output", directory]
            kernel = SimpleNamespace(SetThreadExecutionState=execution_state)
            with patch.object(sys, "argv", arguments), patch.object(soak, "time", clock), \
                    patch.object(soak.serial, "Serial", return_value=link), \
                    patch.object(ctypes, "windll", SimpleNamespace(kernel32=kernel), create=True), \
                    patch.object(soak.os, "name", "nt"):
                if save_error:
                    with patch.object(Path, "write_text", side_effect=OSError("injected save failure")):
                        with self.assertRaisesRegex(OSError, "injected save failure"):
                            soak.main()
                else:
                    message = "monitoring gap" if gap else "uptime decreased"
                    with self.assertRaisesRegex(RuntimeError, message):
                        soak.main()
            self.assertTrue(link.closed)
            self.assertEqual(power_calls, [0x80000001, 0x80000000])
            if not save_error:
                return json.loads((Path(directory)/"summary.json").read_text(encoding="utf-8"))

    def test_read_gap_cannot_pass(self):
        data = self.run_case(gap=True)
        self.assertFalse(data["complete"])
        self.assertFalse(data["passed"])

    def test_restart_preserves_new_boot_evidence(self):
        data = self.run_case(restart=True)
        self.assertFalse(data["passed"])
        self.assertEqual(data["previous_device_uptime_ms"], 1000)
        self.assertEqual(data["device_boot"]["last_uptime_ms"], 500)
        self.assertEqual(data["device_boot"]["reset_reason"], 1)

    def test_save_failure_still_closes_and_restores(self):
        self.run_case(save_error=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
