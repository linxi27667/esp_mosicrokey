"""Serial smoke-test helper. Credentials come only from an ephemeral environment.

MOSICROKEY_DEVICE_CONFIG is JSON containing ssid/password/url/model/key. It is
never saved. UART input is not echoed; output and log files redact all secrets.
Use the verified ESP-IDF Python runtime (pyserial is already installed).
"""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import time
import urllib.error
import urllib.request


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["capture", "command", "wifi", "agent", "host-api"])
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--seconds", type=float, default=15)
    parser.add_argument("--reset", action="store_true")
    parser.add_argument("--command", action="append", default=[])
    parser.add_argument("--command-delay", type=float, default=0.15)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    config = json.loads(os.environ.pop("MOSICROKEY_DEVICE_CONFIG", "{}"))
    secrets = [config.get(name, "") for name in ("password", "key")]
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    def clean(text):
        for secret in secrets:
            if secret:
                text = text.replace(secret, "[REDACTED]")
        return re.sub(r"(?i)(password|api.?key|authorization)(\s*[:=]\s*)[^\r\n]+", r"\1\2[REDACTED]", text)

    if args.action == "host-api":
        body = {"model": config["model"], "messages": [{"role": "user", "content": "请只回复：MosiCrokey连接成功"}], "max_tokens": 128, "stream": False}
        request = urllib.request.Request(config["url"].rstrip("/") + "/chat/completions", data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "Authorization": "Bearer " + config["key"]})
        start = time.monotonic()
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                result = {"http_status": response.status, "seconds": round(time.monotonic()-start, 3), "body": json.loads(response.read())}
        except urllib.error.HTTPError as error:
            result = {"http_status": error.code, "body": error.read().decode("utf-8", errors="replace")}
        except Exception as error:
            result = {"error": str(error)}
        text = clean(json.dumps(result, ensure_ascii=False, indent=2))
        output.write_text(text, encoding="utf-8")
        print(text)
        return 0 if result.get("http_status") == 200 else 1

    import serial
    link = serial.Serial()
    link.port = args.port
    link.baudrate = 115200
    link.timeout = 0.1
    link.dtr = False
    link.rts = False
    link.open()
    try:
        if args.reset:
            link.rts = True
            time.sleep(0.1)
            link.rts = False
        commands = args.command
        if args.action == "wifi":
            commands = ["wifi_config " + json.dumps({name: config[name] for name in ("ssid", "password")})]
        elif args.action == "agent":
            commands = ["agent_config " + json.dumps({name: config[name] for name in ("url", "model", "key")})]
        data = bytearray()
        def collect(seconds):
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                data.extend(link.read(link.in_waiting or 1))

        # Drain while sending, including during delayed input replay; never echo input.
        for command in commands:
            link.write((command + "\n").encode("utf-8"))
            link.flush()
            collect(args.command_delay)
        collect(args.seconds)
        text = clean(data.decode("utf-8", errors="replace").replace("\r", ""))
        output.write_text(text, encoding="utf-8")
        print(text)
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
