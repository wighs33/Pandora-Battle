<#
.SYNOPSIS
    보스 레이드 대규모 소환의 서버·클라이언트 부하를 잰다.

.DESCRIPTION
    로컬 전용 서버(GameLift 없이)와 클라이언트 하나를 띄운다.
    둘 다 -BossRaidBenchmark로 실행되어 UBossRaidBenchmarkSubsystem이 다음 순서로 측정한다.
      1. 서버가 첫 플레이어를 보스 앞에 무적으로 세우고 10초 기다린다.
      2. 소환 전 5초, 소환 직후 5초, 소환 뒤 교전 20초 동안 서버 게임 스레드 프레임 시간과 송신량을 잰다.
      3. 졸개를 한꺼번에 죽이고(범위 공격 상황) 시체가 사라질 때까지 10초를 잰다.
      4. 결과를 로그에 남기고 서버를 끝낸다.
    클라이언트는 기본으로 그림을 그리지 않는다(-nullrhi). -RenderClient를 주면 1280x720 창으로 그리며
    같은 구간(교전은 15초)의 프레임 시간과 게임·렌더 스레드, GPU 시간을 잰다.
    이 스크립트는 서버·클라이언트 로그의 BENCH 줄을 표로 보여 주고 OutDir에 results.json으로 남긴다.

    -Trace를 주면 Unreal Insights 트레이스(Server.utrace, -RenderClient면 Client.utrace도)를 남긴다. 측정 구간은 트레이스 영역(region)으로 표시된다.
    트레이스는 그 자체로 부하가 있으므로, 수치 비교는 -Trace 없이 잰 결과로 한다.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Scripts\RunPerfBenchmark.ps1 -EngineRoot D:\UE_5.8_Src
    powershell -ExecutionPolicy Bypass -File Scripts\RunPerfBenchmark.ps1 -EngineRoot D:\UE_5.8_Src -SummonCount 10 -Trace
    powershell -ExecutionPolicy Bypass -File Scripts\RunPerfBenchmark.ps1 -EngineRoot D:\UE_5.8_Src -RenderClient
    # 스테이징한 Development 서버로 잴 때
    powershell -ExecutionPolicy Bypass -File Scripts\RunPerfBenchmark.ps1 -EngineRoot D:\UE_5.8_Src -ServerExe D:\GL\Stage\WindowsServer\LabProject\Binaries\Win64\LabProjectServer.exe
#>
param(
    [string]$EngineRoot = $env:UE_ROOT,
    [string]$Project = (Join-Path $PSScriptRoot "..\LabProject.uproject"),
    [int]$SummonCount = 200,
    [int]$Port = 7790,
    [string]$Map = "/Game/Map/LV_Colosseum?BossRaid=1",
    # 비우면 에디터 실행 파일을 전용 서버로 띄운다.
    [string]$ServerExe = "",
    [string]$OutDir = "",
    [int]$TimeoutSeconds = 420,
    [switch]$RenderClient,
    [switch]$Trace
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

if (-not $EngineRoot -or -not (Test-Path (Join-Path $EngineRoot "Engine"))) {
    Write-Error "엔진 경로를 -EngineRoot 또는 UE_ROOT 환경 변수로 지정하세요. (예: D:\UE_5.8_Src)"
}
$Project = (Resolve-Path $Project).Path
if (-not $OutDir) {
    $OutDir = Join-Path $PSScriptRoot "..\Saved\Benchmarks\$(Get-Date -Format 'yyyyMMdd-HHmmss')"
}
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$ServerLog = Join-Path $OutDir "Server.log"
$ClientLog = Join-Path $OutDir "Client.log"

$CommonArgs = "-nosteam -unattended -nosplash -nop4 -nosound"
$ServerArgs = "`"$Map`" -port=$Port $CommonArgs -BossRaidBenchmark " +
    "-ExecCmds=`"pd.Monster.PhaseSummonCountOverride $SummonCount`" -abslog=`"$ServerLog`""
if ($Trace) {
    $ServerArgs += " -trace=cpu,frame,bookmark,region,log -statnamedevents -tracefile=`"$(Join-Path $OutDir 'Server.utrace')`""
}
if ($ServerExe) {
    $ServerArgs += " -log"
    $Server = Start-Process -FilePath $ServerExe -ArgumentList $ServerArgs -PassThru
}
else {
    $EditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    $Server = Start-Process -FilePath $EditorCmd -ArgumentList "`"$Project`" $ServerArgs -server" -PassThru -WindowStyle Hidden
}
Write-Host "== 서버 시작 (소환 $SummonCount, 포트 $Port). 로그: $ServerLog"

$Client = $null
$Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
try {
    # 서버가 접속을 받기 시작한 뒤에 클라이언트를 띄운다. 먼저 띄우면 접속에 실패하고 타이틀로 간다.
    while (-not ((Test-Path $ServerLog) -and (Select-String -Path $ServerLog -Pattern "listening on port" -Quiet))) {
        if ($Server.HasExited -or (Get-Date) -gt $Deadline) {
            throw "서버가 접속을 받기 전에 끝났거나 시간이 지났습니다. 로그: $ServerLog"
        }
        Start-Sleep -Seconds 2
    }
    $EditorGame = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
    if ($RenderClient) {
        # 프레임 제한과 수직 동기를 끄고 그려야 클라이언트가 낼 수 있는 프레임 시간이 그대로 보인다.
        $ClientArgs = "`"$Project`" 127.0.0.1:$Port -game -windowed -ResX=1280 -ResY=720 $CommonArgs -BossRaidBenchmark " +
            "-ExecCmds=`"t.MaxFPS 0;r.VSync 0`" -abslog=`"$ClientLog`""
    }
    else {
        # 그림을 그리지 않는 클라이언트도 프레임 제한이 없으면 CPU 코어 하나를 다 써서 서버 측정을 흔든다.
        $ClientArgs = "`"$Project`" 127.0.0.1:$Port -game -nullrhi $CommonArgs -ExecCmds=`"t.MaxFPS 30`" -abslog=`"$ClientLog`""
    }
    if ($Trace -and $RenderClient) {
        $ClientArgs += " -trace=cpu,frame,bookmark,region,log,gpu -statnamedevents -tracefile=`"$(Join-Path $OutDir 'Client.utrace')`""
    }
    # 숨긴 창은 그리기를 건너뛸 수 있어, 그리는 클라이언트만 창을 보이게 띄운다.
    $WindowStyle = if ($RenderClient) { "Normal" } else { "Hidden" }
    $Client = Start-Process -FilePath $EditorGame -ArgumentList $ClientArgs -PassThru -WindowStyle $WindowStyle
    Write-Host "== 클라이언트 접속. 로그: $ClientLog"

    $Remaining = [int]($Deadline - (Get-Date)).TotalMilliseconds
    if (-not $Server.WaitForExit([Math]::Max($Remaining, 0))) {
        throw "측정이 $TimeoutSeconds초 안에 끝나지 않았습니다. 로그: $ServerLog"
    }
    if ($RenderClient) {
        # 클라이언트는 서버보다 늦게 끝나는 일이 없도록 교전 구간을 짧게 잡는다. 그래도 보고가 늦으면 잠시 기다린다.
        $ClientDeadline = (Get-Date).AddSeconds(30)
        while (-not (Select-String -Path $ClientLog -Pattern "BENCH done" -Quiet) -and (Get-Date) -lt $ClientDeadline) {
            Start-Sleep -Seconds 1
        }
    }
}
finally {
    if ($Client -and -not $Client.HasExited) {
        Stop-Process -Id $Client.Id -Force
    }
    if (-not $Server.HasExited) {
        Stop-Process -Id $Server.Id -Force
    }
}

# 예: BENCH Summon frames=150 avg_ms=3.10 p95_ms=4.20 max_ms=120.50 over_budget_frames=2 over_budget_ms=95.0 out_kbps=80.1 monsters=209 targeting=150
$Results = @()
$LogsToRead = @($ServerLog)
if ($RenderClient -and (Test-Path $ClientLog)) {
    $LogsToRead += $ClientLog
}
foreach ($Line in (Select-String -Path $LogsToRead -Pattern "BENCH (\w+) (frames=.*)$")) {
    $Row = [ordered]@{ Stage = $Line.Matches[0].Groups[1].Value }
    foreach ($Pair in ($Line.Matches[0].Groups[2].Value -split " ")) {
        $Key, $Value = $Pair -split "="
        $Row[$Key] = [double]$Value
    }
    $Results += [pscustomobject]$Row
}
$Done = Select-String -Path $ServerLog -Pattern "BENCH done \((.*)\)" | Select-Object -Last 1
if ($RenderClient -and -not ($Results | Where-Object { $_.Stage -eq "ClientAfterSummon" })) {
    Write-Host "클라이언트 결과가 없습니다. 로그: $ClientLog"
}
if ($Results.Count -eq 0 -or -not $Done -or $Done.Matches[0].Groups[1].Value -ne "complete") {
    Write-Host "측정 결과가 없습니다. 로그: $ServerLog"
    exit 1
}
$Summary = [ordered]@{
    SummonCount = $SummonCount
    RenderClient = [bool]$RenderClient
    Server = $(if ($ServerExe) { $ServerExe } else { "UnrealEditor-Cmd -server" })
    Time = (Get-Date -Format "s")
    Stages = $Results
}
$Summary | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $OutDir "results.json")
# 서버와 클라이언트는 기록하는 항목이 달라 표를 나눈다.
$Results | Where-Object { $_.Stage -notlike "Client*" } | Format-Table -AutoSize | Out-String -Width 200 | Write-Host
$ClientResults = @($Results | Where-Object { $_.Stage -like "Client*" })
if ($ClientResults.Count -gt 0) {
    $ClientResults | Format-Table -AutoSize | Out-String -Width 200 | Write-Host
}
Write-Host "결과: $OutDir"
exit 0
