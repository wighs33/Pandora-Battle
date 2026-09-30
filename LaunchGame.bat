@echo off
setlocal EnableExtensions EnableDelayedExpansion
set "PROJECT=%~dp0LabProject.uproject"

if not defined LABPROJECT_UE_EDITOR (
    for /f "usebackq delims=" %%V in (`powershell.exe -NoProfile -Command "(Get-Content -Raw -LiteralPath '%PROJECT%' | ConvertFrom-Json).EngineAssociation"`) do set "LABPROJECT_ENGINE_ASSOCIATION=%%V"
    for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\!LABPROJECT_ENGINE_ASSOCIATION!" /v InstalledDirectory 2^>nul') do set "LABPROJECT_UE_ROOT=%%B"
    if not defined LABPROJECT_UE_ROOT (
        for /f "tokens=2,*" %%A in ('reg query "HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds" /v !LABPROJECT_ENGINE_ASSOCIATION! 2^>nul') do set "LABPROJECT_UE_ROOT=%%B"
    )
    rem Epic Games Launcher can install an engine without the registry key. Its install list keeps the path.
    if not defined LABPROJECT_UE_ROOT (
        for /f "usebackq delims=" %%L in (`powershell.exe -NoProfile -Command "$d = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'; if (Test-Path $d) { ((Get-Content -Raw $d | ConvertFrom-Json).InstallationList | Where-Object AppName -eq 'UE_!LABPROJECT_ENGINE_ASSOCIATION!' | Select-Object -First 1).InstallLocation }"`) do set "LABPROJECT_UE_ROOT=%%L"
    )
    if defined LABPROJECT_UE_ROOT set "LABPROJECT_UE_EDITOR=!LABPROJECT_UE_ROOT!\Engine\Binaries\Win64\UnrealEditor.exe"
)

if not exist "%LABPROJECT_UE_EDITOR%" (
    echo Unreal Editor %LABPROJECT_ENGINE_ASSOCIATION% was not found.
    echo Set LABPROJECT_UE_EDITOR to the full UnrealEditor.exe path and run this file again.
    exit /b 1
)

"%LABPROJECT_UE_EDITOR%" "%PROJECT%" -game -ResX=960 -ResY=540 -WINDOWED -log
