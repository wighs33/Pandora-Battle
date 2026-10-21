<#
.SYNOPSIS
    전용 서버를 빌드·쿡·스테이징하고, 원하면 GameLift Anywhere 서버를 새 빌드로 다시 띄운다.

.DESCRIPTION
    Docs/GameLift_Backend.md의 세 단계를 그대로 한다.
      1. 서버 실행 파일: 소스 엔진(-SourceEngineRoot)으로 LabProjectServer Win64 Development 빌드
      2. 서버 콘텐츠: 런처 엔진(-EngineRoot)으로 WindowsServer 쿡
      3. 스테이징: 소스 엔진 UAT로 -StagingDir에 복사
    스테이징 폴더의 서버가 실행 중이면 파일이 잠겨 덮어쓸 수 없다. 이때 -RestartAnywhere가 없으면
    실행 중인 서버(접속한 플레이어가 있을 수 있다)를 건드리지 않고 멈춘다.
    -RestartAnywhere를 주면 기존 서버를 끄고 스테이징한 뒤 Backend\run-anywhere-server.ps1로 다시 띄운다.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Scripts\StageServer.ps1 -EngineRoot C:\UE\UE_5.8 -SourceEngineRoot D:\UE_5.8_Src
    powershell -ExecutionPolicy Bypass -File Scripts\StageServer.ps1 -EngineRoot C:\UE\UE_5.8 -SourceEngineRoot D:\UE_5.8_Src -RestartAnywhere
#>
param(
    [string]$EngineRoot = $env:UE_ROOT,
    [string]$SourceEngineRoot = $env:PD_SOURCE_ENGINE,
    [string]$Project = (Join-Path $PSScriptRoot "..\LabProject.uproject"),
    [string]$StagingDir = "D:\GL\Stage",
    [switch]$RestartAnywhere
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

foreach ($Root in @($EngineRoot, $SourceEngineRoot)) {
    if (-not $Root -or -not (Test-Path (Join-Path $Root "Engine"))) {
        Write-Error "엔진 경로가 없습니다: '$Root'. -EngineRoot(런처 엔진)와 -SourceEngineRoot(서버 타깃용 소스 엔진)를 지정하세요."
    }
}
$Project = (Resolve-Path $Project).Path
$ServerExe = Join-Path $StagingDir "WindowsServer\LabProject\Binaries\Win64\LabProjectServer.exe"

function Invoke-Step([string]$Name, [scriptblock]$Command) {
    Write-Host "== $Name"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        Write-Error "$Name 단계가 실패했습니다 (종료 코드 $LASTEXITCODE)."
    }
}

$RunningServers = @(Get-Process LabProjectServer -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and $_.Path -ieq $ServerExe })
if ($RunningServers.Count -gt 0 -and -not $RestartAnywhere) {
    Write-Error "스테이징 폴더의 서버가 실행 중입니다(PID $($RunningServers.Id -join ', ')). 덮어쓰려면 -RestartAnywhere를 주세요."
}

Invoke-Step "서버 빌드" {
    & (Join-Path $SourceEngineRoot "Engine\Build\BatchFiles\Build.bat") LabProjectServer Win64 Development "-Project=$Project" -WaitMutex
}
Invoke-Step "서버 콘텐츠 쿡" {
    & (Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe") $Project -run=cook -targetplatform=WindowsServer -unattended -nop4 -utf8output
}

if ($RunningServers.Count -gt 0) {
    # 강제 종료하면 ProcessEnding이 불리지 않아 활성 세션이 몇 분 남는다(Docs/BossRaid.md). 빈 서버일 때 돌리는 것을 전제로 한다.
    Write-Host "== 실행 중인 서버 종료"
    $RunningServers | Stop-Process -Force
    $RunningServers | Wait-Process -Timeout 30 -ErrorAction SilentlyContinue
}

Invoke-Step "스테이징" {
    & (Join-Path $SourceEngineRoot "Engine\Build\BatchFiles\RunUAT.bat") BuildCookRun "-project=$Project" -platform=Win64 -server `
        -serverplatform=Win64 -noclient -serverconfig=Development -skipbuild -skipcook -stage "-stagingdirectory=$StagingDir" -nop4 -unattended -utf8output
}

if ($RestartAnywhere) {
    Write-Host "== GameLift Anywhere 서버 시작"
    $RunScript = (Resolve-Path (Join-Path $PSScriptRoot "..\Backend\run-anywhere-server.ps1")).Path
    # -File로 넘기면 -nosteam이 스크립트 매개변수로 읽힌다. 배열 인자를 그대로 넘기려고 -Command를 쓴다.
    Start-Process -FilePath "powershell.exe" -WindowStyle Hidden -ArgumentList @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
        "& '$RunScript' -ServerExe '$ServerExe' -ExtraArgs @('-nosteam')")
}
Write-Host "스테이징: $StagingDir"
