<#
.SYNOPSIS
  보스 레이드(월드 보스) 회차를 열거나 닫는다. 게임 패치나 백엔드 배포 없이 바로 적용된다.

.DESCRIPTION
  API 서버 EC2에서 SSM Run Command로 python -m app.admin raid ...(Backend/server/app/admin.py)를 실행한다.
  DB는 VPC 밖에서 접속할 수 없으므로, API 서버의 환경 파일(/etc/labproject/api.env)에 있는 API 서버 계정으로 바꾼다.

  open -Hp <체력> -RewardGold <골드>  새 회차를 연다. 체력이 가득 찬 채로 시작하고, 누적 딜과 순위, 도전 횟수도 새로 센다.
  open                               닫았던 지금 회차를 다시 연다. 체력과 누적 딜은 이어진다.
  close                              새 참가만 막는다. 이미 들어간 플레이어는 도전 시간이 끝날 때까지 계속한다.

  보스가 쓰러지면 회차는 새 참가를 받지 않고, 딜을 넣은 플레이어는 다음에 타이틀에 들어올 때 보상 골드를 받는다.
  열기 전에 보스 레이드 세션을 받을 GameLift 서버가 떠 있는지 확인한다.

.EXAMPLE
  .\Backend\bossraid.ps1
  .\Backend\bossraid.ps1 open -Hp 3000000 -RewardGold 1000
  .\Backend\bossraid.ps1 open -Hp 3000000 -RewardGold 1000 -DailyAttempts 5 -AttemptSeconds 180
  .\Backend\bossraid.ps1 close
  .\Backend\bossraid.ps1 open
#>
param(
    [ValidateSet("status", "open", "close")]
    [string]$Action = "status",
    [long]$Hp = 0,
    [int]$RewardGold = -1,
    [int]$DailyAttempts = 3,
    [int]$AttemptSeconds = 300,
    [string]$Stage = "dev",
    [string]$Profile = "gamelift-dev",
    [string]$Region = "ap-northeast-2"
)

# deploy.ps1과 같이, aws CLI의 성공 여부는 종료 코드로만 판단한다.
$ErrorActionPreference = "Continue"
$AwsArgs = @("--profile", $Profile, "--region", $Region)
. (Join-Path $PSScriptRoot "ApiServerCommand.ps1")

$AdminArgs = switch ($Action) {
    "status" { "status" }
    "close" { "close" }
    "open" {
        if ($Hp -le 0) {
            "reopen"
        }
        elseif ($RewardGold -lt 0) {
            throw "-RewardGold is required with -Hp: the gold every player who dealt damage gets when the boss falls."
        }
        else {
            "open --hp $Hp --reward-gold $RewardGold --daily-attempts $DailyAttempts --attempt-seconds $AttemptSeconds"
        }
    }
}

Invoke-ApiServerCommand -StackName "labproject-$Stage" -AwsArgs $AwsArgs -Comment "Boss raid $Action" -Commands @(
    'set -euo pipefail',
    'cd /opt/labproject/current',
    'set -a; . /etc/labproject/api.env; set +a',
    ".venv/bin/python -m app.admin raid $AdminArgs"
)
