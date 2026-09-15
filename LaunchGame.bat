@echo off
setlocal EnableExtensions EnableDelayedExpansion
set "PROJECT=%~dp0LabProject.uproject"

if not defined LABPROJECT_UE_EDITOR (
    for /f "usebackq delims=" %%V in (`powershell.exe -NoProfile -Command "(Get-Content -Raw -LiteralPath '%PROJECT%' | ConvertFrom-Json).EngineAssociation"`) do set "LABPROJECT_ENGINE_ASSOCIATION=%%V"
    for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\!LABPROJECT_ENGINE_ASSOCIATION!" /v InstalledDirectory 2^>nul') do set "LABPROJECT_UE_ROOT=%%B"
    if not defined LABPROJECT_UE_ROOT (
        for /f "tokens=2,*" %%A in ('reg query "HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds" /v !LABPROJECT_ENGINE_ASSOCIATION! 2^>nul') do set "LABPROJECT_UE_ROOT=%%B"
    )
    if defined LABPROJECT_UE_ROOT set "LABPROJECT_UE_EDITOR=!LABPROJECT_UE_ROOT!\Engine\Binaries\Win64\UnrealEditor.exe"
)

if not exist "%LABPROJECT_UE_EDITOR%" (
    echo Unreal Editor %LABPROJECT_ENGINE_ASSOCIATION% was not found.
    echo Set LABPROJECT_UE_EDITOR to the full UnrealEditor.exe path and run this file again.
    exit /b 1
)

"%LABPROJECT_UE_EDITOR%" "%PROJECT%" -game -ResX=960 -ResY=540 -WINDOWED -log
