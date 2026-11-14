# Keeps the backlog readable in one pass (BACKLOG.md "How items are kept"):
# the index lists every open item file once and nothing else, BACKLOG-DONE.md
# lists every finished one, each open item has its Kind and Status lines, and
# the index and the items stay short.
param(
    [int]$IndexLimit = 320,
    [int]$ItemLimit = 80
)
$ErrorActionPreference = 'Stop'
$docs = Join-Path (Split-Path $PSScriptRoot -Parent) 'docs'
$problems = @()
function Check-Listing([string]$indexFile, [string]$folder, [string]$prefix) {
    $text = Get-Content -LiteralPath (Join-Path $docs $indexFile) -Raw
    $links = [regex]::Matches($text, '\]\(' + [regex]::Escape($prefix) + '(L-\d+[a-z]?)\.md\)') | ForEach-Object { $_.Groups[1].Value }
    $files = Get-ChildItem -LiteralPath (Join-Path $docs $folder) -Filter 'L-*.md' -File | ForEach-Object { $_.BaseName }
    $out = @()
    foreach ($group in $links | Group-Object) { if ($group.Count -gt 1) { $out += "$indexFile lists $($group.Name) $($group.Count) times" } }
    foreach ($id in $files) { if ($links -notcontains $id) { $out += "$folder/$id.md is not listed in $indexFile" } }
    foreach ($id in $links | Sort-Object -Unique) { if ($files -notcontains $id) { $out += "$indexFile lists $id but $folder/$id.md does not exist" } }
    return $out
}
$problems += Check-Listing 'BACKLOG.md' 'backlog' 'backlog/'
$problems += Check-Listing 'BACKLOG-DONE.md' 'backlog/done' 'backlog/done/'
$indexLines = (Get-Content -LiteralPath (Join-Path $docs 'BACKLOG.md')).Count
if ($indexLines -gt $IndexLimit) { $problems += "BACKLOG.md has $indexLines lines (limit $IndexLimit): move detail into the item files" }
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $docs 'backlog') -Filter 'L-*.md' -File) {
    $lines = Get-Content -LiteralPath $file.FullName
    if ($lines.Count -gt $ItemLimit) { $problems += "backlog/$($file.Name) has $($lines.Count) lines (limit $ItemLimit): rewrite it as the current state" }
    if (-not ($lines -match '^Kind: ')) { $problems += "backlog/$($file.Name) has no 'Kind:' line" }
    if (-not ($lines -match '^Status: ')) { $problems += "backlog/$($file.Name) has no 'Status:' line" }
    if ($lines[0] -notmatch ('^# ' + [regex]::Escape($file.BaseName) + ' ')) { $problems += "backlog/$($file.Name) does not start with '# $($file.BaseName) <title>'" }
}
if ($problems) { $problems | ForEach-Object { Write-Host $_ }; throw "Backlog check failed ($($problems.Count))." }
$open = (Get-ChildItem -LiteralPath (Join-Path $docs 'backlog') -Filter 'L-*.md' -File).Count
Write-Host "Backlog index matches: $open open items, BACKLOG.md $indexLines lines."
