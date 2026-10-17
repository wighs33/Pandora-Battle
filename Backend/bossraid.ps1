<#
.SYNOPSIS
  보스 레이드(월드 보스) 회차를 열거나 닫는다. 게임 패치나 백엔드 배포 없이 바로 적용된다.

.DESCRIPTION
  백엔드 테이블(labproject-<Stage>)의 두 항목을 바꾼다.
  - PK=CONFIG, SK=BOSSRAID: 지금 회차 ID와 개최 여부(Open). 항목이 없으면 닫힌 것으로 본다.
  - PK=RAID#<회차 ID>, SK=STATE: 모든 서버가 함께 깎는 전역 체력, 하루 도전 횟수, 한 번의 도전 시간, 격파 보상 골드.

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
$Table = "labproject-$Stage"

# Windows PowerShell 5.1은 네이티브 명령에 넘기는 인자의 큰따옴표를 지우므로, JSON은 파일로 넘긴다.
function Invoke-DynamoDb([string]$Command, [hashtable]$JsonOptions, [string[]]$ExtraArgs = @()) {
    $Files = @()
    $CliArgs = @()
    try {
        foreach ($Option in $JsonOptions.Keys) {
            $Path = Join-Path ([System.IO.Path]::GetTempPath()) "labproject-bossraid-$PID-$($Files.Count).json"
            Set-Content -Path $Path -Value (ConvertTo-Json $JsonOptions[$Option] -Depth 6 -Compress) -Encoding ascii
            $Files += $Path
            $CliArgs += @($Option, "file://$Path")
        }
        $Output = & aws dynamodb $Command --table-name $Table @CliArgs @ExtraArgs @AwsArgs
        if ($LASTEXITCODE -ne 0) {
            throw "aws dynamodb $Command failed with exit code $LASTEXITCODE"
        }
        return $Output
    }
    finally {
        $Files | ForEach-Object { Remove-Item -Path $_ -ErrorAction SilentlyContinue }
    }
}

function Read-TableItem([string]$PK, [string]$SK) {
    $Key = @{ PK = @{ S = $PK }; SK = @{ S = $SK } }
    return (Invoke-DynamoDb "get-item" @{ "--key" = $Key } @("--consistent-read") | Out-String | ConvertFrom-Json).Item
}

function Set-Config([string]$RaidId, [bool]$Open) {
    $Item = @{
        PK = @{ S = "CONFIG" }
        SK = @{ S = "BOSSRAID" }
        RaidId = @{ S = $RaidId }
        Open = @{ BOOL = $Open }
        UpdatedAt = @{ S = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'") }
    }
    Invoke-DynamoDb "put-item" @{ "--item" = $Item } | Out-Null
}

$Config = Read-TableItem "CONFIG" "BOSSRAID"
$CurrentRaidId = if ($Config -and $Config.RaidId) { $Config.RaidId.S } else { "" }

if ($Action -eq "open" -and $Hp -gt 0) {
    if ($RewardGold -lt 0) {
        throw "-RewardGold is required with -Hp: the gold every player who dealt damage gets when the boss falls."
    }
    if ($DailyAttempts -lt 1 -or $AttemptSeconds -lt 1) {
        throw "-DailyAttempts and -AttemptSeconds must be at least 1."
    }
    $CurrentRaidId = (Get-Date).ToUniversalTime().ToString("yyyyMMdd-HHmmss")
    $State = @{
        PK = @{ S = "RAID#$CurrentRaidId" }
        SK = @{ S = "STATE" }
        EntityType = @{ S = "Raid" }
        RaidId = @{ S = $CurrentRaidId }
        MaxHp = @{ N = "$Hp" }
        Hp = @{ N = "$Hp" }
        Status = @{ S = "open" }
        DailyAttempts = @{ N = "$DailyAttempts" }
        AttemptSeconds = @{ N = "$AttemptSeconds" }
        RewardGold = @{ N = "$RewardGold" }
        CreatedAt = @{ S = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'") }
    }
    Invoke-DynamoDb "put-item" @{ "--item" = $State } @("--condition-expression", "attribute_not_exists(PK)") | Out-Null
    Set-Config $CurrentRaidId $true
}
elseif ($Action -eq "open") {
    $State = if ($CurrentRaidId) { Read-TableItem "RAID#$CurrentRaidId" "STATE" } else { $null }
    if (-not $State) {
        throw "No raid has been opened yet. Open a new one with -Hp <health> -RewardGold <gold>."
    }
    if ($State.Status.S -eq "defeated") {
        throw "Raid $CurrentRaidId was already defeated. Open a new one with -Hp <health> -RewardGold <gold>."
    }
    Set-Config $CurrentRaidId $true
}
elseif ($Action -eq "close") {
    Set-Config $CurrentRaidId $false
}

$Config = Read-TableItem "CONFIG" "BOSSRAID"
$State = if ($CurrentRaidId) { Read-TableItem "RAID#$CurrentRaidId" "STATE" } else { $null }
if (-not $State) {
    Write-Host "Boss raid is CLOSED on $Table (no raid opened yet)."
    return
}

$MaxHp = [long]$State.MaxHp.N
$HpLeft = [Math]::Max(0, [long]$State.Hp.N)
$Percent = if ($MaxHp -gt 0) { 100.0 * $HpLeft / $MaxHp } else { 0 }
$Opened = $Config.Open.BOOL -and $State.Status.S -ne "defeated"
$Summary = if ($State.Status.S -eq "defeated") { "DEFEATED at $($State.DefeatedAt.S)" } elseif ($Opened) { "OPEN" } else { "CLOSED" }
Write-Host "Boss raid $CurrentRaidId is $Summary on $Table (config updated $($Config.UpdatedAt.S))."
Write-Host ("  Health   {0:N0} / {1:N0} ({2:N1}%)" -f $HpLeft, $MaxHp, $Percent)
Write-Host ("  Rules    {0} attempts a day, {1} s each, {2:N0} gold for every player who dealt damage" -f `
    [int]$State.DailyAttempts.N, [int]$State.AttemptSeconds.N, [int]$State.RewardGold.N)
