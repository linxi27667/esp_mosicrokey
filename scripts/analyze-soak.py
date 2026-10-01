"""Summarize measured USB soak data without certifying untested peripherals."""
import argparse
import csv
import json
from pathlib import Path
import statistics


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("summary")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    data = json.loads(Path(args.summary).read_text(encoding="utf-8-sig"))
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    start = data.get("continuous_game_start_seconds")
    if start is None:
        raise ValueError("Continuous soak has not started")
    fps = [item["value"] for item in data["fps"] if item["seconds"] > start + 20]
    idle = [item for item in data["memory_samples"] if not item["agent_busy"] and item["seconds"] > start + 20]
    owned = {item["request"]: item for item in data["agent_completions"] if item["request"] in data.get("agent_requests_accepted", [])}
    report = {
        "firmware_app_sha256": data["firmware_app_sha256"],
        "complete": data["complete"],
        "runtime_passed": data.get("passed", False),
        "scope": data["scope"],
        "continuous_game_seconds": data.get("continuous_game_seconds", round(data["elapsed_seconds"] - start, 2)),
        "game_cycles_completed": data["game_cycles_completed"],
        "navigation_commands_sent": data.get("navigation_commands_sent", data.get("navigation_transitions")),
        "steady_fps_min": min(fps) if fps else None,
        "steady_fps_mean": round(statistics.mean(fps), 3) if fps else None,
        "agent_calls_submitted": data["agent_calls_submitted"],
        "agent_successes": sum(item["status"] == 0 for item in owned.values()),
        "agent_errors": sum(item["status"] != 0 for item in owned.values()),
        "agent_unfinished": data["agent_calls_submitted"]-len(owned),
        "faults": data["faults"],
        "stack_min_free_bytes": data["stack_min_bytes"],
        "memory_note": "Idle samples exclude Agent HTTP, but Xiaozhi activation/TLS still allocates transient buffers; review quarter medians before a leak conclusion.",
    }
    if len(idle) >= 8:
        report["idle_memory"] = {}
        for name in ["internal", "psram"]:
            quarters = [idle[len(idle)*index//4:len(idle)*(index+1)//4] for index in range(4)]
            medians = [statistics.median(item[name] for item in group) for group in quarters]
            report["idle_memory"][name] = {
                "quarter_medians_bytes": medians,
                "last_minus_first_bytes": medians[-1] - medians[0],
                "minimum_idle_bytes": min(item[name] for item in idle),
                "minimum_observed_bytes": min(item["minimum_" + name] for item in data["memory_samples"]),
            }
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for name, fields in [
        ("fps", ["seconds", "value"]),
        ("memory_samples", ["seconds", "internal", "minimum_internal", "psram", "minimum_psram", "agent_busy"]),
        ("agent_completions", ["seconds", "request", "status"]),
    ]:
        with (output / (name + ".csv")).open("w", newline="", encoding="utf-8") as file:
            writer = csv.DictWriter(file, fieldnames=fields)
            writer.writeheader()
            writer.writerows({field: item[field] for field in fields} for item in data[name])
    print(json.dumps({key: report[key] for key in ["complete", "continuous_game_seconds", "steady_fps_min", "agent_successes", "agent_errors"]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
