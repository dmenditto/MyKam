# Builds the SkunkCrafts Updater depot for MyKam from the release zip and, with -Push, publishes it on
# GitHub Pages (branch gh-pages, folder updater/MyKam). Run tools\package.ps1 first.
#
#   .\tools\publish-depot.ps1           builds dist\depot and lists what it holds (nothing leaves the machine)
#   .\tools\publish-depot.ps1 -Push     builds, commits the depot to gh-pages and pushes it
#
# A depot update is one git commit, and GitHub Pages swaps the whole site at once, so users never see
# half an upload; the updater's `locked` flag is not needed for that.

#   .\tools\publish-depot.ps1 -DepotUrl http://127.0.0.1:8765/updater/MyKam
#                                       a local test depot (serve dist\depot with any static web server)

param(
    [switch]$Push,
    [string]$Remote = 'origin',
    [string]$DepotUrl
)

$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $root
. (Join-Path $PSScriptRoot 'skunkcrafts.ps1')

$versionLine = Select-String -Path (Join-Path $root 'src\core\version.h') -Pattern 'kVersion\s*=\s*"([^"]+)"'
if (-not $versionLine) { throw 'Could not read kVersion from src\core\version.h' }
$version = $versionLine.Matches[0].Groups[1].Value

$zip = Join-Path $root "dist\MyKam-$version-win64.zip"
if (-not (Test-Path $zip)) { throw "Release zip not found: $zip. Run tools\package.ps1 first." }

# The depot is the zip's MyKam folder plus the lists.
$site = Join-Path $root 'dist\depot'
$depot = Join-Path $site ($SkunkcraftsDepotFolder.Replace('/', '\'))
if (Test-Path $site) { Remove-Item -Recurse -Force $site }
New-Item -ItemType Directory -Force -Path $depot | Out-Null
$unpack = Join-Path $root 'dist\depot-unpack'
if (Test-Path $unpack) { Remove-Item -Recurse -Force $unpack }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory($zip, $unpack)
Copy-Item -Path (Join-Path $unpack 'MyKam\*') -Destination $depot -Recurse
Remove-Item -Recurse -Force $unpack

$config = Join-Path $depot $SkunkcraftsConfigName
if (-not (Test-Path $config)) { throw "The zip has no MyKam\$SkunkcraftsConfigName. Re-run tools\package.ps1." }
if ((Get-Content -LiteralPath $config) -notcontains "version|$version") { throw "$SkunkcraftsConfigName in the zip is not version $version." }

if ($DepotUrl) {
    if ($Push) { throw '-DepotUrl is for local tests only; it cannot be combined with -Push.' }
    $SkunkcraftsDepotUrl = $DepotUrl.TrimEnd('/')
    Write-SkunkcraftsConfig -Path $config -Version $version
}

$count = Write-SkunkcraftsLists -Folder $depot
# GitHub Pages: serve every file as it is (no Jekyll processing).
[System.IO.File]::Create((Join-Path $site '.nojekyll')).Dispose()
Write-Output "Depot for MyKam $version in $depot ($count files)"
Get-Content (Join-Path $depot 'skunkcrafts_updater_whitelist.txt') | ForEach-Object { "  $_" }

if (-not $Push) {
    Write-Output 'Not published (run with -Push to publish on GitHub Pages).'
    return
}

$status = git status --porcelain --untracked-files=no
if ($status) { throw 'The working tree has uncommitted changes. Commit the release first, so the depot matches a commit.' }

$worktree = Join-Path $root 'dist\gh-pages'
if (Test-Path $worktree) { git worktree remove --force $worktree 2>$null; if (Test-Path $worktree) { Remove-Item -Recurse -Force $worktree } }
git fetch $Remote gh-pages 2>$null
if ($LASTEXITCODE -eq 0) {
    git worktree add -B gh-pages $worktree "$Remote/gh-pages"
} else {
    git worktree add --detach $worktree
    git -C $worktree checkout --orphan gh-pages
    git -C $worktree rm -rf --quiet . 2>$null
}
if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the gh-pages worktree.' }

try {
    $published = Join-Path $worktree ($SkunkcraftsDepotFolder.Replace('/', '\'))
    if (Test-Path $published) { Remove-Item -Recurse -Force $published }
    New-Item -ItemType Directory -Force -Path $published | Out-Null
    Copy-Item -Path (Join-Path $depot '*') -Destination $published -Recurse
    Copy-Item -Path (Join-Path $site '.nojekyll') -Destination $worktree -Force
    git -C $worktree add --all
    if ($LASTEXITCODE -ne 0) { throw 'git add failed.' }
    git -C $worktree diff --cached --quiet
    if ($LASTEXITCODE -eq 0) {
        Write-Output 'The published depot is already this version: nothing to push.'
        return
    }
    git -C $worktree -c user.name=dmenditto -c user.email=30751107+dmenditto@users.noreply.github.com commit --quiet -m "Updater depot: MyKam $version"
    if ($LASTEXITCODE -ne 0) { throw 'git commit failed.' }
    git -C $worktree push $Remote gh-pages
    if ($LASTEXITCODE -ne 0) { throw 'git push failed.' }
    # Turn GitHub Pages on for gh-pages the first time (needs the GitHub CLI, signed in).
    if (Get-Command gh -ErrorAction SilentlyContinue) {
        gh api repos/dmenditto/MyKam/pages --silent 2>$null
        if ($LASTEXITCODE -ne 0) {
            gh api -X POST repos/dmenditto/MyKam/pages -f 'source[branch]=gh-pages' -f 'source[path]=/' --silent
            if ($LASTEXITCODE -ne 0) { Write-Warning 'Could not turn GitHub Pages on. Do it in the repository settings: Pages, branch gh-pages, folder /.' }
            else { Write-Output 'Turned GitHub Pages on for the gh-pages branch.' }
        }
    } else {
        Write-Warning 'GitHub CLI not found: make sure Pages serves the gh-pages branch (repository settings, Pages).'
    }
    Write-Output "Pushed the depot for MyKam $version. GitHub Pages publishes it in a minute or two at $SkunkcraftsDepotUrl/$SkunkcraftsConfigName"
} finally {
    git worktree remove --force $worktree
}
