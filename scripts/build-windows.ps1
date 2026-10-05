# SPDX-License-Identifier: GPL-3.0-or-later
# Native Windows build + local deployment. No downloads or installations.
# PowerShell 5.1+; use an existing x64 MSVC developer shell or matching MinGW.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$QtPrefix,
    [Parameter(Mandatory=$true)][string]$IcuPrefix,
    [ValidateSet('MSVC', 'MinGW')][string]$Toolchain = 'MSVC',
    [string]$IcuRuntimeDir = '',
    [string]$FfmpegDir = '',
    [string]$BuildDir = 'build-windows',
    [string]$StageDir = 'out/windows/Chosuta',
    [ValidateRange(1, 64)][int]$Jobs = 4
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProjectDir = Split-Path -Parent $PSScriptRoot
if ($env:OS -ne 'Windows_NT') { throw 'Run this script on Windows, not Linux or WSL.' }

function Project-Path([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $ProjectDir $Path))
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
function Invoke-SelfTest([string]$Name, [string]$Executable, [string[]]$Arguments) {
    $Stdout = Join-Path $ValidationDir "$Name.stdout.log"
    $Stderr = Join-Path $ValidationDir "$Name.stderr.log"
    # Arguments are fixed flags, not user-provided paths/text. No shell is used.
    if (@($Arguments | Where-Object { $_ -match '[\s"]' }).Count -gt 0) { throw 'Self-test accepts fixed flag arguments only.' }
    $StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $StartInfo.FileName = $Executable
    $StartInfo.Arguments = $Arguments -join ' '
    $StartInfo.WorkingDirectory = $StageDir
    $StartInfo.UseShellExecute = $false
    $StartInfo.RedirectStandardOutput = $true
    $StartInfo.RedirectStandardError = $true
    $StartInfo.StandardOutputEncoding = New-Object System.Text.UTF8Encoding -ArgumentList $false
    $StartInfo.StandardErrorEncoding = New-Object System.Text.UTF8Encoding -ArgumentList $false
    $Process = New-Object System.Diagnostics.Process
    $Process.StartInfo = $StartInfo
    try {
        if (-not $Process.Start()) { throw "$Name could not start." }
        $OutputTask = $Process.StandardOutput.ReadToEndAsync()
        $ErrorTask = $Process.StandardError.ReadToEndAsync()
        if (-not $Process.WaitForExit(45000)) {
            $Process.Kill()
            $Process.WaitForExit()
            "Timed out: missing runtime/plugin or a startup dialog." | Set-Content -LiteralPath $Stderr -Encoding UTF8
            throw "$Name timed out. See $Stderr."
        }
        $OutputTask.Result | Set-Content -LiteralPath $Stdout -Encoding UTF8
        $ErrorTask.Result | Set-Content -LiteralPath $Stderr -Encoding UTF8
        if ($Process.ExitCode -ne 0) { throw "$Name failed ($($Process.ExitCode)). See $Stderr." }
    } finally { $Process.Dispose() }
    Write-Host "[deployed $Name passed]"
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
foreach ($Tool in @('cmake', 'ninja', 'ctest')) { Get-Command $Tool -CommandType Application -ErrorAction Stop | Out-Null }
if ($Toolchain -eq 'MSVC') { Get-Command 'cl.exe' -CommandType Application -ErrorAction Stop | Out-Null }
else {
    Get-Command 'g++.exe' -CommandType Application -ErrorAction Stop | Out-Null
    $CompilerTarget = & 'g++.exe' -dumpmachine
    if ($LASTEXITCODE -ne 0 -or $CompilerTarget -notmatch '^x86_64-') { throw 'Use the matching x86_64 MinGW toolchain.' }
}

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
if (Test-Path -LiteralPath $StageDir) {
    if (-not (Test-Path -LiteralPath $StageDir -PathType Container) -or @(Get-ChildItem -LiteralPath $StageDir -Force).Count -gt 0) {
        throw "StageDir must be empty: $StageDir. Choose a new -StageDir; existing files will not be deleted."
    }
}
if ($FfmpegDir) {
    $FfmpegDir = (Resolve-Path -LiteralPath $FfmpegDir).Path
    Require-File (Join-Path $FfmpegDir 'ffmpeg.exe')
    Require-File (Join-Path $FfmpegDir 'ffprobe.exe')
}

# Environment changes are scoped to this invocation and restored on failure.
$EnvironmentNames = @('PATH', 'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QT_QPA_PLATFORM', 'QML2_IMPORT_PATH', 'QML_IMPORT_PATH')
$SavedEnvironment = @{}
foreach ($Name in $EnvironmentNames) { $SavedEnvironment[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process') }
try {
    $env:PATH = "$QtBin;$IcuRuntimeDir;$FfmpegDir;$($SavedEnvironment['PATH'])"
    $env:QT_PLUGIN_PATH = Join-Path $QtPrefix 'plugins'
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $QtPrefix 'plugins/platforms'
    $ConfigureArgs = @('-S', $ProjectDir, '-B', $BuildDir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
        '-DBUILD_TESTING=ON', "-DQt6_DIR=$QtPrefix/lib/cmake/Qt6", "-DCMAKE_PREFIX_PATH=$QtPrefix;$IcuPrefix", "-DICU_ROOT=$IcuPrefix")
    if ($Toolchain -eq 'MSVC') { $ConfigureArgs += '-DCMAKE_CXX_COMPILER=cl.exe' }
    else { $ConfigureArgs += '-DCMAKE_CXX_COMPILER=g++.exe' }
    Invoke-Native 'configure' 'cmake' $ConfigureArgs
    $BuildInfo = Get-Content -LiteralPath (Join-Path $BuildDir 'build-info.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($BuildInfo.systemName -ne 'Windows' -or $BuildInfo.pointerBytes -ne 8) { throw 'This deployment script requires a native Windows x64 target.' }
    if ($Toolchain -eq 'MSVC' -and $BuildInfo.compilerArchitecture -notin @('x64', 'X64', 'AMD64')) { throw 'Use an x64 MSVC developer shell.' }
    if (($Toolchain -eq 'MSVC' -and $BuildInfo.compilerID -ne 'MSVC') -or ($Toolchain -eq 'MinGW' -and $BuildInfo.compilerID -ne 'GNU')) {
        throw 'Compiler mismatch. Use a fresh BuildDir and matching Qt/ICU/toolchain.'
    }
    Invoke-Native 'build' 'cmake' @('--build', $BuildDir, '--parallel', "$Jobs")
    Invoke-Native 'CTest' 'ctest' @('--test-dir', $BuildDir, '--output-on-failure')
    Invoke-Native 'install application and notices' 'cmake' @('--install', $BuildDir, '--prefix', $StageDir)
    $BinDir = Join-Path $StageDir 'bin'
    $ValidationDir = Join-Path $StageDir 'validation'
    New-Item -ItemType Directory -Path $ValidationDir -Force | Out-Null
    $RuntimeFlag = '--no-compiler-runtime'
    if ($Toolchain -eq 'MinGW') { $RuntimeFlag = '--compiler-runtime' }
    # MSVC uses the existing official VC++ x64 Redistributable on the target.
    # Do not copy arbitrary compiler runtime DLLs out of Visual Studio.
    Invoke-Native 'deploy Qt GUI and multimedia' $DeployTool @('--release', '--no-translations', $RuntimeFlag, '--dir', $BinDir, (Join-Path $BinDir 'chosuta.exe'))
    Invoke-Native 'deploy Qt CLI' $DeployTool @('--release', '--no-translations', $RuntimeFlag, '--dir', $BinDir, (Join-Path $BinDir 'chosuta-cli.exe'))
    foreach ($Dll in $IcuDlls) { Copy-Checked $Dll.FullName $BinDir }
    $PlatformDir = Join-Path $BinDir 'platforms'
    New-Item -ItemType Directory -Path $PlatformDir -Force | Out-Null
    $Offscreen = Join-Path $QtPrefix 'plugins/platforms/qoffscreen.dll'
    Require-File $Offscreen
    Copy-Checked $Offscreen $PlatformDir
    Require-File (Join-Path $PlatformDir 'qwindows.dll')
    "[Paths]`nPrefix=.`nPlugins=.`n" | Set-Content -LiteralPath (Join-Path $BinDir 'qt.conf') -Encoding ASCII
    Copy-LocalNotices $QtPrefix 'Qt'
    Copy-LocalNotices $IcuPrefix 'ICU'
    $FfmpegVersion = ''
    if ($FfmpegDir) {
        $VersionOutput = & (Join-Path $FfmpegDir 'ffmpeg.exe') -version
        if ($LASTEXITCODE -ne 0) { throw 'Cannot query selected FFmpeg.' }
        $FfmpegVersion = $VersionOutput -join "`n"
        if ($FfmpegVersion -match '--enable-nonfree') { throw 'FFmpeg --enable-nonfree builds cannot be deployed by this project.' }
        foreach ($Name in @('ffmpeg.exe', 'ffprobe.exe')) { Copy-Checked (Join-Path $FfmpegDir $Name) $BinDir }
        foreach ($Dll in @(Get-ChildItem -LiteralPath $FfmpegDir -Filter '*.dll' -File)) { Copy-Checked $Dll.FullName $BinDir }
        Copy-LocalNotices (Split-Path -Parent $FfmpegDir) 'FFmpeg'
        $FfmpegVersion | Set-Content -LiteralPath (Join-Path $ValidationDir 'ffmpeg-version.txt') -Encoding UTF8
    }
    Copy-Item -LiteralPath (Join-Path $BuildDir 'Testing/Temporary/LastTest.log') -Destination (Join-Path $ValidationDir 'ctest.log')
    Copy-Item -LiteralPath (Join-Path $BuildDir 'build-info.json') -Destination $ValidationDir
    $DeployVersion = & $DeployTool --version
    if ($LASTEXITCODE -ne 0) { throw 'Cannot query windeployqt version.' }

    # Test with SDK/plugin/PATH overrides removed; qt.conf must find local plugins.
    foreach ($Name in $EnvironmentNames) { [Environment]::SetEnvironmentVariable($Name, $null, 'Process') }
    $env:PATH = "$BinDir;$env:SystemRoot\System32;$env:SystemRoot"
    Invoke-SelfTest 'cli' (Join-Path $BinDir 'chosuta-cli.exe') @('--version')
    $env:QT_QPA_PLATFORM = 'offscreen'
    Invoke-SelfTest 'gui-offscreen' (Join-Path $BinDir 'chosuta.exe') @('--smoke-test')
    $env:QT_QPA_PLATFORM = 'windows'
    Invoke-SelfTest 'gui-windows' (Join-Path $BinDir 'chosuta.exe') @('--smoke-test')

    $Files = @(Get-ChildItem -LiteralPath $BinDir -File -Recurse | Sort-Object FullName | ForEach-Object {
        [ordered]@{
            path = $_.FullName.Substring($BinDir.Length + 1).Replace('\', '/')
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            fileVersion = $_.VersionInfo.FileVersion
        }
    })
    $Manifest = [ordered]@{
        version = $BuildInfo.chosutaVersion
        target = 'Windows x64'
        build = $BuildInfo
        windeployqt = ($DeployVersion -join "`n")
        ffmpegBundled = [bool]$FfmpegDir
        msvcRuntime = 'Official VC++ x64 Redistributable must already be installed for MSVC builds.'
        ctest = 'passed; see validation/ctest.log for any skipped optional/private-sample cases'
        deployedCli = 'passed'
        deployedOffscreenGui = 'passed'
        deployedWindowsGuiStartup = 'passed (not interactive/audio acceptance)'
        files = $Files
    }
    $Manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $StageDir 'runtime-manifest.json') -Encoding UTF8
    @'
Chosuta local Windows build
Start bin\chosuta.exe directly. Keep the whole directory, including DLLs and plugins.
MSVC builds require the official VC++ x64 Redistributable to be installed.
FFmpeg/ffprobe are optional. Use export settings to choose FFmpeg if not bundled/in PATH.
See runtime-manifest.json and validation/ for this machine's actual results.
The dependency notices collected here do not replace a complete redistribution
source/NOTICE review of the specific Qt, ICU and media dependencies you deploy.
'@ | Set-Content -LiteralPath (Join-Path $StageDir 'RUN.txt') -Encoding ASCII
    Write-Host "Ready: $(Join-Path $BinDir 'chosuta.exe')"
    Write-Host "Evidence: $(Join-Path $StageDir 'runtime-manifest.json')"
} finally {
    foreach ($Name in $EnvironmentNames) { [Environment]::SetEnvironmentVariable($Name, $SavedEnvironment[$Name], 'Process') }
}
