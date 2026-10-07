<#
.SYNOPSIS
    CI 작업 폴더에 git이 담지 않는 에셋과 플러그인을 채운다.

.DESCRIPTION
    공개 저장소는 로직 애셋만 담는다(.gitignore의 Content 정책). 마켓 팩, 아트, 서드파티 플러그인은 빌드 머신의
    전체 프로젝트 폴더(-SourceProject)에만 있다. 이 스크립트는 그 폴더에서 Content와 Plugins를 훑어,
    작업 폴더의 git이 추적하는 파일은 건드리지 않고(체크아웃한 커밋의 버전이 이긴다) 추적하지 않는 파일만 복사한다.

    처음 한 번은 전체(수 GB)를 복사하고, 이후에는 없거나 크기·시각이 다른 파일만 복사한다.
    Plugins의 Intermediate 폴더는 빌드 산출물이라 복사하지 않는다.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Scripts\SyncBuildMachineContent.ps1 -SourceProject C:\Projects\LabProject
    powershell -ExecutionPolicy Bypass -File Scripts\SyncBuildMachineContent.ps1 -SourceProject C:\Projects\LabProject -WhatIf
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceProject,
    [string]$TargetProject = (Join-Path $PSScriptRoot ".."),
    [string[]]$Folders = @("Content", "Plugins"),
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$SourceProject = (Resolve-Path $SourceProject).Path
$TargetProject = (Resolve-Path $TargetProject).Path
if ($SourceProject -eq $TargetProject) {
    Write-Host "원본과 작업 폴더가 같아 복사하지 않습니다."
    exit 0
}

# git ls-files는 '/' 구분자와 저장소 기준 경로를 돌려준다. Windows 경로는 대소문자를 가리지 않는다.
$Tracked = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($Path in (& git -C $TargetProject -c core.quotepath=off ls-files -- $Folders)) {
    [void]$Tracked.Add($Path.Replace('/', '\'))
}

$Copied = 0
$CopiedBytes = 0L
foreach ($Folder in $Folders) {
    $SourceFolder = Join-Path $SourceProject $Folder
    if (-not (Test-Path $SourceFolder)) {
        continue
    }
    foreach ($File in (Get-ChildItem -Path $SourceFolder -Recurse -File)) {
        $Relative = $File.FullName.Substring($SourceProject.Length + 1)
        if ($Tracked.Contains($Relative) -or $Relative -match '\\Intermediate\\') {
            continue
        }
        $Destination = Join-Path $TargetProject $Relative
        $Existing = Get-Item -LiteralPath $Destination -ErrorAction SilentlyContinue
        if ($Existing -and $Existing.Length -eq $File.Length -and $Existing.LastWriteTimeUtc -ge $File.LastWriteTimeUtc) {
            continue
        }
        $Copied++
        $CopiedBytes += $File.Length
        if ($WhatIf) {
            continue
        }
        New-Item -ItemType Directory -Force -Path (Split-Path $Destination) | Out-Null
        Copy-Item -LiteralPath $File.FullName -Destination $Destination -Force
    }
}

$Verb = if ($WhatIf) { "복사할" } else { "복사한" }
Write-Host ("{0} 파일 {1}개, {2:N1} MB (추적 중인 파일 {3}개는 체크아웃 버전 유지)" -f $Verb, $Copied, ($CopiedBytes / 1MB), $Tracked.Count)
