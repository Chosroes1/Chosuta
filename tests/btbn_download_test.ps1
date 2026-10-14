# SPDX-License-Identifier: GPL-3.0-or-later
# Offline synthetic ZIP/network fixtures. No executables are launched.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts/btbn-ffmpeg.ps1')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$Work = Join-Path ([IO.Path]::GetTempPath()) ('chosuta-btbn-test-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($Work) | Out-Null
$Tag = 'autobuild-2026-10-01-12-00'
$Stem = 'ffmpeg-n9.0.2-1-g123456-win64-gpl-shared-9.0'
$AssetName = "$Stem.zip"
$BaseUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/$Tag/"
$script:NetworkCalls = 0
$script:Offline = $false
$script:BadHash = $false
$script:BadUrl = $false
$script:ExtraEntry = ''
$script:NoDll = $false
function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Expect-Failure([scriptblock]$Action, [string]$Pattern) {
    try { & $Action | Out-Null } catch {
        if ($_.Exception.Message -notmatch $Pattern) { throw }
        return
    }
    throw "Expected failure: $Pattern"
}
function New-Fixture {
    $ZipPath = Join-Path $Work 'fixture.zip'
    if (Test-Path -LiteralPath $ZipPath) { Remove-Item -LiteralPath $ZipPath }
    $Zip = [IO.Compression.ZipFile]::Open($ZipPath, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $Names = @("$Stem/LICENSE.txt", "$Stem/bin/ffmpeg.exe", "$Stem/bin/ffprobe.exe")
        if (-not $script:NoDll) { $Names += "$Stem/bin/avcodec-test.dll" }
        if ($script:ExtraEntry) { $Names += $script:ExtraEntry }
        foreach ($Name in $Names) {
            $Stream = $Zip.CreateEntry($Name).Open()
            try {
                $Bytes = [Text.Encoding]::UTF8.GetBytes("synthetic fixture $Name")
                $Stream.Write($Bytes, 0, $Bytes.Length)
            } finally { $Stream.Dispose() }
        }
    } finally { $Zip.Dispose() }
    $Hash = (Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256).Hash
    if ($script:BadHash) { $Hash = '0' * 64 }
    "$Hash  $AssetName" | Set-Content -LiteralPath (Join-Path $Work 'checksums.sha256') -Encoding ASCII
}
# Override the only two network entry points. Any unexpected URL fails the test.
function Get-BtbnJson([string]$Uri) {
    $script:NetworkCalls++
    if ($script:Offline) { throw 'Unexpected network request during cache reuse.' }
    if ($Uri -ceq 'https://api.github.com/repos/BtbN/FFmpeg-Builds/releases?per_page=20') {
        return @([pscustomobject]@{ tag_name = 'latest'; draft = $false; prerelease = $false },
                 [pscustomobject]@{ tag_name = $Tag; draft = $false; prerelease = $false })
    }
    Assert ($Uri -ceq "https://api.github.com/repos/BtbN/FFmpeg-Builds/releases/tags/$Tag") "Unexpected API URL: $Uri"
    $AssetUrl = $BaseUrl + $AssetName
    if ($script:BadUrl) { $AssetUrl = 'https://example.invalid/untrusted.zip' }
    return [pscustomobject]@{ tag_name = $Tag; draft = $false; prerelease = $false; assets = @(
        [pscustomobject]@{ name = $AssetName; browser_download_url = $AssetUrl },
        [pscustomobject]@{ name = 'checksums.sha256'; browser_download_url = $BaseUrl + 'checksums.sha256' },
        [pscustomobject]@{ name = 'ffmpeg-N-123-win64-gpl-shared.zip'; browser_download_url = 'unused' }) }
}
function Save-BtbnDownload([string]$Uri, [string]$Path) {
    $script:NetworkCalls++
    if ($script:Offline) { throw 'Unexpected download during cache reuse.' }
    if ($Uri -ceq ($BaseUrl + $AssetName)) { Copy-Item -LiteralPath (Join-Path $Work 'fixture.zip') -Destination $Path }
    elseif ($Uri -ceq ($BaseUrl + 'checksums.sha256')) { Copy-Item -LiteralPath (Join-Path $Work 'checksums.sha256') -Destination $Path }
    else { throw "Unexpected download URL: $Uri" }
}
try {
    # Also parse the orchestration entry point, without running a build.
    $BuildScript = Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts/build-windows.ps1'
    $ParseTokens = $null
    $ParseErrors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($BuildScript, [ref]$ParseTokens, [ref]$ParseErrors) | Out-Null
    Assert ($ParseErrors.Count -eq 0) "Build script syntax errors: $ParseErrors"
    if ($env:OS -eq 'Windows_NT') {
        Expect-Failure { & $BuildScript -QtPrefix 'unused' -IcuPrefix 'unused' -DownloadFfmpeg -FfmpegDir 'unused' } 'not both'
        Expect-Failure { & $BuildScript -QtPrefix 'unused' -IcuPrefix 'unused' -FfmpegRelease $Tag } 'require -DownloadFfmpeg'
        # The build script changes ErrorActionPreference only in its own scope.
    }
    New-Fixture
    $Tls = [Net.ServicePointManager]::SecurityProtocol
    $Cache = Join-Path $Work 'cache with spaces'
    $First = Get-BtbnFfmpeg $Cache
    Assert (Test-Path -LiteralPath (Join-Path $First.BinDir 'ffprobe.exe')) 'Missing ffprobe.'
    Assert ($First.Source.release -ceq $Tag) 'Floating tag was selected.'
    Assert ($script:NetworkCalls -eq 4) 'Unexpected initial request count.'
    Assert ([Net.ServicePointManager]::SecurityProtocol -eq $Tls) 'TLS setting leaked.'
    $script:Offline = $true
    $Second = Get-BtbnFfmpeg $Cache
    Assert ($First.BinDir -ceq $Second.BinDir) 'Cache was not reused.'
    Assert ($script:NetworkCalls -eq 4) 'Cache reuse accessed network.'
    Set-Content -LiteralPath (Join-Path $First.BinDir 'ffmpeg.exe') -Value 'changed'
    Expect-Failure { Get-BtbnFfmpeg $Cache } 'Cached file changed'
    Assert (Test-Path -LiteralPath (Join-Path $First.CacheDir 'archive.zip')) 'Corrupt cache was deleted.'
    $script:Offline = $false

    $script:BadHash = $true; New-Fixture
    $Failed = Join-Path $Work 'bad-hash'
    [IO.Directory]::CreateDirectory($Failed) | Out-Null
    'keep' | Set-Content -LiteralPath (Join-Path $Failed 'user.txt')
    Expect-Failure { Get-BtbnFfmpeg $Failed } 'SHA-256 mismatch'
    Assert (@(Get-ChildItem -LiteralPath $Failed -Force).Count -eq 1) 'Failed download left partial state or removed a user file.'
    Assert ([Net.ServicePointManager]::SecurityProtocol -eq $Tls) 'TLS setting leaked on failure.'
    $script:BadHash = $false

    foreach ($BadName in @("$Stem/../../escaped.txt", "$Stem/bin/FFMPEG.exe", "$Stem/bin/CON.txt", "$Stem/bin/evil:stream", '/absolute.txt')) {
        $script:ExtraEntry = $BadName; New-Fixture
        Expect-Failure { Get-BtbnFfmpeg (Join-Path $Work 'bad-zip') -Release $Tag } 'Unsafe ZIP|Duplicate ZIP'
        Assert (-not (Test-Path -LiteralPath (Join-Path $Work 'escaped.txt'))) 'ZIP escaped cache.'
    }
    $script:ExtraEntry = ''; $script:NoDll = $true; New-Fixture
    Expect-Failure { Get-BtbnFfmpeg (Join-Path $Work 'no-dll') -Release $Tag } 'shared BtbN build'
    $script:NoDll = $false; New-Fixture
    $script:BadUrl = $true
    Expect-Failure { Get-BtbnFfmpeg (Join-Path $Work 'bad-url') -Release $Tag } 'Unexpected BtbN asset URL'
    $script:BadUrl = $false
    $Count = $script:NetworkCalls
    Expect-Failure { Get-BtbnFfmpeg (Join-Path $Work 'bad-tag') -Release '../latest' } 'exact autobuild'
    Assert ($script:NetworkCalls -eq $Count) 'Invalid tag accessed network.'
    $Explicit = Get-BtbnFfmpeg (Join-Path $Work 'explicit') -Release $Tag
    Assert (-not (Test-Path -LiteralPath (Join-Path $Work 'explicit/selected-release.txt'))) 'Explicit release changed default selection.'
    Add-Content -LiteralPath (Join-Path $Explicit.CacheDir 'archive.zip') -Value 'corrupt'
    Expect-Failure { Get-BtbnFfmpeg (Join-Path $Work 'explicit') -Release $Tag } 'SHA-256 mismatch'
    Write-Host 'BtbN offline fixtures passed (no real downloads or executable launches).'
} finally { Remove-Item -LiteralPath $Work -Recurse -Force }
