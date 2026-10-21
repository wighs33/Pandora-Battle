<#
.SYNOPSIS
    빌드 번호와 커밋 해시를 실행 파일에 넣는다.

.DESCRIPTION
    Source/LabProject/Common/BuildVersionStamp.h를 쓴다. Common/BuildVersion.cpp가 이 파일을 읽어
    시작 로그에 "v<ProjectVersion> · #<빌드 번호> · <커밋>"을 남기고, 타이틀 화면에는 "v<ProjectVersion> · <커밋>"을
    보여 준다(Shipping은 버전만).
    빌드 번호를 주지 않으면 현재 커밋까지의 커밋 수를 써서, 같은 커밋은 늘 같은 번호가 된다(전체 git 이력이 필요).
    내용이 같으면 파일을 다시 쓰지 않아 불필요한 재컴파일을 막는다. 파일은 git에서 추적하지 않는다.
    -Clear를 주면 파일을 지워 개발 빌드 표시로 돌아간다.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Scripts\StampBuildVersion.ps1
    powershell -ExecutionPolicy Bypass -File Scripts\StampBuildVersion.ps1 -BuildNumber 57
    powershell -ExecutionPolicy Bypass -File Scripts\StampBuildVersion.ps1 -Clear
#>
param(
    [int]$BuildNumber = 0,
    [string]$Commit = "",
    [switch]$Clear
)

$ErrorActionPreference = "Stop"
$StampPath = Join-Path $PSScriptRoot "..\Source\LabProject\Common\BuildVersionStamp.h"
$ReaderPath = Join-Path $PSScriptRoot "..\Source\LabProject\Common\BuildVersion.cpp"

# 스탬프 파일을 새로 만들거나 지우면 빌드 도구의 의존성 기록에 없는 변화라, 읽는 cpp의 시각을 갱신해 다시 컴파일하게 한다.
function Update-ReaderTimestamp {
    (Get-Item $ReaderPath).LastWriteTime = Get-Date
}

if ($Clear) {
    if (Test-Path $StampPath) {
        Remove-Item $StampPath
        Update-ReaderTimestamp
    }
    Write-Host "빌드 번호를 지웠습니다(개발 빌드 표시)."
    exit 0
}
if ($BuildNumber -le 0) {
    $BuildNumber = [int](& git -C $PSScriptRoot rev-list --count HEAD).Trim()
}
if ($BuildNumber -le 0) {
    Write-Error "빌드 번호를 정하지 못했습니다."
}
if (-not $Commit) {
    $Commit = (& git -C $PSScriptRoot rev-parse --short=7 HEAD).Trim()
}
if ($Commit -notmatch '^[0-9a-f]{7,40}$') {
    Write-Error "커밋 해시 형식이 아닙니다: $Commit"
}

$Content = @"
// Scripts/StampBuildVersion.ps1이 만든 파일이다. 직접 고치지 않는다.
#pragma once
#define PD_BUILD_NUMBER $BuildNumber
#define PD_BUILD_COMMIT "$Commit"
"@
$Existing = if (Test-Path $StampPath) { Get-Content -Raw $StampPath } else { "" }
if ($Existing.Trim() -ne $Content.Trim()) {
    Set-Content -Path $StampPath -Value $Content -Encoding UTF8
    Update-ReaderTimestamp
}
Write-Host "빌드 번호 #$BuildNumber, 커밋 $Commit"
