# Developer Guide

## Prerequisites

| Tool              | Minimum Version | Notes                                |
|-------------------|-----------------|--------------------------------------|
| CMake             | 3.25            |                                      |
| C++ compiler      | MSVC 2022 / GCC 13 / Clang 17 | Must support C++20     |
| Qt 6              | 6.4             | Install via Qt Online Installer      |
| Git               | 2.40            |                                      |
| libssh2 (optional)| 1.11            | Built automatically via FetchContent |

## Building

The project ships CMake presets (`CMakePresets.json`, Ninja generator). Preset
builds land in `build/<presetName>/`, e.g. the `debug` preset builds into
`build/debug/`.

```powershell
# 1. Clone
git clone <repo-url> cosmostool
cd cosmostool

# 2. Configure + build with a preset (from an MSVC developer shell so cl.exe is on PATH)
cmake --preset debug
cmake --build --preset debug

# Run the app (preset build output)
.\build\debug\bin\OpenC3DevToolkit.exe
```

The pre-existing local build tree used during development lives in
`build/local-debug/`. When that tree is present, the app and tests are:

```powershell
.\build\local-debug\bin\OpenC3DevToolkit.exe   # application
.\build\local-debug\bin\opencosmos_tests.exe   # unit tests
```

> If you configure manually instead of using a preset, pass
> `-DCMAKE_PREFIX_PATH` pointing at your Qt 6 install
> (e.g. `C:/Qt/6.7.0/msvc2022_64`). Qt DLLs are copied next to the executable by
> `windeployqt` in a POST_BUILD step.

> **Windows troubleshooting — CMake exits with `0xC0000409` during configure:**
> CMake 3.30/3.31 (including the Visual Studio-bundled and Qt-bundled copies)
> can crash with `STATUS_STACK_BUFFER_OVERRUN` inside `find_program()` once the
> Qt6 package has been loaded — on affected machines the configure dies right
> after the `WrapAtomic`/`WrapVulkanHeaders` probes or at the
> `windeployqt`/`lrelease` lookups, with no error message. The fix is to use
> CMake ≥ 4.x (e.g. `pip install cmake`, or any recent standalone release) —
> the same configure then succeeds unchanged. Non-ASCII characters in the
> repository path are *not* the problem for configuring or compiling.
>
> One residual quirk remains if the repository lives under a **non-ASCII path**
> (e.g. a Korean folder name): CMake ≥ 4.x's `gtest_discover_tests` post-build
> step runs the test binary with `--gtest_output=json:<build-dir>/...`, and
> GoogleTest 1.14 aborts (exit code 3) when that output path contains
> non-ASCII characters — so the *last* build step fails even though the test
> executable itself linked fine. Work around it by running
> `bin\opencosmos_tests.exe` directly (the documented fallback above), or keep
> the working tree on an ASCII-only path if you need `ctest` integration.

## Windows Release Packaging

`packaging/package.ps1` is the local end-to-end release entry point. It
configures and builds the `release` preset, stages the `windeployqt` output and
MSVC runtime DLLs into `dist/`, creates the portable ZIP, and builds the NSIS
installer when NSIS 3.x is installed:

```powershell
# Optional, one-time installer-tool setup
winget install --id NSIS.NSIS --exact

# Clean release build + portable ZIP + installer
powershell -ExecutionPolicy Bypass -File packaging\package.ps1

# Repackage an already-built build/release/bin tree
powershell -ExecutionPolicy Bypass -File packaging\package.ps1 -SkipBuild

# Repackage a different CMake tree whose executable is under <tree>/bin
powershell -ExecutionPolicy Bypass -File packaging\package.ps1 `
    -SkipBuild -BuildDir build\ci-release
```

The script finds `makensis` on `PATH` or under the standard 32/64-bit Program
Files NSIS directories. NSIS input is explicitly parsed as UTF-8, and every
`/D` define is passed as a separate native-process argument so repository paths
containing spaces or non-ASCII characters remain valid. A non-zero NSIS exit or
a missing installer is a packaging failure, not a warning.

Expected outputs:

```text
dist/OpenC3DevToolkit.exe
OpenC3DevToolkit-portable-windows.zip
OpenC3DevToolkit-0.1.0-Setup.exe
OpenC3DevToolkit-0.1.0-SHA256SUMS.txt
```

The `Build Executable` GitHub Actions workflow runs the same packaging script
against its already-built Windows tree. It publishes the portable ZIP and NSIS
Setup executable and their SHA-256 manifest as separate artifacts, in addition
to the raw executable collection. The installer artifact is built and
structurally validated in CI;
installing it is still a manual acceptance step because it writes to Program
Files and HKLM and therefore requires interactive UAC approval.

CI also creates GitHub build-provenance attestations for the ZIP and installer.
After downloading either package, verify that it came from this repository's
workflow with GitHub CLI:

```powershell
gh attestation verify .\OpenC3DevToolkit-portable-windows.zip `
    --repo underkim/cosmostool
gh attestation verify .\OpenC3DevToolkit-0.1.0-Setup.exe `
    --repo underkim/cosmostool
```

Provenance proves which GitHub Actions workflow produced the exact artifact;
it does not replace Authenticode, which is what Windows uses to identify a
publisher before launch.

Before publishing, verify at least `dist/platforms/qwindows.dll`, the Qt
Core/Gui/Widgets/Network DLLs, and the MSVC runtime DLLs are present; test the
ZIP integrity; and launch the portable executable long enough to reach the Qt
event loop. The installer intentionally writes to Program Files and HKLM, so a
real install/uninstall acceptance run requires interactive Windows UAC approval.
The local and CI packaging flows do not currently Authenticode-sign either
executable; public distribution should add a protected code-signing certificate
and timestamping step rather than embedding signing credentials in the repo.

## Running Tests

Primary fallback (direct binary; works without CMake/CTest on PATH if the
`local-debug` build already exists):

```powershell
.\build\local-debug\bin\opencosmos_tests.exe
```

> **If the test binary exits immediately with code 127** and
> `error while loading shared libraries: Qt6Testd.dll`: the Qt DLLs are only
> deployed next to `OpenC3DevToolkit.exe` by the `windeployqt` POST_BUILD step,
> and the *test* executable gets no such step — so it needs Qt's `bin` on PATH
> when run from a plain shell (a Qt/VS developer prompt already has it):
>
> ```powershell
> $env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;$env:PATH"
> .\build\api-debug\bin\opencosmos_tests.exe
> ```

Preferred CTest flow after running `setup.ps1` and configuring the local preset
(requires `cmake` and `ctest` on PATH, e.g. from a Visual Studio developer
shell or a refreshed VSCode terminal):

```powershell
ctest --preset local-unit
```

Repository presets also provide `ctest --preset unit` for the shared `debug`
build tree. Use `local-unit` when working with `CMakeUserPresets.json`
generated by `setup.ps1`.

### Live transport check (manual)

`HttpCosmosApiClient` talks to COSMOS over real sockets, and the gtest suite
covers it only through `MockCosmosApiClient` — which proves delegation but
cannot prove the things that actually break against a live COSMOS: the
`Authorization` header shape, automatic scope injection, and multipart `.gem`
upload framing. A separate executable exercises those against a real localhost
HTTP server. It is `EXCLUDE_FROM_ALL`, so build it explicitly:

```powershell
cmake --build build\api-debug --target opencosmos_api_livecheck
.\build\api-debug\bin\opencosmos_api_livecheck.exe
```

It needs no running COSMOS instance — it stands up its own capture server.

### Real COSMOS integration check (manual)

`opencosmos_live_cosmos_check` is the actual end-to-end companion to the local
capture-server check above. It connects production `HttpCosmosApiClient`,
`WslExecutor`, and the Doctor/Runtime/Plugin/Script/Command services to a
running COSMOS 6.10.4 Core stack. It verifies authenticated API/scopes,
Docker and file access through WSL, all expected containers, both Redis
instances (the restricted `healthcheck` ACL user on ports 6379/6380), Doctor
L3 runtime state, installed plugins, target/packet/telemetry reads, Script
Runner listing, and the command service's non-existent-command error path.
The default mode never sends a valid command.

The credential is accepted only through an environment variable so it is not
embedded in source or exposed in a process command line:

```powershell
cmake --build build\api-debug --target opencosmos_live_cosmos_check
$env:OPENC3_LIVE_PASSWORD = '<live Core token>'
.\build\api-debug\bin\opencosmos_live_cosmos_check.exe `
    http://127.0.0.1:2900 DEFAULT
```

By default this is read-only. Setting `OPENC3_LIVE_MUTATING=1` additionally
creates a reserved temporary plugin, builds/downloads/uploads it, polls the
real install `ProcessStatusModel`, runs harmless temporary Script Runner
procedures (including pause/resume/stop), waits for a dedicated in-memory
interface to reach `CONNECTED`, sends a valid NOOP command, verifies hazardous
refusal and explicit-confirmation resend, uninstalls by the exact suffixed
instance id returned by `listInstalled()`, polls uninstall to completion, and
checks that plugin/script/source residues are gone. The temporary interface
opens no socket or file and maps only the reserved `HERMES_LIVECHECK` target;
the probe never sends a command to an existing deployment target. It never
touches a plugin whose gem name does not start with the probe's reserved
`openc3-cosmos-hermes-livecheck-` prefix:

```powershell
$env:OPENC3_LIVE_MUTATING = '1'
.\build\api-debug\bin\opencosmos_live_cosmos_check.exe `
    http://127.0.0.1:2900 DEFAULT
```

The production Workspace build still requires RubyGems on the connected WSL
host. If it is absent, the probe verifies the real actionable error and uses
the COSMOS cmd-tlm-api container's Ruby only as integration-test scaffolding
so the API lifecycle can continue; the application itself does not silently
fall back to building inside a COSMOS container.

### Startup wiring check (manual)

> **Do not try to verify startup by running `OpenC3DevToolkit.exe` and reading
> its output — you will see nothing and draw the wrong conclusion.** The app is
> built `WIN32_EXECUTABLE TRUE` (no console), so spdlog's console sink goes
> nowhere, and the file sink is `flush_on(warn)`, so `info` lines sit in a
> buffer that is lost when the process is killed. A GUI run therefore produces
> an empty log whether startup succeeded or failed. Two further traps:
> `timeout`-killing the app yields exit code 124 and `taskkill /F` yields 1 —
> **neither indicates a crash**, and 124 in particular means it started fine
> and reached the event loop.

Use this console executable instead. It builds the same composition root
(services, proxies, DI registrations) and prints a real result:

```powershell
cmake --build build\api-debug --target opencosmos_startup_check
.\build\api-debug\bin\opencosmos_startup_check.exe   # exit 0 = wiring OK
```

## Adding a New Module

1. **Define the interface** in `src/services/<module>/I<Module>Service.h`
2. **Implement the service** in `src/services/<module>/<Module>Service.h/.cpp`
3. **Create the ViewModel** in `src/viewmodels/<module>/<Module>ViewModel.h/.cpp`
4. **Create the View** in `src/ui/views/<Module>View.h/.cpp`
5. **Register in Application.cpp** (`registerServices()` and `registerViewModels()`)
6. **Add nav item** in `MainWindow::setupNavigation()`
7. **Add view** to `MainWindow::setupViews()` via `contentStack_->addWidget()`
8. **Write mock** in `tests/mocks/Mock<Module>Service.h`
9. **Write tests** in `tests/unit/services/<Module>ServiceTest.cpp`

## Module Status

All initially planned phases (1–11) are implemented: the architecture is in
place and every module below is registered in `Application.cpp` and reachable
from `MainWindow`'s navigation rail. The table reflects the current state of
each area rather than the original phase plan.

The app has two top-level **modes** (a toggle button under the nav rail, or
the View menu's exclusive Plugin Creation / Connect & Operate actions),
persisted across restarts and defaulting to **Plugin Creation**:

- **Plugin Creation mode** shows only **Home, Workspace, Settings** in the
  nav rail. This mode is meant to feel like a dedicated plugin-creation
  editor (comparable to opening a project in an IDE): it never blocks on a
  manual "Connect" step - `MainWindow::autoConnectIfNeeded()` silently
  connects in the background using the default saved profile, the first
  saved profile, or (if none exist yet) a freshly created Quick-WSL profile,
  the same one-click sequence `ConnectionDialog` already offers, just
  headless. A connection failure never pops a blocking dialog either; it
  just leaves the status-bar connection button showing the error, which
  doubles as the click-to-configure recovery path. Workspace in this mode
  only shows wizard steps **1. Plugin → 2. File → 3. Edit**.
- **Connect & Operate mode** shows **Home, Settings, Environment, Validator,
  Packet Tools, Logs, Check & Build**. This is the traditional
  connected-environment surface, and it keeps the original startup
  `ConnectionDialog` behavior (an explicit, user-confirmed connection is
  expected here). "Check & Build" is a *second* nav entry onto the exact
  same `PluginView` page Workspace uses - `PluginView::setStepStripMode()`
  just filters which step range is visible (steps **4. Check → 5. Build &
  Install** here) - so switching between Workspace and Check & Build never
  loses an open file's edits; it's the same widget instance throughout.

No feature is removed by hiding rows for the inactive mode; everything
remains reachable, and navigating to a page hidden by the current mode
programmatically (e.g. Home's "Run Doctor" quick action, or a Ctrl+N
shortcut) transparently switches mode first rather than landing on an
invisible-in-rail page. Workspace/Check & Build together walk plugin
management and CMD/TLM editing as a 5-step wizard - a step strip above the
content area lets you jump directly to any step already reachable within the
current mode's range (no plugin selected caps navigation at Plugin, no file
open caps it at File; Back is always free), selecting a plugin or opening a
file auto-advances to the next step, and a persistent breadcrumb
("plugin › file") above the strip keeps you oriented regardless of which step
is active. The collapsible log-streaming terminal and the Quick Reference
panel remain global, toggleable from any step. The **Home** page's guidance
is mode-aware too: Plugin Creation mode collapses to a single "Open
Workspace" entry point (no numbered "Connect" step, since connecting is
transparent); Connect & Operate mode keeps the original 3-step-card
guidance (Connect → Run Doctor → Open Check & Build).

Note for anyone with muscle memory for an older layout: the nav rail's Ctrl+N
shortcuts are positional within the rail's fixed row order (not the currently
visible subset), and a nav row's index no longer always matches its content
page index one-to-one (Workspace and Check & Build intentionally share one
page) - check the rail's tooltips (hover any row) for the current mapping.

| Module          | Status         | Notes                                                              |
|-----------------|----------------|--------------------------------------------------------------------|
| Architecture    | Implemented    | Layered app -> ui -> viewmodels -> services -> core/models         |
| Home (Dashboard)| Implemented    | Mode-aware status summary and quick actions; Connection / Docker / system metrics |
| Docker Manager  | Implemented    | Under the Environment nav entry (Connect & Operate mode). Container list/control; shell commands built via the `shellQuote` helper (`src/core/connection/ShellQuote.h`) |
| Infra Manager   | Implemented    | Under the Environment nav entry (Connect & Operate mode). ENV/Compose editing, volume overrides, plugin scaffolding |
| Doctor          | Implemented    | Under the Environment nav entry. Three-layer checks: infrastructure (Docker/CPU/memory/disk), COSMOS stack (all expected `openc3-*` containers, persistent/ephemeral Redis PING, MinIO, version), and COSMOS logical state through the API (reachability/auth, scope, installed plugins, microservices, interfaces). The microservices and interfaces checks go past "is it defined": microservices calls `microservice_status#show` per service (`MicroserviceStatusModel`) to catch one that crashed after starting, and interfaces calls JSON-DRB `get_all_interface_info` (`InterfaceStatusModel`) for live CONNECTED/DISCONNECTED state that the plain `GET /interfaces` route (`InterfaceModel` only) cannot report - falling back to the plain route (Warning, not a silent Pass) if the JSON-DRB call itself fails. API checks are Skipped for a valid shell-only profile rather than reported as failures. |
| Workspace       | Implemented    | 5-step wizard (Plugin -> File -> Edit -> Check -> Build & Install). Steps 1-3 operate on plugin *source folders* through `IWorkspaceService` (shell). Build creates an `openc3-cosmos-*` gem; Build & Install then downloads that remote artifact to a temporary local file and submits it through the real two-phase COSMOS API operation (MinIO/Redis), while Install Local Gem keeps the manual path. A separate Installed in COSMOS table is populated through `ICosmosPluginService`; uninstall is available only there and sends the exact suffixed instance id (`gem__0`), never a source-folder name. Install/uninstall responses provide a ProcessManager handle; the ViewModel polls `/openc3-api/process_status/:id` through terminal states (`Complete`, `Warning`, `Error`, `Crashed`, `Expired`), displays detail/output, and refreshes the Installed table automatically when the job finishes. A COSMOS Script Runner panel (`IScriptService`/`ScriptViewModel`, `script-api` transport) lists scripts actually deployed in the connected scope's bucket storage, starts one disconnected (`/scripts/*name/run/disconnect` - no interactive prompt/breakpoint channel in this client), and polls `OpenC3::ScriptStatusModel` through to a terminal state (`completed`, `completed_errors`, `stopped`, `crashed`, `killed`), separate from the still-present host-shell "Run Script" quick check (`ruby <file>` on the connected WSL/SSH host, useful as a syntax/smoke check but not real Script Runner - `cmd()`/`tlm()` only resolve if openc3 script libraries are loadable there). A "COSMOS Targets" panel (`ICosmosRuntimeService`/`RuntimeViewModel`, JSON-DRB `POST /openc3-api/api`) lists real targets and their command/telemetry packet names and polls current telemetry item values with limits color coding every second while a telemetry packet is selected. Selecting a *command* packet enables Send Command, which goes through the separate `ICosmosCommandService`/`CommandSenderViewModel` write path: every send first calls COSMOS's safety-checked `cmd`, and a `HazardousError` (JSON-RPC code -32502) is surfaced as an explicit confirmation prompt rather than silently retried - only an explicit second confirmation resends with `cmd_no_hazardous_check`. A disabled command (`DisabledError`) is reported distinctly rather than as a generic failure. Steps 1-3 live under Workspace (Plugin Creation mode); steps 4-5 under Check & Build (Connect & Operate mode), using the same `PluginView` instance. The Edit step provides structured Manifest/CMD-TLM/screen editing and static validation. |
| Validator       | Implemented    | Standalone nav entry (Connect & Operate mode) - offline checks for CMD/TLM, screens, plugin.txt |
| Packet Tools    | Implemented    | Connect & Operate mode. Packet log file analysis plus a packet simulator (`PacketSimulator`, viewmodels layer) with UDP listener and TCP server/client endpoints, hex payload validation, and send/receive logging with ASCII preview. No `services/packettools` layer — the simulator's socket I/O is Qt networking (`QUdpSocket`/`QTcpServer`), which belongs above the Qt-free services boundary |
| Log Viewer      | Implemented    | Connect & Operate mode. Real-time log streaming                    |
| Settings        | Implemented    | Profile CRUD, WSL/SSH detection, JSON persistence, and COSMOS API URL/scope/OPENC3_API_PASSWORD/TLS controls. SSH and API secrets use protected persistence (DPAPI on Windows). Visible in both modes |
| Release packaging | Implemented  | NSIS installer + `windeployqt` packaging scripts                   |

> The shell-quoting and Doctor path-hardcoding hardening tracked in the
> project work plan (`CLAUDE_TASKS.md`) has since landed - see that file's
> own "Definition of Done" notes for what was changed.

## Code Quality

```powershell
# Format all source files
Get-ChildItem -Recurse src,tests -Include *.h,*.cpp |
    ForEach-Object { clang-format -i $_.FullName }

# Enable static analysis during build
cmake -B build -DENABLE_CLANG_TIDY=ON
cmake --build build
```
