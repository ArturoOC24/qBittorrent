#Requires -Version 5.1
<#
.SYNOPSIS
    Build and deploy qBittorrent (Windows).
    Kills any running qbittorrent.exe, rebuilds, then optionally launches.

.PARAMETER Launch
    Start qbittorrent.exe after a successful build.

.PARAMETER BuildType
    CMake build type. Default: Release (matches the existing build cache).
#>
param(
    [switch]$Launch,
    [string]$BuildType = "Release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot  = $PSScriptRoot
$BuildDir  = Join-Path $RepoRoot "build"
$Exe       = Join-Path $BuildDir "qbittorrent.exe"
$VcVars    = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

# ── 1. Kill running instance ──────────────────────────────────────────────────
$running = Get-Process -Name "qbittorrent" -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "Stopping running qbittorrent.exe (PID $($running.Id))..." -ForegroundColor Yellow
    $running | Stop-Process -Force
    Start-Sleep -Milliseconds 500
    Write-Host "Process stopped." -ForegroundColor Green
} else {
    Write-Host "qbittorrent.exe is not running." -ForegroundColor Cyan
}

# ── 2. Build ──────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "Building (config: $BuildType)..." -ForegroundColor Cyan

# We need the full VS environment, so we call vcvars64.bat via cmd.exe,
# then chain the cmake build in the same session.
$BuildCmd = @"
call "$VcVars" 2>nul
cmake --build "$BuildDir" --config $BuildType --parallel
"@

$TempBat = [System.IO.Path]::GetTempFileName() + ".bat"
[System.IO.File]::WriteAllText($TempBat, $BuildCmd, [System.Text.Encoding]::ASCII)

try {
    cmd.exe /c $TempBat
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
} finally {
    Remove-Item -Force $TempBat -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "Build succeeded." -ForegroundColor Green
Write-Host "Output: $Exe"

# ── 3. Launch (optional) ─────────────────────────────────────────────────────
if ($Launch) {
    Write-Host ""
    Write-Host "Launching qbittorrent.exe..." -ForegroundColor Cyan
    Start-Process -FilePath $Exe -WorkingDirectory $BuildDir
}
