<#
.SYNOPSIS
  보스 레이드를 열거나 닫는다. 게임 패치나 백엔드 배포 없이 바로 적용된다.

.DESCRIPTION
  열림 여부는 백엔드 테이블(labproject-<Stage>)의 PK=CONFIG, SK=BOSSRAID 항목에 둔다. 항목이 없으면 닫힌 것으로 본다.
  게임은 타이틀 화면에 들어올 때마다 이 값을 물어 버튼을 켜고 끄며, 매치 참가 API도 닫힌 동안에는 레이드 자리를 잡아 주지 않는다.
  닫으면 새 참가만 막는다. 이미 레이드에 들어간 플레이어는 나갈 때까지 계속 플레이한다.
  열기 전에 보스 레이드 세션을 받을 GameLift 서버가 떠 있는지 확인한다.

.EXAMPLE
  .\Backend\bossraid.ps1
  .\Backend\bossraid.ps1 open
  .\Backend\bossraid.ps1 close
#>
param(
    [ValidateSet("status", "open", "close")]
    [string]$Action = "status",
    [string]$Stage = "dev",
    [string]$Profile = "gamelift-dev",
    [string]$Region = "ap-northeast-2"
)

# deploy.ps1과 같이, aws CLI의 성공 여부는 종료 코드로만 판단한다.
$ErrorActionPreference = "Continue"
$AwsArgs = @("--profile", $Profile, "--region", $Region)
$Table = "labproject-$Stage"

# Windows PowerShell 5.1은 네이티브 명령에 넘기는 인자의 큰따옴표를 지우므로, JSON은 파일로 넘긴다.
function Invoke-DynamoDb([string]$Command, [string]$JsonOption, [hashtable]$Json) {
    $Path = Join-Path ([System.IO.Path]::GetTempPath()) "labproject-bossraid-$PID.json"
    try {
        Set-Content -Path $Path -Value (ConvertTo-Json $Json -Depth 4 -Compress) -Encoding ascii
        $Output = & aws dynamodb $Command --table-name $Table $JsonOption "file://$Path" @AwsArgs
        if ($LASTEXITCODE -ne 0) {
            throw "aws dynamodb $Command failed with exit code $LASTEXITCODE"
        }
        return $Output
    }
    finally {
        Remove-Item -Path $Path -ErrorAction SilentlyContinue
    }
}

$Key = @{ PK = @{ S = "CONFIG" }; SK = @{ S = "BOSSRAID" } }

if ($Action -ne "status") {
    $Item = $Key.Clone()
    $Item.Open = @{ BOOL = ($Action -eq "open") }
    $Item.UpdatedAt = @{ S = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'") }
    Invoke-DynamoDb "put-item" "--item" $Item | Out-Null
}

$Stored = (Invoke-DynamoDb "get-item" "--key" $Key | Out-String | ConvertFrom-Json).Item
if ($Stored -and $Stored.Open.BOOL) {
    Write-Host "Boss raid is OPEN on $Table (updated $($Stored.UpdatedAt.S))."
}
elseif ($Stored) {
    Write-Host "Boss raid is CLOSED on $Table (updated $($Stored.UpdatedAt.S))."
}
else {
    Write-Host "Boss raid is CLOSED on $Table (never opened)."
}
