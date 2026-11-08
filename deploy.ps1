# Copies the built MyKam plugin into X-Plane 12 (a developer helper; users install from the release zip).
# Keeps the user's saved cameras (cameras\) and an existing mykam.ini untouched.
#
# The X-Plane folder comes from, in this order:
#   1. the -XPlanePath parameter
#   2. a path set in deploy.local.ps1 (next to this script, git-ignored, run first)
#   3. the XPLANE12_PATH environment variable
#
# Usage: .\deploy.ps1 -XPlanePath "C:\path\to\X-Plane 12"   (after a Release build)

param(
    [string]$XPlanePath
)

$ErrorActionPreference = 'Stop'

# Optional local settings (not part of the repository). Dot-sourced so it can set $XPlanePath.
# A path given with -XPlanePath on the command line always wins over the local file.
$givenOnCommandLine = $XPlanePath
$localSettings = Join-Path $PSScriptRoot 'deploy.local.ps1'
if (Test-Path $localSettings) {
    . $localSettings
}
if (-not [string]::IsNullOrWhiteSpace($givenOnCommandLine)) {
    $XPlanePath = $givenOnCommandLine
}

if ([string]::IsNullOrWhiteSpace($XPlanePath)) {
    $XPlanePath = $env:XPLANE12_PATH
}
if ([string]::IsNullOrWhiteSpace($XPlanePath)) {
    throw "No X-Plane folder given. Pass -XPlanePath ""<your X-Plane 12 folder>"" or set the XPLANE12_PATH environment variable."
}
if (-not (Test-Path (Join-Path $XPlanePath 'Resources\plugins'))) {
    throw "Not an X-Plane 12 folder (Resources\plugins not found): $XPlanePath"
}

$source = Join-Path $PSScriptRoot 'build\MyKam'
$pluginsFolder = Join-Path $XPlanePath 'Resources\plugins'
$target = Join-Path $pluginsFolder 'MyKam'

$plugin = Join-Path $source 'win_x64\MyKam.xpl'
if (-not (Test-Path $plugin)) {
    throw "Build output not found: $plugin. Build the Release configuration first."
}

New-Item -ItemType Directory -Force -Path (Join-Path $target 'win_x64') | Out-Null

try {
    Copy-Item -Path $plugin -Destination (Join-Path $target 'win_x64\MyKam.xpl') -Force
} catch {
    throw "Could not replace MyKam.xpl. Quit X-Plane 12 first, then run deploy again. ($($_.Exception.Message))"
}

# The UI fonts (Inter and Material Icons, with their licences). Only the fonts folder is replaced;
# cameras\ and mykam.ini are never touched. Without the fonts the plugin still runs on ImGui's built-in font.
$fonts = Join-Path $source 'fonts'
if (Test-Path $fonts) {
    $fontTarget = Join-Path $target 'fonts'
    New-Item -ItemType Directory -Force -Path $fontTarget | Out-Null
    Copy-Item -Path (Join-Path $fonts '*') -Destination $fontTarget -Force
    Write-Output "Deployed $((Get-ChildItem $fontTarget -File).Count) font files."
} else {
    Write-Warning "No fonts folder in $source. Build the Release configuration first; the plugin will use the built-in font."
}

$ini = Join-Path $target 'mykam.ini'
if (-not (Test-Path $ini)) {
    Copy-Item -Path (Join-Path $source 'mykam.ini') -Destination $ini
    Write-Output 'Copied default mykam.ini.'
} else {
    Write-Output 'Kept existing mykam.ini.'
}

$installed = Get-Item (Join-Path $target 'win_x64\MyKam.xpl')
Write-Output "Deployed MyKam.xpl ($($installed.Length) bytes, $($installed.LastWriteTime)) to $target"
