<#
.SYNOPSIS
  API 서버 EC2에서 셸 명령을 SSM Run Command로 실행하고 출력을 보여 준다. deploy.ps1과 bossraid.ps1이 dot-source한다.

.DESCRIPTION
  SSH와 열린 포트 없이 Session Manager 권한(인스턴스 역할의 AmazonSSMManagedInstanceCore)으로 실행한다.
  명령은 root로 돈다. 실패하면 표준 오류를 보여 주고 예외를 던진다.
#>

function Get-ApiInstanceId([string]$StackName, [string[]]$AwsArgs) {
    $InstanceId = & aws cloudformation describe-stacks --stack-name $StackName `
        --query "Stacks[0].Outputs[?OutputKey=='ApiInstanceId'].OutputValue" --output text @AwsArgs
    if ($LASTEXITCODE -ne 0 -or -not $InstanceId -or $InstanceId -eq "None") {
        throw "Stack $StackName has no ApiInstanceId output. Deploy it with Backend\deploy.ps1 first."
    }
    return $InstanceId
}

function Invoke-ApiServerCommand {
    param(
        [Parameter(Mandatory = $true)][string]$StackName,
        [Parameter(Mandatory = $true)][string[]]$AwsArgs,
        [Parameter(Mandatory = $true)][string[]]$Commands,
        [string]$Comment = "LabProject API server",
        [int]$TimeoutSeconds = 600
    )

    $InstanceId = Get-ApiInstanceId $StackName $AwsArgs
    # 명령에 따옴표가 섞여도 깨지지 않게 JSON 파일로 넘긴다.
    $ParametersFile = Join-Path $env:TEMP "labproject-ssm-$([guid]::NewGuid()).json"
    @{ commands = $Commands; executionTimeout = @("$TimeoutSeconds") } | ConvertTo-Json -Compress |
        Set-Content -Encoding Ascii $ParametersFile
    try {
        $CommandId = & aws ssm send-command --instance-ids $InstanceId --document-name AWS-RunShellScript `
            --comment $Comment --parameters "file://$ParametersFile" --query Command.CommandId --output text @AwsArgs
        if ($LASTEXITCODE -ne 0) { throw "ssm send-command failed with exit code $LASTEXITCODE" }
    }
    finally {
        Remove-Item $ParametersFile -ErrorAction SilentlyContinue
    }

    # 보낸 직후에는 실행 기록이 아직 없을 수 있다(InvocationDoesNotExist). 끝날 때까지 다시 묻는다.
    $Status = "Pending"
    while ($Status -in @("Pending", "InProgress", "Delayed")) {
        Start-Sleep -Seconds 5
        $Status = & aws ssm get-command-invocation --command-id $CommandId --instance-id $InstanceId `
            --query Status --output text @AwsArgs 2>$null
        if ($LASTEXITCODE -ne 0) { $Status = "Pending" }
    }

    $Invocation = & aws ssm get-command-invocation --command-id $CommandId --instance-id $InstanceId --output json @AwsArgs |
        ConvertFrom-Json
    if ($Invocation.StandardOutputContent) { Write-Host $Invocation.StandardOutputContent.TrimEnd() }
    if ($Status -ne "Success") {
        if ($Invocation.StandardErrorContent) { Write-Host $Invocation.StandardErrorContent.TrimEnd() }
        throw "Command on $InstanceId ended with $Status."
    }
}
