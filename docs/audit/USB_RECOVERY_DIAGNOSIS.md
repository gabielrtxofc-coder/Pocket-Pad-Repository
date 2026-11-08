# USB-first automatic connection and recovery diagnosis

**Scope:** source inspection and implementation record for Mission “Pocket Pad UX Overhaul V1.” The findings below describe the checked-in baseline at `552b925`; the implementation status describes the current V1 work. This is not a hardware-test report: no Galaxy A12, Windows desktop, or live ADB device was connected during this audit.

## Executive summary

The baseline treated `adb reverse` as a manual on/off toggle rather than a monitored USB transport. It launched an unscoped shell command, could run `adb kill-server` when closing the app, had no device-state monitor or serial selection, and blocked the Windows startup path. Android sampled USB once and had no retry owner. TCP/UDP listeners were wildcard-bound despite the unauthenticated input protocol.

V1 replaces that flow with an asynchronous ADB monitor and explicit serial selection, preserving Android's normal RSA authorization. The Windows TCP/UDP listeners now bind to loopback, and Android Wi-Fi server mode remains disabled until authenticated pairing exists. The application does not call `adb kill-server`, `reverse --remove-all`, or an unscoped reverse command. Android owns one retrying USB connection and reports waiting, connecting, retrying, and connected status without disconnect popups. Regression tests and CI checks were added; live-device validation remains outstanding.

## Baseline findings

| Area | Existing behavior | Risk / consequence |
|---|---|---|
| ADB process launch | `src/adb.hpp` built a command string from `GetModuleFileNameA` and called `system()`. Arguments were not structured. | Paths containing spaces could fail; shell parsing added quoting/injection hazards; the caller blocked and fixed-size ANSI path handling could truncate paths. |
| `adb reverse` scope | `adb::start()` ran `adb reverse tcp:6969 tcp:6969` without `-s <serial>`. The non-Windows settings branch also invoked an unscoped command. | Multiple devices made the target ambiguous; no state machine restored the mapping after USB loss, daemon restart, or server restart. |
| ADB shutdown/global impact | `adb::kill()` chained `reverse --remove tcp:6969` and `adb kill-server`; Linux close used `reverse --remove-all`. | Closing this app could interrupt Android Studio/other ADB clients and remove mappings owned by other apps. |
| Windows startup/UI blocking | The server constructor called the ADB toggle from `wxApplication::OnInit()`. `system()` was synchronous. | A slow ADB command could delay or freeze window creation; there was no timeout. |
| Device-state detection | There was no server-side `adb devices` poller. Android checked the USB broadcast once when the user pressed its server button. | Absent, unauthorized, offline, and usable devices were not distinguished; cable presence alone did not establish ADB authorization. |
| Multiple devices | There was no serial choice or remembered selection. | The app could not safely decide which phone should receive a reverse mapping. |
| TCP startup/USB loss | TCP/UDP bound to wildcard interfaces. The server performed a blocking greeting read on its single Asio worker. | Slow peers could block other I/O; the unauthenticated input service was reachable from the network. |
| Android socket lifecycle | `ConnectionManager` launched independent IO scopes and stored mutable connections without synchronization. EOF and reconnect handling were incomplete. | Attempts could race, stale sockets/callbacks could affect a newer session, and disconnect/retry behavior was unreliable. |
| Android UX/lifecycle | A single USB attempt fell back to Wi-Fi; failure and disconnect navigated away and displayed popups. | USB availability after app open was missed and ordinary cable interruptions caused noisy UX. |
| Network exposure | TCP and UDP listeners were wildcard-bound, with no authentication. Wi-Fi mode was reachable from the fallback flow. `GetHostInfo()` probed `8.8.8.8`. | LAN hosts could attempt unauthenticated input; startup depended on an external-route probe. |
| Thread ownership/UI | The Asio worker mutated connection state and callbacks could update wx controls directly. | Connection/UI races and thread-unsafe wx calls were possible. |
| RSA authorization | The baseline relied on standard ADB transport and did not bypass RSA authorization. | That security property must remain: a connected cable must not be treated as authorized until Android approves the host. |

## V1 implementation

1. **ADB process and monitor:** a background monitor polls the bundled ADB executable via direct process creation, a structured argument vector, captured output, and bounded command timeouts. Wide-character executable paths handle Windows package directories containing spaces. The monitor reports no device, unauthorized, offline, multiple devices, selected-device missing, ready, reverse failure, and reverse conflict. It never selects a different phone when an explicit serial becomes unavailable.
2. **Safe reverse ownership:** only `adb -s <serial> reverse ...` is used. The monitor first inspects that serial's current reverse rules, reuses an exact mapping, restores a missing mapping, and refuses to overwrite a conflicting or uninspectable mapping. There is no global ADB shutdown or `--remove-all` path; closing Mousedroid leaves the shared ADB server running.
3. **Loopback-only desktop service:** TCP and UDP bind to `127.0.0.1`; the Android Wi-Fi server route is disabled with persistent status feedback. No firewall/router exception is required. Device handshakes are length-prefixed and read asynchronously; accepted connections are synchronized, and wx callbacks are posted to the UI thread.
4. **Android recovery/lifecycle:** one process-wide supervisor checks current USB state, listens for USB state changes where available, and retries with capped exponential backoff. There is one active USB socket, explicit connect timeouts, EOF and heartbeat detection, and bounded socket cleanup. The Touchpad stays available during USB recovery, and status updates replace repeated error popups. An explicit Disconnect pauses automatic USB attempts until the user resumes them. Bluetooth remains a separate user-selected mode.
5. **Regression/operations documentation:** fake ADB coordinator tests cover parsing, serial selection, reverse recovery/conflicts, and protection against global operations; an asynchronous monitor test checks wake-up and clean stop. A Windows process test covers spaced executable paths, exact argument boundaries, and timeouts. CI runs Android unit tests/lint/APK checks and Windows Release/CTest/package checks. `USB_RECOVERY_TEST_MATRIX.md` maps acceptance cases to tests, while `FIRST_TEST_GUIDE.md` gives the Portuguese first-test steps.

## Verification status

- **Passed locally:** strict `g++ -std=c++17 -Wall -Wextra -Werror -pedantic -pthread` build/run of 17 fake-ADB coordinator scenarios and the asynchronous monitor test; `git diff --check`; main Android layout XML parsing.
- **Passed in CI:** [run 42224403618](https://github.com/gabielrtxofc-coder/Pocket-Pad-Repository/actions/runs/42224403618) built/tests/linted the Android app, produced the Debug APK, compiled the Windows x64 Release server and tests, passed CTest (including the Windows process-path case), and packaged the Windows ZIP. The workflow computed and checked the sidecar hashes before upload: Android APK `bfe2a0a971d09adffe4cbf5b0bdc07e7dca27c82e1fb933d7ebbe04cfcd3c5f2`; Windows ZIP `d672408b7ab142f8688363872d9f9d1f8baec292359bc4b18c3906b78e9069a5`.
- **Not performed:** live-device ADB integration, Windows GUI launch, Wi-Fi exposure scan, physical cable loss/reconnect, or Galaxy A12 validation. Automated fake tests do not replace those manual checks.
