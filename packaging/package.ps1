#Requires -Version 5.1
<#
.SYNOPSIS
  Phase 11 release packaging for OpenC3 Developer Toolkit.

.DESCRIPTION
  1. Configures and builds the release preset (MSVC x64, RelWithDebInfo).
  2. Copies the output to dist/ (windeployqt runs automatically via CMake POST_BUILD).
  3. Optionally builds an NSIS installer if makensis is on PATH.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File packaging\package.ps1
  powershell -ExecutionPolicy Bypass -File packaging\package.ps1 -SkipBuild
#>

param(
    [switch] $SkipBuild,
    [string] $QtPath    = "C:\Qt\6.11.1\msvc2022_64",
    [string] $VcVarsAll = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root     = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build\local-release"
$BinDir   = Join-Path $BuildDir "bin"
$DistDir  = Join-Path $Root "dist"
$Exe      = Join-Path $BinDir "OpenC3DevToolkit.exe"
$NsiFile  = Join-Path $PSScriptRoot "installer.nsi"

Write-Host ""
Write-Host "=== OpenC3 Developer Toolkit — Release Packaging ===" -ForegroundColor Cyan
Write-Host "  Root:      $Root"
Write-Host "  BuildDir:  $BuildDir"
Write-Host "  DistDir:   $DistDir"
Write-Host ""

# ── 1. Build ──────────────────────────────────────────────────────────────────
if (-not $SkipBuild) {
    Write-Host "[1/4] Configuring release..." -ForegroundColor Yellow
    $configCmd = "`"$VcVarsAll`" x64 && cmake --preset local-release -S `"$Root`""
    cmd /d /c $configCmd
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }

    Write-Host "[2/4] Building release..." -ForegroundColor Yellow
    $buildCmd = "`"$VcVarsAll`" x64 && cmake --build --preset local-release"
    cmd /d /c $buildCmd
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)" }
} else {
    Write-Host "[1-2/4] Skipping build (–SkipBuild)" -ForegroundColor Gray
}

if (-not (Test-Path $Exe)) {
    throw "Executable not found: $Exe`nRun without -SkipBuild to build first."
}

# ── 2. Stage dist/ ────────────────────────────────────────────────────────────
Write-Host "[3/4] Staging dist/..." -ForegroundColor Yellow

if (Test-Path $DistDir) {
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Path $DistDir | Out-Null

# windeployqt runs as POST_BUILD → Qt DLLs are already in $BinDir
# Copy the entire bin directory into dist/
Copy-Item -Recurse -Path "$BinDir\*" -Destination $DistDir

# Add the license / readme
$licenseFile = Join-Path $Root "LICENSE"
if (Test-Path $licenseFile) {
    Copy-Item $licenseFile $DistDir
}
$readmeFile = Join-Path $Root "README.md"
if (Test-Path $readmeFile) {
    Copy-Item $readmeFile $DistDir
}

Write-Host "  Staged to: $DistDir" -ForegroundColor Green

# ── 3. NSIS installer (optional) ──────────────────────────────────────────────
Write-Host "[4/4] Checking for NSIS..." -ForegroundColor Yellow
$makensis = (Get-Command "makensis" -ErrorAction SilentlyContinue)?.Source

if ($makensis) {
    Write-Host "  Building installer with makensis..." -ForegroundColor Yellow
    $defines = "/DROOT_DIR=`"$Root`" /DDIST_DIR=`"$DistDir`""
    & $makensis $defines $NsiFile
    if ($LASTEXITCODE -ne 0) { Write-Warning "NSIS exited with $LASTEXITCODE" }
    else {
        $installer = Join-Path $Root "OpenC3DevToolkit-Setup.exe"
        Write-Host "  Installer: $installer" -ForegroundColor Green
    }
} else {
    Write-Host "  makensis not found — skipping installer." -ForegroundColor Gray
    Write-Host "  Install NSIS from https://nsis.sourceforge.io/ to build an installer."
}

Write-Host ""
Write-Host "=== Packaging complete ===" -ForegroundColor Cyan
Write-Host "  Distribution: $DistDir"
