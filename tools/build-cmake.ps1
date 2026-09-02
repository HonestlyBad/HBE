#requires -Version 5.1
<#
.SYNOPSIS
    Configure + build the HBE workspace with the CMake `windows-msvc` preset.
.DESCRIPTION
    Replaces the old .slnx-driven workflow. Uses the VS-bundled CMake (from
    Visual Studio 2022 / 18) so no separate CMake install is required. On the
    first run it configures the build tree; subsequent runs skip configure
    unless -Reconfigure is passed.
.PARAMETER Configuration
    Debug (default) or Release.
.PARAMETER Target
    Which target to build. Defaults to `all`. Common values:
        all, HBMapMaker, MegaX, HBE.Sandbox
.PARAMETER Reconfigure
    Force cmake configure even if the build tree already exists.
.PARAMETER Clean
    Wipe build\windows-msvc before configuring.
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build-cmake.ps1
    powershell -ExecutionPolicy Bypass -File tools\build-cmake.ps1 -Target HBMapMaker
    powershell -ExecutionPolicy Bypass -File tools\build-cmake.ps1 -Configuration Release -Target MegaX
    powershell -ExecutionPolicy Bypass -File tools\build-cmake.ps1 -Clean
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')]
    [string]$Configuration = 'Debug',

    [string]$Target = 'all',

    [switch]$Reconfigure,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot   = Split-Path -Parent $PSScriptRoot
$Preset     = 'windows-msvc'
$BuildTree  = Join-Path $RepoRoot "build\$Preset"

# --- Locate cmake.exe -------------------------------------------------------
function Find-CMake {
    $onPath = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\CMake\bin\cmake.exe'
    )
    foreach ($c in $candidates) { if (Test-Path $c) { return $c } }

    throw "cmake.exe not found. Install Visual Studio's C++ CMake tools or standalone CMake."
}

$CMake = Find-CMake
Write-Host "==> cmake:  $CMake"
Write-Host "==> repo:   $RepoRoot"
Write-Host "==> preset: $Preset"
Write-Host "==> config: $Configuration"
Write-Host "==> target: $Target"

if ($Clean -and (Test-Path $BuildTree)) {
    Write-Host "==> cleaning $BuildTree"
    Remove-Item -Recurse -Force $BuildTree
}

Push-Location $RepoRoot
try {
    $needsConfigure = $Reconfigure -or -not (Test-Path (Join-Path $BuildTree 'CMakeCache.txt'))
    if ($needsConfigure) {
        Write-Host "==> configure"
        & $CMake --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($LASTEXITCODE)." }
    } else {
        Write-Host "==> reusing existing build tree (use -Reconfigure to regen)"
    }

    Write-Host "==> build"
    $buildArgs = @('--build', $BuildTree, '--config', $Configuration)
    if ($Target -and $Target -ne 'all') {
        $buildArgs += @('--target', $Target)
    }
    & $CMake @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed ($LASTEXITCODE)." }

    Write-Host ""
    Write-Host "==> success. Binaries:"
    Get-ChildItem (Join-Path $BuildTree "bin\$Configuration") -Filter *.exe -ErrorAction SilentlyContinue |
        ForEach-Object { Write-Host ("    " + $_.FullName) }
}
finally {
    Pop-Location
}
