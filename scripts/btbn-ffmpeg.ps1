# SPDX-License-Identifier: GPL-3.0-or-later
# PowerShell 5.1+. Importing this file never accesses the network.
function Get-BtbnJson([string]$Uri) {
    Invoke-RestMethod -Uri $Uri -Headers @{ 'User-Agent' = 'Chosuta-build'; Accept = 'application/vnd.github+json' } -TimeoutSec 60
}
function Save-BtbnDownload([string]$Uri, [string]$Path) {
    Invoke-WebRequest -UseBasicParsing -Uri $Uri -OutFile $Path -TimeoutSec 600
}
function Assert-BtbnRelease([string]$Release) {
    if ($Release -cnotmatch '^autobuild-\d{4}-\d{2}-\d{2}-\d{2}-\d{2}$') {
        throw 'BtbN release must be an exact autobuild-YYYY-MM-DD-HH-MM tag.'
    }
}
function Get-BtbnChecksum([string]$Path, [string]$Asset) {
    $Hashes = @(Get-Content -LiteralPath $Path | ForEach-Object {
        if ($_ -match '^([a-fA-F0-9]{64})\s+\*?(.+)$' -and $Matches[2] -ceq $Asset) { $Matches[1].ToLowerInvariant() }
    })
    if ($Hashes.Count -ne 1) { throw "Expected exactly one SHA-256 entry for $Asset." }
    return $Hashes[0]
}
function Expand-BtbnArchive([string]$Archive, [string]$Destination, [string]$Stem, [switch]$VerifyOnly) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $Zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    $Sha = [Security.Cryptography.SHA256]::Create()
    $Paths = @{}
    $Files = @{}
    $Total = [long]0
    try {
        # Validate every path before writing any entry. Reject Windows aliases,
        # links and duplicate paths even when running the offline test on Unix.
        foreach ($Entry in $Zip.Entries) {
            $Name = $Entry.FullName
            $Parts = $Name.TrimEnd('/').Split('/')
            if ($Name.Contains('\') -or $Parts[0] -cne $Stem) { throw "Unsafe ZIP entry: $Name" }
            foreach ($Part in $Parts) {
                if (-not $Part -or $Part -in @('.', '..') -or $Part -match '[<>:"|?*\x00-\x1f]' -or
                    $Part -match '[. ]$' -or $Part -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') {
                    throw "Unsafe ZIP entry: $Name"
                }
            }
            $Mode = ($Entry.ExternalAttributes -shr 16) -band 0xF000
            if ($Mode -eq 0xA000 -or ($Entry.ExternalAttributes -band 0x400) -ne 0) { throw "ZIP link rejected: $Name" }
            $Key = $Name.TrimEnd('/')
            if ($Paths.ContainsKey($Key)) { throw "Duplicate ZIP entry: $Name" }
            $Paths[$Key] = $true
            $Total += $Entry.Length
            if ($Total -gt 2GB -or $Paths.Count -gt 20000) { throw 'BtbN ZIP exceeds extraction limits.' }
            if (-not $Name.EndsWith('/')) { $Files[$Key] = $true }
        }
        foreach ($Required in @("$Stem/bin/ffmpeg.exe", "$Stem/bin/ffprobe.exe", "$Stem/LICENSE.txt")) {
            if (-not $Files.ContainsKey($Required)) { throw "BtbN ZIP missing $Required." }
        }
        if (@($Files.Keys | Where-Object { $_ -match ('^' + [regex]::Escape($Stem) + '/bin/[^/]+\.dll$') }).Count -eq 0) {
            throw 'Expected a shared BtbN build with DLLs in bin.'
        }
        if ($VerifyOnly) {
            $Existing = @(Get-ChildItem -LiteralPath $Destination -Recurse -Force)
            if (@($Existing | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw 'Cache links are not allowed.' }
            if (@($Existing | Where-Object { -not $_.PSIsContainer }).Count -ne $Files.Count) { throw 'Cached payload file set changed.' }
        }
        foreach ($Entry in $Zip.Entries) {
            $Target = Join-Path $Destination $Entry.FullName
            if ($Entry.FullName.EndsWith('/')) { continue }
            $InputStream = $Entry.Open()
            try {
                if ($VerifyOnly) {
                    $Expected = [BitConverter]::ToString($Sha.ComputeHash($InputStream)).Replace('-', '')
                    if (-not (Test-Path -LiteralPath $Target -PathType Leaf) -or
                        (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash -ne $Expected) { throw "Cached file changed: $Target" }
                } else {
                    [IO.Directory]::CreateDirectory((Split-Path -Parent $Target)) | Out-Null
                    $OutputStream = [IO.File]::Open($Target, [IO.FileMode]::CreateNew)
                    try { $InputStream.CopyTo($OutputStream) } finally { $OutputStream.Dispose() }
                }
            } finally { $InputStream.Dispose() }
        }
    } finally { $Sha.Dispose(); $Zip.Dispose() }
}
function Get-BtbnFfmpeg([string]$CacheDir, [string]$Release = '') {
    $CacheDir = [IO.Path]::GetFullPath($CacheDir)
    $Pin = Join-Path $CacheDir 'selected-release.txt'
    $UseDefault = -not $Release
    $OldTls = [Net.ServicePointManager]::SecurityProtocol
    $Temporary = $null
    try {
        [Net.ServicePointManager]::SecurityProtocol = $OldTls -bor [Net.SecurityProtocolType]::Tls12
        if ($UseDefault -and (Test-Path -LiteralPath $Pin -PathType Leaf)) {
            $Release = (Get-Content -LiteralPath $Pin -Raw).Trim()
        }
        if (-not $Release) {
            Write-Host '[resolve BtbN dated release; first use only]'
            $Releases = @(Get-BtbnJson 'https://api.github.com/repos/BtbN/FFmpeg-Builds/releases?per_page=20')
            $Candidates = @($Releases | Where-Object { -not $_.draft -and -not $_.prerelease -and $_.tag_name -cmatch '^autobuild-\d{4}-\d{2}-\d{2}-\d{2}-\d{2}$' } | Sort-Object tag_name -Descending)
            if ($Candidates.Count -eq 0) { throw 'No dated BtbN release found.' }
            $Release = $Candidates[0].tag_name
        }
        Assert-BtbnRelease $Release
        $Final = Join-Path $CacheDir $Release
        $Ancestor = $Final
        while ($Ancestor) {
            if ((Test-Path -LiteralPath $Ancestor) -and ((Get-Item -LiteralPath $Ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                throw 'BtbN cache must not use symlinks or junctions.'
            }
            $Ancestor = Split-Path -Parent $Ancestor
        }
        $BaseUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/$Release/"
        if (-not (Test-Path -LiteralPath $Final)) {
            $Info = Get-BtbnJson "https://api.github.com/repos/BtbN/FFmpeg-Builds/releases/tags/$Release"
            if ($Info.tag_name -cne $Release -or $Info.draft -or $Info.prerelease) { throw 'Unexpected BtbN release metadata.' }
            # Stable 9.0 branch, x64 GPL shared only; no master/LGPL/static/nonfree fallback.
            $Assets = @($Info.assets | Where-Object { $_.name -cmatch '^ffmpeg-n9\.0[0-9A-Za-z.-]*-win64-gpl-shared-9\.0\.zip$' })
            $Checksums = @($Info.assets | Where-Object { $_.name -ceq 'checksums.sha256' })
            if ($Assets.Count -ne 1 -or $Checksums.Count -ne 1) { throw 'BtbN release must contain one win64 GPL shared 9.0 ZIP and checksums.sha256.' }
            $Asset = $Assets[0].name
            if ($Assets[0].browser_download_url -cne ($BaseUrl + $Asset) -or
                $Checksums[0].browser_download_url -cne ($BaseUrl + 'checksums.sha256')) { throw 'Unexpected BtbN asset URL.' }
            [IO.Directory]::CreateDirectory($CacheDir) | Out-Null
            $Temporary = Join-Path $CacheDir ('.download-' + [guid]::NewGuid().ToString('N'))
            [IO.Directory]::CreateDirectory($Temporary) | Out-Null
            Write-Host "[download BtbN $Release / $Asset]"
            Save-BtbnDownload ($BaseUrl + 'checksums.sha256') (Join-Path $Temporary 'checksums.sha256')
            Save-BtbnDownload ($BaseUrl + $Asset) (Join-Path $Temporary 'archive.zip')
            $Hash = Get-BtbnChecksum (Join-Path $Temporary 'checksums.sha256') $Asset
            if ((Get-FileHash -LiteralPath (Join-Path $Temporary 'archive.zip') -Algorithm SHA256).Hash -ne $Hash) { throw 'BtbN ZIP SHA-256 mismatch.' }
            $Stem = [IO.Path]::GetFileNameWithoutExtension($Asset)
            Expand-BtbnArchive (Join-Path $Temporary 'archive.zip') (Join-Path $Temporary 'payload') $Stem
            [ordered]@{ provider = 'BtbN/FFmpeg-Builds'; release = $Release; asset = $Asset; url = $BaseUrl + $Asset; sha256 = $Hash } |
                ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Temporary 'source.json') -Encoding UTF8
            [IO.Directory]::Move($Temporary, $Final)
            $Temporary = $null
        }
        foreach ($Child in @(Get-ChildItem -LiteralPath $Final -Force -Recurse)) {
            if ($Child.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'BtbN cache links are not allowed.' }
        }
        $Source = Get-Content -LiteralPath (Join-Path $Final 'source.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($Source.provider -cne 'BtbN/FFmpeg-Builds' -or $Source.release -cne $Release -or
            $Source.asset -cnotmatch '^ffmpeg-n9\.0[0-9A-Za-z.-]*-win64-gpl-shared-9\.0\.zip$' -or
            $Source.url -cne ($BaseUrl + $Source.asset)) { throw 'Invalid BtbN cache metadata.' }
        $Hash = Get-BtbnChecksum (Join-Path $Final 'checksums.sha256') $Source.asset
        if ($Source.sha256 -ne $Hash -or (Get-FileHash -LiteralPath (Join-Path $Final 'archive.zip') -Algorithm SHA256).Hash -ne $Hash) {
            throw 'Cached BtbN ZIP SHA-256 mismatch. Preserve it for diagnosis and choose a fresh -FfmpegCacheDir.'
        }
        $Stem = [IO.Path]::GetFileNameWithoutExtension($Source.asset)
        Expand-BtbnArchive (Join-Path $Final 'archive.zip') (Join-Path $Final 'payload') $Stem -VerifyOnly
        # Commit the default selection only after the full cache passes validation.
        if ($UseDefault -and -not (Test-Path -LiteralPath $Pin)) {
            $PinStream = [IO.File]::Open($Pin, [IO.FileMode]::CreateNew)
            try {
                $Bytes = [Text.Encoding]::UTF8.GetBytes($Release)
                $PinStream.Write($Bytes, 0, $Bytes.Length)
            } finally { $PinStream.Dispose() }
        }
        Write-Host "[verified BtbN cache: $Final]"
        return [pscustomobject]@{ BinDir = Join-Path $Final "payload/$Stem/bin"; CacheDir = $Final; Source = $Source }
    } catch {
        throw "BtbN preparation failed: $($_.Exception.Message) Use -FfmpegDir with an existing GPL shared bin directory, or retry with a fresh -FfmpegCacheDir."
    } finally {
        if ($Temporary -and (Test-Path -LiteralPath $Temporary)) { Remove-Item -LiteralPath $Temporary -Recurse -Force }
        [Net.ServicePointManager]::SecurityProtocol = $OldTls
    }
}
