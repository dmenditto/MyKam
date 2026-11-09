# Helpers for SkunkCrafts Updater support (https://updater.skunkcrafts.com/). Dot-sourced by
# tools\package.ps1 and tools\publish-depot.ps1.
#
# How it works: the release zip carries MyKam\skunkcrafts_updater.cfg. The updater finds it in
# Resources\plugins\MyKam, reads the depot URL (`module`) and compares the installed `version` with the
# one published in the depot. The depot is a plain folder on GitHub Pages holding every file of the
# MyKam folder plus the updater's lists (CRC32 and size per file).

$SkunkcraftsConfigName = 'skunkcrafts_updater.cfg'
$SkunkcraftsProductName = 'MyKam'
# GitHub Pages of the MyKam repository, branch gh-pages, folder updater/MyKam.
$SkunkcraftsDepotUrl = 'https://dmenditto.github.io/MyKam/updater/MyKam'
$SkunkcraftsDepotFolder = 'updater/MyKam'

# Files the user edits: installed when missing, never replaced afterwards (CRC -1 in the whitelist).
$SkunkcraftsOnceFiles = @('mykam.ini')

# Files to delete from users' installs (renamed or removed files). Paths relative to the MyKam folder.
$SkunkcraftsBlacklist = @()

# The text files the updater reads: no byte order mark, LF line ends.
function Write-SkunkcraftsText {
    param([string]$Path, [string[]]$Lines)
    $text = ($Lines -join "`n") + "`n"
    [System.IO.File]::WriteAllText($Path, $text, (New-Object System.Text.UTF8Encoding($false)))
}

function Write-SkunkcraftsConfig {
    param([string]$Path, [string]$Version, [bool]$Locked = $false)
    if ($Version -match '\|') { throw "The version must not contain '|': $Version" }
    Write-SkunkcraftsText -Path $Path -Lines @(
        "name|$SkunkcraftsProductName",
        "module|$SkunkcraftsDepotUrl",
        "version|$Version",
        "locked|$($Locked.ToString().ToLowerInvariant())",
        'zone|custom'
    )
}

# CHANGELOG.md (Keep a Changelog) to the updater's changelog.txt: newest version first as a bare line,
# entries starting with "- ", a blank line between versions. "Unreleased" is skipped; the newest released
# section must be $Version so the changelog and the configuration agree.
function Write-SkunkcraftsChangelog {
    param([string]$MarkdownPath, [string]$Path, [string]$Version)
    $out = New-Object System.Collections.Generic.List[string]
    $current = $null
    $first = $null
    foreach ($line in Get-Content -LiteralPath $MarkdownPath -Encoding UTF8) {
        if ($line -match '^##\s+\[([^\]]+)\]') {
            $name = $Matches[1]
            if ($name -ieq 'Unreleased') { $current = $null; continue }
            if ($null -eq $first) { $first = $name }
            if ($out.Count -gt 0) { $out.Add('') }
            $out.Add($name)
            $current = $name
            continue
        }
        if ($null -eq $current) { continue }
        $text = $line.Trim()
        if ($text -eq '' -or $text.StartsWith('#')) { continue }
        if ($text.StartsWith('- ')) { $out.Add($text) } else { $out.Add("- $text") }
    }
    if ($first -ne $Version) {
        throw "CHANGELOG.md: the newest released section is '$first' but the version is '$Version'. Add a '## [$Version]' section."
    }
    Write-SkunkcraftsText -Path $Path -Lines $out.ToArray()
}

if (-not ('MyKamCrc32' -as [type])) {
    Add-Type -TypeDefinition @'
public static class MyKamCrc32 {
    static readonly uint[] Table = Build();
    static uint[] Build() {
        var t = new uint[256];
        for (uint n = 0; n < 256; n++) {
            uint c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }
    public static uint Of(string path) {
        uint crc = 0xFFFFFFFFu;
        var buffer = new byte[1 << 16];
        using (var s = System.IO.File.OpenRead(path)) {
            int read;
            while ((read = s.Read(buffer, 0, buffer.Length)) > 0)
                for (int i = 0; i < read; i++) crc = Table[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFu;
    }
}
'@
}

# Writes the updater's lists next to the product files in $Folder (a copy of the MyKam folder).
function Write-SkunkcraftsLists {
    param([string]$Folder)
    $base = (Resolve-Path -LiteralPath $Folder).Path.TrimEnd('\') + '\'
    $listNames = @('skunkcrafts_updater_whitelist.txt', 'skunkcrafts_updater_sizeslist.txt',
                   'skunkcrafts_updater_blacklist.txt', $SkunkcraftsConfigName)
    $white = New-Object System.Collections.Generic.List[string]
    $sizes = New-Object System.Collections.Generic.List[string]
    Get-ChildItem -LiteralPath $Folder -Recurse -File | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($base.Length).Replace('\', '/')
        if ($listNames -contains $relative) { return }
        if (($relative -split '/') | Where-Object { $_.StartsWith('.') }) { return }  # never published
        if ($relative.Contains('|')) { throw "A file name must not contain '|': $relative" }
        $crc = if ($SkunkcraftsOnceFiles -contains $relative) { '-1' } else { [MyKamCrc32]::Of($_.FullName).ToString() }
        $white.Add("$relative|$crc")
        $sizes.Add("$relative|$($_.Length)")
    }
    Write-SkunkcraftsText -Path (Join-Path $Folder 'skunkcrafts_updater_whitelist.txt') -Lines $white.ToArray()
    Write-SkunkcraftsText -Path (Join-Path $Folder 'skunkcrafts_updater_sizeslist.txt') -Lines $sizes.ToArray()
    $black = [System.IO.File]::Create((Join-Path $Folder 'skunkcrafts_updater_blacklist.txt'))
    $black.Dispose()
    if ($SkunkcraftsBlacklist.Count -gt 0) {
        Write-SkunkcraftsText -Path (Join-Path $Folder 'skunkcrafts_updater_blacklist.txt') -Lines $SkunkcraftsBlacklist
    }
    return $white.Count
}
