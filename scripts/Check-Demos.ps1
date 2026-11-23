# Keeps the design mockups on the kit (docs/demos/README.md "Making a mockup"):
# - docs/demos/kit/lamium-data.js exports every palette token Widgets.h defines (CI also
#   regenerates the file with LamiumDemoData and fails on a difference, so it is current);
# - a demo not in DemoBaseline.txt (the hand-made ones from before the kit) loads the kit,
#   carries no color literal of its own and starts from today's screen (kind: 'today');
# - every demo is listed in docs/demos/README.md.
# Whether a demo's script runs is checked in a browser: docs/demos/kit/check.html.
param([string]$Baseline = (Join-Path $PSScriptRoot 'DemoBaseline.txt'))
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$demos = Join-Path $root 'docs/demos'
$problems = @()

$data = Get-Content -LiteralPath (Join-Path $demos 'kit/lamium-data.js') -Raw
$widgets = Get-Content -LiteralPath (Join-Path $root 'src/ui/Widgets.h') -Raw
# The palette namespace up to the opacity namespace holds every color token, game-sampled ones included.
$block = [regex]::Match($widgets, '(?s)namespace palette \{(.*?)namespace opacity').Groups[1].Value
$block = [regex]::Replace($block, '//[^\n]*', '')
$tokens = [regex]::Matches($block, '\b([a-z][A-Za-z]*)\s*\{') | ForEach-Object { $_.Groups[1].Value } |
    Where-Object { $_ -notin @('palette', 'vanilla', 'namespace', 'switch', 'textCode', 'c') } | Sort-Object -Unique
foreach ($token in $tokens) {
    if ($data -notmatch ('"' + $token + '":\s*"#')) { $problems += "palette token '$token' is not in kit/lamium-data.js: add it to tools/DemoData.cpp" }
}

$legacy = @(Get-Content -LiteralPath $Baseline | Where-Object { $_ -notmatch '^\s*(#|$)' })
$readme = Get-Content -LiteralPath (Join-Path $demos 'README.md') -Raw
foreach ($file in Get-ChildItem -LiteralPath $demos -File -Filter '*.html' | Sort-Object Name) {
    $name = $file.Name
    if ($readme -notmatch [regex]::Escape("($name)")) { $problems += "$name is not listed in docs/demos/README.md" }
    if ($legacy -contains $name) { continue }
    $text = Get-Content -LiteralPath $file.FullName -Raw
    if ($text -notmatch 'kit/lamium-data\.js' -or $text -notmatch 'kit/kit\.js') { $problems += "${name}: load kit/lamium-data.js and kit/kit.js (start from docs/demos/kit/template.html)" }
    if ($text -match '(?<![\w&])#[0-9a-fA-F]{3,8}\b(?![\w-])|\brgba?\(') { $problems += "${name}: color literal; use Lamium.color('<token>') or a --lm-* variable" }
    if ($text -notmatch "kind:\s*'today'") { $problems += "${name}: no today case; compare a proposal with today's screen (Lamium.page.compare)" }
}
foreach ($name in $legacy) {
    if (-not (Test-Path -LiteralPath (Join-Path $demos $name))) { $problems += "DemoBaseline.txt lists $name, which no longer exists" }
}
if ($problems) { $problems | ForEach-Object { Write-Host $_ }; throw "Demo check failed ($($problems.Count))." }
Write-Host "Demos match the kit rules: $($tokens.Count) palette tokens exported, $($legacy.Count) hand-made demos kept as they are."
