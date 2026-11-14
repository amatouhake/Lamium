# Keeps the backlog readable in one pass (BACKLOG.md "How items are kept"):
# the index lists every open item file once and nothing else, BACKLOG-DONE.md
# lists every finished one, each open item has its Kind and Status lines, and
# the index and the items stay short. Each index line is generated from its
# item (title, kind, status): -Update rewrites them after an item changes.
# A Ready item has no Open section: an open choice makes it Design.
param(
    [switch]$Update,
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
$indexPath = Join-Path $docs 'BACKLOG.md'
$index = [IO.File]::ReadAllLines($indexPath)
$changed = $false
for ($i = 0; $i -lt $index.Count; $i++) {
    if ($index[$i] -notmatch '^- \[(L-\d+[a-z]?)\]\(backlog/(L-\d+[a-z]?)\.md\) ' -or $Matches[1] -ne $Matches[2]) { continue }
    $id = $Matches[1]
    $item = Join-Path $docs "backlog/$id.md"
    if (-not (Test-Path -LiteralPath $item)) { continue }
    $lines = [IO.File]::ReadAllLines($item)
    $title = $lines[0] -replace '^# L-\d+[a-z]?\s+', ''
    $kind = (($lines -match '^Kind: ') | Select-Object -First 1) -replace '^Kind: ', '' -replace '[*]', ''
    $kind = ($kind -split '[.,(]')[0].Trim()
    $status = (($lines -match '^Status: ') | Select-Object -First 1) -replace '^Status: ', ''
    $expected = "- [$id](backlog/$id.md) $title ($kind): $status"
    if ($index[$i] -ne $expected) {
        if ($Update) { $index[$i] = $expected; $changed = $true }
        else { $problems += "BACKLOG.md line for $id differs from its item: run Check-Backlog.ps1 -Update" }
    }
}
if ($changed) { [IO.File]::WriteAllText($indexPath, ($index -join "`n") + "`n", [Text.UTF8Encoding]::new($false)); Write-Host 'Index lines regenerated.' }
$indexLines = $index.Count
if ($indexLines -gt $IndexLimit) { $problems += "BACKLOG.md has $indexLines lines (limit $IndexLimit): move detail into the item files" }
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $docs 'backlog') -Filter 'L-*.md' -File) {
    $lines = Get-Content -LiteralPath $file.FullName
    if ($lines.Count -gt $ItemLimit) { $problems += "backlog/$($file.Name) has $($lines.Count) lines (limit $ItemLimit): rewrite it as the current state" }
    if (-not ($lines -match '^Kind: ')) { $problems += "backlog/$($file.Name) has no 'Kind:' line" }
    if (($lines -match '^Kind: Ready') -and ($lines -match '^## Open')) { $problems += "backlog/$($file.Name) is Ready but has an Open section: make it Design until the choice is agreed" }
    if (-not ($lines -match '^Status: ')) { $problems += "backlog/$($file.Name) has no 'Status:' line" }
    if ($lines[0] -notmatch ('^# ' + [regex]::Escape($file.BaseName) + ' ')) { $problems += "backlog/$($file.Name) does not start with '# $($file.BaseName) <title>'" }
}
if ($problems) { $problems | ForEach-Object { Write-Host $_ }; throw "Backlog check failed ($($problems.Count))." }
$open = (Get-ChildItem -LiteralPath (Join-Path $docs 'backlog') -Filter 'L-*.md' -File).Count
Write-Host "Backlog index matches: $open open items, BACKLOG.md $indexLines lines."
