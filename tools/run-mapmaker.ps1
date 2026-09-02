#requires -Version 5.1
<#
.SYNOPSIS
    Build (if needed) and launch HBMapMaker from the CMake build tree.
.PARAMETER Configuration
    Debug (default) or Release.
.PARAMETER SkipBuild
    Skip the build step and just launch the existing exe.
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\run-mapmaker.ps1
    powershell -ExecutionPolicy Bypass -File tools\run-mapmaker.ps1 -Configuration Release
    powershell -ExecutionPolicy Bypass -File tools\run-mapmaker.ps1 -SkipBuild
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','RelWithDebInfo','MinSizeRel')]
    [string]$Configuration = 'Debug',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Exe      = Join-Path $RepoRoot "build\windows-msvc\bin\$Configuration\HBMapMaker.exe"

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-cmake.ps1') -Configuration $Configuration -Target HBMapMaker
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }
}

if (-not (Test-Path $Exe)) { throw "HBMapMaker.exe not found at $Exe. Run without -SkipBuild first." }

Write-Host "==> launching $Exe"
Push-Location (Split-Path -Parent $Exe)
try {
    & $Exe
    $code = $LASTEXITCODE
}
finally {
    Pop-Location
}
exit $code
