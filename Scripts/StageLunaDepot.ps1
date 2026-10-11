<#
.SYNOPSIS
    Luna 로컬 모델(llama-server + GGUF)을 Steam 무료 DLC 데포로 올릴 수 있게 ContentBuilder에 스테이징한다.

.DESCRIPTION
    프로젝트의 Luna\ 폴더에서 llama-server 실행에 필요한 파일과 모델만 골라
    ContentBuilder\content\PandoraBattle_Luna\에 복사하고, 설치 폴더의 Luna\로 들어가도록 데포 스크립트를 만든다.
    게임은 설치 폴더(또는 개발 실행에서는 프로젝트 폴더)의 Luna\에서 실행 파일과 모델을 찾는다.

    Steamworks에서 먼저 할 일(직접 로그인):
      1. 무료 DLC를 만들고 그 DLC에 데포를 추가한다. 그 데포 번호를 -DepotId로 넘긴다.
      2. 업로드는 본편 앱 빌드(app_4972140.vdf)에 이 데포를 함께 넣어 올린다. 이 스크립트가 depots 목록에 추가한다.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Scripts\StageLunaDepot.ps1 -DepotId 4972142
#>
param(
    [Parameter(Mandatory = $true)][string]$DepotId,
    [string]$SourceDir = "",
    [string]$ContentBuilder = "C:\Users\whgus\Downloads\sdk\tools\ContentBuilder",
    [string]$AppId = "4972140",
    [string]$ModelFile = "models\gemma-3-4b-it-Q4_K_M.gguf"
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

if (-not $SourceDir) { $SourceDir = Join-Path $PSScriptRoot "..\Luna" }
$SourceDir = (Resolve-Path $SourceDir).Path
$ContentDir = Join-Path $ContentBuilder "content\PandoraBattle_Luna"
$ScriptsDir = Join-Path $ContentBuilder "scripts"
$AppScript = Join-Path $ScriptsDir "app_$AppId.vdf"
$DepotScript = Join-Path $ScriptsDir "depot_$DepotId.vdf"

# llama-server가 실제로 쓰는 파일만 보낸다. 다른 llama.cpp 도구와 RPC 백엔드는 뺀다.
$Runtime = @(
    "llama-server.exe", "llama-server-impl.dll", "llama-common.dll", "llama.dll", "mtmd.dll",
    "ggml.dll", "ggml-base.dll", "ggml-vulkan.dll", "libomp.dll"
)
$Notices = @("LICENSE-LLVM-OpenMP", "LICENSE-llama.cpp", "NOTICE-Gemma.txt", "GEMMA-TERMS-OF-USE.txt")

foreach ($Name in $Runtime + $Notices + @($ModelFile)) {
    if (-not (Test-Path (Join-Path $SourceDir $Name))) {
        Write-Error "필요한 파일이 없습니다: $SourceDir\$Name"
    }
}

if (Test-Path $ContentDir) { Remove-Item -LiteralPath $ContentDir -Recurse -Force -Confirm:$false }
New-Item -ItemType Directory -Force (Join-Path $ContentDir "models") | Out-Null
foreach ($Name in $Runtime + $Notices) { Copy-Item (Join-Path $SourceDir $Name) $ContentDir }
# CPU 종류별 백엔드는 실행할 때 그 PC에 맞는 것 하나를 고르므로 모두 넣는다.
Get-ChildItem $SourceDir -Filter "ggml-cpu-*.dll" | Copy-Item -Destination $ContentDir
Copy-Item (Join-Path $SourceDir $ModelFile) (Join-Path $ContentDir "models")

@"
"DepotBuildConfig"
{
	"DepotID" "$DepotId"
	"contentroot" "$ContentDir"
	"FileMapping"
	{
		"LocalPath" "*"
		"DepotPath" "Luna"
		"recursive" "1"
	}
}
"@ | Set-Content -Path $DepotScript -Encoding ASCII

# 본편 앱 빌드 스크립트의 depots 목록에 이 데포를 넣는다. 이미 있으면 그대로 둔다.
$App = Get-Content -Raw $AppScript
$AlreadyListed = $App -match "`"$DepotId`""
if (-not $AlreadyListed) {
    $Line = "`t`t`"$DepotId`"`t`"$DepotScript`""
    $App = $App -replace '("depots"\s*\{)', "`$1`r`n$Line"
    Set-Content -Path $AppScript -Value $App -Encoding ASCII -NoNewline
}

$Size = (Get-ChildItem $ContentDir -Recurse | Measure-Object Length -Sum).Sum / 1MB
"스테이징 완료: $ContentDir ({0:N0} MB)" -f $Size
"데포 스크립트: $DepotScript"
if ($AlreadyListed) { "앱 스크립트에 데포 $DepotId 이미 있음: $AppScript" } else { "앱 스크립트에 데포 $DepotId 추가: $AppScript" }
