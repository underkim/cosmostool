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

```powershell
# 1. Clone
git clone <repo-url> cosmostool
cd cosmostool

# 2. Configure — set CMAKE_PREFIX_PATH to your Qt6 installation
cmake -B build ^
      -DCMAKE_PREFIX_PATH="C:/Qt/6.7.0/msvc2022_64" ^
      -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 3. Build
cmake --build build --config RelWithDebInfo -j4

# 4. Run
.\build\bin\OpenC3DevToolkit.exe
```

## Running Tests

```powershell
cmake --build build --target opencosmos_tests
cd build && ctest -C RelWithDebInfo --output-on-failure
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

## Phase Roadmap

| Phase | Status    | Scope                             |
|-------|-----------|-----------------------------------|
| 1     | ✅ Done   | Architecture design               |
| 2     | ✅ Done   | Project skeleton                  |
| 3     | Pending   | Core framework hardening          |
| 4     | Pending   | Dashboard polish                  |
| 5     | Pending   | Docker Manager full features      |
| 6     | Pending   | Doctor full checks                |
| 7     | Pending   | Plugin Manager                    |
| 8     | Pending   | CMD/TLM Editor                    |
| 9     | Pending   | Packet Tools                      |
| 10    | Pending   | Log Viewer                        |
| 11    | Pending   | Release packaging                 |

## Code Quality

```powershell
# Format all source files
Get-ChildItem -Recurse src,tests -Include *.h,*.cpp |
    ForEach-Object { clang-format -i $_.FullName }

# Enable static analysis during build
cmake -B build -DENABLE_CLANG_TIDY=ON
cmake --build build
```
