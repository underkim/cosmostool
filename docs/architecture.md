# OpenC3 Developer Toolkit — Architecture

## Overview

The OpenC3 Developer Toolkit is a professional Windows desktop application that improves developer productivity when working with OpenC3 COSMOS 6.10.4. It connects to OpenC3 environments running in WSL or on remote Linux servers via SSH.

---

## Layer Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    UI Layer (Qt Widgets)                  │
│  MainWindow · DashboardView · DockerView · DoctorView    │
│  SettingsView · MetricCard · StatusBadge · LogWidget     │
└────────────────────────┬────────────────────────────────┘
                         │ signals/slots
┌────────────────────────▼────────────────────────────────┐
│               ViewModel Layer (Qt + QtConcurrent)        │
│  DashboardViewModel · DockerViewModel · DoctorViewModel  │
│  SettingsViewModel · ViewModelBase                       │
└────────────────────────┬────────────────────────────────┘
                         │ std::function calls (sync)
┌────────────────────────▼────────────────────────────────┐
│                  Service Layer (Pure C++)                 │
│  ConnectionService · DockerService · SystemService       │
│  DoctorService · PluginService · SettingsService         │
└────────────────────────┬────────────────────────────────┘
                         │ ICommandExecutor&
┌────────────────────────▼────────────────────────────────┐
│               Connection Layer (Pure C++)                 │
│  ICommandExecutor · WslExecutor · SshExecutor            │
│  NullCommandExecutor                                      │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│            Linux / WSL / OpenC3 COSMOS                   │
└─────────────────────────────────────────────────────────┘
```

**Key invariant:** no layer may call upward. Every arrow points strictly downward.

---

## Connection Abstraction

`ICommandExecutor` is the central interface that makes all services blind to whether execution happens in WSL or over SSH.

```
ICommandExecutor (interface)
    ├── WslExecutor     — runs wsl.exe -d <distro> -- <cmd>
    ├── SshExecutor     — uses libssh2 for remote execution
    └── NullCommandExecutor — safe default (null-object pattern)
```

The `ConnectionService` owns the active executor. After `connect()` succeeds, the `ExecutorHolder` inside `Application` swaps its pointer, so all downstream services automatically use the live connection without needing to be re-created.

---

## MVVM Pattern

```
View                 ViewModel                Service
────                 ─────────                ───────
bind properties  ←── Q_PROPERTY              calls
react to signals ←── emit signal   ←───────── callback / result
                                   run on bg thread (QtConcurrent)
```

Rules:
- Views never call services directly.
- ViewModels never create Qt widgets.
- Services never include Qt headers.
- Models have zero dependencies.

---

## Dependency Injection

`ServiceRegistry` (composition root) in `Application` wires all dependencies at startup:

```
Application::run()
  └── registerServices()
        ├── SettingsService        (no dependencies)
        ├── ConnectionService      (SettingsService)
        ├── DockerService          (ICommandExecutor via ExecutorHolder)
        ├── SystemService          (ICommandExecutor via ExecutorHolder)
        ├── DoctorService          (ICommandExecutor via ExecutorHolder)
        └── PluginService          (ICommandExecutor via ExecutorHolder)
  └── registerViewModels()
        ├── DashboardViewModel     (IConnectionService, IDockerService, ISystemService)
        ├── DockerViewModel        (IDockerService)
        ├── DoctorViewModel        (IDoctorService)
        └── SettingsViewModel      (ISettingsService, IConnectionService)
```

All service registrations use interfaces as the key. ViewModels and UI classes only hold references to interfaces, never concrete types.

---

## Directory Structure

```
cosmostool/
├── cmake/                        # Build infrastructure
│   ├── CompilerWarnings.cmake
│   ├── Dependencies.cmake        # FetchContent: spdlog, json, libssh2, GTest
│   └── StaticAnalysis.cmake
├── src/
│   ├── core/                     # No Qt. No business logic.
│   │   ├── connection/           # ICommandExecutor + implementations
│   │   ├── events/               # Type-safe EventBus
│   │   └── logging/              # spdlog façade
│   ├── models/                   # Plain C++ structs. Zero dependencies.
│   ├── services/                 # Business logic. No Qt.
│   │   ├── connection/
│   │   ├── docker/
│   │   ├── system/
│   │   ├── doctor/
│   │   ├── plugin/
│   │   └── settings/
│   ├── viewmodels/               # Qt + QtConcurrent. Bridges services to UI.
│   │   ├── base/
│   │   ├── dashboard/
│   │   ├── docker/
│   │   ├── doctor/
│   │   └── settings/
│   ├── ui/                       # Qt Widgets. Binds to ViewModels only.
│   │   ├── widgets/              # Reusable: MetricCard, StatusBadge, LogWidget
│   │   ├── views/                # Full-page module views
│   │   ├── dialogs/
│   │   └── styles/               # ThemeManager + QSS
│   └── app/                      # Entry point + DI composition root
├── resources/
│   ├── themes/dark.qss
│   └── resources.qrc
├── tests/
│   ├── mocks/                    # GMock implementations of interfaces
│   └── unit/
│       ├── core/
│       └── services/
└── docs/
```

---

## Thread Safety Model

- **Main thread:** Qt event loop, all UI operations.
- **Worker threads:** Service calls dispatched via `QtConcurrent::run()`.
- **Result delivery:** `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` marshals results back to the main thread.
- **Service layer:** Services are not thread-safe themselves. Each `QtConcurrent` task serializes its own execution. The `ExecutorHolder` performs a single pointer swap under the ConnectionService state change callback (main-thread only), making it safe.

---

## Third-Party Dependencies

| Library         | Version  | Purpose              | Delivery    |
|-----------------|----------|----------------------|-------------|
| Qt6             | ≥ 6.4    | GUI framework        | System/Qt installer |
| spdlog          | 1.14.1   | Logging              | FetchContent |
| nlohmann/json   | 3.11.3   | JSON serialization   | FetchContent |
| libssh2         | 1.11.0   | SSH client           | FetchContent |
| GoogleTest      | 1.14.0   | Unit / mock testing  | FetchContent |

---

## Coding Conventions

- C++20 everywhere. No extensions.
- Namespace: `OpenC3::Layer::Module` (e.g. `OpenC3::Services::DockerService`).
- Private members end with `_` (e.g. `executor_`).
- Interface prefix `I` (e.g. `IDockerService`).
- `[[nodiscard]]` on every function returning a value.
- RAII for all resources. No raw `new` in business code.
- `std::shared_ptr` for shared ownership; `std::unique_ptr` for exclusive ownership; references for non-owning access.
- No global state except the logger (infrastructure singleton, acceptable).
