param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [Parameter(Mandatory = $true)]
    [string]$Destination,
    [string]$QtBin = "",
    [string]$CompilerBin = "",
    [string]$SourceRoot = "",
    [string]$Version = "dev",
    [switch]$IncludeVoiceLibrary,
    [switch]$CreateZip,
    [string]$ArchivePath = ""
)

$ErrorActionPreference = "Stop"

function Copy-DirectoryContent {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Target
    )
    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Resource directory does not exist: $Source"
    }
    New-Item -ItemType Directory -Path $Target -Force | Out-Null
    & robocopy.exe $Source $Target /E /R:2 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -gt 7) {
        throw "Directory copy failed (robocopy exit code $LASTEXITCODE): $Source"
    }
}

function Copy-QtPlugin {
    param(
        [Parameter(Mandatory = $true)][string]$PluginRoot,
        [Parameter(Mandatory = $true)][string]$PackageRoot,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )
    $source = Join-Path $PluginRoot $RelativePath
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required Qt plugin does not exist: $source"
    }
    $target = Join-Path $PackageRoot $RelativePath
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Force
}

$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$destinationPath = [System.IO.Path]::GetFullPath($Destination)
if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $sourceRootPath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
} else {
    $sourceRootPath = (Resolve-Path -LiteralPath $SourceRoot).Path
}

if (Test-Path -LiteralPath $destinationPath) {
    $existingItems = @(Get-ChildItem -LiteralPath $destinationPath -Force)
    if ($existingItems.Count -gt 0) {
        throw "Destination must not exist or must be empty: $destinationPath"
    }
}

if ([string]::IsNullOrWhiteSpace($QtBin)) {
    $qmakeCommand = Get-Command qmake.exe -ErrorAction Stop
    $qtBinPath = Split-Path -Parent $qmakeCommand.Source
} else {
    $qtBinPath = (Resolve-Path -LiteralPath $QtBin).Path
}
$compilerBinPath = ""
if (-not [string]::IsNullOrWhiteSpace($CompilerBin)) {
    $compilerBinPath = (Resolve-Path -LiteralPath $CompilerBin).Path
}

$qmakePath = Join-Path $qtBinPath "qmake.exe"
if (-not (Test-Path -LiteralPath $qmakePath -PathType Leaf)) {
    throw "Required Qt tool does not exist: $qmakePath"
}
if ([string]::IsNullOrWhiteSpace($compilerBinPath)) {
    $compilerCommand = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($null -ne $compilerCommand) {
        $compilerBinPath = Split-Path -Parent $compilerCommand.Source
    }
}
if ([string]::IsNullOrWhiteSpace($compilerBinPath)) {
    throw "CompilerBin is required unless g++.exe is available on PATH"
}

$pluginRoot = (& $qmakePath -query QT_INSTALL_PLUGINS).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($pluginRoot)) {
    throw "Unable to query the Qt plugin directory with qmake"
}

$assetsSource = Join-Path $sourceRootPath "assets"
$neutralSprite = Join-Path $assetsSource "modes\default\neutral.png"
if (-not (Test-Path -LiteralPath $neutralSprite -PathType Leaf)) {
    throw "Default sprite does not exist: $neutralSprite"
}
$voiceSource = Join-Path $sourceRootPath "resources\voice"
$referenceVoice = Join-Path $voiceSource "ko\ko0007.ogg"
if ($IncludeVoiceLibrary -and -not (Test-Path -LiteralPath $referenceVoice -PathType Leaf)) {
    throw "Voice library requested, but reference voice does not exist: $referenceVoice"
}

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("miyohashikori-deploy-" + [Guid]::NewGuid().ToString("N"))
$packagePath = Join-Path $temporaryRoot "package"
$previousPath = $env:Path
$previousQtPluginPath = $env:QT_PLUGIN_PATH

try {
    $deploymentPathParts = @($qtBinPath)
    if (-not [string]::IsNullOrWhiteSpace($compilerBinPath)) {
        $deploymentPathParts += $compilerBinPath
    }
    $deploymentPathParts += $previousPath
    $env:Path = $deploymentPathParts -join ";"
    $env:QT_PLUGIN_PATH = $pluginRoot
    New-Item -ItemType Directory -Path $packagePath -Force | Out-Null

    # Copy the exact runtime set used by this qmake target. This avoids a
    # windeployqt 6.5 plugin-discovery failure seen with some MinGW installs.
    $qtRuntimeFiles = @(
        "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Network.dll",
        "Qt6Multimedia.dll", "Qt6MultimediaWidgets.dll", "Qt6Sql.dll"
    )
    foreach ($runtimeFile in $qtRuntimeFiles) {
        $runtimeSource = Join-Path $qtBinPath $runtimeFile
        if (-not (Test-Path -LiteralPath $runtimeSource -PathType Leaf)) {
            throw "Required Qt runtime does not exist: $runtimeSource"
        }
        Copy-Item -LiteralPath $runtimeSource -Destination $packagePath -Force
    }
    foreach ($runtimeFile in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
        $runtimeSource = Join-Path $compilerBinPath $runtimeFile
        if (-not (Test-Path -LiteralPath $runtimeSource -PathType Leaf)) {
            throw "Required MinGW runtime does not exist: $runtimeSource"
        }
        Copy-Item -LiteralPath $runtimeSource -Destination $packagePath -Force
    }

    $requiredPlugins = @(
        "platforms\qwindows.dll",
        "multimedia\ffmpegmediaplugin.dll",
        "multimedia\windowsmediaplugin.dll",
        "networkinformation\qnetworklistmanager.dll",
        "styles\qwindowsvistastyle.dll",
        "tls\qcertonlybackend.dll",
        "tls\qschannelbackend.dll",
        "imageformats\qgif.dll",
        "imageformats\qico.dll",
        "imageformats\qjpeg.dll",
        "imageformats\qwebp.dll",
        "sqldrivers\qsqlite.dll"
    )
    foreach ($plugin in $requiredPlugins) {
        Copy-QtPlugin -PluginRoot $pluginRoot -PackageRoot $packagePath `
            -RelativePath $plugin
    }

    Copy-Item -LiteralPath $executablePath `
        -Destination (Join-Path $packagePath "Miyohashikori.exe") -Force
    Copy-DirectoryContent -Source $assetsSource -Target (Join-Path $packagePath "assets")

    $iconsSource = Join-Path $sourceRootPath "resources\icons"
    if (Test-Path -LiteralPath $iconsSource -PathType Container) {
        Copy-DirectoryContent -Source $iconsSource `
            -Target (Join-Path $packagePath "resources\icons")
    }
    if ($IncludeVoiceLibrary) {
        Copy-DirectoryContent -Source $voiceSource `
            -Target (Join-Path $packagePath "resources\voice")
    }

    $readmeSource = Join-Path $sourceRootPath "README.md"
    if (Test-Path -LiteralPath $readmeSource -PathType Leaf) {
        Copy-Item -LiteralPath $readmeSource -Destination (Join-Path $packagePath "README.md")
    }
    $ttsScriptSource = Join-Path $sourceRootPath "scripts\start_hyori_tts.ps1"
    if (Test-Path -LiteralPath $ttsScriptSource -PathType Leaf) {
        $toolsDestination = Join-Path $packagePath "tools"
        New-Item -ItemType Directory -Path $toolsDestination -Force | Out-Null
        Copy-Item -LiteralPath $ttsScriptSource `
            -Destination (Join-Path $toolsDestination "start_hyori_tts.ps1")
    }

    $portableNote = @"
Miyohashikori $Version portable edition

1. Extract the entire archive, then run Miyohashikori.exe. No installation is required.
2. The first run creates config.json and hyori.db under %USERPROFILE%\.hyori.
3. Add your own llmApiKey to config.json. Never share your API key.
4. GPT-SoVITS is optional. The bundled local voice library is used when it is unavailable.
5. User data is kept outside this folder, so the application folder can be replaced for upgrades.

See README.md for the complete Chinese documentation.
"@
    Set-Content -LiteralPath (Join-Path $packagePath "QUICK_START.txt") `
        -Value $portableNote -Encoding UTF8

    $requiredRuntimeFiles = @(
        "Miyohashikori.exe", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll",
        "Qt6Network.dll", "Qt6Multimedia.dll", "Qt6MultimediaWidgets.dll", "Qt6Sql.dll",
        "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll",
        "platforms\qwindows.dll", "sqldrivers\qsqlite.dll",
        "multimedia\ffmpegmediaplugin.dll", "multimedia\windowsmediaplugin.dll",
        "tls\qschannelbackend.dll", "imageformats\qjpeg.dll",
        "assets\modes\default\neutral.png"
    )
    if ($IncludeVoiceLibrary) {
        $requiredRuntimeFiles += "resources\voice\ko\ko0007.ogg"
    }
    foreach ($relativePath in $requiredRuntimeFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $packagePath $relativePath) -PathType Leaf)) {
            throw "Deployment validation failed; missing: $relativePath"
        }
    }

    New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null
    Copy-DirectoryContent -Source $packagePath -Target $destinationPath

    $fileStats = Get-ChildItem -LiteralPath $destinationPath -File -Recurse |
        Measure-Object -Property Length -Sum
    $exeHash = (Get-FileHash -LiteralPath `
        (Join-Path $destinationPath "Miyohashikori.exe") -Algorithm SHA256).Hash
    $manifest = @(
        "Version=$Version",
        "CreatedAt=$([DateTime]::Now.ToString('yyyy-MM-dd HH:mm:ss zzz'))",
        "Files=$($fileStats.Count)",
        "Bytes=$($fileStats.Sum)",
        "VoiceLibraryIncluded=$($IncludeVoiceLibrary.IsPresent)",
        "ExecutableSHA256=$exeHash"
    )
    Set-Content -LiteralPath (Join-Path $destinationPath "release-manifest.txt") `
        -Value $manifest -Encoding UTF8

    Write-Output "Portable directory created: $destinationPath"
    Write-Output "File count: $($fileStats.Count)"
    Write-Output ("Directory size: {0:N2} MiB" -f ($fileStats.Sum / 1MB))
    Write-Output "Full local voice library: $($IncludeVoiceLibrary.IsPresent)"

    if ($CreateZip) {
        $archiveFullPath = if ([string]::IsNullOrWhiteSpace($ArchivePath)) {
            "$destinationPath.zip"
        } else {
            [System.IO.Path]::GetFullPath($ArchivePath)
        }
        if (Test-Path -LiteralPath $archiveFullPath) {
            throw "Archive already exists; choose a new path: $archiveFullPath"
        }
        Compress-Archive -Path (Join-Path $destinationPath "*") `
            -DestinationPath $archiveFullPath -CompressionLevel Optimal
        Write-Output "ZIP created: $archiveFullPath"
        $archiveHash = (Get-FileHash -LiteralPath $archiveFullPath -Algorithm SHA256).Hash
        Write-Output "ZIP SHA256: $archiveHash"
    }
} finally {
    $env:Path = $previousPath
    $env:QT_PLUGIN_PATH = $previousQtPluginPath
    $resolvedTempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    $resolvedTemporaryRoot = [System.IO.Path]::GetFullPath($temporaryRoot)
    $isSafeTemporaryPath = $resolvedTemporaryRoot.StartsWith(
        $resolvedTempBase, [System.StringComparison]::OrdinalIgnoreCase)
    if ($isSafeTemporaryPath -and (Test-Path -LiteralPath $resolvedTemporaryRoot)) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
