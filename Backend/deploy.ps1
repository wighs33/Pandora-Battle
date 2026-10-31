<#
.SYNOPSIS
  LabProject 백엔드 스택을 배포한다(API Gateway, Lambda + DynamoDB, EC2 API 서버 + RDS MySQL).

.DESCRIPTION
  AWS CLI만 사용한다.
  1. 처음 실행하면 배포 아티팩트 버킷과 비밀 값(SSM SecureString)을 만든다: 세션 토큰 서명 키, 전용 서버의 서버 키,
     DB 비밀번호 셋(RDS 마스터, 스키마 담당, API 서버). 한 번 만든 값은 유지하고 화면에 출력하지 않는다.
  2. Lambda 코드와 API 서버 배포 묶음(Backend/server)을 S3에 올리고 스택을 배포한다.
  3. API 서버 EC2에서 SSM Run Command로 Backend/server/deploy/install.sh를 실행한다
     (새 릴리스, DB 계정, 마이그레이션, 서비스 재시작).
  Steam Web API 키는 비밀 값이라 스크립트가 만들지 않는다. 아래 명령으로 직접 등록한다.

    aws ssm put-parameter --profile gamelift-dev --region ap-northeast-2 `
      --name /labproject/dev/steam-web-api-key --type SecureString --value <퍼블리셔 Web API 키>

.EXAMPLE
  .\Backend\deploy.ps1
  .\Backend\deploy.ps1 -GameLiftFleetId fleet-xxxx -GameLiftLocation custom-labproject-dev
#>
param(
    [string]$Stage = "dev",
    [string]$Profile = "gamelift-dev",
    [string]$Region = "ap-northeast-2",
    [string]$GameLiftFleetId = "",
    [string]$GameLiftLocation = "",
    [ValidateSet("", "true", "false")]
    [string]$AllowDevLogin = ""
)

# Windows PowerShell 5.1은 Stop 모드에서 네이티브 명령의 stderr 출력을 예외로 바꾼다.
# aws CLI의 성공 여부는 종료 코드로만 판단하고, 실패는 아래 함수에서 직접 throw한다.
$ErrorActionPreference = "Continue"
$AwsArgs = @("--profile", $Profile, "--region", $Region)
$StackName = "labproject-$Stage"
. (Join-Path $PSScriptRoot "ApiServerCommand.ps1")

# install.sh가 받는 Amazon Linux 2023(arm64) 최신 AMI. 첫 배포에만 고르고 이후에는 스택 값을 유지한다.
$ApiImageParameter = "/aws/service/ami-amazon-linux-latest/al2023-ami-kernel-default-arm64"
# 처음 배포할 때만 만드는 비밀 값(이름 → 무작위 바이트 수). DB 비밀번호는 RDS 마스터 비밀번호 길이 한도(41자) 안이다.
$GeneratedSecrets = [ordered]@{
    "server-key" = 32
    "db-master-password" = 24
    "db-owner-password" = 24
    "db-app-password" = 24
}

# aws CLI 인자(--query 등)를 그대로 넘기기 위해 param 없이 $args를 쓴다.
function Invoke-Aws {
    $output = & aws @args @AwsArgs
    if ($LASTEXITCODE -ne 0) {
        throw "aws $($args -join ' ') failed with exit code $LASTEXITCODE"
    }
    return $output
}

function Test-AwsCall {
    & aws @args @AwsArgs 2>&1 | Out-Null
    return $LASTEXITCODE -eq 0
}

# 연결 URL에 그대로 넣을 수 있게 base64url(A-Z a-z 0-9 - _)로 만든다.
function New-UrlSafeSecret([int]$ByteCount) {
    $Bytes = New-Object byte[] $ByteCount
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($Bytes)
    return [Convert]::ToBase64String($Bytes).TrimEnd("=").Replace("+", "-").Replace("/", "_")
}

function Get-StackParameter([string]$Name) {
    $Value = & aws cloudformation describe-stacks --stack-name $StackName `
        --query "Stacks[0].Parameters[?ParameterKey=='$Name'].ParameterValue" --output text @AwsArgs 2>$null
    if ($LASTEXITCODE -ne 0 -or $Value -eq "None") { return "" }
    return $Value
}

Push-Location $PSScriptRoot
try {
    $Account = Invoke-Aws sts get-caller-identity --query Account --output text
    Write-Host "Account $Account / $Region / stack $StackName"

    # 1. 배포 아티팩트 버킷 (Lambda 코드 zip 업로드용, 공개 접근 차단)
    $Bucket = "labproject-deploy-$Account-$Region"
    if (-not (Test-AwsCall s3api head-bucket --bucket $Bucket)) {
        Write-Host "Creating artifact bucket $Bucket"
        Invoke-Aws s3api create-bucket --bucket $Bucket --create-bucket-configuration "LocationConstraint=$Region" | Out-Null
        Invoke-Aws s3api put-public-access-block --bucket $Bucket --public-access-block-configuration `
            "BlockPublicAcls=true,IgnorePublicAcls=true,BlockPublicPolicy=true,RestrictPublicBuckets=true" | Out-Null
    }

    # 2. 세션 토큰 서명 키. 한 번 만들면 유지한다(바꾸면 발급된 토큰이 모두 무효가 된다).
    $TokenParam = "/labproject/$Stage/token-signing-key"
    if (-not (Test-AwsCall ssm get-parameter --name $TokenParam)) {
        Write-Host "Creating $TokenParam"
        $Bytes = New-Object byte[] 48
        [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($Bytes)
        Invoke-Aws ssm put-parameter --name $TokenParam --type SecureString --value ([Convert]::ToBase64String($Bytes)) | Out-Null
    }

    $SteamParam = "/labproject/$Stage/steam-web-api-key"
    if (-not (Test-AwsCall ssm get-parameter --name $SteamParam)) {
        Write-Warning "$SteamParam is not set. Steam login returns 503 until you add it (dev login still works)."
    }

    foreach ($Secret in $GeneratedSecrets.GetEnumerator()) {
        $Name = "/labproject/$Stage/$($Secret.Key)"
        if (-not (Test-AwsCall ssm get-parameter --name $Name)) {
            Write-Host "Creating $Name"
            Invoke-Aws ssm put-parameter --name $Name --type SecureString --value (New-UrlSafeSecret $Secret.Value) | Out-Null
        }
    }

    # 3. API 서버 배포 묶음. 릴리스마다 새 키로 올리므로 install.sh는 항상 이번 묶음을 받는다.
    $ArtifactKey = "$Stage/server/server-$([DateTime]::UtcNow.ToString("yyyyMMdd'T'HHmmss'Z'")).tar.gz"
    & tar -czf .server.tar.gz --exclude=__pycache__ -C server app migrations deploy requirements.txt
    if ($LASTEXITCODE -ne 0) { throw "tar failed with exit code $LASTEXITCODE" }
    Invoke-Aws s3 cp --only-show-errors .server.tar.gz "s3://$Bucket/$ArtifactKey" | Out-Null
    Remove-Item .server.tar.gz

    # 4. Lambda 코드를 업로드하고 스택을 배포한다. 지정하지 않은 파라미터는 기존 스택 값을 유지한다.
    Invoke-Aws cloudformation package --template-file template.yaml --s3-bucket $Bucket --s3-prefix $Stage `
        --output-template-file .packaged.yaml | Out-Null

    # 기본 VPC의 가용 영역별 기본 서브넷. 가용 영역 순서로 정렬해, 첫 서브넷(API 서버 자리)이 배포마다 바뀌지 않게 한다.
    # 필터는 따옴표로 묶는다. 묶지 않으면 PowerShell이 쉼표를 배열로 읽는다.
    $VpcId = Invoke-Aws ec2 describe-vpcs --filters "Name=isDefault,Values=true" --query "Vpcs[0].VpcId" --output text
    $SubnetIds = (Invoke-Aws ec2 describe-subnets --filters "Name=vpc-id,Values=$VpcId" "Name=default-for-az,Values=true" `
        --query "sort_by(Subnets, &AvailabilityZone)[].SubnetId" --output text) -split "\s+"

    $Overrides = @("Stage=$Stage", "VpcId=$VpcId", "SubnetIds=$($SubnetIds -join ',')")
    if (-not (Get-StackParameter "ApiImageId")) {
        $Overrides += "ApiImageId=$(Invoke-Aws ssm get-parameter --name $ApiImageParameter --query Parameter.Value --output text)"
    }
    if ($GameLiftFleetId) { $Overrides += "GameLiftFleetId=$GameLiftFleetId" }
    if ($GameLiftLocation) { $Overrides += "GameLiftLocation=$GameLiftLocation" }
    if ($AllowDevLogin) { $Overrides += "AllowDevLogin=$AllowDevLogin" }

    & aws cloudformation deploy --template-file .packaged.yaml --stack-name $StackName `
        --capabilities CAPABILITY_IAM CAPABILITY_AUTO_EXPAND --no-fail-on-empty-changeset `
        --parameter-overrides @Overrides @AwsArgs
    if ($LASTEXITCODE -ne 0) { throw "cloudformation deploy failed with exit code $LASTEXITCODE" }

    # 5. API 서버에 이번 묶음을 설치한다. 새로 만든 인스턴스는 SSM 에이전트가 붙을 때까지 기다린다.
    $InstanceId = Get-ApiInstanceId $StackName $AwsArgs
    $AgentDeadline = (Get-Date).AddMinutes(5)
    while ((Invoke-Aws ssm describe-instance-information --filters "Key=InstanceIds,Values=$InstanceId" `
            --query "InstanceInformationList[0].PingStatus" --output text) -ne "Online") {
        if ((Get-Date) -gt $AgentDeadline) { throw "The SSM agent on $InstanceId did not come online." }
        Start-Sleep -Seconds 10
    }
    $Artifact = "s3://$Bucket/$ArtifactKey"
    Write-Host "Installing $Artifact on $InstanceId"
    Invoke-ApiServerCommand -StackName $StackName -AwsArgs $AwsArgs -Comment "Install $ArtifactKey" -Commands @(
        'set -euo pipefail',
        'work="$(mktemp -d)"',
        'trap ''rm -rf "$work"'' EXIT',
        "aws s3 cp --quiet --region $Region $Artifact `"`$work/server.tar.gz`"",
        'tar -xzf "$work/server.tar.gz" -C "$work" deploy/install.sh',
        "bash `"`$work/deploy/install.sh`" $Stage $Region $Artifact"
    )

    Invoke-Aws cloudformation describe-stacks --stack-name $StackName --query "Stacks[0].Outputs" --output table
}
finally {
    Pop-Location
}
