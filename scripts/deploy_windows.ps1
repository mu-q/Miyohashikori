param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$Destination,

    [string]$QtBin = ""
)

$ErrorActionPreference = "Stop"

$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$destinationPath = [System.IO.Path]::GetFullPath($Destination)

if ([string]::IsNullOrWhiteSpace($QtBin)) {
    $deployCommand = Get-Command windeployqt.exe -ErrorAction Stop
    $qtBinPath = Split-Path -Parent $deployCommand.Source
} else {
    $qtBinPath = (Resolve-Path -LiteralPath $QtBin).Path
}

$windeployqtPath = Join-Path $qtBinPath "windeployqt.exe"
$qmakePath = Join-Path $qtBinPath "qmake.exe"
if (-not (Test-Path -LiteralPath $windeployqtPath)) {
    throw "windeployqt.exe 不存在：$windeployqtPath"
}
if (-not (Test-Path -LiteralPath $qmakePath)) {
    throw "qmake.exe 不存在：$qmakePath"
}

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("miyohashikori-deploy-" + [Guid]::NewGuid().ToString("N"))
$stagedExecutable = Join-Path $temporaryRoot "Miyohashikori.exe"
$packagePath = Join-Path $temporaryRoot "package"

try {
    New-Item -ItemType Directory -Path $packagePath -Force | Out-Null
    Copy-Item -LiteralPath $executablePath -Destination $stagedExecutable -Force

    & $windeployqtPath --release --no-translations --dir $packagePath $stagedExecutable
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt 执行失败，退出码：$LASTEXITCODE"
    }

    $pluginRoot = (& $qmakePath -query QT_INSTALL_PLUGINS).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($pluginRoot)) {
        throw "无法通过 qmake 查询 Qt 插件目录"
    }

    $sqliteDriver = Join-Path $pluginRoot "sqldrivers\qsqlite.dll"
    if (-not (Test-Path -LiteralPath $sqliteDriver)) {
        throw "QSQLITE 发布驱动不存在：$sqliteDriver"
    }

    $sqlDriverDestination = Join-Path $packagePath "sqldrivers"
    New-Item -ItemType Directory -Path $sqlDriverDestination -Force | Out-Null
    Copy-Item -LiteralPath $sqliteDriver -Destination $sqlDriverDestination -Force
    Copy-Item -LiteralPath $executablePath `
        -Destination (Join-Path $packagePath (Split-Path -Leaf $executablePath)) -Force

    New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null
    Copy-Item -Path (Join-Path $packagePath "*") -Destination $destinationPath `
        -Recurse -Force

    $deployedSqlLibrary = Join-Path $destinationPath "Qt6Sql.dll"
    $deployedSqliteDriver = Join-Path $destinationPath "sqldrivers\qsqlite.dll"
    if ((-not (Test-Path -LiteralPath $deployedSqlLibrary)) -or (-not (Test-Path -LiteralPath $deployedSqliteDriver))) {
        throw "部署校验失败：Qt6Sql.dll 或 sqldrivers/qsqlite.dll 缺失"
    }

    Write-Output "部署完成：$destinationPath"
    Write-Output "SQLite 驱动：$deployedSqliteDriver"
} finally {
    $resolvedTempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    $resolvedTemporaryRoot = [System.IO.Path]::GetFullPath($temporaryRoot)
    $isSafeTemporaryPath = $resolvedTemporaryRoot.StartsWith(
        $resolvedTempBase, [System.StringComparison]::OrdinalIgnoreCase)
    if ($isSafeTemporaryPath -and (Test-Path -LiteralPath $resolvedTemporaryRoot)) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
