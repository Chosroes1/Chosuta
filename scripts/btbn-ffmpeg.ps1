# SPDX-License-Identifier: GPL-3.0-or-later
# PowerShell 5.1+. Importing this file never accesses the network.
function Get-BtbnJson([string]$Uri) {
    # Explicit enumeration handles Invoke-RestMethod arrays on PS 5.1.
    $Response = Invoke-RestMethod -Uri $Uri -UserAgent 'Chosuta-build' -Headers @{ Accept = 'application/vnd.github+json' } -TimeoutSec 60
    foreach ($Item in $Response) { $Item }
}
function Save-BtbnDownload([string]$Uri, [string]$Path) {
    Invoke-WebRequest -UseBasicParsing -Uri $Uri -OutFile $Path -TimeoutSec 600
}
function Get-BtbnChecksum([string]$Path, [string]$Asset) {
    $Hashes = @(Get-Content -LiteralPath $Path | ForEach-Object {
        if ($_ -match '^([a-fA-F0-9]{64})\s+\*?(.+)$' -and $Matches[2] -ceq $Asset) { $Matches[1].ToLowerInvariant() }
    })
    if ($Hashes.Count -ne 1) { throw "Expected exactly one SHA-256 entry for $Asset." }
    return $Hashes[0]
}
function Expand-BtbnArchive([string]$Archive, [string]$Destination) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($Archive, $Destination)
    # Floating release ZIP names need not match the directory inside the ZIP.
    $Executables = @(Get-ChildItem -LiteralPath $Destination -Filter ffmpeg.exe -Recurse -File)
    if ($Executables.Count -ne 1) { throw 'BtbN ZIP must contain ffmpeg.exe.' }
    return $Executables[0].Directory.Parent.Name
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
        if (-not $Release) { $Release = 'latest' }
        $Tag = [Uri]::EscapeDataString($Release)
        if ($Tag -in @('.', '..')) { throw 'Invalid release tag.' }
        $Final = Join-Path $CacheDir $Tag
        if (-not (Test-Path -LiteralPath $Final)) {
            $Info = Get-BtbnJson "https://api.github.com/repos/BtbN/FFmpeg-Builds/releases/tags/$Tag"
            # Prefer the newest numbered release branch; use the official shared
            # GPL build when that release only provides the main branch.
            $Assets = @($Info.assets | Where-Object { $_.name -cmatch '^ffmpeg-.*-win64-gpl-shared(?:-[0-9.]+)?\.zip$' })
            $Stable = @($Assets | Where-Object { $_.name -cmatch '^ffmpeg-n[0-9]' } | Sort-Object -Property @{ Expression = {
                if ($_.name -match '^ffmpeg-n(\d+)\.(\d+)') { [long]$Matches[1] * 100000 + [long]$Matches[2] } else { 0 }
            }; Descending = $true })
            if ($Stable.Count) { $Assets = $Stable }
            $Checksums = @($Info.assets | Where-Object { $_.name -ceq 'checksums.sha256' })
            if (-not $Assets.Count -or -not $Checksums.Count) { throw 'BtbN release has no win64 GPL shared ZIP or checksums.sha256.' }
            $Asset = $Assets[0]
            [IO.Directory]::CreateDirectory($CacheDir) | Out-Null
            $Temporary = Join-Path $CacheDir ('.download-' + [guid]::NewGuid().ToString('N'))
            [IO.Directory]::CreateDirectory($Temporary) | Out-Null
            Write-Host "[download BtbN $Release / $($Asset.name)]"
            Save-BtbnDownload $Checksums[0].browser_download_url (Join-Path $Temporary 'checksums.sha256')
            Save-BtbnDownload $Asset.browser_download_url (Join-Path $Temporary 'archive.zip')
            $Hash = Get-BtbnChecksum (Join-Path $Temporary 'checksums.sha256') $Asset.name
            if ((Get-FileHash -LiteralPath (Join-Path $Temporary 'archive.zip') -Algorithm SHA256).Hash -ne $Hash) { throw 'BtbN ZIP SHA-256 mismatch.' }
            $Stem = Expand-BtbnArchive (Join-Path $Temporary 'archive.zip') (Join-Path $Temporary 'payload')
            Remove-Item -LiteralPath (Join-Path $Temporary 'archive.zip')
            [ordered]@{ provider = 'BtbN/FFmpeg-Builds'; release = $Info.tag_name; asset = $Asset.name; url = $Asset.browser_download_url; sha256 = $Hash; payloadRoot = $Stem } |
                ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Temporary 'source.json') -Encoding UTF8
            [IO.Directory]::Move($Temporary, $Final)
            $Temporary = $null
        }
        $Source = Get-Content -LiteralPath (Join-Path $Final 'source.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        $Stem = if ($Source.PSObject.Properties['payloadRoot']) { $Source.payloadRoot } else { [IO.Path]::GetFileNameWithoutExtension($Source.asset) }
        $BinDir = Join-Path $Final "payload/$Stem/bin"
        foreach ($Name in @('ffmpeg.exe', 'ffprobe.exe')) {
            if (-not (Test-Path -LiteralPath (Join-Path $BinDir $Name) -PathType Leaf)) { throw "Cached tool missing: $BinDir/$Name" }
        }
        if ($UseDefault -and -not (Test-Path -LiteralPath $Pin)) {
            [IO.File]::WriteAllText($Pin, $Release)
        }
        Write-Host "[using BtbN cache: $Final]"
        return [pscustomobject]@{ BinDir = $BinDir; CacheDir = $Final; Source = $Source }
    } catch {
        throw "BtbN preparation failed: $($_.Exception.Message)"
    } finally {
        if ($Temporary -and (Test-Path -LiteralPath $Temporary)) { Remove-Item -LiteralPath $Temporary -Recurse -Force }
        [Net.ServicePointManager]::SecurityProtocol = $OldTls
    }
}
