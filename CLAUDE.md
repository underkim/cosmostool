# CLAUDE.md

Guidance for Claude Code (and other AI assistants) working in this repository.

## What this is

**OpenC3 Developer Toolkit** — a C++20 / Qt 6 Widgets desktop app (Windows-first,
Linux buildable) that connects to OpenC3 COSMOS environments over WSL or SSH and
provides a dashboard, Docker manager, infra editor, health-check ("Doctor")
runner, plugin creation/scaffolding wizard, CMD/TLM editor, packet-log tools,
log streaming, and connection-profile settings.

Two top-level UI **modes** (persisted, default = Plugin Creation):
- **Plugin Creation**: Home, Workspace, Settings only — auto-connects silently
  in the background, feels like an IDE.
- **Connect & Operate**: Home, Settings, Environment, Validator, Packet Tools,
  Logs, Check & Build — the traditional connected-environment surface with an
  explicit `ConnectionDialog` at startup.

Workspace and Check & Build are the *same* `PluginView` widget instance,
filtered by `PluginView::setStepStripMode()` — switching modes never loses
open-file edits. See `docs/developer-guide.md`'s "Module Status" table for the
current implementation status of every module; keep it updated when module
status changes.

## Architecture (strict layering — read before touching anything)

```
app → ui → viewmodels → services → core
                    ↘              ↗
                      models (shared, header-only)
```

Lower layers must **never** reference upper layers. Qt only appears starting
at the `viewmodels` layer — `services` and `core` are plain C++20 with no Qt
dependency, which is what makes them unit-testable without a GUI.

| Layer | Path | Contains |
|---|---|---|
| `models/` | `src/models/` | Header-only value structs shared across all layers (`ConnectionProfile`, `DockerContainer`, `SystemMetrics`, `HealthCheckResult`, `Plugin`) |
| `core/` | `src/core/` | Platform abstraction: `ICommandExecutor` + `WslExecutor`/`SshExecutor`/`NullCommandExecutor`/`ExecutorProxy`, logging, events |
| `services/` | `src/services/` | Business logic behind `I<Name>Service` interfaces: connection, docker, doctor, plugin, settings, filesystem, system |
| `viewmodels/` | `src/viewmodels/` | Qt/`QObject` bridge — async via `QtConcurrent::run`, marshals results back to the GUI thread with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`, emits signals |
| `ui/` | `src/ui/` | Qt Widgets views/dialogs/widgets. Talks only to ViewModels — never calls a Service directly or implements business logic |
| `app/` | `src/app/` | Composition root: `Application::run()` wires concrete services into `ServiceRegistry` (a type-indexed DI container), constructs ViewModels, shows `ConnectionDialog`/`MainWindow` |

Key patterns in play (details + diagrams in `ARCHITECTURE.md`):
- **Null Object** — `NullCommandExecutor` lets services run safely before a
  connection exists; `NullCosmosApiClient` does the same for the API transport.
- **Strategy** — `ICommandExecutor` implementations are swappable at runtime.
- **Proxy** — `ExecutorProxy` holds an `std::atomic` pointer so services keep
  a stable reference across connect/disconnect without races; `ApiClientProxy`
  mirrors it exactly for `ICosmosApiClient`.
- **Observer** — services expose plain C++ `onStateChanged` callbacks;
  ViewModels translate them into Qt signals on the GUI thread.
- **MVVM** — View ↔ ViewModel via signals/slots and `Q_PROPERTY`; ViewModel ↔
  Service via plain function calls/callbacks.

## Two transports (shell + COSMOS API)

This app targets **OpenC3 COSMOS 6.10.4 Core** and speaks to it over *two*
transports. Using the wrong one is the single biggest correctness trap here:

| Transport | Interface | Correct for |
|---|---|---|
| Shell (WSL/SSH) | `core/connection/ICommandExecutor.h` | Docker control, remote file editing, `gem build` |
| HTTP API | `core/api/ICosmosApiClient.h` | Everything about COSMOS *itself* |

A shell on the COSMOS host cannot answer any question about COSMOS. Installed
plugins live in MinIO + Redis, **not** in a host directory; targets,
interfaces, and microservices are Redis-backed models; installing a plugin is
a two-phase server-side operation. All of that is only reachable through the
API traefik exposes on port 2900. Do **not** implement COSMOS operations by
copying files around with shell commands.

Non-obvious facts, each verified against the 6.10.4 sources (not the docs):

- **No `Bearer` prefix.** Core's `OpenC3Authentication#token` returns
  `ENV['OPENC3_API_PASSWORD']` verbatim and `AuthModel.verify(token)` compares
  it directly, so `Authorization` carries the raw password. Adding `Bearer `
  makes every request fail authorization. (Keycloak/Bearer is Enterprise only,
  and is not implemented.)
- **Scope is mandatory.** `authorize()` raises "Scope is required", so scope is
  part of the connection identity, not a per-call argument —
  `HttpCosmosApiClient` appends it to every request automatically.
- **Plugin install is two phases.** `POST /openc3-api/plugins` uploads the
  `.gem` (multipart field name `plugin`) and returns a plugin hash; then
  `POST /openc3-api/plugins/install/:id` performs the install. Note the id
  comes *after* `install` in that route, reversing the usual Rails ordering.
- **Two URL prefixes.** cmd-tlm-api is under `/openc3-api`, Script Runner under
  `/script-api`, both behind the same port.

Never spell a route out at a call site — use `core/api/CosmosRoutes.h`.

The plugin domain is split along the same transport line, and the split is
load-bearing — do not merge them back:

| Service | Question it answers | Transport |
|---|---|---|
| `IWorkspaceService` | "what plugin *sources* are on this host?" | shell |
| `ICosmosPluginService` | "what is *installed* in COSMOS right now?" | API |

A plugin can be installed with no source folder present, and a source folder
may never have been installed. `IWorkspaceService` was split out of the old
`IPluginService`, whose `listInstalled()` scanned `<cosmosRoot>/plugins/*` on
disk while claiming to report installed plugins — that conflation is why the
toolkit used to show development folders as if they were live COSMOS state.
The old `install()`/`remove()` were deleted rather than ported: they copied or
deleted a `.gem` on the host and reported success while having no effect on a
running COSMOS.

`DoctorService` takes both transports for the same reason. Its checks are
layered: L1 infrastructure (Docker/CPU/memory/disk, shell), L2 COSMOS stack
(each expected container, Redis PING, MinIO — shell), L3 COSMOS logical state
(API reachable, scope exists, plugins, microservices, interfaces — API). When
no API password is configured the L3 checks report **Skipped, not Fail** — a
shell-only profile is a valid setup, and crying wolf there teaches users to
ignore Doctor. Container detection matches `openc3-*` **substrings** against
unfiltered `docker ps`, never `--filter name=cosmos`: every 6.10.4 service is
named `openc3-*`, and Compose prefixes a configurable project name.

Qt stays out of `core/`, so `ICosmosApiClient` is pure C++20 and its Qt Network
implementation lives in `viewmodels/api/HttpCosmosApiClient.{h,cpp}` (the
precedent `PluginTemplateEngine` set). Services therefore depend only on the
interface and are testable with `tests/mocks/MockCosmosApiClient.h` — no
network, no Qt event loop, no running COSMOS.

Every remote command executor call funnels through `ShellQuote`
(`src/core/connection/ShellQuote.h`) when interpolating user-controlled
strings (paths, container names, plugin names) into shell commands — this was
a deliberate hardening pass (see `CLAUDE_TASKS.md`). Keep using it; don't
reintroduce raw string concatenation into shell commands.

## Adding a new module

Follow this exact sequence (from `docs/developer-guide.md`):

1. Interface: `src/services/<module>/I<Module>Service.h`
2. Implementation: `src/services/<module>/<Module>Service.h/.cpp`
3. ViewModel: `src/viewmodels/<module>/<Module>ViewModel.h/.cpp`
4. View: `src/ui/views/<Module>View.h/.cpp`
5. Register in `Application.cpp` (`registerServices()` and `registerViewModels()`)
6. Nav item in `MainWindow::setupNavigation()`
7. Add the view via `contentStack_->addWidget()` in `MainWindow::setupViews()`
8. Mock: `tests/mocks/Mock<Module>Service.h`
9. Tests: `tests/unit/services/<Module>ServiceTest.cpp`

## Build

CMake + Ninja + presets (`CMakePresets.json`). C++20, `CMAKE_AUTOMOC`/`AUTORCC`/
`AUTOUIC` on. Requires Qt 6.4+, and libssh2/spdlog/nlohmann_json/GoogleTest
(fetched via `FetchContent` when not present as system packages — see
`cmake/Dependencies.cmake`).

```powershell
# Windows (MSVC developer shell so cl.exe is on PATH)
cmake --preset debug
cmake --build --preset debug
.\build\debug\bin\OpenC3DevToolkit.exe
```

```bash
# Linux (system packages instead of FetchContent — see tests.yml)
sudo apt-get install -y qt6-base-dev libspdlog-dev nlohmann-json3-dev \
    libgtest-dev libgmock-dev libssh2-1-dev
cmake --preset linux-debug
cmake --build --preset linux-debug
```

A pre-existing local tree may live at `build/local-debug/` — if present, the
binaries are `build/local-debug/bin/OpenC3DevToolkit.exe` and
`build/local-debug/bin/opencosmos_tests.exe`.

Output layout is pinned flat (`CMAKE_RUNTIME_OUTPUT_DIRECTORY_<CONFIG>` forced
to the same dir) even under multi-config generators, because
`translations/CMakeLists.txt` drops `openc3_ko.qm` next to the exe and looks
it up at runtime via `applicationDirPath()`. Don't remove that pinning.

## Tests

GoogleTest, run via CTest or the test binary directly.

```powershell
ctest --preset unit           # or: ctest --preset local-unit (after setup.ps1)
.\build\local-debug\bin\opencosmos_tests.exe   # direct fallback if ctest isn't on PATH
```

```bash
ctest --preset linux-unit --output-on-failure --verbose   # QT_QPA_PLATFORM=offscreen
```

Test layout mirrors the source layout: `tests/unit/{services,viewmodels,core,app,widgets}/`,
plus `tests/mocks/` for `Mock<Name>Service.h`-style interface mocks. When
adding a service, add a mock and a test in the corresponding paths.

## Style / lint

- `.clang-format` — enforced formatting.
- `.clang-tidy` — `cppcoreguidelines-*`, `modernize-*`, `performance-*`,
  `readability-*`, `bugprone-*`, `misc-*` (with a short exclusion list).
  `bugprone-*`, `cppcoreguidelines-slicing`, `modernize-use-nullptr`,
  `cppcoreguidelines-avoid-non-const-global-variables`, and
  `performance-move-const-arg` are `WarningsAsErrors`. Enabled via the
  `debug-analysis` preset (`ENABLE_CLANG_TIDY=ON`).
- Naming: classes/structs `CamelCase`, functions/variables `camelCase`,
  private members suffixed `_` (e.g. `busy_`, `connected_`).
- SRP/DIP/ISP/OCP conventions are spelled out with examples in
  `ARCHITECTURE.md` §7 — skim it before adding a new service or pattern.
- Prefer `const`/`noexcept`, RAII (`std::unique_ptr`, Qt parent ownership),
  and keep all remote I/O off the GUI thread (`QtConcurrent::run`).

## CI

- `.github/workflows/tests.yml` — Linux build + `ctest` on push/PR to
  `main`/`master`/`claude/**`.
- `.github/workflows/build-executable.yml` — builds Linux + Windows release
  executables (Windows job also produces a portable `windeployqt` zip) on
  push to `main`/`master`/`claude/**`.

## Key docs to keep in sync

- `ARCHITECTURE.md` — layer-by-layer design rationale, patterns, data-flow
  example, connection state machine. Update when the architecture changes.
- `docs/developer-guide.md` — prerequisites, build/test instructions, the
  "Adding a New Module" checklist, and the authoritative **Module Status**
  table. Update the status table when a module's implementation state
  changes — don't let it drift back into a stale roadmap.
- `docs/architecture.md` — secondary architecture doc; check for consistency
  with `ARCHITECTURE.md` when editing either.
- `CLAUDE_TASKS.md` — historical Claude Code work-plan/handoff doc (mojibake
  cleanup, doc sync, shell-quoting hardening, Doctor path hardening, test
  expansion). Useful for "why does X exist" archaeology; its "Definition of
  Done" section records what was actually completed.

## Working conventions

- Don't build `NullCommandExecutor`-bypassing paths — every service must stay
  safe to call before a connection is established.
- Don't add Qt includes to `core/` or `services/`; if a pure helper needs Qt
  types, it belongs in `viewmodels/` (see `PluginTemplateEngine` for the
  precedent and rationale).
- Don't commit anything under `build/` or other generated/third-party output.
- When touching shell-command construction, always route user-controlled
  values through `shellQuote()`.
- Keep `docs/developer-guide.md`'s Module Status table truthful — it's the
  single source of truth for "is X actually implemented" and this codebase
  has a documented history of the docs drifting from reality.
