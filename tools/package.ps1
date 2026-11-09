# Builds MyKam (Release, x64), runs the unit tests and packs the release zip:
#   dist\MyKam-<version>-win64.zip with a top-level MyKam\ folder.
# Stops at the first failure (build or tests). Usage: .\tools\package.ps1 [-MSBuildPath <MSBuild.exe>]

param(
    [string]$MSBuildPath
)

$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $root

# The version comes from the single place that defines it.
$versionLine = Select-String -Path (Join-Path $root 'src\core\version.h') -Pattern 'kVersion\s*=\s*"([^"]+)"'
if (-not $versionLine) { throw 'Could not read kVersion from src\core\version.h' }
$version = $versionLine.Matches[0].Groups[1].Value

# Find MSBuild: parameter, then PATH, then vswhere, then the usual Build Tools folder.
if ([string]::IsNullOrWhiteSpace($MSBuildPath)) {
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { $MSBuildPath = $cmd.Source }
}
if ([string]::IsNullOrWhiteSpace($MSBuildPath)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
        if ($found) { $MSBuildPath = $found }
    }
}
if ([string]::IsNullOrWhiteSpace($MSBuildPath) -or -not (Test-Path $MSBuildPath)) {
    throw 'MSBuild.exe not found. Install the Visual Studio 2022 Build Tools or pass -MSBuildPath.'
}

Write-Output "Building MyKam $version with $MSBuildPath"
& $MSBuildPath (Join-Path $root 'MyKam.sln') /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m /nologo /v:m
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit code $LASTEXITCODE)." }

Write-Output 'Running the unit tests'
$tests = Join-Path $root 'build\tests\Release\mykam_tests.exe'
if (-not (Test-Path $tests)) { throw "Test executable not found: $tests" }
& $tests
if ($LASTEXITCODE -ne 0) { throw "Unit tests failed (exit code $LASTEXITCODE). No zip was made." }

$built = Join-Path $root 'build\MyKam'
$xpl = Join-Path $built 'win_x64\MyKam.xpl'
foreach ($required in @($xpl, (Join-Path $built 'mykam.ini'), (Join-Path $built 'fonts'))) {
    if (-not (Test-Path $required)) { throw "Build output missing: $required" }
}

# Stage the files in dist\stage\MyKam, then zip so the archive has a top-level MyKam\ folder.
$dist = Join-Path $root 'dist'
$stage = Join-Path $dist 'stage'
$package = Join-Path $stage 'MyKam'
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path (Join-Path $package 'win_x64') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $package 'licences') | Out-Null

Copy-Item $xpl (Join-Path $package 'win_x64\MyKam.xpl')
Copy-Item (Join-Path $built 'mykam.ini') (Join-Path $package 'mykam.ini')
Copy-Item (Join-Path $built 'fonts') (Join-Path $package 'fonts') -Recurse
foreach ($doc in 'README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'CHANGELOG.md') {
    Copy-Item (Join-Path $root $doc) (Join-Path $package $doc)
}

# SkunkCrafts Updater support (https://updater.skunkcrafts.com/): the configuration that lets the
# updater find this install and the depot it follows, and the changelog in the updater's format.
. (Join-Path $PSScriptRoot 'skunkcrafts.ps1')
Write-SkunkcraftsConfig -Path (Join-Path $package $SkunkcraftsConfigName) -Version $version
Write-SkunkcraftsChangelog -MarkdownPath (Join-Path $root 'CHANGELOG.md') -Path (Join-Path $package 'changelog.txt') -Version $version

$licences = @{
    'third_party\XPSDK\license.txt'             = 'X-Plane-SDK-license.txt'
    'third_party\imgui\LICENSE.txt'             = 'Dear-ImGui-LICENSE.txt'
    'third_party\doctest\LICENSE.txt'           = 'doctest-LICENSE.txt'
    'third_party\fonts\inter\OFL.txt'           = 'Inter-OFL.txt'
    'third_party\fonts\material\LICENSE'        = 'MaterialIcons-LICENSE.txt'
}
foreach ($entry in $licences.GetEnumerator()) {
    $from = Join-Path $root $entry.Key
    if (-not (Test-Path $from)) { throw "Licence file missing: $from" }
    Copy-Item $from (Join-Path $package "licences\$($entry.Value)")
}

$zip = Join-Path $dist "MyKam-$version-win64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }

# Written entry by entry so the names use forward slashes whatever PowerShell version runs this.
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipStream = [System.IO.File]::Open($zip, [System.IO.FileMode]::Create)
try {
    $writer = New-Object System.IO.Compression.ZipArchive($zipStream, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        $baseLength = (Resolve-Path $stage).Path.TrimEnd('\').Length + 1
        Get-ChildItem -Path $package -Recurse -File | Sort-Object FullName | ForEach-Object {
            $entryName = $_.FullName.Substring($baseLength).Replace('\', '/')
            [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($writer, $_.FullName, $entryName,
                [System.IO.Compression.CompressionLevel]::Optimal)
        }
    } finally {
        $writer.Dispose()
    }
} finally {
    $zipStream.Dispose()
}
Remove-Item -Recurse -Force $stage

$item = Get-Item $zip
Write-Output "Created $($item.FullName) ($($item.Length) bytes)"
$archive = [System.IO.Compression.ZipFile]::OpenRead($zip)
try {
    $archive.Entries | Where-Object { $_.Name } | ForEach-Object { Write-Output ("  {0}  ({1} bytes)" -f $_.FullName, $_.Length) }
} finally {
    $archive.Dispose()
}
