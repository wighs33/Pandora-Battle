<#
.SYNOPSIS
  스테이징한 전용 서버를 GameLift Anywhere 프로세스로 실행한다.

.DESCRIPTION
  실행할 때마다 compute 인증 토큰을 새로 받아 -glAnywhere 인자로 넘긴다.
  결과 보고 서명에 쓸 자격 증명은 AWS CLI 프로필에서 꺼내 서버 프로세스의 환경 변수로만 넘긴다(명령줄에 남기지 않음).
  Anywhere는 프로세스를 자동으로 다시 띄우지 않으므로, 한 경기가 끝나면 이 스크립트를 다시 실행한다.

.EXAMPLE
  .\Backend\run-anywhere-server.ps1 -ServerExe D:\GL\Stage\WindowsServer\LabProject\Binaries\Win64\LabProjectServer.exe

.EXAMPLE
  # 한 PC에서 -nosteam 클라이언트 여러 개로 시험할 때는 서버도 Steam 없이 띄운다.
  .\Backend\run-anywhere-server.ps1 -ServerExe <경로> -ExtraArgs @('-nosteam')
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$ServerExe,
    [string]$FleetId = "fleet-51383f38-c462-47e6-bbc9-e0e41cad61ac",
    [string]$ComputeName = "labproject-dev-pc",
    [string]$Profile = "gamelift-dev",
    [string]$Region = "ap-northeast-2",
    [int]$Port = 7777,
    # 소스 빌드 엔진은 네트워크 호환 체인지리스트가 0이라, 런처 엔진(5.8) 클라이언트의 접속을 버전 불일치로 거절한다.
    # 런처 엔진 Engine/Build/Build.version의 CompatibleChangelist로 맞춘다. 엔진을 바꾸면 이 값도 바꾼다.
    [int]$NetworkVersion = 55116800,
    [string[]]$ExtraArgs = @()
)

$ErrorActionPreference = "Continue"
$AwsArgs = @("--profile", $Profile, "--region", $Region)

if (-not (Test-Path $ServerExe)) { throw "Server executable not found: $ServerExe" }

$Endpoint = & aws gamelift describe-compute --fleet-id $FleetId --compute-name $ComputeName `
    --query "Compute.GameLiftServiceSdkEndpoint" --output text @AwsArgs
if ($LASTEXITCODE -ne 0 -or -not $Endpoint) { throw "describe-compute failed. Is $ComputeName registered to $FleetId?" }

$AuthToken = & aws gamelift get-compute-auth-token --fleet-id $FleetId --compute-name $ComputeName `
    --query AuthToken --output text @AwsArgs
if ($LASTEXITCODE -ne 0 -or -not $AuthToken) { throw "get-compute-auth-token failed." }

# 결과 보고(SigV4) 서명용. 서버는 Anywhere 키 인자가 없으면 AWS_* 환경 변수를 쓴다.
$Credentials = & aws configure export-credentials --profile $Profile | ConvertFrom-Json
$env:AWS_ACCESS_KEY_ID = $Credentials.AccessKeyId
$env:AWS_SECRET_ACCESS_KEY = $Credentials.SecretAccessKey
$env:AWS_SESSION_TOKEN = $Credentials.SessionToken

Write-Host "Starting $ServerExe on port $Port (fleet $FleetId, compute $ComputeName)"
& $ServerExe -log "-port=$Port" "-networkversionoverride=$NetworkVersion" -glAnywhere `
    "-glAnywhereWebSocketUrl=$Endpoint" "-glAnywhereFleetId=$FleetId" "-glAnywhereHostId=$ComputeName" `
    "-glAnywhereAuthToken=$AuthToken" "-glAnywhereAwsRegion=$Region" @ExtraArgs
