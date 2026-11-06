# USB/ADB recovery test matrix

This matrix maps the 15 USB-first acceptance scenarios to deterministic coverage. The C++ coordinator suite uses a fake command runner; no `adb.exe`, physical phone, Wi-Fi network, firewall, or shared ADB daemon is used by those tests. The separate Windows process test launches a copied test executable named `fake adb.exe` from a directory containing spaces.

| # | Scenario | Automated coverage |
|---:|---|---|
| 1 | No ADB device is attached; the state remains waiting and no reverse command is issued. | `adb_coordinator_fake_device_scenarios`, case 1 — **passed locally** |
| 2 | A connected phone reports `unauthorized`; the GUI state asks for RSA approval and no reverse is attempted. | Case 2 — **passed locally** |
| 3 | Accepting Android's RSA prompt changes the state from `unauthorized` to `device`, then permits the reverse. No authorization bypass is present. | Case 13 — **passed locally** |
| 4 | ADB reports `offline`; it is not displayed as an authorized/ready device. | Case 3 — **passed locally** |
| 5 | One online authorized device with no mapping gets `adb -s <serial> reverse tcp:6969 tcp:6969`. | Case 4 — **passed locally** |
| 6 | Restarting the Android app when the correct mapping already exists reuses it rather than issuing a duplicate mapping. | Case 5 — **passed locally** |
| 7 | Restarting the ADB server and losing its mapping causes the monitor to inspect and re-apply the reverse. | Case 6 — **passed locally** |
| 8 | USB disappearance followed by reappearance recovers without picking a different phone. | Case 7 — **passed locally** |
| 9 | Restarting the Windows server creates a fresh coordinator that restores the selected phone's reverse. | Case 8 — **passed locally** |
| 10 | Two authorized devices with no preference produce an explicit-selection state and no reverse command. | Case 9 — **passed locally** |
| 11 | Selecting a serial targets only that serial. | Cases 10 and 16 — **passed locally** |
| 12 | A selected serial that is unauthorized or missing is not silently replaced by another authorized phone. | Cases 11 and 12 — **passed locally** |
| 13 | Reverse failure, an existing conflicting local port, or an unavailable reverse-list inspection is surfaced; another mapping is not overwritten and no global shutdown is used. | Cases 14, 15, and 17 — **passed locally** |
| 14 | Spaced executable/working paths, spaces and shell metacharacters in arguments, trailing backslashes, and a bounded process timeout preserve direct-process argument boundaries. | `adb_windows_argument_safe_process` — **Windows CI required; not run in this Linux workspace** |
| 15 | A second ADB client/another attached device is not targeted or stopped; there is no `kill-server`, `--remove-all`, or unscoped reverse invocation. | Case 16 plus command-vector assertions — **passed locally** |

Additional checks:

- `adb_background_monitor_fake_device` verifies the monitor publishes on a background thread, wakes immediately on serial selection, applies the chosen serial, and stops cleanly — **passed locally**.
- Android unit tests cover the two-byte UTF-8 handshake length and capped exponential backoff — **added; Android Gradle/SDK are unavailable locally, so CI must run them**.
- The Windows packaging job must run CTest, including the fake process-path test, and then produce the updated ZIP. Until that job passes, Windows C++ compilation and packaging are **not verified**.
- No ADB integration test with a live device, Wi-Fi exposure scan, Windows GUI launch, or Galaxy A12 physical reconnect test has been performed. Those remain manual verification items.
