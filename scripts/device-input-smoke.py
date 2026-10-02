"""Actual board APP/UART input smoke. Injects input, never executes computer keys.

Physical GPIO contacts, encoder and TFT require separate wiring acceptance.
Reuses the real computer bridge Dispatcher and sends explicitly simulated ACKs.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import secrets
import time
import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", default="0.2.1")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    module_path = Path(__file__).with_name("pc_bridge.py")
    spec = importlib.util.spec_from_file_location("pc_bridge", module_path)
    bridge = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bridge)
    link = serial.Serial(port=None, baudrate=115200, timeout=0.05, write_timeout=1)
    link.dtr, link.rts, link.port = False, False, args.port
    log = bytearray()
    buffer = bytearray()
    actions, checks = [], []
    session = secrets.randbelow(0x7FFFFFFE) + 1
    heartbeat_enabled = False
    heartbeat_at = 0

    def send(command):
        link.write((command + "\n").encode("utf-8"))

    def acknowledge(active_session, sequence, success):
        send(f"pc_ack {active_session} {sequence} {2 if success else 0}")

    dispatcher = bridge.Dispatcher(session, lambda action, amount: actions.append((action, amount)), acknowledge)

    def collect(seconds):
        nonlocal heartbeat_at
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if heartbeat_enabled and time.monotonic() >= heartbeat_at:
                send(f"pc_link {session}")
                heartbeat_at = time.monotonic() + 0.8
            chunk = link.read(link.in_waiting or 1)
            log.extend(chunk)
            for byte in chunk:
                if byte == 10:
                    dispatcher.receive(bytes(buffer).rstrip(b"\r"))
                    buffer.clear()
                elif len(buffer) < 8192:
                    buffer.append(byte)

    def command(text, seconds=0.18):
        begin = len(log)
        send(text)
        collect(seconds)
        return log[begin:].decode("utf-8", errors="replace").replace("\r", "")

    def check(condition, name):
        checks.append(dict(name=name, passed=bool(condition)))
        if not condition: raise RuntimeError(name)

    def tap(name):
        command(f"key {name} 1")
        command(f"key {name} 0")

    report = dict(port=args.port, firmware=args.version, transport="UART injected inputs + simulated PC ACK",
                  physical_gpio_tested=False, physical_tft_tested=False, computer_os_keys_tested=False,
                  tf_persistence_tested=False)
    try:
        link.open()
        status = command("status", 0.5)
        check(f"firmware={args.version} " in status, "actual flashed firmware version")
        command("page inputs")
        names = ["up", "down", "left", "right", "confirm", "back", "function", "mode", "encoder"]
        for i, name in enumerate(names):
            command("page inputs")
            command(f"key {name} 1")
            state = command("input_status")
            mask = re.search(r"input_mask=0x([0-9a-f]+)", state)
            check(mask and int(mask[1], 16) == 1 << i, f"{name} press mask")
            command(f"key {name} 0")
            check("input_mask=0x000" in command("input_status"), f"{name} release mask")
        command("encoder 3")
        command("encoder -2")
        check("encoder_total=1 " in command("input_status"), "rotary net count and direction")
        command("page display")
        check("mode=9 " in command("status"), "display calibration page")
        tap("back")
        check("mode=5 " in command("status"), "display back returns settings")
        command("page home")
        heartbeat_enabled = True
        collect(0.25)
        command("page pc")
        for name in names[:6]: tap(name)
        command("encoder 2")
        command("encoder -2")
        tap("encoder")
        tap("function")
        for name in names[:6]: tap(name)
        expected = [(action, 1) for action in ("volume_up", "volume_down", "previous", "next", "play_pause", "mute")]
        expected += [("volume_up", 2), ("volume_down", 2), ("mute", 1)]
        expected += [(action, 1) for action in ("copy", "paste", "undo", "redo", "save", "tab")]
        check(actions == expected, "all media/shortcut/rotary mappings use actual firmware protocol")
        check("pc_result=电脑模拟确认" in command("input_status"), "dry-run acknowledgement distinguished")
        heartbeat_enabled = False
        collect(3.3)
        tap("up")
        check(len(actions) == len(expected), "lease expiry emits no old/offline PC action")
        check("pc_connected=0 " in command("input_status"), "expired bridge is offline")
        tap("mode")
        check("mode=0 " in command("status"), "mode key returns to prior device page")
        check("mosi: OK" in command("preview_demo", 0.6), "reference Snake preview created without TF")
        command("page games")
        check("games=1 " in command("status"), "preview appears in game library")
        tap("confirm")
        check("mode=4 " in command("status"), "preview launches from library")
        tap("down")
        tap("right")
        collect(12)
        check("MOSI_FRAME_END" in command("frame", 8), "actual game canvas exported completely")
        tap("back")
        check("mode=3 " in command("status"), "game exits to library")
        text = log.decode("utf-8", errors="replace").replace("\r", "")
        check(not re.search(r"Guru Meditation|panic'ed|Task watchdog got triggered|MosiCrokey v.*Send help", text), "no captured crash or console restart during smoke")
        report["passed"] = True
    except Exception as error:
        report["passed"] = False
        report["error"] = str(error)
    finally:
        link.close()
        (args.output / "serial.txt").write_text(log.decode("utf-8", errors="replace").replace("\r", ""), encoding="utf-8")
        report["checks"], report["pc_actions"] = checks, actions
        (args.output / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report.get("passed") else 1


if __name__ == "__main__":
    raise SystemExit(main())
