<#
.SYNOPSIS
    Windows 클라이언트를 패키징한다(기본 Shipping).

.DESCRIPTION
    런처 엔진의 UAT BuildCookRun으로 빌드 → 쿡 → 스테이징 → pak → 패키지 → 보관까지 한다.
    결과는 -ArchiveDir에 남고, UAT 로그는 ArchiveDir\Logs로 복사한다.
    Shipping에서는 보스 레이드 버튼이 닫힌다(LabGameSession::bBossRaidOpen).
    Shipping은 배포용(-distribution)으로 만들어 pak을 더 높은 Oodle 레벨(PackageCompressionLevel_Distribution)로 압축한다.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Scripts\PackageClient.ps1 -EngineRoot C:\UE\UE_5.8
    powershell -ExecutionPolicy Bypass -File Scripts\PackageClient.ps1 -EngineRoot C:\UE\UE_5.8 -Configuration Development -ArchiveDir D:\CI\Builds\test
#>
param(
    [string]$EngineRoot = $env:UE_ROOT,
    [string]$Project = (Join-Path $PSScriptRoot "..\LabProject.uproject"),
    [ValidateSet("Shipping", "Development")]
    [string]$Configuration = "Shipping",
    [string]$ArchiveDir = ""
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$StartTime = Get-Date

if (-not $EngineRoot -or -not (Test-Path (Join-Path $EngineRoot "Engine"))) {
    Write-Error "엔진 경로를 -EngineRoot 또는 UE_ROOT 환경 변수로 지정하세요. (예: C:\UE\UE_5.8)"
}
$Project = (Resolve-Path $Project).Path
if (-not $ArchiveDir) {
    $ArchiveDir = Join-Path $PSScriptRoot "..\Saved\Packages\$(Get-Date -Format 'yyyyMMdd-HHmmss')"
}
New-Item -ItemType Directory -Force $ArchiveDir | Out-Null
$ArchiveDir = (Resolve-Path $ArchiveDir).Path

$RunUAT = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
$DistributionArgs = @()
if ($Configuration -eq "Shipping") {
    $DistributionArgs += "-distribution"
}
Write-Host "== 클라이언트 $Configuration 패키징 → $ArchiveDir"
& $RunUAT BuildCookRun "-project=$Project" -platform=Win64 "-clientconfig=$Configuration" `
    -build -cook -stage -pak -package -archive "-archivedirectory=$ArchiveDir" `
    -nop4 -unattended -utf8output @DistributionArgs
$ExitCode = $LASTEXITCODE

# UAT 로그는 사용자 AppData에 쌓이므로, 이번 실행에서 생긴 파일만 패키지 옆에 남긴다.
$UatLogDir = Join-Path $env:APPDATA "Unreal Engine\AutomationTool\Logs"
$LogArchive = New-Item -ItemType Directory -Force (Join-Path $ArchiveDir "Logs")
Get-ChildItem -Path $UatLogDir -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $StartTime } |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $LogArchive.FullName -Force }

if ($ExitCode -ne 0) {
    Write-Error "패키징에 실패했습니다 (종료 코드 $ExitCode). 로그: $ArchiveDir\Logs"
}
Write-Host "패키지: $ArchiveDir"
