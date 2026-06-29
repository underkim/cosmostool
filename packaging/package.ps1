#Requires -Version 5.1
<#
.SYNOPSIS
  Release packaging for OpenC3 Developer Toolkit.

.DESCRIPTION
  1. Configures and builds the Windows release preset.
  2. Stages the executable and deployed runtime files into dist/.
  3. Creates OpenC3DevToolkit-portable-windows.zip for extract-and-run use.
  4. Optionally builds an NSIS installer when makensis is on PATH.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File packaging\package.ps1
  powershell -ExecutionPolicy Bypass -File packaging\package.ps1 -SkipBuild
#>

param(
    [switch] $SkipBuild,
    [string] $Preset    = "release",
    [string] $QtPath    = "C:\Qt\6.11.1\msvc2022_64",
    [string] $VcVarsAll = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root     = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build\$Preset"
$BinDir   = Join-Path $BuildDir "bin"
$DistDir  = Join-Path $Root "dist"
$Exe      = Join-Path $BinDir "OpenC3DevToolkit.exe"
$NsiFile  = Join-Path $PSScriptRoot "installer.nsi"
$ZipPath  = Join-Path $Root "OpenC3DevToolkit-portable-windows.zip"

Write-Host ""
Write-Host "=== OpenC3 Developer Toolkit Release Packaging ===" -ForegroundColor Cyan
Write-Host "  Root:      $Root"
Write-Host "  Preset:    $Preset"
Write-Host "  BuildDir:  $BuildDir"
Write-Host "  DistDir:   $DistDir"
Write-Host "  Zip:       $ZipPath"
Write-Host ""

if (-not $SkipBuild) {
    Write-Host "[1/5] Configuring release..." -ForegroundColor Yellow
    $configCmd = "`"$VcVarsAll`" x64 && cmake --preset $Preset -S `"$Root`" -DCMAKE_PREFIX_PATH=`"$QtPath`""
    cmd /d /c $configCmd
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }

    Write-Host "[2/5] Building release..." -ForegroundColor Yellow
    $buildCmd = "`"$VcVarsAll`" x64 && cmake --build --preset $Preset"
    cmd /d /c $buildCmd
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)" }
} else {
    Write-Host "[1-2/5] Skipping build (-SkipBuild)" -ForegroundColor Gray
}

if (-not (Test-Path $Exe)) {
    throw "Executable not found: $Exe`nRun without -SkipBuild to build first."
}

Write-Host "[3/5] Staging dist/..." -ForegroundColor Yellow
if (Test-Path $DistDir) {
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Path $DistDir | Out-Null

# windeployqt runs as a CMake post-build step, so Qt DLLs and plugins should
# already be present beside OpenC3DevToolkit.exe.
Copy-Item -Recurse -Path "$BinDir\*" -Destination $DistDir

Get-ChildItem -Path $DistDir -Recurse -File |
    Where-Object { $_.Extension -in ".ilk", ".pdb" } |
    Remove-Item -Force

$vcRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $VcVarsAll))
$redistRoot = Join-Path $vcRoot "Redist\MSVC"
$crtDir = Get-ChildItem -Path $redistRoot -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName "x64\Microsoft.VC143.CRT" } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1

if ($crtDir) {
    Copy-Item -Path "$crtDir\*.dll" -Destination $DistDir -Force
} else {
    Write-Warning "MSVC runtime DLL folder not found under $redistRoot"
}

$vcRedist = Join-Path $DistDir "vc_redist.x64.exe"
if (Test-Path $vcRedist) {
    Remove-Item -Force $vcRedist
}

$licenseFile = Join-Path $Root "LICENSE"
if (Test-Path $licenseFile) {
    Copy-Item $licenseFile $DistDir
}
$readmeFile = Join-Path $Root "README.md"
if (Test-Path $readmeFile) {
    Copy-Item $readmeFile $DistDir
}
Write-Host "  Staged to: $DistDir" -ForegroundColor Green

Write-Host "[4/5] Creating portable zip..." -ForegroundColor Yellow
if (Test-Path $ZipPath) {
    Remove-Item -Force $ZipPath
}
Compress-Archive -Path "$DistDir\*" -DestinationPath $ZipPath -Force
Write-Host "  Portable zip: $ZipPath" -ForegroundColor Green

Write-Host "[5/5] Checking for NSIS..." -ForegroundColor Yellow
$makensisCommand = Get-Command "makensis" -ErrorAction SilentlyContinue
$makensis = if ($makensisCommand) { $makensisCommand.Source } else { $null }
if ($makensis) {
    Write-Host "  Building installer with makensis..." -ForegroundColor Yellow
    $defines = "/DROOT_DIR=`"$Root`" /DDIST_DIR=`"$DistDir`""
    & $makensis $defines $NsiFile
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "NSIS exited with $LASTEXITCODE"
    } else {
        $installer = Join-Path $Root "OpenC3DevToolkit-0.1.0-Setup.exe"
        Write-Host "  Installer: $installer" -ForegroundColor Green
    }
} else {
    Write-Host "  makensis not found; skipping installer." -ForegroundColor Gray
    Write-Host "  Install NSIS from https://nsis.sourceforge.io/ to build an installer."
}

Write-Host ""
Write-Host "=== Packaging complete ===" -ForegroundColor Cyan
Write-Host "  Distribution: $DistDir"
Write-Host "  Portable zip: $ZipPath"
