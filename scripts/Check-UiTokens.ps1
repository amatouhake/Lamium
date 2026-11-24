# Keeps screens on the shared widgets (AGENTS.md rule 7, DESIGN.md "Tokens"):
# a color literal (`Rgb{...}`, `Rgb name{...}`, a returned `{r, g, b}`) or a hand-drawn button fill
# (`palette::keyFill`) outside the shared widget files may not grow. Existing
# ones are counted per file in UiTokenBaseline.txt (L-341 and L-347 removed them); a
# file over its count fails, and so does one under it, so the baseline shrinks
# with the code. -Update rewrites the baseline after a removal.
# A color in another spelling counts too (L-400): `packColor(242, 204, 61)`, an opaque hex
# `0xff5a64ffu`, the game's `mce::Color{.25f, ...}`. A palette color goes into a pixel buffer as
# `ui::packed(palette::token)`; the map picture's own colors stay in the map's headers, counted.
param(
    [switch]$Update,
    [string]$Baseline = (Join-Path $PSScriptRoot 'UiTokenBaseline.txt')
)
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path $PSScriptRoot -Parent
$shared = @('src/ui/Widgets.h', 'src/ui/Widgets.cpp', 'src/ui/ListViewWidgets.h', 'src/ui/ListViewWidgets.cpp')
$patterns = [ordered]@{
    # A literal starts with a number; Rgb{c.r, c.g, c.b} converts a game or waypoint color and is not one.
    # A returned brace list of three numbers with a fraction is one too (whole numbers are a direction).
    color  = '\bRgb(\s+\w+)?\s*\{\s*[-.\d]|\breturn\s*\{(?=[^}]*\.)\s*[-.\df]+\s*,\s*[-.\df]+\s*,\s*[-.\df]+\s*\}'
    button = '\bpalette::keyFill\b'
    # All ones and ff000000 are skipped: mostly masks and "unchanged" (MapImage.h's
    # opaque white arrow default is the one color among them, left uncounted).
    packed = '\bpackColor\(\s*\d|\bmce::Color(\s+\w+)?\s*[{(]\s*[-.\d]|\b0x[fF]{2}(?![fF]{6}|0{6})[0-9a-fA-F]{6}u?\b'
}
$counts = [ordered]@{}
foreach ($folder in 'src/ui', 'src/features', 'src/overlay') {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $projectDirectory $folder) -Recurse -File -Include '*.cpp', '*.h' | Sort-Object FullName) {
        $relative = [IO.Path]::GetRelativePath($projectDirectory, $file.FullName).Replace('\', '/')
        if ($shared -contains $relative) { continue }
        $text = Get-Content -LiteralPath $file.FullName -Raw
        foreach ($kind in $patterns.Keys) {
            $n = [regex]::Matches($text, $patterns[$kind]).Count
            if ($n -gt 0) { $counts["$kind $relative"] = $n }
        }
    }
}
if ($Update) {
    $lines = @('# Check-UiTokens.ps1 baseline: kind, file, count. Only ever goes down.')
    $lines += foreach ($key in $counts.Keys) { "$key $($counts[$key])" }
    Set-Content -LiteralPath $Baseline -Value $lines -Encoding utf8NoBOM
    Write-Host "UI token baseline written: $($counts.Count) entries."
    return
}
$allowed = @{}
foreach ($line in Get-Content -LiteralPath $Baseline) {
    if ($line -match '^\s*(#|$)') { continue }
    $parts = $line -split ' '
    $allowed["$($parts[0]) $($parts[1])"] = [int]$parts[2]
}
$problems = @()
foreach ($key in $counts.Keys) {
    $limit = if ($allowed.ContainsKey($key)) { $allowed[$key] } else { 0 }
    if ($counts[$key] -gt $limit) {
        $what = if ($key.StartsWith('color')) { 'color literal(s): add a palette token in Widgets.h' }
            elseif ($key.StartsWith('packed')) { 'packed color literal(s): use ui::packed(palette::token), or keep a map color in the map headers' }
            else { 'hand-drawn button fill(s): use the shared button in Widgets.h' }
        $problems += "$key has $($counts[$key]) (allowed $limit) $what"
    }
}
foreach ($key in $allowed.Keys) {
    $now = if ($counts.Contains($key)) { $counts[$key] } else { 0 }
    if ($now -lt $allowed[$key]) { $problems += "$key went down to $now (baseline $($allowed[$key])): run Check-UiTokens.ps1 -Update" }
}
if ($problems) { $problems | ForEach-Object { Write-Host $_ }; throw "UI token check failed ($($problems.Count))." }
$total = ($counts.Values | Measure-Object -Sum).Sum
Write-Host "UI tokens match the baseline: $total remaining in $($counts.Count) entries."
