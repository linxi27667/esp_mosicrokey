"""Validate actual bridge parser, replay guard and Win32 keyboard ABI without OS actions."""
import ctypes
import importlib.util
import json
from pathlib import Path
import sys
import types
import io
from unittest.mock import patch
import unittest

source = Path(__file__).resolve().parents[2] / "scripts" / "pc_bridge.py"
spec = importlib.util.spec_from_file_location("pc_bridge", source)
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)


def frame(**changes):
    data = dict(v=1, session=123, seq=1, action="copy", amount=1)
    data.update(changes)
    return b"MOSI_PC " + json.dumps(data).encode()


class BridgeTests(unittest.TestCase):
    def run_main_transport(self, scenario):
        class Clock:
            now = 0
            def monotonic(self): return self.now
        clock = Clock()
        class Port:
            session = 0
            old_session = 0
            reads = 0
            resets = 0
            closed = False
            acknowledgements = []
            def open(self): pass
            def close(self): self.closed = True
            def reset_input_buffer(self): self.resets += 1
            def write(self, data):
                parts = data.decode().split()
                if parts[0] == "pc_link": self.session = int(parts[1])
                if parts[0] == "pc_ack": self.acknowledgements.append(tuple(map(int, parts[1:])))
            def read(self, _):
                self.reads += 1
                if self.reads == 1:
                    self.old_session = self.session
                    if scenario in ("suspend", "suspend_full_frame"):
                        clock.now = 5
                        return frame(session=self.session, seq=8)+b"\n" if scenario=="suspend_full_frame" else b"MOSI_PC {"
                    clock.now = 0.1
                    return frame(session=self.session) + b"\nMosiCrokey v0.2.1. Send help; UART 115200\n" + frame(session=self.session) + b"\n"
                if self.reads == 2:
                    clock.now += 0.1
                    old = frame(session=self.old_session, seq=9) + b"\n" if scenario in ("suspend", "suspend_full_frame") else b""
                    return old + frame(session=self.session) + b"\n"
                clock.now += 0.5
                return b""
        port = Port()
        fake_serial = types.SimpleNamespace(Serial=lambda **_: port)
        with patch.dict(sys.modules, serial=fake_serial), patch.object(sys, "argv", ["pc_bridge", "--port", "TEST", "--dry-run", "--seconds", "6"]), patch.object(bridge.time, "monotonic", clock.monotonic), patch.object(bridge.secrets, "randbelow", return_value=0), patch("sys.stdout", new_callable=io.StringIO):
            self.assertEqual(bridge.main(), 0)
        self.assertTrue(port.closed)
        self.assertNotEqual(port.session, port.old_session)
        return port

    def test_suspended_host_replaces_session_before_reading_old_frames(self):
        port = self.run_main_transport("suspend")
        self.assertEqual(port.resets, 1)
        self.assertEqual(port.acknowledgements, [(port.session, 1, 2)])

    def test_suspension_inside_read_discards_complete_old_action(self):
        port = self.run_main_transport("suspend_full_frame")
        self.assertEqual(port.resets, 1)
        self.assertEqual(port.acknowledgements, [(port.session, 1, 2)])

    def test_device_reboot_replaces_session_and_accepts_new_sequence_one(self):
        port = self.run_main_transport("reboot")
        self.assertEqual(port.acknowledgements, [(port.old_session, 1, 2), (port.session, 1, 2)])

    def test_all_fixed_actions(self):
        for action in bridge.ACTIONS:
            self.assertEqual(bridge.parse_action(frame(action=action), 123)["action"], action)

    def test_noise_and_invalid_json(self):
        for line in (b"boot log", b"MOSI_PC {", b"MOSI_PC []", b"MOSI_PC \xff", b"MOSI_PC " + b"x" * 513):
            self.assertIsNone(bridge.parse_action(line, 123))

    def test_protocol_bounds(self):
        for changes in (dict(v=2), dict(session=12), dict(seq=0), dict(seq=0x100000000),
                        dict(amount=0), dict(amount=21), dict(amount=True), dict(seq=True),
                        dict(action="shell"), dict(extra="not allowed"), dict(action=[])):
            self.assertIsNone(bridge.parse_action(frame(**changes), 123))

    def test_replay_guard_and_ack(self):
        calls, acknowledgements = [], []
        dispatcher = bridge.Dispatcher(123, lambda *args: calls.append(args), lambda *args: acknowledgements.append(args))
        self.assertTrue(dispatcher.receive(frame()))
        self.assertFalse(dispatcher.receive(frame()))
        self.assertTrue(dispatcher.receive(frame(seq=3)))
        self.assertFalse(dispatcher.receive(frame(seq=2)))
        self.assertEqual(calls, [("copy", 1), ("copy", 1)])
        self.assertEqual(acknowledgements, [(123, 1, True), (123, 3, True)])

    def test_failed_action_is_not_replayed(self):
        acknowledgements = []
        def fail(*_): raise RuntimeError("injected rejection")
        dispatcher = bridge.Dispatcher(123, fail, lambda *args: acknowledgements.append(args))
        self.assertTrue(dispatcher.receive(frame()))
        self.assertFalse(dispatcher.receive(frame()))
        self.assertEqual(acknowledgements, [(123, 1, False)])

    def test_chord_releases_reverse_order(self):
        events = bridge.keyboard_events((0x11, 0x43), 1)
        self.assertEqual([(e.data.ki.wVk, e.data.ki.dwFlags) for e in events], [(0x11, 0), (0x43, 0), (0x43, 2), (0x11, 2)])

    def test_volume_amount_and_abi(self):
        calls = []
        def sender(count, events, size):
            calls.append((count, size, [(e.data.ki.wVk, e.data.ki.dwFlags) for e in events]))
            return count
        bridge.WindowsKeys(sender)("volume_up", 20)
        self.assertEqual(calls[0][0], 40)
        self.assertTrue(all(vk == 0xAF for vk, _ in calls[0][2]))
        if sys.platform == "win32":
            self.assertEqual(ctypes.sizeof(bridge.INPUT), 40 if ctypes.sizeof(ctypes.c_void_p) == 8 else 28)

    def test_partial_send_releases_modifiers(self):
        calls = []
        def sender(count, events, size):
            calls.append([(e.data.ki.wVk, e.data.ki.dwFlags) for e in events])
            return 1 if len(calls) == 1 else count
        with self.assertRaises(RuntimeError): bridge.WindowsKeys(sender)("save", 1)
        self.assertEqual(calls[1], [(0x53, 2), (0x11, 2)])


if __name__ == "__main__":
    unittest.main()
