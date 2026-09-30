<#
.SYNOPSIS
  LabProject 백엔드 스택을 배포한다(API Gateway + Lambda + DynamoDB + GameLift 플릿 역할).

.DESCRIPTION
  AWS CLI만 사용한다. 처음 실행하면 배포 아티팩트 버킷과 세션 토큰 서명 키(SSM SecureString)를 만든다.
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

    # 3. Lambda 코드를 업로드하고 스택을 배포한다. 지정하지 않은 파라미터는 기존 스택 값을 유지한다.
    Invoke-Aws cloudformation package --template-file template.yaml --s3-bucket $Bucket --s3-prefix $Stage `
        --output-template-file .packaged.yaml | Out-Null

    $Overrides = @("Stage=$Stage")
    if ($GameLiftFleetId) { $Overrides += "GameLiftFleetId=$GameLiftFleetId" }
    if ($GameLiftLocation) { $Overrides += "GameLiftLocation=$GameLiftLocation" }
    if ($AllowDevLogin) { $Overrides += "AllowDevLogin=$AllowDevLogin" }

    & aws cloudformation deploy --template-file .packaged.yaml --stack-name $StackName `
        --capabilities CAPABILITY_IAM CAPABILITY_AUTO_EXPAND --no-fail-on-empty-changeset `
        --parameter-overrides @Overrides @AwsArgs
    if ($LASTEXITCODE -ne 0) { throw "cloudformation deploy failed with exit code $LASTEXITCODE" }

    Invoke-Aws cloudformation describe-stacks --stack-name $StackName --query "Stacks[0].Outputs" --output table
}
finally {
    Pop-Location
}
