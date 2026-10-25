param(
    [string]$EngineRoot = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ProjectFile = Join-Path $ProjectRoot "MyProject3.uproject"

if (-not (Test-Path $ProjectFile)) {
    throw "Project file not found: $ProjectFile"
}

function Get-BuildBatchPath([string]$Root) {
    if ([string]::IsNullOrWhiteSpace($Root)) { return $null }

    $candidate1 = Join-Path $Root "Engine\Build\BatchFiles\Build.bat"
    if (Test-Path $candidate1) { return $candidate1 }

    $candidate2 = Join-Path $Root "Build\BatchFiles\Build.bat"
    if ((Test-Path $candidate2) -and (Split-Path $Root -Leaf) -eq "Engine") {
        return $candidate2
    }

    return $null
}

$BuildBatch = Get-BuildBatchPath $EngineRoot

if (-not $BuildBatch) {
    $SearchBases = @(
        (Join-Path $env:ProgramFiles "Epic Games"),
        "C:\UnrealEngine",
        "D:\Epic Games",
        "D:\UnrealEngine"
    ) | Where-Object { $_ -and (Test-Path $_) }

    $EngineCandidates = @()
    foreach ($Base in $SearchBases) {
        $EngineCandidates += Get-ChildItem -Path $Base -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match "5\.8|UE_5" }
    }

    foreach ($Candidate in ($EngineCandidates | Sort-Object @{Expression={ if ($_.Name -match "5\.8") { 0 } else { 1 } }}, FullName)) {
        $BuildBatch = Get-BuildBatchPath $Candidate.FullName
        if ($BuildBatch) { break }
    }
}

if (-not $BuildBatch) {
    throw @"
Could not find Unreal Engine 5.8 Build.bat.
Run again with the engine root explicitly, for example:
  powershell -ExecutionPolicy Bypass -File .\UnrealEditorScripts\BuildLogSnapPrototype.ps1 -EngineRoot "D:\Epic Games\UE_5.8"
EngineRoot must be the folder that contains the Engine folder.
"@
}

Write-Host "Project: $ProjectFile"
Write-Host "Build tool: $BuildBatch"
Write-Host "Building MyProject3Editor Win64 Development..."

& $BuildBatch "MyProject3Editor" "Win64" "Development" "-Project=$ProjectFile" "-WaitMutex" "-FromMsBuild"
$BuildExitCode = $LASTEXITCODE

if ($BuildExitCode -ne 0) {
    throw "Unreal Editor target build failed with exit code $BuildExitCode. Review the first compiler error above it."
}

Write-Host "Build completed successfully. No Unreal Editor play/test was run by this script."
