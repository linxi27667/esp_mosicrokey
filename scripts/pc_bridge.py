"""MosiCrokey UART -> Windows media/shortcut keys. No arbitrary commands/macros.

Run: python scripts/pc_bridge.py --port COM8
Use --dry-run to validate transport without changing the computer.
Close the bridge before flashing: one process owns the UART at a time.
"""
import argparse
import ctypes
from ctypes import wintypes
import json
import secrets
import sys
import time

ACTIONS = {
    "volume_up": (0xAF,), "volume_down": (0xAE,), "mute": (0xAD,),
    "previous": (0xB1,), "next": (0xB0,), "play_pause": (0xB3,),
    "copy": (0x11, 0x43), "paste": (0x11, 0x56), "undo": (0x11, 0x5A),
    "redo": (0x11, 0x59), "save": (0x11, 0x53), "tab": (0x09,),
}


class KEYBOARDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wintypes.WORD), ("wScan", wintypes.WORD),
                ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD),
                ("dwExtraInfo", ctypes.c_size_t)]


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [("dx", wintypes.LONG), ("dy", wintypes.LONG),
                ("mouseData", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
                ("time", wintypes.DWORD), ("dwExtraInfo", ctypes.c_size_t)]


class HARDWAREINPUT(ctypes.Structure):
    _fields_ = [("uMsg", wintypes.DWORD), ("wParamL", wintypes.WORD), ("wParamH", wintypes.WORD)]


class INPUTDATA(ctypes.Union):
    _fields_ = [("ki", KEYBOARDINPUT), ("mi", MOUSEINPUT), ("hi", HARDWAREINPUT)]


class INPUT(ctypes.Structure):
    _fields_ = [("type", wintypes.DWORD), ("data", INPUTDATA)]


def keyboard_events(keys, amount):
    events = []
    for _ in range(amount):
        for key in keys:
            event = INPUT(type=1)
            event.data.ki.wVk = key
            events.append(event)
        for key in reversed(keys):
            event = INPUT(type=1)
            event.data.ki.wVk = key
            event.data.ki.dwFlags = 2  # KEYEVENTF_KEYUP
            events.append(event)
    return (INPUT * len(events))(*events)


class WindowsKeys:
    def __init__(self, sender=None):
        if sender is None:
            if sys.platform != "win32":
                raise RuntimeError("Real computer key control currently supports Windows")
            sender = ctypes.WinDLL("user32", use_last_error=True).SendInput
            sender.argtypes = [wintypes.UINT, ctypes.POINTER(INPUT), ctypes.c_int]
            sender.restype = wintypes.UINT
        self.sender = sender

    def __call__(self, action, amount):
        events = keyboard_events(ACTIONS[action], amount)
        if self.sender(len(events), events, ctypes.sizeof(INPUT)) != len(events):
            # A partial SendInput may leave a modifier down. Release, never replay the action.
            release = keyboard_events(ACTIONS[action], 1)
            keyups = (INPUT * len(ACTIONS[action]))(*release[len(ACTIONS[action]):])
            self.sender(len(keyups), keyups, ctypes.sizeof(INPUT))
            raise RuntimeError("Windows rejected keyboard input (check target privilege level)")


def parse_action(line, session):
    if len(line) > 512 or not line.startswith(b"MOSI_PC "):
        return None
    try:
        data = json.loads(line[8:])
    except (ValueError, UnicodeDecodeError):
        return None
    if not isinstance(data, dict) or set(data) != {"v", "session", "seq", "action", "amount"}:
        return None
    if any(type(data[name]) is not int for name in ("v", "session", "seq", "amount")):
        return None
    if data["v"] != 1 or data["session"] != session or not 1 <= data["seq"] <= 0xFFFFFFFF:
        return None
    if type(data["action"]) is not str or data["action"] not in ACTIONS or not 1 <= data["amount"] <= 20:
        return None
    return data


class Dispatcher:
    def __init__(self, session, execute, acknowledge):
        self.session, self.execute, self.acknowledge = session, execute, acknowledge
        self.last_sequence = 0

    def receive(self, line):
        data = parse_action(line, self.session)
        if data is None or data["seq"] <= self.last_sequence:
            return False
        self.last_sequence = data["seq"]
        success = True
        try:
            self.execute(data["action"], data["amount"])
        except (OSError, RuntimeError) as error:
            success = False
            print(f"Computer action failed: {error}", file=sys.stderr)
        self.acknowledge(self.session, data["seq"], success)
        return True


def new_dispatcher(previous_session, execute, acknowledge):
    # Always change the token, even if the random generator repeats a value.
    delta = secrets.randbelow(0x7FFFFFFE) + 1
    session = (previous_session + delta - 1) % 0x7FFFFFFF + 1
    return Dispatcher(session, execute, acknowledge)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--seconds", type=float, default=0, help="0: run until Ctrl+C")
    args = parser.parse_args()
    import serial
    execute = (lambda action, amount: print(f"DRY_RUN {action} x{amount}", flush=True)) if args.dry_run else WindowsKeys()
    port = serial.Serial(port=None, baudrate=115200, timeout=0.1, write_timeout=1)
    port.dtr = False
    port.rts = False
    port.port = args.port
    try:
        port.open()
        def acknowledge(active_session, sequence, success):
            status = 2 if success and args.dry_run else int(success)
            port.write(f"pc_ack {active_session} {sequence} {status}\n".encode("ascii"))
            print(f"ACK seq={sequence} status={status}", flush=True)
        dispatcher = new_dispatcher(0, execute, acknowledge)
        buffer = bytearray()
        dropping = False
        start, heartbeat = time.monotonic(), 0
        last_loop = start
        print(f"Bridge {args.port}: {'dry run' if args.dry_run else 'Windows keys'}; K8 selects computer mode", flush=True)
        while not args.seconds or time.monotonic() - start < args.seconds:
            now = time.monotonic()
            if now - last_loop >= 3:
                # Suspended host: reject buffered commands under the previous lease.
                dispatcher = new_dispatcher(dispatcher.session, execute, acknowledge)
                buffer.clear()
                dropping, heartbeat = False, 0
                port.reset_input_buffer()
            last_loop = now
            if now >= heartbeat:
                port.write(f"pc_link {dispatcher.session}\n".encode("ascii"))
                heartbeat = now + 1
            chunk = port.read(1024)
            if time.monotonic() - last_loop >= 3:
                # The process can also be suspended inside read(): drop that complete old frame.
                dispatcher = new_dispatcher(dispatcher.session, execute, acknowledge)
                buffer.clear()
                dropping, heartbeat = False, 0
                port.reset_input_buffer()
                last_loop = time.monotonic()
                continue
            for byte in chunk:
                if byte == 10:
                    if not dropping:
                        line = bytes(buffer).rstrip(b"\r")
                        if line.startswith(b"MosiCrokey v") and b". Send help;" in line:
                            # Explicit device boot record, never a mere sequence decrease.
                            dispatcher = new_dispatcher(dispatcher.session, execute, acknowledge)
                            heartbeat = 0
                        else:
                            dispatcher.receive(line)
                    buffer.clear()
                    dropping = False
                elif not dropping:
                    if len(buffer) >= 4096:
                        buffer.clear()
                        dropping = True
                    else:
                        buffer.append(byte)
    except KeyboardInterrupt:
        pass
    finally:
        port.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
