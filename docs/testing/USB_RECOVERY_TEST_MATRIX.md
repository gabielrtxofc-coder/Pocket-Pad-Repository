# USB/ADB recovery test matrix

This matrix maps the 15 USB-first acceptance scenarios to deterministic coverage. The C++ coordinator suite uses a fake command runner; no `adb.exe`, physical phone, Wi-Fi network, firewall, or shared ADB daemon is used by those tests. The separate Windows process test launches a copied test executable named `fake adb.exe` from a directory containing spaces.

| # | Scenario | Automated coverage |
|---:|---|---|
| 1 | No ADB device is attached; the state remains waiting and no reverse command is issued. | `adb_coordinator_fake_device_scenarios`, case 1 — **passed locally** |
| 2 | ADB reports `unauthorized`; the coordinator returns a distinct RSA-approval status and issues no reverse command. | Case 2 — **passed locally** |
| 3 | A fake ADB state transition from `unauthorized` to `device` permits the reverse only after the authorized state is reported. This is not a live RSA-prompt test. | Case 13 — **passed locally** |
| 4 | ADB reports `offline`; the coordinator distinguishes it from an authorized/ready device. | Case 3 — **passed locally** |
| 5 | One online authorized device with no mapping gets `adb -s <serial> reverse tcp:6969 tcp:6969`. | Case 4 — **passed locally** |
| 6 | Restarting the Android app when the correct mapping already exists reuses it rather than issuing a duplicate mapping. | Case 5 — **passed locally** |
| 7 | Restarting the ADB server and losing its mapping causes the monitor to inspect and re-apply the reverse. | Case 6 — **passed locally** |
| 8 | USB disappearance followed by reappearance recovers without picking a different phone. | Case 7 — **passed locally** |
| 9 | Restarting the Windows server creates a fresh coordinator that restores the selected phone's reverse. | Case 8 — **passed locally** |
| 10 | Two authorized devices with no preference produce an explicit-selection state and no reverse command. | Case 9 — **passed locally** |
| 11 | Selecting a serial targets only that serial. | Cases 10 and 16 — **passed locally** |
| 12 | A selected serial that is unauthorized or missing is not silently replaced by another authorized phone. | Cases 11 and 12 — **passed locally** |
| 13 | Reverse failure, an existing conflicting local port, or an unavailable reverse-list inspection is surfaced; another mapping is not overwritten and no global shutdown is used. | Cases 14, 15, and 17 — **passed locally** |
| 14 | Spaced executable/working paths, spaces and shell metacharacters in arguments, trailing backslashes, and a bounded process timeout preserve direct-process argument boundaries. | `adb_windows_argument_safe_process` — **passed in Windows CI run 42224403618** |
| 15 | A second ADB client/another attached device is not targeted or stopped; there is no `kill-server`, `--remove-all`, or unscoped reverse invocation. | Case 16 plus command-vector assertions — **passed locally** |

Additional checks:

- `adb_background_monitor_fake_device` verifies the monitor publishes on a background thread, wakes immediately on serial selection, applies the chosen serial, and stops cleanly — **passed locally**.
- Android unit tests cover the two-byte UTF-8 handshake length and capped exponential backoff — **passed in CI run 42224403618**, along with `lintDebug` and `assembleDebug`.
- Windows x64 Release compilation and all CTest cases, including the fake process-path test, passed in [CI run 42224403618](https://github.com/gabielrtxofc-coder/Pocket-Pad-Repository/actions/runs/42224403618); the Windows ZIP was packaged and uploaded.
- CI SHA-256 notices: Android APK `bfe2a0a971d09adffe4cbf5b0bdc07e7dca27c82e1fb933d7ebbe04cfcd3c5f2`; Windows ZIP `d672408b7ab142f8688363872d9f9d1f8baec292359bc4b18c3906b78e9069a5`. The workflow generated and checked each sidecar before upload.
- No live-device ADB integration test, Wi-Fi exposure scan, Windows GUI launch, or Galaxy A12 physical reconnect test has been performed. Those remain manual verification items.
