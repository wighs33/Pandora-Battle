<#
.SYNOPSIS
    LabProject 자동화 테스트를 한 번에 돌리는 CI 진입점이다.

.DESCRIPTION
    단계는 세 가지다.
      1. 에디터 빌드 (LabProjectEditor Win64 Development)
      2. 에디터 프로세스: 단위 테스트(LabProject.Unit.*)와 콘텐츠 연결 테스트(LabProject.Integration.Content.*)
      3. -game 프로세스: 훈련장 게임 월드의 입력·UI·장비·전투 연결 테스트(LabProject.Integration.TrainingRoom.*)

    단계마다 ReportDir 아래에 엔진 리포트(index.json)와 로그를 남긴다.
    실패한 테스트가 있거나, 기대한 단계에서 테스트가 하나도 돌지 않았거나, 리포트가 없으면 종료 코드 1을 돌려준다.

    공개 저장소는 로직 애셋만 담으므로, 연결 테스트는 전체 Content가 있는 빌드 머신(self-hosted runner)에서 돌린다.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Scripts\RunAutomationTests.ps1 -EngineRoot C:\UE\UE_5.8
    powershell -ExecutionPolicy Bypass -File Scripts\RunAutomationTests.ps1 -EngineRoot C:\UE\UE_5.8 -Suite Unit -SkipBuild
#>
param(
    [string]$EngineRoot = $env:UE_ROOT,
    [string]$Project = (Join-Path $PSScriptRoot "..\LabProject.uproject"),
    [ValidateSet("All", "Unit", "Integration")]
    [string]$Suite = "All",
    [string]$ReportDir = (Join-Path $PSScriptRoot "..\Saved\AutomationReports"),
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
# CI 로그에 한국어 메시지가 깨지지 않게 출력 인코딩을 UTF-8로 맞춘다.
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

if (-not $EngineRoot -or -not (Test-Path (Join-Path $EngineRoot "Engine"))) {
    Write-Error "엔진 경로를 -EngineRoot 또는 UE_ROOT 환경 변수로 지정하세요. (예: C:\UE\UE_5.8)"
}
$Project = (Resolve-Path $Project).Path
$EditorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$EditorGame = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$BuildBat = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"
New-Item -ItemType Directory -Force $ReportDir | Out-Null
$ReportDir = (Resolve-Path $ReportDir).Path

# 단계 이름, 실행 파일, 추가 인자, 테스트 필터
$Passes = @()
if ($Suite -eq "All" -or $Suite -eq "Unit") {
    $Passes += @{ Name = "Unit"; Exe = $EditorCmd; Args = ""; Filter = "LabProject.Unit" }
}
if ($Suite -eq "All" -or $Suite -eq "Integration") {
    $Passes += @{ Name = "IntegrationEditor"; Exe = $EditorCmd; Args = ""; Filter = "LabProject.Integration.Content" }
    $Passes += @{ Name = "IntegrationGame"; Exe = $EditorGame; Args = "-game"; Filter = "LabProject.Integration.TrainingRoom" }
}

if (-not $SkipBuild) {
    Write-Host "== 에디터 빌드"
    & $BuildBat LabProjectEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) {
        Write-Error "에디터 빌드에 실패했습니다 (종료 코드 $LASTEXITCODE)."
    }
}

$Failed = $false
$Summary = @()
foreach ($Pass in $Passes) {
    $PassDir = Join-Path $ReportDir $Pass.Name
    if (Test-Path $PassDir) {
        Remove-Item -Recurse -Force $PassDir
    }
    New-Item -ItemType Directory -Force $PassDir | Out-Null
    $LogPath = Join-Path $PassDir "Automation.log"

    Write-Host "== $($Pass.Name): $($Pass.Filter)"
    $Arguments = "`"$Project`" $($Pass.Args) -ExecCmds=`"Automation RunTests $($Pass.Filter);Quit`" " +
        "-unattended -nullrhi -nosplash -nosound -nop4 -nosteam " +
        "-ReportExportPath=`"$PassDir`" -abslog=`"$LogPath`""
    $Process = Start-Process -FilePath $Pass.Exe -ArgumentList $Arguments -Wait -PassThru -NoNewWindow

    $ReportPath = Join-Path $PassDir "index.json"
    if (-not (Test-Path $ReportPath)) {
        Write-Host "   리포트가 없습니다 (종료 코드 $($Process.ExitCode)). 로그: $LogPath"
        $Summary += [pscustomobject]@{ Pass = $Pass.Name; Passed = 0; Failed = "-"; Note = "리포트 없음" }
        $Failed = $true
        continue
    }

    $Report = Get-Content -Raw -Encoding UTF8 $ReportPath | ConvertFrom-Json
    $Tests = @($Report.tests)
    $FailedTests = @($Tests | Where-Object { $_.state -ne "Success" })
    $Note = ""
    if ($Tests.Count -eq 0) {
        $Note = "실행된 테스트 없음"
        $Failed = $true
    }
    foreach ($Test in $FailedTests) {
        $Failed = $true
        Write-Host "   실패: $($Test.fullTestPath)"
        foreach ($Entry in @($Test.entries | Where-Object { $_.event.type -eq "Error" } | Select-Object -First 5)) {
            Write-Host "      $($Entry.event.message)"
        }
    }
    $Summary += [pscustomobject]@{ Pass = $Pass.Name; Passed = $Tests.Count - $FailedTests.Count; Failed = $FailedTests.Count; Note = $Note }
}

$Summary | Format-Table -AutoSize | Out-String | Write-Host
Write-Host "리포트: $ReportDir"
if ($Failed) {
    exit 1
}
exit 0
