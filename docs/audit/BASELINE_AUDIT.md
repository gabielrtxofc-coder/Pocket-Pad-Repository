# Mousedroid — baseline import and technical audit

**Audit date:** 2026-11-06 (UTC)<br>
**Upstream:** [`darusc/Mousedroid`](https://github.com/darusc/Mousedroid), branch `master`<br>
**Imported source revision:** `3d077786c4ab333f648fc3c2b2ad668203af09b2` (`v1.5`)<br>
**Destination:** [`gabielrtxofc-coder/Pocket-Pad-Repository`](https://github.com/gabielrtxofc-coder/Pocket-Pad-Repository)<br>
**Destination branch used by this session:** `arena/877c740f-pocket-pad-repository`

> **Scope and interpretation.** This is a baseline investigation, not a remediation. No Mousedroid production source was changed, no bug fix or feature was implemented, and no destructive installer, ADB, firewall, registry, or hardware test was run. “Confirmed” below means the defect is directly evident from the source or a dependency’s published affected-version range; it does **not** mean that Windows, Android hardware, or a desktop binary was run in this environment. Runtime findings are explicitly identified as untested.

## 1. Executive summary

- The destination was not empty: it contained a single `README.md` and the original initial commit. The upstream project was therefore imported intact under **`mousedroid/`**, rather than replacing destination files. A Git subtree merge preserves the upstream master history as a parent; its split is exactly the source master commit listed above. The imported MIT license, original documentation, attribution, resources, build files, and binaries are present.
- The destination’s default branch `main` was not changed. The source branches `master` and `bl` and tags `v1.0` through `v1.5`, plus `v1.5-beta`, are fetched in the local checkout under the `upstream` remote. `origin` points to the existing Pocket Pad repository and `upstream` to the official source.
- The audit found **22 code- or advisory-confirmed findings**: **5 high, 15 medium, 2 low**. The most consequential are unauthenticated remote input on desktop TCP/UDP listeners; unsafe, unframed protocol parsing; the Windows/Linux ADB executable-path mismatch; unhandled server startup errors; and a Windows CMake resource-path mismatch. Five additional risks remain unverified and are not included in those counts.
- The Android build, Android unit tests, desktop build, and CTest suite could not execute. The Gradle wrapper is not executable in Git, the available environment has no Java/JDK or Android SDK, and it has no CMake, wxWidgets, or standalone Asio headers. No Android device, Windows machine, desktop server binary, or ADB CLI is available here.
- Two template Android tests exist (one trivial local arithmetic assertion and one app-context assertion); there are no desktop/protocol tests and no CI workflow in the imported tree. **Project tests actually executed: 0.** Static XML/JSON parsing and shell syntax checks did execute successfully.
- GitHub reports about ADB startup, the Windows executable location, drag/drop, binding to port 6969, and interface selection were reviewed. Some are strongly corroborated by current code, but none was reproduced on the reported hardware/OS. The release-signing concern in issue #33 is a supply-chain warning, not a confirmed compromise.

### Finding counts

| Classification | Count | Notes |
|---|---:|---|
| High | 5 | Includes network exposure/parser safety, ADB startup, server startup, and Windows source-build configuration. |
| Medium | 15 | Includes connection/gesture/platform behavior, build packaging, movement precision, and the Kotlin Gradle plugin advisory. |
| Low | 2 | Per-client native buffer leak and close-confirmation behavior. |
| **Confirmed total** | **22** | Code/advisory evidence; not equivalent to hardware reproduction. |
| Suspicions not counted above | 5 | Require runtime, lifecycle, concurrency, or gesture reproduction. |

## 2. Import and Git baseline

### Destination checks completed before import

- Exact destination URL: `https://github.com/gabielrtxofc-coder/Pocket-Pad-Repository.git` (the configured `origin`).
- Authenticated GitHub identity: `gabielrtxofc-coder`; GitHub repository permissions reported `push: true` (also admin/maintain).
- Existing GitHub default branch: `main`, at `caac5a53ffc1b48dd509256095dbaf875b964c07` (`Initial commit`). The workspace was clean on the session branch before import; the only existing file was the destination `README.md`.
- No competing destination named Pocket Pad Repository was used or created.

### Import record

- A full mirror clone of `https://github.com/darusc/Mousedroid.git` was performed (not a ZIP download). Source `master` is commit `3d077786c4ab333f648fc3c2b2ad668203af09b2`, tagged `v1.5`; it contains 109 commits reachable on that branch. The other source branch fetched is `bl` at `4062a054f4a9fc83a6a4c4a848f70de2b5882388`.
- The upstream tree was added at `mousedroid/` with `git subtree add`. The resulting import merge commit is `48f0b581dab5aa317f9d3291b2a874364b70195e`; its parents are the original Pocket Pad commit and the exact Mousedroid master commit. `git subtree split --prefix=mousedroid HEAD` returned `3d077786c4ab333f648fc3c2b2ad668203af09b2`, confirming the imported subtree has not diverged from upstream master.
- The destination README remains byte-for-byte unchanged from the original destination commit. The imported `mousedroid/LICENSE` matches upstream master’s `LICENSE`.
- Locally configured remotes:
  - `origin` fetch/push: `https://github.com/gabielrtxofc-coder/Pocket-Pad-Repository.git`
  - `upstream` fetch/push: `https://github.com/darusc/Mousedroid.git`
- The local checkout tracks upstream `master` and `bl`, and has tags `v1.0`, `v1.1`, `v1.2`, `v1.3`, `v1.4`, `v1.5`, and `v1.5-beta`. GitHub remote configuration is a local-clone setting; it is not stored as part of the GitHub repository’s tree.

### Branch-policy deviation

The request asked for `import/mousedroid-upstream` and `audit/baseline-analysis`. This Arena session is fixed to `arena/877c740f-pocket-pad-repository` and cannot switch to, create, or push another branch. To preserve the destination and comply with that constraint, the import and this report are kept on the fixed session branch; the existing `main` is untouched. **Those two requested branch names were not created.** This is a workflow limitation, not a change to upstream source. No force push or history rewrite was used.

## 3. Architecture identified from source

### Languages, frameworks, and entry points

| Component | Implementation | Entry point / key files |
|---|---|---|
| Android client | Kotlin, Android Views/XML, AndroidX Fragments/Navigation/ViewModel, Data Binding, Kotlin coroutines | Manifest launches `com.darusc.mousedroid.MainActivity`; navigation graph starts at `fragments.Main`; `fragments.Input` hosts child input-mode fragments. |
| Desktop server | C++17, standalone Asio, wxWidgets GUI | `server/src/wxapplication.cpp`: `wxIMPLEMENT_APP(wxApplication)`; `wxApplication::OnInit()` constructs the frame and `Server(6969, ...)`, then starts it. |
| Windows input injection | Win32 `SendInput` | `server/src/input/win32/windowsinputmanager.cpp` |
| Linux input injection | Linux `/dev/uinput` | `server/src/input/linux/linuxinputmanager.cpp` |
| Build/config | Gradle wrapper/Android Gradle Plugin; CMake; vcpkg on Windows; system packages on Linux | `client/*.gradle`, `client/gradle/wrapper/gradle-wrapper.properties`, `server/CMakeLists.txt`, `server/README.md` |

The Android side contains about 3.7k lines of Kotlin source, the desktop side about 2.1k lines across C++ sources/headers, and XML resources/layouts. The committed tree also includes generated `.gradle`/IDE state, a machine-specific `client/local.properties`, and Windows ADB executable/DLL binaries.

### End-to-end flow: opening the app to a desktop mouse event

1. Android starts `MainActivity` from the manifest. It sets `activity_main`, requests a Bluetooth runtime permission under a version check, initializes `BluetoothAdapterWrapper`, and leaves battery monitoring commented out. The navigation graph opens the `Main` fragment.
2. **USB:** tapping Server invokes `ConnectionViewModel.startServerMode()`. The app checks the Android USB-state broadcast; when connected it creates a TCP client to `127.0.0.1:6969`. The desktop server attempts `adb reverse tcp:6969 tcp:6969`, so device loopback traffic is intended to reach the desktop listener. The app sends a slash-delimited identity string: `manufacturer/deviceName/model/modeOrdinal`.
3. **Wi-Fi:** when Wi-Fi is active, the app navigates to a saved-device list. Selecting an IPv4 address opens TCP to port `6969`, sends the same identity string, and creates a UDP socket to that address/port. The TCP socket is used for registration/liveness; the Android client sends the input reports over UDP.
4. **Bluetooth:** the Android phone registers a Bluetooth HID Device profile, the user selects a paired host, and the phone emits standard HID reports through `BluetoothHidDevice.sendReport()`. In this mode the desktop Mousedroid server is not the input receiver; the paired computer consumes Android as a Bluetooth HID peripheral.
5. On successful network connection the app navigates to `Input`, which creates a `Touchpad` child by default. `GestureHandler` converts touch callbacks to `InputEvent`s; `TouchpadViewModel` forwards them to the singleton `ConnectionManager`.
6. TCP/UDP events are serialized in `networking/Translators.kt`. The desktop listener accepts TCP registrations and stores `Connection` objects; UDP receive and TCP read callbacks call `InputManager::Base::execute()`. The protocol event type selects mouse move/click/scroll, drag, key, zoom, or media handling.
7. Windows applies events using `SendInput`; Linux writes `input_event`s to `/dev/uinput`. Mouse sensitivity and scroll sensitivity are loaded by `SettingsManager` from a working-directory-relative `config.ini`.

### Transport/protocol map confirmed from code

| Mode | Transport and data flow | Desktop-side path |
|---|---|---|
| Wi-Fi | TCP registration/identity; UDP input datagrams to port `6969` | `Server::TCPDoAccept()` and `Server::StartReceive()` |
| USB | TCP to Android loopback, intended to be forwarded by host-side `adb reverse`; raw event stream | `TCPConnection` → `Connection::Read()` → `InputManager::Base::execute()` |
| Bluetooth | Android implements Bluetooth HID Device; no Mousedroid desktop protocol socket | `BluetoothConnection` → HID reports; host OS handles HID |

Raw socket event IDs in `Translators.kt`: `0x01` left click, `0x02` right click, `0x03` left-down, `0x04` left-up, `0x05` move (`dx`,`dy`), `0x06` vertical scroll, `0x07` key (`key`,`modifier`), `0x08` horizontal scroll, `0x09` zoom, `0x0A` media. TCP uses a byte stream; UDP preserves individual datagrams. The source has no length-prefixed/versioned frame format.

### Startup, connection, and settings management

- `ConnectionManager` is a process singleton with nullable TCP, UDP, and Bluetooth connection fields. It chooses UDP first, then TCP, then Bluetooth. It creates ad-hoc `CoroutineScope`s for connect/send/disconnect rather than one owned connection scope.
- The desktop `Server` binds TCP and UDP port `6969`, starts an Asio context on a `std::thread`, tracks TCP clients in a vector, and calls `SettingsManager::ADBOn()` in its constructor.
- Windows ADB is invoked through `system()` in `server/src/adb.hpp`; Linux uses `system("./adb ...")` in `settingsmanager.cpp`.
- Android remembers Wi-Fi device name/IP pairs in private SharedPreferences named `devices`. Desktop settings use `config.ini` relative to the current working directory. There is no server-side authentication/session-secret mechanism in the inspected protocol.

### Gesture and lifecycle behavior mapped from source

| Gesture/control | Current implementation found in code | Audit note |
|---|---|---|
| Left click | `onSingleTapUp()` schedules a left-click event after 150 ms. | Delay is intended to allow double-tap recognition; boundary timing remains untested (S-01). |
| Double click | `onDoubleTap()` cancels the pending single click, then sends two left-click events with a 75 ms gap. | Timing and transport ordering need a device test. |
| Right click | On `ACTION_POINTER_UP` with two pointers, emits right click if not currently scrolling and `lastScrolled` is more than 500 ms ago. | Heuristic; slow pinch/scroll vs stationary two-finger tap needs manual tests. |
| Vertical/horizontal scroll | `onScroll()` chooses the larger axis; sends `MouseScroll` and clamps each delta to one byte. | Android Bluetooth divides each delta by 10 without retaining remainder; Linux horizontal backend uses `REL_WHEEL` (C-12/C-13). |
| Drag and drop | Double-tap sets a time-limited state; movement during the short window sends left-down; `ACTION_UP` sends left-up. | Does not match README long-press description; inbound `ACTION_CANCEL` does not release (C-07/C-08). |
| Pinch zoom | `ScaleGestureDetector` accumulates span changes and emits one zoom tick per 20 pixels; desktop translates to Ctrl+wheel, Bluetooth to a wheel HID report. | Scale and scroll detectors share the touch stream; overlap not reproduced (S-02). |
| Slow/fast cursor movement | `onScroll()` converts `Float` distance to `Int`, clamps to `[-128,127]`; desktop socket backends then apply settings sensitivity, while Bluetooth sends raw HID deltas. | Fractional/overflow movement and cross-transport sensitivity differ (C-22). |
| Rotation/fullscreen | `Touchpad.onCreateView()` requests portrait; the fullscreen button requests landscape and manually updates constraints/padding; `onDestroyView()` resets to unspecified. The manifest handles orientation/screen-size changes without Activity recreation. | Layout behavior on Galaxy A12/rotation remains a manual test. |
| Battery/background | `BatteryMonitor.start()` is commented out in `MainActivity`; no Service is declared, although `FOREGROUND_SERVICE`/`WAKE_LOCK` permissions are present. Battery-event serialization is partly dead (socket translation returns an empty report; battery descriptor is commented). | No active periodic battery monitor was found; background connection survival and battery impact need measurement. |

## 4. Technologies and dependency inventory

### Android

- `compileSdk 34`, `minSdk 28`, `targetSdk 34`; Java/Kotlin bytecode target 17.
- Android Gradle Plugin `8.9.1`, Kotlin Android plugin `2.1.0`, Gradle wrapper `8.11.1`.
- Declared app dependencies: Gson `2.10.1`, AndroidX Core KTX `1.8.0`, AppCompat `1.4.1`, Material `1.5.0`, ConstraintLayout `2.1.3`, Navigation Fragment KTX `2.7.7`, coroutines-android `1.3.9`; JUnit `4.13.2`; AndroidX test JUnit `1.1.5` and Espresso `3.5.1`.
- Gson has no use found in application/test sources. No version catalog, dependency lockfile, or generated SBOM is present. The transitive graph could not be resolved in this environment.

### Desktop

- C++17, wxWidgets (Windows vcpkg; Linux system wxWidgets), standalone Asio (vcpkg on Windows; README requests `libasio-dev` on Linux), CMake minimum `3.26`.
- No C++ package lockfile or version-pinned dependency manifest is present. `CMakeSettings.json` contains Visual Studio/Ninja configurations.
- Bundled Windows ADB files are tracked under `server/adb/`; their provenance/version/checksum is not recorded in the source tree.

### Dependency advisory check

GitHub Advisory Database’s Maven package advisories were queried for the declared direct artifacts on the audit date. This was an advisory-range check, **not** a complete dependency scan (no Gradle resolution, lockfile, or transitive inventory was available).

- **Confirmed affected build dependency — medium:** Kotlin Gradle plugin `2.1.0` falls within GHSA [GHSA-r937-wjx7-w2jp](https://github.com/advisories/GHSA-r937-wjx7-w2jp), CVE-2026-53914, affected range `< 2.4.20-Beta1`. The advisory describes unsafe deserialization in Kotlin build-cache metadata that can enable code execution. The project does not enable Gradle caching in its checked-in `gradle.properties`, so exploitability depends on the build environment (for example, consuming untrusted/shared cache metadata); this is not evidence of an app-runtime exploit. No fix was applied.
- Gson `2.10.1` is outside the affected range `< 2.8.9` in GHSA [GHSA-4jrv-ppp4-jm57](https://github.com/advisories/GHSA-4jrv-ppp4-jm57).
- JUnit `4.13.2` is outside the affected range `< 4.13.1` in GHSA [GHSA-269g-pwp5-87pp](https://github.com/advisories/GHSA-269g-pwp5-87pp).
- No matching GitHub advisory result was returned for the other queried direct Maven coordinates. This is not a guarantee that every transitive or native dependency is vulnerability-free.

## 5. Build, tests, and static checks

| Check | Command/approach | Result |
|---|---|---|
| Android unit-test task | `./gradlew testDebugUnitTest` | Could not start: `gradlew` is mode `100644` (not executable); direct invocation returned permission denied. Running `bash ./gradlew testDebugUnitTest` then stopped because no `java`/`JAVA_HOME` exists. **Not run.** |
| Android debug build | `./gradlew assembleDebug` (then shell invocation) | Same blockers; no JDK or Android SDK detected. `client/local.properties` also points to a Windows-only SDK path. **Not built; no APK produced.** |
| Android lint/static tasks | AGP task discovery not possible | Requires Gradle/JDK/SDK; not run. No Detekt/Ktlint configuration found. |
| Desktop CMake build | `cmake -S mousedroid/server -B /tmp/mousedroid-server-build -DCMAKE_BUILD_TYPE=Release` | `cmake: command not found`. C++ compiler exists, but wxWidgets and `asio.hpp` headers are absent. **Not built.** |
| Desktop test discovery | `ctest --test-dir /tmp/mousedroid-server-build --output-on-failure` | `ctest: command not found`; CMake project has no `enable_testing()`/`add_test()` test definitions. **No desktop tests available/run.** |
| Existing Android tests | Source inventory | 2 test methods found: example `2 + 2` JUnit assertion and a basic instrumentation package-name assertion. Neither was run. |
| Protocol/integration/reconnect tests | Source inventory | No protocol test suite, mock server, or reconnection test found. No device/server binary was available to test. |
| Shell syntax | `bash -n mousedroid/server/release.sh mousedroid/server/install.sh` | Passed syntax parsing only; neither script was executed. `install.sh` was deliberately not run because it uses `sudo` and changes system files/udev rules. |
| Resource syntax | Python XML parse for Android source resources; JSON parse for `CMakeSettings.json` | Parsed 67 Android XML files and the JSON file successfully. This is syntax validation, not Android resource linking. |
| Static/dependency review | Source inspection, repository searches, GitHub Advisory Database | Findings documented below. No full Android Lint, C++ static analyzer, ShellCheck, or transitive dependency scanner was available/run. |

**Test accounting:** automated project tests executed **0**, passed **0**, failed **0**. One Gradle test-task attempt was blocked before Gradle could start. Static XML/JSON parsing and shell syntax checks are reported separately above; they are not counted as project tests.

## 6. Confirmed findings (code/advisory evidence; runtime not reproduced here)

For each item, “observed” describes the source path or advisory range unless a public issue report is explicitly cited. No destructive proof-of-concept was run.

### C-01 — Unauthenticated remote input over all IPv4 interfaces — **HIGH / security**

- **Location/evidence:** `mousedroid/server/src/net/server.cpp:11,13-17,100-127`; `server/src/net/connection.cpp:10-26`; `server/src/input/inputmanager.hpp:43-85`. TCP and UDP sockets bind IPv4 wildcard endpoints (`v4()` with port, no interface address); any accepted TCP peer can register, and every received UDP datagram is passed to the OS input manager. No authentication, pairing token, TLS, origin/session validation, or authorization check is present. The README even tells users to permit the server through both Private and Public Windows firewall profiles.
- **Description:** A reachable LAN peer can inject mouse/keyboard/media input into the desktop session. This is remote input injection, not shell-command execution, but the effect is control of the logged-in desktop user. Exposure depends on host firewall and network reachability.
- **Reproduction steps:** Not performed. In an isolated disposable lab only, start the server with port 6969 reachable and submit a valid input report from an unpaired client. Do **not** test on a shared/public network or a production workstation.
- **Expected:** Reject non-paired/unauthorized clients and restrict listening to an explicitly trusted interface.
- **Observed:** Static code accepts TCP registration and UDP input without credentials or peer validation; both listeners are wildcard-bound.
- **Likely cause:** Trust is implicit in network reachability; the protocol has no security/session layer.
- **Recommendation:** Add authenticated pairing/session authorization, protect confidentiality/integrity, validate peer identity, bind only to selected trusted interfaces, and document/firewall only private-network access. Treat this as a blocker for public/untrusted networks.

### C-02 — Packet lengths and TCP framing are not validated — **HIGH / security and stability**

- **Location/evidence:** `mousedroid/server/src/input/inputmanager.hpp:43-85` receives `size` but ignores it and unconditionally reads `bytes[0]`, `bytes[1]`, and `bytes[2]` depending on the command. `server/src/net/server.cpp:102-112` performs a single `read_some()` for the identity, then `server/src/net/connection.cpp:10-22` indexes `tokens[0..3]` without checking token count. `Connection::Read()` passes arbitrary `async_read_some()` chunks to the parser.
- **Description:** Short/empty UDP datagrams or fragmented TCP input can cause out-of-bounds reads, uninitialized/incorrect arguments, dropped commands, or exceptions. TCP is a byte stream, but the protocol has no frame length or partial-frame accumulator. A malformed media byte can also reach `.at()` in platform maps. In addition, `TCPDoAccept()` performs a blocking `read_some()` for device details inside the single Asio context thread without a timeout; a peer that connects and sends no identity can stall that thread, stopping other accepts and UDP handling.
- **Reproduction steps:** Not performed because malformed input may crash or destabilize a server. A future test should use an isolated process and cover empty/short UDP payloads, fragmented/coalesced TCP commands, malformed identity strings, unknown opcodes, and invalid media codes.
- **Expected:** Reject malformed/incomplete messages safely and continue operating; preserve each complete event across TCP fragmentation/coalescing.
- **Observed:** Static code reads beyond the supplied length and assumes four identity tokens. The TCP parser processes each read buffer as if it begins at an event boundary.
- **Likely cause:** Ad-hoc raw-byte protocol without framing, length checks, or schema validation.
- **Recommendation:** Define explicit versioned frames and bounded parsers; validate lengths/opcodes/fields before dispatch; accumulate TCP bytes until complete frames; fuzz the parser and add regression tests.

### C-03 — ADB executable discovery does not match the packaged layout — **HIGH / USB Windows and Linux**

- **Location/evidence:** Windows `mousedroid/server/src/adb.hpp:30-35` builds `<exe directory>\adb.exe`, while the tracked executable is `mousedroid/server/adb/adb.exe`; `server/release.bat:8` copies the `adb` directory under `dist\adb\`. The `kill()` path is inconsistent too (`adb.hpp:51-61`). Linux uses `system("./adb ...")` and `system("./adb ... --remove-all")` in `server/src/settingsmanager.cpp:120-149`, while the Linux README asks users to install the system `adb` package and the source tree has no root-level `./adb` executable.
- **Description:** The normal Windows package layout does not place `adb.exe` at the location `adb::start()` executes. The Linux code searches relative to the current working directory instead of the system `adb` executable. The code also supplies no device serial selector, so multiple attached Android devices are not explicitly handled.
- **Reproduction steps:** Not performed on Windows/Linux. The matching open reports are [#40](https://github.com/darusc/Mousedroid/issues/40), [#47](https://github.com/darusc/Mousedroid/issues/47), and [#44](https://github.com/darusc/Mousedroid/issues/44). #47’s author reports copying the ADB files from the `adb` folder into the application root as a workaround; the current path construction matches that report.
- **Expected:** Start ADB forwarding from the bundled/system ADB executable for an authorized, selected device.
- **Observed:** Source constructs paths that do not match the tracked/release directory layout; Linux calls `./adb` rather than the documented package command.
- **Likely cause:** Executable path assumptions differ between source, packaged binary, and OS.
- **Recommendation:** Resolve an absolute ADB path once, verify it exists, quote/pass arguments safely, detect/identify devices and authorization state, and test packaged layouts and multi-device behavior. Do not invoke global `kill-server` as the normal per-app cleanup path.

### C-04 — Server startup errors can escape and terminate the desktop app — **HIGH / availability**

- **Location/evidence:** `mousedroid/server/src/net/server.cpp:5-22` binds UDP in the member initializer and binds/listens on TCP in the constructor without a catch. `wxapplication.cpp:9-27` constructs `Server` and calls `GetHostInfo()` without error handling. `GetHostInfo()` attempts a UDP route probe to `8.8.8.8:80` at `server.cpp:25-43`.
- **Description:** If port 6969 is already occupied, bind can throw `std::system_error`; no GUI-level handler converts it into a useful message. A missing route/network can also make the host-info probe fail without a handled path.
- **Reproduction steps:** Not run on Windows. The open report [#46](https://github.com/darusc/Mousedroid/issues/46) describes an unhandled bind error after restart. A safe future reproduction is to reserve port 6969 in a disposable test environment, then start Mousedroid. Offline startup should also be tested without changing a real host’s network.
- **Expected:** Start with a clear diagnostic or continue in a supported offline state; do not terminate unexpectedly.
- **Observed:** The constructor and `OnInit()` have no catch around socket binding/route discovery; the issue report’s error is consistent with the unhandled bind path, but was not reproduced here.
- **Likely cause:** Socket setup and public-route probing are treated as infallible.
- **Recommendation:** Handle each bind/route error, report the port and cause in UI/logs, allow graceful retry/port selection, and avoid requiring an external route just to display local host information.

### C-05 — Windows CMake references resource files at the wrong paths — **HIGH / Windows source-build blocker**

- **Location/evidence:** `mousedroid/server/CMakeLists.txt:21-25` references `resource.rc` for MSVC and `resource.o` for non-MSVC Windows. The tracked resource is `mousedroid/server/res/resource.rc`; there is no `server/resource.rc` or tracked `resource.o`.
- **Description:** The Windows source build references resource paths that do not exist at the declared locations. This is a high-confidence build-configuration defect; the Windows build could not be executed in this Linux environment.
- **Reproduction steps:** Not run. Configure/build the desktop project with a Windows CMake generator after installing the documented dependencies; observe resource source resolution.
- **Expected:** CMake compiles the tracked Windows resource file and completes the target.
- **Observed:** CMakeLists names a different relative path from the tracked resource.
- **Likely cause:** Resource file moved under `res/` without updating CMake references.
- **Recommendation:** Reconcile resource paths and validate fresh MSVC and MinGW builds from a clean checkout in CI.

### C-06 — Wi-Fi TCP EOF is not treated as a disconnect — **MEDIUM**

- **Location/evidence:** `mousedroid/client/app/src/main/java/com/darusc/mousedroid/networking/sockets/TCPConnection.kt:67-83` calls `onDisconnected()` for `-1` only when `isOverAdb` is true. For Wi-Fi, `read()` returning `-1` is passed to `onBytesReceived()` and the `while (running)` loop continues.
- **Description:** When the Wi-Fi server closes its TCP socket cleanly, the client may repeatedly read EOF, fail to notify the manager, and spin instead of transitioning to disconnected/reconnecting state.
- **Reproduction steps:** Not run. On an isolated Wi-Fi test, connect, then stop the desktop server while observing Android logs/CPU and UI state.
- **Expected:** One disconnect callback, socket cleanup, an idle/error UI, and an explicit reconnect opportunity.
- **Observed:** Static path sends `bytes == -1` to the empty manager callback and loops for Wi-Fi.
- **Likely cause:** EOF special-cased only for ADB, with no general `bytes == -1` handling.
- **Recommendation:** Handle EOF for both transports, make close idempotent, propagate connection state, and test loss/reconnect.

### C-07 — Drag button release is missing for canceled touch streams — **MEDIUM**

- **Location/evidence:** `GestureHandler.kt:187-193` sends `MouseDragState(LEFT, false)` only when an incoming event is `MotionEvent.ACTION_UP`. It does not handle an incoming `ACTION_CANCEL`; the synthetic `ACTION_CANCEL` at line 221 is sent only to `GestureDetector` and does not release the mouse button. Drag-down is sent at lines 202-218.
- **Description:** If Android cancels the touch stream after a drag-down (for example interruption, view detach, system gesture, or navigation), the matching mouse-up may never be sent; the desktop can retain a pressed left button.
- **Reproduction steps:** Not run on touch hardware. Begin a drag, then interrupt/cancel the touch stream and check whether the host pointer remains pressed.
- **Expected:** Every drag-down has a matching release on `UP`, `CANCEL`, teardown, and connection loss.
- **Observed:** Only `ACTION_UP` produces the release event.
- **Likely cause:** Drag state is not modeled as a lifecycle-balanced state machine.
- **Recommendation:** Balance release events on cancellation/teardown/disconnect and add touch-event tests for all terminal actions.

### C-08 — Documented long-press drag is not what the handler implements — **MEDIUM**

- **Location/evidence:** The upstream README says “Long press and drag.” `GestureHandler.kt` has no `onLongPress` override; it sets `state.doublePress` in `onDoubleTap()` and emits drag-down only when movement occurs within `EV_DELAY_MILLIS` after that double tap (`GestureHandler.kt:93-114, 200-218`). Ordinary single-finger movement emits only `MouseMove` (`118-159`).
- **Description:** The advertised long-press gesture is not implemented as a long press. The code’s drag-start path is coupled to a double tap and short timing window.
- **Reproduction steps:** Not run. On an Android touch screen, long-press then move and compare with double-tap-then-move.
- **Expected:** Long-press-and-move starts a drag, matching the published gesture guide.
- **Observed:** Source has no long-press callback and only sends drag-down from the double-tap path.
- **Likely cause:** Gesture behavior and README diverged during implementation.
- **Recommendation:** Define/document the gesture state machine and validate long-press, double-tap, and drag separately on physical hardware.

### C-09 — Bluetooth connection-success state is never set — **MEDIUM**

- **Location/evidence:** `BluetoothConnection.kt:30-31` initializes `connectionEstablished = false`; the connected callback at lines 73-76 does not set it true; the only later assignment at line 98 sets it false. On disconnection, lines 89-104 therefore take the “connection failed” path rather than the established-session disconnect path.
- **Description:** A Bluetooth link loss after a successful connection is classified as an initial connection failure. `ConnectionViewModel.onConnectionFailed()` does not navigate to the normal disconnected state, so the input UI/manager can remain stale.
- **Reproduction steps:** Not run; requires a paired HID host. Connect, then disable Bluetooth or disconnect the host during input.
- **Expected:** A disconnected-session callback, socket/profile cleanup, and a clear return to the connection UI.
- **Observed:** The state flag is never set true, so the callback follows the failure branch for both failed and established connections.
- **Likely cause:** Missing state transition in `STATE_CONNECTED`.
- **Recommendation:** Model connection states explicitly and test connect/fail/disconnect/reconnect transitions on supported Android and host versions.

### C-10 — Android 12 (API 31) Bluetooth runtime-permission gate is late; Advertise is never requested — **MEDIUM**

- **Location/evidence:** `MainActivity.kt:26-30` requests `BLUETOOTH_CONNECT` only for `SDK_INT >= S_V2` (API 32), although the permission applies starting with Android 12/API 31. `DeviceList` reads bonded devices through a `@RequiresPermission(BLUETOOTH_CONNECT)` getter. The manifest declares `BLUETOOTH_ADVERTISE`, but no runtime request for it was found; `Main.kt:80-85` launches `ACTION_REQUEST_DISCOVERABLE`.
- **Description:** On API 31, the app can call Bluetooth operations without having requested the required connect permission; on Android 12+ the discoverable flow also does not request the declared advertise permission. Runtime outcome depends on the device/OS permission behavior and was not reproduced.
- **Reproduction steps:** Not run. Exercise Bluetooth device listing and “Pair BT” on API 31, then API 32+ with each permission independently denied/granted.
- **Expected:** Request the relevant runtime permissions before calling protected APIs and handle denial without crashing.
- **Observed:** The request condition skips API 31 and there is no `BLUETOOTH_ADVERTISE` runtime request.
- **Likely cause:** Permission handling was updated for a later Android release but not the API-31 boundary or discoverability flow.
- **Recommendation:** Gate permissions at the API where they became runtime permissions, request only those needed for the action, and test denial/one-time revocation across API 31–34.

### C-11 — Socket middle-click is serialized as right-click — **MEDIUM**

- **Location/evidence:** `networking/Translators.kt:173-176` maps `MouseClick` to left-click only when `button == LEFT`; every other button maps to `RCLICK`. The touchpad UI/ViewModel exposes a middle-click button (`TouchpadViewModel.kt:32-37`). Bluetooth translation does preserve a middle HID button at `Translators.kt:38-44, 82-85`.
- **Description:** Middle click works differently by transport: on Wi-Fi/USB it emits the right-click opcode; Bluetooth uses a proper middle button.
- **Reproduction steps:** Not run. Click the middle mouse button in the Android touchpad while connected over Wi-Fi or USB, then compare Bluetooth.
- **Expected:** Middle-click event is distinct from right-click in every transport.
- **Observed:** Socket report for any non-left button is `RCLICK`.
- **Likely cause:** Socket protocol has no middle-click opcode or translator does not map one.
- **Recommendation:** Define a distinct middle-button protocol event and test equivalent events across TCP, UDP, and HID.

### C-12 — Linux horizontal scroll emits the vertical wheel event — **MEDIUM**

- **Location/evidence:** `server/src/input/linux/linuxinputmanager.cpp:111-115`, `Linux::scroll_h()` emits `REL_WHEEL`, not `REL_HWHEEL`. The shared protocol calls this operation `SCROLL_H` (`inputmanager.hpp:28-31`).
- **Description:** Horizontal scroll reports are sent as vertical wheel events on Linux.
- **Reproduction steps:** Not run; requires Linux `/dev/uinput`. Send a horizontal scroll report and observe the receiving application’s axis.
- **Expected:** Horizontal scroll moves along the horizontal axis.
- **Observed:** The Linux backend writes `REL_WHEEL` for `scroll_h()`.
- **Likely cause:** Wrong evdev relative-axis code in the Linux implementation.
- **Recommendation:** Validate all Linux event codes against `uinput` and add a platform-independent protocol-to-input test plus a Linux integration test.

### C-13 — Bluetooth scroll rounds each small touch delta down to zero — **MEDIUM**

- **Location/evidence:** `networking/Translators.kt:94-109` computes `dy / 10` and `dx / 10` for every individual HID scroll event, then returns an empty report array when the result is zero. `GestureHandler.kt:136-150` sends separate integer deltas and maintains no scroll accumulator.
- **Description:** Slow two-finger movement with per-callback deltas below 10 pixels produces no Bluetooth scroll report; the discarded remainder is not carried between callbacks. TCP/UDP translation does not use the same division, so sensitivity differs by transport.
- **Reproduction steps:** Not run. On Bluetooth, perform a slow, continuous two-finger swipe while capturing HID reports/host wheel input; compare with a large swipe and Wi-Fi.
- **Expected:** Small movements accumulate into smooth scroll across gesture callbacks.
- **Observed:** For `abs(dx) < 10` and `abs(dy) < 10`, integer division produces zero and no report is emitted.
- **Likely cause:** Per-event truncation without an accumulator.
- **Recommendation:** Preserve fractional/remaining deltas across callbacks and use cross-transport gesture tests.

### C-14 — Missing `config.ini` silently sets desktop sensitivity to zero — **MEDIUM**

- **Location/evidence:** `server/src/settingsmanager.cpp:19-37` reads only the current working directory’s `config.ini`; missing settings are accessed via `settings[...]`, then `atoi("")` sets `MOVE_SENSITIVITY` and `SCROLL_SENSITIVITY` to `0`. No `server/config.ini` is tracked.
- **Description:** A fresh source/development launch without a config file can make movement/scroll sensitivity zero, so received relative motion is multiplied by zero. Runtime behavior depends on the launch directory and whether packaging supplied a readable config.
- **Reproduction steps:** Not run. Build and launch the desktop server from a clean directory without `config.ini`, then send a relative-move event.
- **Expected:** Sensible documented defaults or a visible configuration error.
- **Observed:** Missing keys become empty strings and convert to integer zero; Linux/Windows input code scales deltas by those globals.
- **Likely cause:** No fallback values or config-path resolution relative to the executable.
- **Recommendation:** Provide validated defaults, resolve config relative to a known application-data path, and report parse/load failures explicitly.

### C-15 — Windows ADB invocation is unquoted and can disrupt other ADB clients — **MEDIUM / security and robustness**

- **Location/evidence:** `server/src/adb.hpp:30-35,51-62` concatenates `GetModuleFileNameA()`’s directory into a command passed to `system()` without quoting the executable path. `kill()` joins two shell commands with `&` and invokes `adb kill-server`; `SettingsManager::ADBOff()` calls it on close (`settingsmanager.cpp:138-150`).
- **Description:** An install path containing spaces can cause command parsing failure. Shell metacharacters in a path can alter the command parsed by `cmd.exe` (path-based command-injection susceptibility; no exploit was run). The global `adb kill-server` can stop ADB sessions owned by unrelated tools/users on that host.
- **Reproduction steps:** Not run. On a disposable Windows VM, test an installation path with spaces and a separate running ADB client; do not add shell metacharacters or execute an injection test on a real machine.
- **Expected:** Invoke only the intended executable with safely separated arguments and clean up only this app’s forwarding.
- **Observed:** A shell command string is built from the executable directory; `kill()` terminates the global ADB server.
- **Likely cause:** `system()` used instead of a process API with argument separation and app-owned lifecycle.
- **Recommendation:** Use a process API with explicit executable/argument vectors, validate the path, apply a timeout, select a device, and remove only the forwarding this instance created.

### C-16 — Desktop callbacks touch wxWidgets UI from the Asio worker thread — **MEDIUM**

- **Location/evidence:** `Server::Start()` runs `context.run()` in a new `std::thread` (`server.cpp:46-54`). The accept/disconnect callbacks invoke `wxApplication::OnDeviceConnected/OnDeviceDisconnected()` (`wxapplication.cpp:30-40`), which call `wxDeviceList::SetDevices()` and mutate controls directly. The logger also writes to a wx list view (`logger.cpp:8-18`) and uses global mutable state.
- **Description:** wxWidgets UI updates are made from the networking thread, not marshalled to the UI thread. This can race with window teardown or UI work and can produce intermittent GUI failures. The server’s `connections` vector is likewise modified by Asio callbacks and read/cleared from other paths without visible synchronization.
- **Reproduction steps:** Not run; requires desktop runtime. Connect/disconnect while opening/closing the UI and monitor for assertions/races under a thread sanitizer/debug build.
- **Expected:** All wx control updates occur on the GUI thread, with synchronized server state.
- **Observed:** Callback code calls controls directly from the context thread.
- **Likely cause:** Asynchronous network callbacks were connected to UI callbacks without a thread boundary.
- **Recommendation:** Marshal UI work to the wx event loop and synchronize/own the connection collection; add repeat connect/disconnect and shutdown stress tests.

### C-17 — Windows release script does not produce the documented folder/config layout — **MEDIUM**

- **Location/evidence:** `server/release.bat:4-19` creates `mousedroid_win64` but copies the build output into `dist\`; it writes `distconfig.ini`, not `dist\config.ini`. `SettingsManager` reads `config.ini` from the working directory. It copies ADB to `dist\adb\`, which also conflicts with `adb::start()`’s expected root-level `adb.exe` path.
- **Description:** The script’s output directory and settings/ADB paths disagree with the README’s stated `mousedroid_win64/` deployment and runtime lookup. The README also refers to an `install.bat` for missing DLLs, but no `install.bat` is tracked.
- **Reproduction steps:** Not run (Windows-only script). Run the release packaging script in a clean Windows build tree and inspect generated directory names and config/ADB paths.
- **Expected:** One self-contained, documented package with the config and executable dependencies at runtime lookup locations.
- **Observed:** Script creates one directory, copies to another, and writes config outside that output directory.
- **Likely cause:** Packaging directory names changed without synchronizing the script, settings loader, ADB resolver, and README.
- **Recommendation:** Define one package layout and verify it with a clean Windows packaging/install/smoke-test job.

### C-18 — Kotlin Gradle plugin version falls inside a published vulnerable range — **MEDIUM / dependency security**

- **Location/evidence:** `mousedroid/client/build.gradle:6-7` pins `org.jetbrains.kotlin.android` to `2.1.0`. GitHub Advisory Database reports `org.jetbrains.kotlin:kotlin-gradle-plugin < 2.4.20-Beta1` as affected by GHSA-r937-wjx7-w2jp / CVE-2026-53914.
- **Description:** The build plugin version matches the advisory’s vulnerable range. The advisory concerns unsafe deserialization in build-cache metadata; the checked-in project does not enable Gradle caching by default, so exposure depends on the build/cache environment.
- **Reproduction steps:** Not performed; no Gradle/JDK is installed, and no untrusted cache artifact was used.
- **Expected:** Build tooling should be outside affected ranges, and untrusted cache metadata should not be consumed.
- **Observed:** Declared plugin version `2.1.0` is less than the advisory boundary.
- **Likely cause:** The project’s pinned Kotlin plugin predates the advisory fix.
- **Recommendation:** In a separate remediation mission, select a fixed stable plugin version compatible with AGP/Gradle, review build-cache policy, and verify release reproducibility. No dependency was changed in this audit.

### C-19 — One native read buffer leaks for every TCP client — **LOW**

- **Location/evidence:** `server/src/net/connection.cpp:24` allocates `byteBuffer = new char[MAX_BUFFER_SIZE]`; `Connection` has no destructor in `connection.h`, and no `delete[]` is present before connection erasure.
- **Description:** Each TCP registration leaks a small (255-byte) heap buffer for the lifetime of the server process. Repeated client sessions accumulate the leak.
- **Reproduction steps:** Not measured; requires repeated connect/disconnect under a native memory profiler.
- **Expected:** Buffer memory is released when each `Connection` is destroyed.
- **Observed:** Allocation has no matching deallocation in the class lifecycle.
- **Likely cause:** Raw owning pointer without destructor/RAII.
- **Recommendation:** Use RAII-owned storage and add a repeated-session memory test.

### C-20 — Android wrapper is not executable and local SDK path is machine-specific — **MEDIUM / build reproducibility**

- **Location/evidence:** Git records `mousedroid/client/gradlew` as mode `100644`, so `./gradlew ...` returns permission denied on Unix. `client/local.properties` is tracked and sets `sdk.dir=C:\Users\Darius\AppData\Local\Android\Sdk`; its own header says not to version it. The committed tree also contains generated `.gradle` and `.idea` files.
- **Description:** A standard Unix wrapper invocation does not start, and the checked-in Android SDK path is specific to a Windows user. Even after installing Java/Android SDK, this file is not portable without local override.
- **Reproduction steps:** Executed here: `./gradlew testDebugUnitTest` returned permission denied; `bash ./gradlew testDebugUnitTest` then reported no Java. Android SDK resolution could not be reached.
- **Expected:** A fresh checkout can invoke the wrapper and resolve the SDK from the developer/CI environment.
- **Observed:** Wrapper lacks execute permission; local SDK path is hard-coded to a Windows home directory.
- **Likely cause:** Local IDE/build state was committed and wrapper executable mode was not preserved.
- **Recommendation:** Restore the wrapper executable bit, exclude local SDK/IDE/build state, and use documented environment-based SDK setup in CI. No imported source file was changed during this audit.

### C-21 — Close always opens a modal disconnect confirmation — **LOW / desktop usability**

- **Location/evidence:** `mousedroid/server/src/wxapplication.cpp:42-55` always calls `wxMessageDialog::ShowModal()` on the close event before the server can close. This matches open upstream report [#22](https://github.com/darusc/Mousedroid/issues/22) about confirmation blocking Windows shutdown.
- **Description:** The server can block shutdown/close behind a modal question, even when the OS is attempting to shut down. The behavior was not reproduced on Windows here.
- **Reproduction steps:** Close the application or initiate OS shutdown while it is running; not performed.
- **Expected:** Handle application close and system shutdown without an unexpected modal stall, while still warning where appropriate.
- **Observed:** The same modal dialog is shown on every close event.
- **Likely cause:** No distinction between user-close and system-session-ending events.
- **Recommendation:** Review close-event handling and test shutdown/logoff in a Windows VM.

### C-22 — Motion deltas are truncated/clamped per callback, and sensitivity differs by transport — **MEDIUM**

- **Location/evidence:** `GestureHandler.kt:152-159` converts floating-point distances to integers and clamps each callback to `[-128,127]`; it neither accumulates fractional movement nor splits large deltas. Bluetooth HID translation sends those raw deltas (`networking/Translators.kt:72-79`), while USB/Wi-Fi movement is additionally scaled by desktop `MOVE_SENSITIVITY` (`server/src/input/linux/linuxinputmanager.cpp:92-97` and `server/src/input/win32/windowsinputmanager.cpp:34-44`). The desktop slider therefore does not apply to Bluetooth.
- **Description:** Repeated subpixel deltas are discarded by `toInt()`, and movement beyond the byte-range limit is saturated rather than carried into subsequent reports. Bluetooth and socket modes also have different sensitivity controls/gain paths. This can make very slow movements imprecise and very fast swipes under-report movement; the exact perceptual impact requires hardware testing.
- **Reproduction steps:** Not run. On Galaxy A12, compare a slow subpixel-scale swipe and a fast swipe on all three transports, then change the desktop sensitivity slider and compare Bluetooth with Wi-Fi/USB.
- **Expected:** Continuous movement without losing fractional or overflow distance, with documented/consistent sensitivity behavior across transports.
- **Observed:** Per-callback integer truncation and saturation are explicit; Bluetooth bypasses the desktop sensitivity multiplier.
- **Likely cause:** One-byte deltas are clamped at gesture dispatch rather than accumulated/split, and sensitivity is implemented only in the desktop socket backends.
- **Recommendation:** Define a shared sensitivity model, preserve residual movement, split large deltas into valid protocol reports, and validate with slow/fast motion tests across transports.

## 7. Suspicions requiring reproduction (not counted in totals)

1. **Tap/double-tap timing:** `EV_DELAY_MILLIS` is 150 ms (`GestureHandler.kt:25,83-114`). Depending on platform double-tap timing, the first click may be emitted before a second tap is classified, potentially making a double tap produce an extra click. Needs event-timing tests on Galaxy A12 and other Android versions.
2. **Scroll/zoom/drag recognizers may overlap:** `GestureHandler` invokes `GestureDetector` and `ScaleGestureDetector` on the same stream (`195-199`); two-pointer scroll is handled in `onScroll` while scale changes emit `Zoom`. The open report [#38](https://github.com/darusc/Mousedroid/issues/38) describes drag/zoom overlap. Code makes overlap plausible, but the reported behavior was not reproduced.
3. **Connection Manager lifecycle/order races:** connect/send/disconnect use independent `CoroutineScope`s; sends are launched separately per event; `connectWIFI()` stores TCP before creating UDP and only catches `ConnectionFailedException`. Partial connection cleanup, event reordering, duplicate connect, and old references need stress tests. No reconnect loop is present in the inspected manager.
4. **Android view/resource retention:** Several fragments keep `lateinit` view bindings and `PopupWindow`s without clearing/dismissing them in `onDestroyView`; some delayed gesture work is not lifecycle-owned. This is a memory/window-leak risk, not confirmed with LeakCanary or rotation/navigation tests.
5. **Desktop shutdown/vector races:** Asio handlers capture raw `this`; server shutdown closes/clears connections while the context thread may be active, and `connections` has no visible mutex. A race/use-after-free is plausible but was not reproduced under sanitizers.

## 8. Upstream issue investigation

The issue list was reviewed through GitHub. Open reports were treated as user reports, not as proof by themselves.

| Upstream issue | Report | Audit result |
|---|---|---|
| [#40](https://github.com/darusc/Mousedroid/issues/40), [#47](https://github.com/darusc/Mousedroid/issues/47) | Windows ADB startup says failed; `adb.exe` is in a subdirectory | Strongly corroborated by `adb.hpp` looking in the executable directory root while tracked/release layout nests ADB under `adb/`. Not reproduced on Windows. |
| [#44](https://github.com/darusc/Mousedroid/issues/44) | Linux invokes `./adb` rather than system `adb` | Corroborated by `SettingsManager::ADBOn/ADBOff` using `./adb`; not reproduced on Linux with ADB installed. |
| [#46](https://github.com/darusc/Mousedroid/issues/46) | Restart reports port 6969 already in use / unhandled bind error | Code binds fixed TCP/UDP port without catching constructor errors. Reported Windows scenario not reproduced; another process/leftover process was not identified here. |
| [#27](https://github.com/darusc/Mousedroid/issues/27) | Startup error when no Wi-Fi; restart/ADB process concern | `GetHostInfo()` probes route to `8.8.8.8` without an error path; ADB cleanup uses a global `kill-server`. Correlation is plausible; issue environment not reproduced. |
| [#38](https://github.com/darusc/Mousedroid/issues/38) | Drag/drop and zoom overlap on Windows 11 | README says long-press drag, while code starts drag from a double-tap path; scale and scroll detectors share touch streams. Strong code-level concerns, but exact report not reproduced. |
| [#37](https://github.com/darusc/Mousedroid/issues/37) | Listening on the wrong interface | Current TCP/UDP endpoints are wildcard IPv4 binds. User screenshot/report not reproduced; binding behavior is directly visible in code. |
| [#39](https://github.com/darusc/Mousedroid/issues/39) | Linux Mint Bluetooth connection failure | No Linux desktop/Bluetooth hardware was available. Bluetooth profile readiness and state handling have code risks above, but the reported host-specific failure remains unconfirmed. |
| [#33](https://github.com/darusc/Mousedroid/issues/33) | v1.5 reproducible-build/signing-key concern raised by IzzyOnDroid | The issue/comments report a signature mismatch and stalled updates. No release APK was verified in this workspace; this is an unresolved provenance signal, **not** a finding of compromise. |
| [#22](https://github.com/darusc/Mousedroid/issues/22) | Confirmation dialog blocks Windows shutdown | The unconditional modal close handler matches the report; Windows shutdown behavior was not tested. |
| [#9](https://github.com/darusc/Mousedroid/issues/9), [#12](https://github.com/darusc/Mousedroid/issues/12) | Older Windows releases missing DLLs | Those reports are closed historical issues. Current packaging/runtime DLL completeness could not be tested; current release script has layout inconsistencies recorded in C-17. |

## 9. Security assessment

### Confirmed security findings

1. **High — unauthenticated remote input:** desktop TCP/UDP listens on wildcard IPv4 port 6969 and dispatches received input to the logged-in desktop session without a client secret or peer check (C-01). Do not expose it to a public/untrusted network.
2. **High — malformed-message memory safety/availability risk:** desktop input parser ignores payload lengths and unchecked identity tokens can be indexed (C-02). This also creates a remote denial-of-service/crash surface.
3. **Medium — unsafe Windows shell command construction:** ADB path is concatenated into `system()`; spaces break execution and shell metacharacters can change parsing if present in the installation path (C-15). No exploit was attempted.
4. **Medium — build-tool advisory:** Kotlin Gradle plugin `2.1.0` is in GHSA-r937-wjx7-w2jp’s affected range; impact is conditional on untrusted build-cache metadata (C-18).
5. **Operational risk — global ADB daemon:** `adb kill-server` can interrupt unrelated Android tooling/ADB sessions. No command was executed in this audit.

### Other observations

- Wi-Fi/USB custom input traffic has no TLS, authentication, sequence number, or replay protection in the inspected protocol. UDP accepts packets without associating them with a registered TCP client.
- Android manifest declares `INTERNET`, Bluetooth permissions, `FOREGROUND_SERVICE`, and `WAKE_LOCK`; `allowBackup="true"` is enabled. Saved Wi-Fi device names/IP addresses are in private SharedPreferences. No passwords, API tokens, or credential storage were found in source.
- The README’s instruction to allow the server through both Private **and Public** Windows firewall profiles is broader than necessary for a local input tool and increases exposure.
- Bundled ADB executable/DLL version and hash are not recorded. Issue #33 raises a release signing/reproducibility concern; the APK was not available for verification.
- No claim is made that every transitive/native dependency is free of known vulnerabilities; the dependency graph and artifacts were not resolved.

## 10. Limitations and manual tests pending (Galaxy A12 + Windows)

No physical Android, Galaxy A12, Windows host, paired Bluetooth adapter, USB cable/ADB authorization flow, desktop binary, or simulator was available. The following are **pending**, not passed:

1. **Record baseline:** exact Galaxy A12 Android/API level, Windows 10/11 build, Mousedroid APK/server revision, desktop CPU architecture, and firewall profile.
2. **Windows server startup:** cold start, clean close/reopen, second-instance launch, port 6969 occupied, no network, Wi-Fi-only, and app launched from paths with spaces. Capture `app.log`, Event Viewer output, and process/port state.
3. **USB/ADB:** enable USB debugging; accept/revoke RSA authorization; compare `adb devices` states `device`/`unauthorized`/`offline`; test the packaged `adb.exe` location and `adb reverse`; test with one and two devices; unplug/replug; switch USB modes; stop/restart server; confirm only this app’s forwarding/process is cleaned up and other ADB sessions survive.
4. **Wi-Fi protocol:** same LAN, private/public firewall profiles, wrong IP, server stopped, server restart, Wi-Fi loss/roam/sleep/wake, duplicate clients, malformed/partial TCP registration, and disconnect detection. Use an isolated test network for any malformed-packet test.
5. **Bluetooth:** grant/deny Connect and Advertise permissions on API 31+; pair/unpair; verify host selection; test slow/fast pointer, buttons, scroll, media, disconnect/reconnect, and profile readiness. Compare Android 11/API 30 with API 31 and API 32+ if available.
6. **Touch gestures (each transport):** single left click, double click at several intervals, two-finger right click, vertical scroll, horizontal scroll, long-press drag, double-tap drag, pinch zoom, slow movement, fast movement, diagonal movement, multi-touch, lifting one pointer, interruption/ACTION_CANCEL, app navigation while dragging, and connection loss while a button is held. Check that no input remains stuck.
7. **Cursor/sensitivity:** measure small/large deltas, direction, acceleration and clipping; compare Bluetooth vs Wi-Fi vs USB; move/scroll sliders through all values; test fresh launch with and without `config.ini`.
8. **Windows behavior:** normal vs elevated applications (UIPI), lock screen, multi-monitor/rotation, suspend/resume, firewall policy, taskbar/tray, startup registry entry, shutdown/logoff while server is open, and resource usage over a long session.
9. **Android lifecycle/battery:** rotate via the app fullscreen button and system rotation; background/foreground; screen off/on; low battery; repeated navigation/connect/disconnect; inspect memory and active threads with LeakCanary/Android Studio profiler; verify whether the app is expected to remain connected in background without a foreground service.
10. **Packaging/build:** clean Windows CMake build and release package; install/launch package without developer tools; verify DLLs, ADB, icon, config path and settings persistence. On Linux, separately test `/dev/uinput` permissions, desktop launcher working directory, system ADB discovery, horizontal scroll, and suspend/resume.

## 11. Recommendations for a later remediation mission (not applied)

1. Treat network authorization and packet validation as security gates before using the desktop server on shared networks.
2. Specify and test a framed, versioned protocol; validate every field; add protocol unit/fuzz/integration tests for fragmentation, truncation, reordering, and malformed packets.
3. Replace shell-built ADB commands with controlled executable discovery/process invocation; select the device; avoid global ADB teardown; test path spaces, no-device, unauthorized, multiple-device, disconnect, and reconnect cases.
4. Make server startup/teardown explicit and exception-safe; validate port availability; marshal all GUI operations to the UI thread; synchronize connection ownership.
5. Model input gestures as mutually exclusive states with balanced down/up on every terminal event. Test each documented gesture separately on physical hardware.
6. Unify transport semantics for middle click, scroll, sensitivity, and zoom; preserve small deltas and correct platform-axis mappings.
7. Add validated config defaults and executable-relative/user-data path resolution; repair Windows/Linux package layouts and test clean distributions.
8. Resolve the Kotlin Gradle plugin advisory and establish dependency locking/SBOM/advisory scans; verify release signing/reproducibility with the upstream maintainers before distributing builds.
9. Add CI for Android build, Android unit/instrumentation tests, lint, Linux and Windows CMake builds, and desktop protocol tests. Keep generated `.gradle`/`.idea` state and local SDK paths out of source control.

## 12. Baseline conclusion

Mousedroid is a compact, understandable cross-platform prototype with a clear separation between Android client, socket/HID transports, and native desktop input backends. The original repository carries its MIT license and project history, and the import has preserved that history without rewriting the destination’s existing commit.

The current source is **not yet a safe or reliable Pocket Pad baseline for immediate feature development**. In particular, network input is unauthenticated, parser bounds/framing are inadequate, ADB packaging/path handling appears broken on the documented layouts, and the Windows build/release configuration has unresolved path mismatches. Gesture and connection lifecycle behavior also needs hardware validation. The Android build and desktop server build remain unverified in this environment; no USB, Bluetooth, Wi-Fi cursor, gesture, reconnection, or Windows runtime behavior should be described as validated until the manual/CI checks above pass.
