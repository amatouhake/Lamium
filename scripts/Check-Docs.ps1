# Keeps the documents current and linked (docs/README.md "Feature document
# shape"): a document under docs/ stays under the line limit, except the
# records and the script-checked inventory, and every relative link in the
# docs, README, AGENTS.md and CONTRIBUTING.md reaches an existing file and,
# for a Markdown target, an existing heading.
param([int]$Limit = 400)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$exempt = @{
    'VALIDATION-LOG.md' = 'append-only record'
    'GAME-UPDATES.md'   = 'hook inventory checked by Check-GameInventory.ps1'
    'FEATURES.md'       = 'player guide, one section per feature'
}
$problems = @()
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $root 'docs') -Filter '*.md' -File) {
    if ($exempt.ContainsKey($file.Name)) { continue }
    $n = (Get-Content -LiteralPath $file.FullName).Count
    if ($n -gt $Limit) { $problems += "docs/$($file.Name) has $n lines (limit $Limit): rewrite it as the current contract" }
}
function Slugs([string]$path) {
    $seen = @{}
    $inFence = $false
    foreach ($line in Get-Content -LiteralPath $path -Encoding utf8) {
        if ($line -match '^```') { $inFence = -not $inFence; continue }
        if ($inFence -or $line -notmatch '^#{1,6}\s+(.*)$') { continue }
        $slug = $Matches[1].Trim().ToLowerInvariant() -replace '[^\p{L}\p{Nd}\s_-]', '' -replace '\s', '-'
        $base = $slug
        if ($seen.ContainsKey($base)) { $seen[$base]++; $slug = "$base-$($seen[$base])" } else { $seen[$base] = 0 }
        $slug
    }
}
$slugCache = @{}
$sources = @(Get-ChildItem -LiteralPath (Join-Path $root 'docs') -Filter '*.md' -File -Recurse | Where-Object { $_.FullName -notmatch '[\\/]demos[\\/]' })
$sources += Get-Item -LiteralPath (Join-Path $root 'README.md'), (Join-Path $root 'AGENTS.md'), (Join-Path $root 'CONTRIBUTING.md')
foreach ($source in $sources) {
    $text = Get-Content -LiteralPath $source.FullName -Raw -Encoding utf8
    $text = [regex]::Replace($text, '(?ms)^```.*?^```', '')
    foreach ($m in [regex]::Matches($text, '\]\(([^)\s]+)\)')) {
        $url = $m.Groups[1].Value
        if ($url -match '^[a-z]+:') { continue }
        $target, $anchor = $url -split '#', 2
        $targetPath = if ($target) { Join-Path $source.DirectoryName $target } else { $source.FullName }
        $relative = [IO.Path]::GetRelativePath($root, $source.FullName).Replace('\', '/')
        if (-not (Test-Path -LiteralPath $targetPath)) { $problems += "$relative links to missing $url"; continue }
        if ($anchor -and $targetPath.EndsWith('.md')) {
            $full = (Resolve-Path -LiteralPath $targetPath).Path
            if (-not $slugCache.ContainsKey($full)) { $slugCache[$full] = @(Slugs $full) }
            if ($slugCache[$full] -notcontains $anchor) { $problems += "$relative links to missing heading $url" }
        }
    }
}
if ($problems) { $problems | ForEach-Object { Write-Host $_ }; throw "Docs check failed ($($problems.Count))." }
Write-Host "Docs are within $Limit lines and every relative link resolves ($($sources.Count) files)."
