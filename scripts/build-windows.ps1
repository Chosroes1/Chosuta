# SPDX-License-Identifier: GPL-3.0-or-later
# Native Windows build + local deployment. BtbN download is explicitly opt-in.
# PowerShell 5.1+; use an existing x64 MSVC developer shell or matching MinGW.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$QtPrefix,
    [Parameter(Mandatory=$true)][string]$IcuPrefix,
    [ValidateSet('MSVC', 'MinGW')][string]$Toolchain = 'MSVC',
    [string]$IcuRuntimeDir = '',
    [string]$FfmpegDir = '',
    [switch]$DownloadFfmpeg,
    [string]$FfmpegCacheDir = 'out/deps/btbn',
    [string]$FfmpegRelease = '',
    [switch]$BothVariants,
    [switch]$CreateZip,
    [string]$BuildDir = 'build-windows',
    [string]$StageDir = 'out/windows/Chosuta',
    [ValidateRange(1, 64)][int]$Jobs = 4
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProjectDir = Split-Path -Parent $PSScriptRoot
if ($env:OS -ne 'Windows_NT') { throw 'Run this script on Windows, not Linux or WSL.' }
if ($DownloadFfmpeg -and $FfmpegDir) { throw 'Choose -DownloadFfmpeg or -FfmpegDir, not both.' }
if (-not $DownloadFfmpeg -and ($PSBoundParameters.ContainsKey('FfmpegCacheDir') -or $PSBoundParameters.ContainsKey('FfmpegRelease'))) {
    throw '-FfmpegCacheDir and -FfmpegRelease require -DownloadFfmpeg.'
}
. (Join-Path $PSScriptRoot 'btbn-ffmpeg.ps1')
$DownloadedFfmpeg = $null

function Project-Path([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { $FullPath = [IO.Path]::GetFullPath($Path) }
    else { $FullPath = [IO.Path]::GetFullPath((Join-Path $ProjectDir $Path)) }
    # Keep drive/UNC roots intact; normalize other directories before deriving
    # sibling variants and paths relative to the deployment root.
    if ($FullPath.Length -gt ([IO.Path]::GetPathRoot($FullPath)).Length) {
        return $FullPath.TrimEnd([char[]]'\/')
    }
    return $FullPath
}
function Require-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required file missing: $Path" }
}
function Invoke-Native([string]$Step, [string]$Program, [string[]]$Arguments) {
    Write-Host "[$Step]"
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Step failed (exit $LASTEXITCODE)." }
}
function Copy-Checked([string]$Source, [string]$Destination) {
    $Target = Join-Path $Destination (Split-Path -Leaf $Source)
    if (Test-Path -LiteralPath $Target) {
        if ((Get-FileHash -LiteralPath $Source).Hash -ne (Get-FileHash -LiteralPath $Target).Hash) {
            throw "Different DLLs/files have the same name: $Target. Use matching SDKs."
        }
    } else { Copy-Item -LiteralPath $Source -Destination $Target }
}
function Copy-LocalNotices([string]$Prefix, [string]$Name) {
    $Destination = Join-Path $StageDir "dependency-notices/$Name"
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($File in @('LICENSE', 'LICENSE.txt', 'LICENSE.md', 'COPYING', 'COPYING.LGPLv3', 'COPYING.GPLv3')) {
        $Source = Join-Path $Prefix $File
        if (Test-Path -LiteralPath $Source -PathType Leaf) { Copy-Item -LiteralPath $Source -Destination $Destination }
    }
    foreach ($Directory in @('licenses', 'LICENSES')) {
        $Source = Join-Path $Prefix $Directory
        if (Test-Path -LiteralPath $Source -PathType Container) {
            Copy-Item -LiteralPath $Source -Destination (Join-Path $Destination $Directory) -Recurse
            break
        }
    }
}

$QtPrefix = (Resolve-Path -LiteralPath $QtPrefix).Path
$IcuPrefix = (Resolve-Path -LiteralPath $IcuPrefix).Path
$BuildDir = Project-Path $BuildDir
$StageDir = Project-Path $StageDir
$QtBin = Join-Path $QtPrefix 'bin'
$DeployTool = Join-Path $QtBin 'windeployqt.exe'
Require-File $DeployTool
Require-File (Join-Path $QtPrefix 'lib/cmake/Qt6/Qt6Config.cmake')
Require-File (Join-Path $IcuPrefix 'include/unicode/uversion.h')
foreach ($Tool in @('cmake', 'ninja')) { Get-Command $Tool -CommandType Application -ErrorAction Stop | Out-Null }
if ($Toolchain -eq 'MSVC') { $CompilerPath = (Get-Command 'cl.exe' -CommandType Application -ErrorAction Stop).Source }
else {
    $CompilerPath = (Get-Command 'g++.exe' -CommandType Application -ErrorAction Stop).Source
    $CompilerTarget = & $CompilerPath -dumpmachine
    if ($LASTEXITCODE -ne 0 -or $CompilerTarget -notmatch '^x86_64-') { throw 'Use the matching x86_64 MinGW toolchain.' }
}
$CompilerDir = Split-Path -Parent $CompilerPath

if (-not $IcuRuntimeDir) {
    foreach ($Candidate in @('bin64', 'bin')) {
        $Directory = Join-Path $IcuPrefix $Candidate
        if (Test-Path -LiteralPath $Directory -PathType Container) {
            $Dlls = @(Get-ChildItem -LiteralPath $Directory -Filter '*.dll' -File | Where-Object { $_.Name -match '^(lib)?icu' })
            if ($Dlls.Count -gt 0) { $IcuRuntimeDir = $Directory; break }
        }
    }
}
if (-not $IcuRuntimeDir) { throw 'ICU runtime DLLs missing. Supply a matching shared ICU SDK or -IcuRuntimeDir.' }
$IcuRuntimeDir = (Resolve-Path -LiteralPath $IcuRuntimeDir).Path
$IcuDlls = @(Get-ChildItem -LiteralPath $IcuRuntimeDir -Filter '*.dll' -File | Where-Object { $_.Name -match '^(lib)?icu' })
foreach ($Pattern in @('^(lib)?icuuc', '^(lib)?icu(in|i18n)', '^(lib)?icu(dt|data)')) {
    if (@($IcuDlls | Where-Object { $_.Name -match $Pattern }).Count -eq 0) { throw "ICU DLL component missing ($Pattern) in $IcuRuntimeDir." }
}
if ($BothVariants -and -not ($FfmpegDir -or $DownloadFfmpeg)) { throw '-BothVariants requires -DownloadFfmpeg or -FfmpegDir.' }
$Stages = @([pscustomobject]@{ Path = $StageDir; Bundled = [bool]($FfmpegDir -or $DownloadFfmpeg) })
if ($BothVariants) { $Stages += [pscustomobject]@{ Path = "${StageDir}-no-ffmpeg"; Bundled = $false } }
foreach ($Stage in $Stages) {
    if (Test-Path -LiteralPath $Stage.Path) {
        if (-not (Test-Path -LiteralPath $Stage.Path -PathType Container) -or @(Get-ChildItem -LiteralPath $Stage.Path -Force).Count -gt 0) {
            throw "StageDir must be empty: $($Stage.Path). Existing files will not be deleted."
        }
    }
    if ($CreateZip -and (Test-Path -LiteralPath "$($Stage.Path).zip")) { throw "Archive already exists: $($Stage.Path).zip" }
}
if ($FfmpegDir) {
    $FfmpegDir = (Resolve-Path -LiteralPath $FfmpegDir).Path
    Require-File (Join-Path $FfmpegDir 'ffmpeg.exe')
    Require-File (Join-Path $FfmpegDir 'ffprobe.exe')
}

# Network access occurs only after local SDK/tool/output preflight succeeds.
if ($DownloadFfmpeg) {
    $FfmpegCacheDir = Project-Path $FfmpegCacheDir
    $DownloadedFfmpeg = Get-BtbnFfmpeg -CacheDir $FfmpegCacheDir -Release $FfmpegRelease
    $FfmpegDir = $DownloadedFfmpeg.BinDir
}

# Environment changes are scoped to this invocation and restored on failure.
$EnvironmentNames = @('PATH', 'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QT_QPA_PLATFORM', 'QML2_IMPORT_PATH', 'QML_IMPORT_PATH')
$SavedEnvironment = @{}
foreach ($Name in $EnvironmentNames) { $SavedEnvironment[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process') }
try {
    # Keep the selected compiler and its companion tools ahead of runtime DLLs.
    # FFmpeg is only needed during deployment, never compiler initialization.
    $env:PATH = "$CompilerDir;$($SavedEnvironment['PATH']);$QtBin;$IcuRuntimeDir"
    $env:QT_PLUGIN_PATH = Join-Path $QtPrefix 'plugins'
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $QtPrefix 'plugins/platforms'
    $ConfigureArgs = @('-S', $ProjectDir, '-B', $BuildDir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
        '-DBUILD_TESTING=OFF', "-DQt6_DIR=$QtPrefix/lib/cmake/Qt6", "-DCMAKE_PREFIX_PATH=$QtPrefix;$IcuPrefix", "-DICU_ROOT=$IcuPrefix")
    $ConfigureArgs += "-DCMAKE_CXX_COMPILER=$($CompilerPath.Replace('\', '/'))"
    Invoke-Native 'configure' 'cmake' $ConfigureArgs
    $BuildInfo = Get-Content -LiteralPath (Join-Path $BuildDir 'build-info.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($BuildInfo.systemName -ne 'Windows' -or $BuildInfo.pointerBytes -ne 8) { throw 'This deployment script requires a native Windows x64 target.' }
    if ($Toolchain -eq 'MSVC' -and $BuildInfo.compilerArchitecture -notin @('x64', 'X64', 'AMD64')) { throw 'Use an x64 MSVC developer shell.' }
    if (($Toolchain -eq 'MSVC' -and $BuildInfo.compilerID -ne 'MSVC') -or ($Toolchain -eq 'MinGW' -and $BuildInfo.compilerID -ne 'GNU')) {
        throw 'Compiler mismatch. Use a fresh BuildDir and matching Qt/ICU/toolchain.'
    }
    Invoke-Native 'build' 'cmake' @('--build', $BuildDir, '--parallel', "$Jobs")
    foreach ($Stage in $Stages) {
        $StageDir = $Stage.Path
        $BundleMedia = $Stage.Bundled
        foreach ($Name in $EnvironmentNames) { [Environment]::SetEnvironmentVariable($Name, $SavedEnvironment[$Name], 'Process') }
        $env:PATH = "$QtBin;$IcuRuntimeDir;$FfmpegDir;$($SavedEnvironment['PATH'])"
        $env:QT_PLUGIN_PATH = Join-Path $QtPrefix 'plugins'
        Invoke-Native 'install application and notices' 'cmake' @('--install', $BuildDir, '--prefix', $StageDir)
        $AppDir = $StageDir
        Require-File (Join-Path $AppDir 'chosuta.exe')
        Require-File (Join-Path $AppDir 'chosuta-cli.exe')
        $ValidationDir = Join-Path $StageDir 'validation'
        New-Item -ItemType Directory -Path $ValidationDir -Force | Out-Null
        $RuntimeFlag = '--no-compiler-runtime'
        if ($Toolchain -eq 'MinGW') { $RuntimeFlag = '--compiler-runtime' }
        # MSVC uses the existing official VC++ x64 Redistributable on the target.
        # Do not copy arbitrary compiler runtime DLLs out of Visual Studio.
        Invoke-Native 'deploy Qt GUI and multimedia' $DeployTool @('--release', '--no-translations', $RuntimeFlag, '--dir', $AppDir, (Join-Path $AppDir 'chosuta.exe'))
        Invoke-Native 'deploy Qt CLI' $DeployTool @('--release', '--no-translations', $RuntimeFlag, '--dir', $AppDir, (Join-Path $AppDir 'chosuta-cli.exe'))
        foreach ($Dll in $IcuDlls) { Copy-Checked $Dll.FullName $AppDir }
        $PlatformDir = Join-Path $AppDir 'platforms'
        New-Item -ItemType Directory -Path $PlatformDir -Force | Out-Null
        Require-File (Join-Path $PlatformDir 'qwindows.dll')
        "[Paths]`nPrefix=.`nPlugins=.`n" | Set-Content -LiteralPath (Join-Path $AppDir 'qt.conf') -Encoding ASCII
        Copy-LocalNotices $QtPrefix 'Qt'
        Copy-LocalNotices $IcuPrefix 'ICU'
        $FfmpegVersion = ''
        $MediaDir = Join-Path $AppDir 'tools/ffmpeg'
        if ($BundleMedia) {
            New-Item -ItemType Directory -Path $MediaDir -Force | Out-Null
            $VersionOutput = & (Join-Path $FfmpegDir 'ffmpeg.exe') -version
            if ($LASTEXITCODE -ne 0) { throw 'Cannot query selected FFmpeg.' }
            $FfmpegVersion = $VersionOutput -join "`n"
            if ($FfmpegVersion -match '--enable-nonfree') { throw 'FFmpeg --enable-nonfree builds cannot be deployed by this project.' }
            $ProbeVersion = & (Join-Path $FfmpegDir 'ffprobe.exe') -version
            if ($LASTEXITCODE -ne 0) { throw 'Cannot query selected ffprobe.' }
            if (($ProbeVersion -join "`n") -match '--enable-nonfree') { throw 'ffprobe --enable-nonfree builds cannot be deployed by this project.' }
            $ProbeVersion | Set-Content -LiteralPath (Join-Path $ValidationDir 'ffprobe-version.txt') -Encoding UTF8
            foreach ($Name in @('ffmpeg.exe', 'ffprobe.exe')) { Copy-Checked (Join-Path $FfmpegDir $Name) $MediaDir }
            foreach ($Dll in @(Get-ChildItem -LiteralPath $FfmpegDir -Filter '*.dll' -File)) { Copy-Checked $Dll.FullName $MediaDir }
            Copy-LocalNotices (Split-Path -Parent $FfmpegDir) 'FFmpeg'
            if ($DownloadedFfmpeg) {
                foreach ($Name in @('source.json', 'checksums.sha256')) {
                    Copy-Item -LiteralPath (Join-Path $DownloadedFfmpeg.CacheDir $Name) -Destination (Join-Path $StageDir 'dependency-notices/FFmpeg')
                }
            }
            $FfmpegVersion | Set-Content -LiteralPath (Join-Path $ValidationDir 'ffmpeg-version.txt') -Encoding UTF8
        }
        Copy-Item -LiteralPath (Join-Path $BuildDir 'build-info.json') -Destination $ValidationDir
        $DeployVersion = & $DeployTool --version
        if ($LASTEXITCODE -ne 0) { throw 'Cannot query windeployqt version.' }
    
        # Runtime paths are relative to the deployment root. Exclude evidence,
        # documentation and notices from the runtime byte total.
        $Files = @(Get-ChildItem -LiteralPath $AppDir -File -Recurse | Where-Object {
            $Relative = $_.FullName.Substring($AppDir.Length + 1).Replace('\', '/')
            $Relative -notmatch '^(validation|dependency-notices|share)/'
        } | Sort-Object FullName | ForEach-Object {
            [ordered]@{
                path = $_.FullName.Substring($AppDir.Length + 1).Replace('\', '/')
                bytes = $_.Length
            }
        })
        $RuntimeBytes = [long]0
        $MediaToolsBytes = [long]0
        foreach ($File in $Files) {
            $RuntimeBytes += $File['bytes']
            if ($File['path'].StartsWith('tools/ffmpeg/')) { $MediaToolsBytes += $File['bytes'] }
        }
        $Manifest = [ordered]@{
            version = $BuildInfo.chosutaVersion
            target = 'Windows x64'
            entryPoint = 'chosuta.exe'
            runtimeDirectory = '.'
            build = $BuildInfo
            windeployqt = ($DeployVersion -join "`n")
            ffmpegBundled = $BundleMedia
            ffmpegDownload = $(if ($BundleMedia -and $DownloadedFfmpeg) { $DownloadedFfmpeg.Source } else { $null })
            variant = $(if ($BundleMedia) { 'with-ffmpeg' } else { 'no-ffmpeg' })
            mediaToolsDirectory = $(if ($BundleMedia) { 'tools/ffmpeg' } else { $null })
            runtimeBytes = $RuntimeBytes
            mediaToolsBytes = $MediaToolsBytes
            mediaCapabilities = $(if ($BundleMedia) { 'not run by build script' } else { 'not bundled' })
            msvcRuntime = 'Official VC++ x64 Redistributable must already be installed for MSVC builds.'
            ctest = 'not run by build script'
            deployedCli = 'not run by build script'
            deployedOffscreenGui = 'not run by build script'
            deployedWindowsGuiStartup = 'not run by build script'
            files = $Files
        }
        $Manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $StageDir 'runtime-manifest.json') -Encoding UTF8
        @'
Chosuta local Windows build
Start chosuta.exe in this directory directly. Keep the whole directory, including DLLs and plugins.
MSVC builds require the official VC++ x64 Redistributable to be installed.
FFmpeg/ffprobe are optional command-line tools, isolated under tools/ffmpeg.
For no-ffmpeg builds, choose ffmpeg.exe in Export Settings; keep ffprobe.exe and
its shared DLLs beside it. Qt media DLLs/plugins must remain in the application.
Missing FFmpeg disables video export and waveform decoding/correction.
Missing ffprobe requires manual audio duration. Editing/saving still work.
Build only: automated tests and application startup checks were not run.
See runtime-manifest.json and validation/ for build and dependency information.
The dependency notices collected here do not replace a complete redistribution
source/NOTICE review of the specific Qt, ICU and media dependencies you deploy.
'@ | Set-Content -LiteralPath (Join-Path $StageDir 'RUN.txt') -Encoding ASCII
        Write-Host "Built: $(Join-Path $AppDir 'chosuta.exe')"
        Write-Host "Evidence: $(Join-Path $StageDir 'runtime-manifest.json')"
    }
    # Archive after every requested deployment finishes. Never overwrite.
    if ($CreateZip) {
        foreach ($Stage in $Stages) {
            Compress-Archive -LiteralPath $Stage.Path -DestinationPath "$($Stage.Path).zip" -CompressionLevel Optimal
            Write-Host "Archive: $($Stage.Path).zip"
        }
    }
} finally {
    foreach ($Name in $EnvironmentNames) { [Environment]::SetEnvironmentVariable($Name, $SavedEnvironment[$Name], 'Process') }
}
