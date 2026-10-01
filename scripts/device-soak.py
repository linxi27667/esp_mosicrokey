"""Finite hardware soak: real game VM, navigation and background free-model calls.

Start after an Agent-created USB preview exists. Does not format or write TF.
Credentials are accepted only for log redaction via the ephemeral config env.
"""
import argparse
import codecs
import json
import os
from pathlib import Path
import re
import statistics
import time
import serial


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--hours", type=float, default=2)
    parser.add_argument("--cycles", type=int, default=100)
    parser.add_argument("--output", required=True)
    parser.add_argument("--firmware-sha256", required=True)
    args = parser.parse_args()
    config = json.loads(os.environ.pop("MOSICROKEY_DEVICE_CONFIG", "{}"))
    secrets = [config.get(name, "") for name in ("password", "key")]
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    summary = {"firmware_app_sha256": args.firmware_sha256, "port": args.port,
               "requested_hours": args.hours, "requested_game_cycles": args.cycles,
               "game_cycles_completed": 0, "navigation_commands_sent": 0,
               "agent_calls_submitted": 0, "agent_completions": [], "samples": [], "fps": [], "faults": [],
               "agent_request_baseline": None, "agent_requests_accepted": [],
               "memory_samples": [], "stack_min_bytes": {},
               "scope": "USB temporary Lua preview; TF and physical display NOT TESTED",
               "complete": False}
    link = serial.Serial()
    link.port, link.baudrate, link.timeout = args.port, 115200, 0.1
    link.dtr = link.rts = False
    link.open()
    busy, mode = False, -1
    pending = ""
    awaiting_request = False
    power_state = None
    last_uptime = None
    last_poll = time.monotonic()
    decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")

    def clean(text):
        for secret in secrets:
            if secret:
                text = text.replace(secret, "[REDACTED]")
        return re.sub(r"(?i)(password|api.?key|authorization)(\s*[:=]\s*)[^\r\n]+", r"\1\2[REDACTED]", text)

    def save():
        summary["elapsed_seconds"] = round(time.monotonic() - started, 2)
        (output / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    with (output / "serial.log").open("w", encoding="utf-8", buffering=1) as log:
        def check_monitor_gap():
            nonlocal last_poll
            now = time.monotonic()
            if now-last_poll > 5:
                raise RuntimeError(f"Host monitoring gap of {now-last_poll:.1f}s; continuous test interrupted")
            last_poll = now

        def collect(seconds):
            nonlocal pending, busy, mode, awaiting_request, last_uptime
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                check_monitor_gap()
                data = link.read(link.in_waiting or 1)
                check_monitor_gap()
                pending += decoder.decode(data)
                while "\n" in pending:
                    line, pending = pending.split("\n", 1)
                    line = clean(line.replace("\r", ""))
                    log.write(line + "\n")
                    boot = re.search(r"firmware=(\S+) reset_reason=(\d+) uptime_ms=(\d+)", line)
                    if boot:
                        uptime = int(boot[3])
                        summary["device_boot"] = {"version": boot[1], "reset_reason": int(boot[2]), "last_uptime_ms": uptime}
                        if last_uptime is not None and uptime < last_uptime:
                            summary["previous_device_uptime_ms"] = last_uptime
                            raise RuntimeError("Device uptime decreased; board restarted during test")
                        last_uptime = uptime
                    status = re.search(r"mode=(\d+) wifi=(\d+) tf=(\d+) request=(\d+) busy=(\d+) games=(\d+) heap=(\d+) min=(\d+)", line)
                    if status:
                        values = dict(zip(["mode", "wifi", "tf", "request", "busy", "games", "heap", "minimum_heap"], map(int, status.groups())))
                        mode, busy = values["mode"], bool(values["busy"])
                        values["seconds"] = round(time.monotonic() - started, 2)
                        summary["samples"].append(values)
                    progress = re.search(r"mosi_agent: request=(\d+) busy=(\d)", line)
                    if progress:
                        busy = bool(int(progress[2]))
                        if awaiting_request and busy and int(progress[1]) > summary["agent_request_baseline"]:
                            summary["agent_requests_accepted"].append(int(progress[1]))
                            awaiting_request = False
                    completion = re.search(r"mosi_agent: request=(\d+) completed status=(\d+)", line)
                    if completion:
                        summary["agent_completions"].append({"request": int(completion[1]), "status": int(completion[2]), "seconds": round(time.monotonic()-started, 2)})
                    memory = re.search(r"heap_internal=(\d+) min_internal=(\d+) psram=(\d+) min_psram=(\d+)", line)
                    if memory:
                        values = dict(zip(["internal", "minimum_internal", "psram", "minimum_psram"], map(int, memory.groups())))
                        values["seconds"] = round(time.monotonic()-started, 2)
                        values["agent_busy"] = busy
                        summary["memory_samples"].append(values)
                    stack = re.search(r"task=(\S+).*stack_free_min_bytes=(\d+)", line)
                    if stack:
                        name, free = stack[1], int(stack[2])
                        summary["stack_min_bytes"][name] = min(free, summary["stack_min_bytes"].get(name, free))
                    fps = re.search(r"mosi_game: fps=([0-9.]+)", line)
                    if fps:
                        summary["fps"].append({"seconds": round(time.monotonic()-started, 2), "value": float(fps.group(1))})
                    if re.search(r"Guru Meditation|panic'ed|Stack smashing|task_wdt.*trigger|ESP-ROM:|^rst:|abort\(\)", line):
                        summary["faults"].append(line)

        def send(command, wait=0.2):
            previous_samples = len(summary["samples"])
            log.write(f"HOST t={time.monotonic()-started:.3f} command={command.split(' ', 1)[0]}\n")
            link.write((command + "\n").encode("utf-8"))
            link.flush()
            collect(wait)
            if command == "status":
                deadline = time.monotonic()+5
                while len(summary["samples"]) == previous_samples and time.monotonic()<deadline:
                    collect(0.1)
                if len(summary["samples"]) == previous_samples:
                    raise RuntimeError("No fresh status response from device")

        try:
            if os.name == "nt":
                import ctypes
                keep_awake = ctypes.windll.kernel32.SetThreadExecutionState
                keep_awake.argtypes = [ctypes.c_uint32]
                keep_awake.restype = ctypes.c_uint32
                power_state = keep_awake(0x80000001)  # This thread: prevent idle sleep, allow display off.
                if not power_state:
                    raise OSError("Cannot prevent host idle sleep during serial test")
                summary["host_idle_sleep_prevented"] = True
            send("page games")
            send("key confirm")
            send("status", 0.5)
            if mode != 4:
                raise RuntimeError("Start requires a valid Agent-created preview in game library")
            summary["agent_request_baseline"] = summary["samples"][-1]["request"]
            for index in range(args.cycles):
                send("key back")
                send("page home")
                send("page voice")
                send("page home")
                send("page games")
                send("key confirm")
                send("status", 0.15)
                if mode != 4:
                    raise RuntimeError(f"Game launch failed at cycle {index+1}")
                summary["game_cycles_completed"] += 1
                summary["navigation_commands_sent"] += 6
                if index % 10 == 9:
                    save()
                    print(f"Hardware load/exit cycles {index+1}/{args.cycles}", flush=True)
            soak_started = time.monotonic()
            summary["continuous_game_start_seconds"] = round(soak_started-started, 2)
            deadline = soak_started + args.hours * 3600
            next_status, next_agent = soak_started, soak_started + 20
            while time.monotonic() < deadline:
                collect(1)
                now = time.monotonic()
                if now >= next_status:
                    send("status", 0.5)
                    if mode != 4:
                        raise RuntimeError("Game left foreground during continuous soak")
                    save()
                    print(f"Soak {now-soak_started:.0f}/{args.hours*3600:.0f}s; busy={int(busy)}", flush=True)
                    next_status = now + 60
                if now >= next_agent and not busy:
                    awaiting_request = True
                    summary["agent_calls_submitted"] += 1
                    send("ask 不调用工具，请只回复：板载稳定性检查通过。")
                    next_agent = now + 300
            send("audio_status")
            send("status", 0.5)
            summary["continuous_game_seconds"] = round(time.monotonic()-soak_started, 2)
            summary["complete"] = True
            # Exclude startup and navigation windows from sustained-frame assessment.
            steady = [sample["value"] for sample in summary["fps"] if sample["seconds"] > summary["continuous_game_start_seconds"]+20]
            summary["steady_fps_min"] = min(steady) if steady else None
            owned = {item["request"]: item for item in summary["agent_completions"] if item["request"] in summary["agent_requests_accepted"]}
            summary["agent_successes"] = sum(item["status"] == 0 for item in owned.values())
            summary["agent_errors"] = sum(item["status"] != 0 for item in owned.values())
            summary["agent_unfinished"] = summary["agent_calls_submitted"]-len(owned)
            idle = [item for item in summary["memory_samples"] if not item["agent_busy"] and item["seconds"] > summary["continuous_game_start_seconds"]+20]
            # TLS/activation allocate transient buffers. Report comparable idle
            # quarter medians for review; a single low-water mark is not a leak.
            if len(idle) >= 8:
                count = max(2, len(idle)//4)
                summary["idle_heap_trend"] = {name: {"first_quarter_median": statistics.median(item[name] for item in idle[:count]), "last_quarter_median": statistics.median(item[name] for item in idle[-count:])} for name in ["internal", "psram"]}
            summary["passed_scope"] = "Game runtime/navigation/background API recovery; excludes physical peripherals, Xiaozhi voice and memory trend review"
            summary["passed"] = not summary["faults"] and bool(steady) and min(steady)>=20 and summary["game_cycles_completed"]==args.cycles and summary["agent_successes"]>0 and summary["agent_unfinished"]==0
        except Exception as error:
            summary["error"] = clean(str(error))
            summary["passed"] = False
            raise
        finally:
            try:
                save()
            finally:
                try:
                    link.close()
                finally:
                    if power_state:
                        keep_awake(power_state)
    print(json.dumps({name: summary.get(name) for name in ["complete", "passed", "continuous_game_seconds", "game_cycles_completed", "agent_calls_submitted", "error"]}, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    main()
