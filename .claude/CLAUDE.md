# MosiCrokey

ESP-IDF v6.1 / ESP32-S31-Function-CoreBoard-1. Build with scripts/build.ps1.
Project business uses ports and adapters; shared contracts have no ESP-IDF or vendor types.
Xiaozhi and ESPClaw sources are pinned in upstream-lock.json. Do not update silently.
One app_main, one Board factory, one Wi-Fi owner, one LVGL owner.
Lua games run only through restricted game APIs. TF errors never trigger formatting.
Never claim real hardware validation based on a build or host simulation.
See docs/development-guide.md and docs/verification-plan.md.

v0.1.0 software validation: 1035 host checks, 12 shared LVGL captures, S31 build and independent rebuild passed.
FreeRTOS tick is 1000 Hz; never allow 5 ms delays to round to zero ticks.
Do not expose Lua metatables, bytecode, filesystem/network or uninterruptible native loops.
Suppress raw compatible-API error logs and redact configured credentials before displaying results.
TF remount is serialized against request submission and sample installation.
Release and dated evidence: release/manifest.json and docs/validation/2026-10-01/README.md.
2026-10-01 hardware: COM8 confirmed ESP32-S31 rev0.0, 16 MiB Flash/PSRAM; Wi-Fi and real Zhipu calls passed. GLM-4.7-Flash generated Snake through the board's actual ESPClaw tool loop.
USB RAM previews are temporary, not TF installation; lost on reboot. Never mark physical TFT/keys/TF or complete Xiaozhi voice as passed without evidence. Xiaozhi account activation remains pending.
v0.1.9 host validation: 1178 checks (632 game, 448 App, 50 actual OpenAI backend retry and 48 actual HTTP transport fault-injection), plus 13 shared LVGL captures and 32 behavior checks on exported device Snake V3 source. Latest hardware evidence and soak state: docs/validation/hardware-2026-10-02. Previous interrupted runs remain in hardware-2026-10-01; never combine their durations with a fresh run.
Game task runs on CPU1 at priority 3; UI, Agent and TCP/IP use CPU0. Do not relax Lua timing limits to hide network preemption failures.
2026-10-02 v0.1.9 independent USB soak completed: 7201.29 seconds, 100 load/exit cycles, 600 navigation commands, minimum/mean 30.3 FPS, no captured panic/reset/WDT. Background calls: 21 success and 3 offline connection errors, followed by 4 automatic-recovery successes. Do not label every API request successful or certify unconnected peripherals/voice. Post-soak p1 source is byte-identical and relaunch passed.
Only explicit HTTP 429/503 are retried, at most twice within the original per-iteration timeout; cancellation interrupts backoff and late HTTP 200 is discarded. A successful tool publication must remain visible if the final model summary fails.
Build and packaging verify patches offline by reverse application and the six pinned upstream file hashes in patches/provenance.json.
Export actual device Lua before gameplay verification. Syntax/probe validation alone does not prove Snake scoring or restart behavior.

v0.1.5 gameask requires actual publication, never treat model text as a tool result. Creator initial context contains the current USER only; native tool-loop results remain real and ordinary chat history is preserved. Current shared UI captures cover 13 states.
