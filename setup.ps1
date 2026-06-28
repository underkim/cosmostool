#!/usr/bin/env pwsh
# setup.ps1 — 첫 번째 빌드 전에 한 번만 실행합니다
# 사용법: .\setup.ps1
# 또는:   .\setup.ps1 -QtPath "C:\Qt\6.11.1\msvc2022_64"

param(
    [string]$QtPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "=== OpenC3 Developer Toolkit — 환경 설정 ===" -ForegroundColor Cyan
Write-Host ""

# ── 1. Qt MSVC 키트 경로 자동 탐색 ───────────────────────────────────────────
if (-not $QtPath) {
    $roots = @("C:\Qt", "D:\Qt", "$env:USERPROFILE\Qt")
    # MSVC 키트 우선순위: msvc2022_64 > msvc2019_64
    $msvcKits = @("msvc2022_64", "msvc2019_64")

    :outer foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }

        $versions = Get-ChildItem $root -Directory |
                    Where-Object { $_.Name -match '^\d+\.\d+' } |
                    Sort-Object Name -Descending

        foreach ($ver in $versions) {
            foreach ($kit in $msvcKits) {
                $candidate = Join-Path $ver.FullName $kit
                if (Test-Path $candidate) {
                    $QtPath = $candidate
                    break outer
                }
            }
        }
    }
}

if (-not $QtPath) {
    Write-Host "[오류] Qt MSVC 키트를 찾을 수 없습니다." -ForegroundColor Red
    Write-Host ""
    Write-Host "  현재 설치된 Qt:" -ForegroundColor Yellow
    Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d' } |
        ForEach-Object {
            Write-Host "    버전: $($_.Name)"
            Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue |
                ForEach-Object { Write-Host "      키트: $($_.Name)" }
        }
    Write-Host ""
    Write-Host "  해결 방법:" -ForegroundColor Yellow
    Write-Host "  1. C:\Qt\MaintenanceTool.exe 실행"
    Write-Host "  2. 'Add or remove components' 선택"
    Write-Host "  3. Qt 6.x.x → 'MSVC 2022 64-bit' 체크 후 설치"
    Write-Host ""
    Write-Host "  설치 후 다시 실행하거나 경로를 직접 지정:"
    Write-Host '  .\setup.ps1 -QtPath "C:\Qt\6.11.1\msvc2022_64"'
    exit 1
}

Write-Host "[OK] Qt 경로: $QtPath" -ForegroundColor Green

# ── 2. CMakeUserPresets.json 생성 ────────────────────────────────────────────
$presetsPath = Join-Path $PSScriptRoot "CMakeUserPresets.json"
$QtPathForward = $QtPath -replace '\\', '/'

$presets = [ordered]@{
    version = 6
    include = @("CMakePresets.json")
    configurePresets = @(
        [ordered]@{ name="local-debug";   displayName="[Local] Debug";   inherits="debug";   cacheVariables=[ordered]@{ CMAKE_PREFIX_PATH=$QtPathForward } },
        [ordered]@{ name="local-release"; displayName="[Local] Release"; inherits="release"; cacheVariables=[ordered]@{ CMAKE_PREFIX_PATH=$QtPathForward } }
    )
    buildPresets = @(
        [ordered]@{ name="local-debug";   displayName="[Local] Build Debug";   configurePreset="local-debug";   jobs=0 },
        [ordered]@{ name="local-release"; displayName="[Local] Build Release"; configurePreset="local-release"; jobs=0 }
    )
    testPresets = @(
        [ordered]@{ name="local-unit"; displayName="[Local] Unit Tests"; configurePreset="local-debug"; output=[ordered]@{ outputOnFailure=$true } }
    )
}
$presets | ConvertTo-Json -Depth 10 | Set-Content -Path $presetsPath -Encoding UTF8
Write-Host "[OK] CMakeUserPresets.json 생성" -ForegroundColor Green

# ── 3. launch.json Qt PATH 업데이트 ──────────────────────────────────────────
$launchPath = Join-Path $PSScriptRoot ".vscode\launch.json"
if (Test-Path $launchPath) {
    $qtBin       = (Join-Path $QtPath "bin") -replace '\\', '\\\\'
    $launchText  = Get-Content $launchPath -Raw
    $launchText  = $launchText -replace 'C:\\\\Qt\\\\[^;\"]+\\\\bin', $qtBin
    Set-Content -Path $launchPath -Value $launchText -Encoding UTF8
    Write-Host "[OK] .vscode/launch.json Qt 경로 업데이트" -ForegroundColor Green
}

# ── 4. 도구 확인 ──────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "--- 필수 도구 확인 ---" -ForegroundColor Cyan

function Test-Command {
    param([string]$Name, [string]$Cmd)
    try {
        $null = Invoke-Expression $Cmd 2>&1
        Write-Host "  [OK] $Name" -ForegroundColor Green
        return $true
    } catch {
        Write-Host "  [없음] $Name" -ForegroundColor Yellow
        return $false
    }
}

$hasCmake  = Test-Command "CMake"        "cmake --version"
$hasCtest  = Test-Command "CTest"        "ctest --version"
$hasNinja  = Test-Command "Ninja"        "ninja --version"
$hasCl     = Test-Command "MSVC (cl)"   "cl 2>&1 | Select-Object -First 1"
$hasGit    = Test-Command "Git"         "git --version"
$hasClangF = Test-Command "clang-format" "clang-format --version"

Write-Host ""

if (-not $hasCl) {
    Write-Host "[주의] MSVC 컴파일러(cl.exe)를 찾을 수 없습니다." -ForegroundColor Yellow
    Write-Host "  VSCode 에서 새 터미널을 열면 자동으로 MSVC 환경이 로드됩니다."
    Write-Host "  (settings.json 에 'MSVC Developer Shell' 프로필이 설정되어 있습니다)"
    Write-Host ""
}

if (-not $hasNinja) {
    Write-Host "[주의] Ninja가 없습니다. Qt 설치 시 포함된 Ninja 경로를 추가합니다." -ForegroundColor Yellow
    $ninjaPath = "C:\Qt\Tools\Ninja"
    if (Test-Path $ninjaPath) {
        $env:PATH = "$ninjaPath;$env:PATH"
        Write-Host "  [OK] Ninja 임시 추가: $ninjaPath" -ForegroundColor Green
    }
    Write-Host ""
}

if (-not $hasCmake -or -not $hasCtest) {
    Write-Host "[주의] CMake/CTest 명령을 찾을 수 없습니다." -ForegroundColor Yellow
    Write-Host "  CMake가 설치되어 있고 PATH에 등록되어 있는지 확인하세요."
    Write-Host "  Visual Studio Developer PowerShell 또는 새 VSCode 터미널에서 다시 시도하세요."
    Write-Host "  이미 빌드된 local-debug 테스트가 있으면 아래 직접 실행 명령을 사용할 수 있습니다:"
    Write-Host "    .\build\local-debug\bin\opencosmos_tests.exe" -ForegroundColor Gray
    Write-Host ""
}

# ── 5. 완료 메시지 ────────────────────────────────────────────────────────────
Write-Host "=== 설정 완료 ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "다음 단계:" -ForegroundColor White
Write-Host "  1. VSCode에서 이 폴더 열기:  code ." -ForegroundColor Gray
Write-Host "  2. 터미널 새로 열기 (자동으로 MSVC 환경 로드됨)"
Write-Host "  3. Ctrl+Shift+P → 'CMake: Configure'" -ForegroundColor Gray
Write-Host "  4. Ctrl+Shift+B  빌드" -ForegroundColor Gray
Write-Host "  5. F5           실행 + 디버그" -ForegroundColor Gray
Write-Host ""
Write-Host "터미널에서 직접 빌드:" -ForegroundColor White
Write-Host "  cmake --preset local-debug" -ForegroundColor Gray
Write-Host "  cmake --build --preset local-debug" -ForegroundColor Gray
Write-Host ""
Write-Host "테스트 실행:" -ForegroundColor White
Write-Host "  ctest --preset local-unit" -ForegroundColor Gray
Write-Host "  # 또는 CTest가 PATH에 없고 local-debug 빌드가 이미 있으면:" -ForegroundColor Gray
Write-Host "  .\build\local-debug\bin\opencosmos_tests.exe" -ForegroundColor Gray
Write-Host ""
