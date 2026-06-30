# Claude Code Work Plan

This document is a handoff for Claude Code. The repository is a C++20 / Qt 6 / CMake Windows desktop application for OpenC3 COSMOS developer workflows.

Repository root:

```text
C:\Users\rlaeh\Desktop\cosmostool\cosmostool
```

Current known state:

- Branch: `master`
- Working tree was clean when this file was created.
- Existing executable: `build/local-debug/bin/OpenC3DevToolkit.exe`
- Existing unit test binary: `build/local-debug/bin/opencosmos_tests.exe`
- Direct test run passed: 33 tests from 5 suites.
- `ctest` was not available on PATH in the current shell, so use the test binary directly unless the environment is fixed.

## Project Summary

OpenC3 Developer Toolkit is a Qt Widgets desktop app that connects to OpenC3 COSMOS environments through WSL or SSH. It provides:

- Dashboard
- Docker manager
- Infra manager
- Doctor checks
- Plugin manager/scaffolding
- CMD/TLM editor
- Packet tools
- Log viewer
- Settings and connection profiles

Architecture layers:

```text
app -> ui -> viewmodels -> services -> core/models
```

Important rule: keep dependencies pointing downward. Services should stay free of Qt headers. UI should talk to ViewModels, not Services directly.

## Priority 1: Fix Mojibake / Broken Text

### Problem

Many files contain broken Korean comments or broken icon strings. They usually appear as repeated question marks, replacement characters, or unreadable mixed symbols in comments, menu labels, and status labels. This affects documentation readability and parts of the UI.

### Main Targets

- `ARCHITECTURE.md`
- `docs/architecture.md`
- `docs/developer-guide.md`
- `CMakeLists.txt`
- `src/CMakeLists.txt`
- `src/ui/MainWindow.cpp`
- Other source files where comments are visibly corrupted

### Required Work

1. Replace corrupted comments with clear English comments.
2. Replace corrupted UI icon text in `MainWindow.cpp`.
3. Prefer ASCII labels if icon encoding is uncertain.
4. Do not alter behavior while cleaning comments/text.
5. Keep documentation consistent with the current implementation.

### Suggested UI Text Fix

In `src/ui/MainWindow.cpp`, either:

- remove decorative icon prefixes and use plain labels, or
- replace them with stable ASCII prefixes.

Recommended safe option:

```cpp
addItem("", "Dashboard");
addItem("", "Docker");
addItem("", "Infra");
addItem("", "Doctor");
addItem("", "Plugins");
addItem("", "CMD / TLM");
addItem("", "Packet Tools");
addItem("", "Log Viewer");
addItem("", "Settings");
```

Also change status labels to plain readable text:

```cpp
connectionLabel_ = new QLabel("  Disconnected  ", this);
dockerLabel_     = new QLabel("  Docker: --  ", this);
```

## Priority 2: Update Documentation to Match Reality

### Problem

The roadmap in docs still says many phases are pending, but the current code already registers and displays many modules:

- Dashboard
- Docker
- Infra
- Doctor
- Plugins
- CMD/TLM
- Packet Tools
- Log Viewer
- Settings

### Required Work

1. Rewrite `docs/developer-guide.md` roadmap into a current status table.
2. Mark modules as one of:
   - Implemented
   - Partial
   - Needs hardening
   - Not implemented
3. Update build/test instructions:
   - Primary local test command:
     ```powershell
     .\build\local-debug\bin\opencosmos_tests.exe
     ```
   - CMake preset commands may remain, but mention that `ctest` requires CMake tools on PATH.
4. Ensure app executable path is documented:
   ```powershell
   .\build\local-debug\bin\OpenC3DevToolkit.exe
   ```

## Priority 3: Harden Shell Command Construction

### Problem

Services build shell commands by string concatenation. This is fragile for paths/names with spaces and can become command injection risk if user-controlled input is passed through.

### Main Targets

- `src/services/docker/DockerService.cpp`
- `src/services/doctor/DoctorService.cpp`
- `src/services/plugin/PluginService.cpp`
- `src/services/filesystem/RemoteFileService.cpp`
- `src/viewmodels/infra/InfraViewModel.cpp`

### Required Work

1. Add a small shell quoting helper in the service/core layer.
2. Use it wherever a path, container name, plugin name, or user-provided value is interpolated into a shell command.
3. Add unit tests for quoting.
4. Add/adjust service tests for values containing spaces, quotes, and shell metacharacters.

### Suggested Design

Add files:

```text
src/core/connection/ShellQuote.h
src/core/connection/ShellQuote.cpp
tests/unit/core/ShellQuoteTest.cpp
```

Suggested API:

```cpp
namespace OpenC3::Core::Connection {

[[nodiscard]] std::string shellQuote(const std::string& value);

}
```

For POSIX shell commands, single-quote strings and escape embedded single quotes using:

```text
'abc'\''def'
```

Example expected behavior:

```text
hello        -> 'hello'
hello world  -> 'hello world'
a'b          -> 'a'\''b'
$(rm -rf /)  -> '$(rm -rf /)'
```

### First Places to Patch

In `DockerService.cpp`:

- `docker start <nameOrId>`
- `docker stop <nameOrId>`
- `docker restart <nameOrId>`
- `docker rm <nameOrId>`
- `docker logs ... <nameOrId>`
- `cd <composeDir> && docker compose ...`

In `DoctorService.cpp`:

- hardcoded paths should be quoted if used in shell commands.

## Priority 4: Reduce DoctorService Hardcoding

### Problem

Doctor checks hardcode OpenC3 paths such as:

```text
/cosmos/plugins
/cosmos/openc3-cosmos-init/plugins/openc3-tool-base/VERSION
```

This can produce false warnings when OpenC3 is installed elsewhere.

### Required Work

1. Inspect `ConnectionProfile` and settings services for existing COSMOS path fields.
2. Prefer using configured COSMOS root path where available.
3. If no configured path is available, keep `/cosmos` as fallback.
4. Update Doctor UI text/suggestions to mention the configured path.
5. Add tests for default and custom path behavior.

### Suggested Approach

Do not make `DoctorService` depend directly on Qt or UI. If settings are needed, inject a plain service/interface or pass a configuration value into the service constructor.

## Priority 5: Expand Tests

### Existing Passing Tests

Direct run:

```powershell
.\build\local-debug\bin\opencosmos_tests.exe
```

Result observed:

```text
33 tests from 5 test suites passed.
```

Existing test coverage includes:

- `ExecutorResult`
- `EventBus`
- `DockerService`
- `DoctorService`
- `SettingsService`

### Missing / Weak Areas

Add tests for:

- `ShellQuote`
- `PluginService`
- `RemoteFileService`
- `CmdTlmParser`
- `PluginTemplateEngine`
- Docker command generation with unsafe input
- Doctor path handling
- ViewModel state transitions where practical

Focus on service/core tests first. UI tests can come later.

## Priority 6: Build/Test Environment Cleanup

### Problem

`ctest --preset unit` failed because `ctest` was not found on PATH in the current shell.

### Required Work

1. Update `setup.ps1` or docs to check for:
   - `cmake`
   - `ctest`
   - `ninja`
   - MSVC compiler environment
   - Qt path
2. Document fallback direct test command.
3. Avoid requiring network during normal test runs.

## Priority 7: End-to-End Manual Validation

Once the cleanup and hardening work is done, manually validate:

1. Start app.
2. Open connection dialog.
3. Test WSL profile detection/connection.
4. Test SSH connection if credentials/environment are available.
5. Refresh Docker view.
6. Run all Doctor checks.
7. Create/plugin scaffold flow.
8. Open and save CMD/TLM files.
9. Start/stop log streaming.
10. Close app while streaming to ensure clean shutdown.

## Guardrails

- Do not rewrite architecture unnecessarily.
- Keep services independent of Qt.
- Do not remove existing features while cleaning text.
- Do not commit build artifacts.
- Avoid touching generated or third-party files under `build/`.
- Prefer small, reviewable patches.
- After each logical change, run:
  ```powershell
  .\build\local-debug\bin\opencosmos_tests.exe
  ```

## Suggested Work Order

1. Clean broken text in `MainWindow.cpp` and docs.
2. Update roadmap/build/test documentation.
3. Add shell quoting helper and tests.
4. Apply quoting to DockerService.
5. Apply quoting to RemoteFileService/PluginService/Infra paths.
6. Refactor Doctor path assumptions.
7. Add missing service/parser/template tests.
8. Run direct test binary and record result.

## Definition of Done

The work is ready when:

- User-facing text no longer shows corrupted characters.
- Key docs reflect the actual current codebase.
- Shell command construction is safe for spaces, quotes, and metacharacters.
- Unit tests pass locally.
- New tests cover the new quoting helper and at least Docker command generation.
- Any remaining risks are documented clearly.

## Current Handoff Status

Status as of the latest local check:

- Priorities 1-6 are implemented or materially addressed.
- Priority 7, end-to-end manual validation, remains pending.
- The repository has uncommitted source, test, and documentation changes.
- Do not assume the changes are committed.

Observed test result:

```text
.\build\local-debug\bin\opencosmos_tests.exe
64 tests from 10 test suites passed.
```

Notable completed work:

- Developer guide now reflects the implemented modules instead of the old phase roadmap.
- `setup.ps1` checks for CMake, CTest, Ninja, MSVC, Git, and clang-format, and documents a direct test-binary fallback.
- `ShellQuote` helper was added under `src/core/connection`.
- Docker, Doctor, Plugin, and Infra command construction were hardened where practical.
- Doctor checks now support configured COSMOS root path behavior.
- Tests were expanded for shell quoting, Docker quoting, Doctor path handling, PluginService, CmdTlmParser, and PluginTemplateEngine.

### Later improvement pass (validation + service correctness)

- `ConfigValidator::validateFolder` no longer reports "No COSMOS config files
  found" when a folder full of valid files is scanned; it now distinguishes an
  empty folder (`folder.empty`) from a clean scan (`folder.clean`) and reports
  how many files validated.
- The CMD/TLM parser recognises the valid COSMOS keywords `SELECT_PARAMETER`,
  `SELECT_ITEM`, `DELETE_PARAMETER`, `DELETE_ITEM`, `PROCESSOR`,
  `RELATED_ITEM`, `KEY`, `OBFUSCATE`, and `VIRTUAL`, instead of flagging them
  as "Unknown keyword".
- `DockerService::getStats` no longer throws `std::out_of_range` when Docker
  reports a CPU value without a trailing `%`.
- Added regression tests for each of the above.

### Building and testing on Linux (developer/CI environments)

The CMake build pulls spdlog / nlohmann_json / GoogleTest / libssh2 via
`FetchContent` from GitHub, which is unavailable in network-restricted
environments. On Debian/Ubuntu the same dependencies are available as system
packages and the pure-logic layers can be compiled directly with `g++`
(no Qt MOC needed):

```bash
sudo apt-get install -y qt6-base-dev libspdlog-dev nlohmann-json3-dev \
    libgtest-dev libgmock-dev libssh2-1-dev

# Validation / parser unit tests (Qt Core only):
g++ -std=c++20 -I src $(ls src/viewmodels/validation/*.cpp) \
    src/viewmodels/cmdtlm/CmdTlmParser.cpp \
    src/viewmodels/plugin/PluginTemplateEngine.cpp \
    tests/unit/viewmodels/*Validator*Test.cpp \
    tests/unit/viewmodels/CmdTlmParserTest.cpp \
    $(pkg-config --cflags --libs Qt6Core gtest gtest_main) -o /tmp/vmtests

# Service unit tests that avoid libssh2 (Docker/Doctor/Plugin):
g++ -std=c++20 -I src -I tests \
    src/services/docker/DockerService.cpp src/core/connection/ShellQuote.cpp \
    src/core/logging/Logger.cpp tests/unit/services/DockerServiceTest.cpp \
    $(pkg-config --cflags --libs gtest gtest_main gmock) -lspdlog -lfmt \
    -o /tmp/dockertest
```

This is a verification convenience; the canonical build remains CMake +
`opencosmos_tests`.

Manual validation checklist:

1. Launch `build/local-debug/bin/OpenC3DevToolkit.exe`.
2. Confirm the startup connection dialog appears and does not block normal app use.
3. Create or select a WSL profile and connect.
4. If available, create or select an SSH profile and connect.
5. Open Dashboard and confirm connection/Docker/system status refreshes.
6. Open Docker and confirm container list refresh and basic actions behave correctly.
7. Open Infra and confirm ENV/Compose-related screens load without errors.
8. Open Doctor and run all checks.
9. Open Plugins and test list, validation, scaffold, and add-target flows where safe.
10. Open CMD/TLM and test file browse, read, parse, edit, and save on a safe test file.
11. Open Packet Tools and test log-file listing/analysis.
12. Open Log Viewer, start streaming, stop streaming, then close the app.
13. Repeat app close while streaming is active and confirm the app exits cleanly.

Manual validation notes:

- Prefer a disposable OpenC3/WSL target for write operations.
- Avoid plugin install/remove actions on production targets until the list/scaffold flows are confirmed.
- If CTest is unavailable on PATH, use the direct test binary:
  ```powershell
  .\build\local-debug\bin\opencosmos_tests.exe
  ```
