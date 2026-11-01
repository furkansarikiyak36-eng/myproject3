# Copies the UE 5.8 Third Person template character (Blueprint version) into Content/.
# These assets ship with the engine, so they are not in git. The world map's game mode
# (BP_ThirdPersonGameMode, set by -run=BuildValleyWorld) needs them.
param([string]$Engine = "C:\Program Files\Epic Games\UE_5.8")
$t = Join-Path $Engine "Templates"
$c = Join-Path (Split-Path $PSScriptRoot -Parent) "Content"
$pairs = @(
    @("$t\TemplateResources\High\Characters\Content\Mannequins", "$c\Characters\Mannequins"),
    @("$t\TemplateResources\High\Input\Content", "$c\Input"),
    @("$t\TP_ThirdPersonBP\Content\ThirdPerson\Blueprints", "$c\ThirdPerson\Blueprints")
)
foreach ($p in $pairs) {
    if (Test-Path $p[1]) { "exists  $($p[1])" }
    else { Copy-Item $p[0] $p[1] -Recurse; "copied  $($p[1])" }
}
