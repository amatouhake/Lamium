# Keeps screens on the shared widgets (AGENTS.md rule 7, DESIGN.md "Tokens"):
# a color literal (`Rgb{...}`, `Rgb name{...}`) or a hand-drawn button fill
# (`palette::keyFill`) outside the shared widget files may not grow. Existing
# ones are counted per file in UiTokenBaseline.txt (L-341 removes them); a
# file over its count fails, and so does one under it, so the baseline shrinks
# with the code. -Update rewrites the baseline after a removal.
param(
    [switch]$Update,
    [string]$Baseline = (Join-Path $PSScriptRoot 'UiTokenBaseline.txt')
)
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path $PSScriptRoot -Parent
$shared = @('src/ui/Widgets.h', 'src/ui/Widgets.cpp', 'src/ui/ListViewWidgets.h', 'src/ui/ListViewWidgets.cpp')
$patterns = [ordered]@{
    color  = '\bRgb(\s+\w+)?\s*\{'
    button = '\bpalette::keyFill\b'
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
    $lines = @('# Check-UiTokens.ps1 baseline: kind, file, count. Only ever goes down (L-341).')
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
        $what = if ($key.StartsWith('color')) { 'color literal(s): add a palette token in Widgets.h' } else { 'hand-drawn button fill(s): use the shared button in Widgets.h' }
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
