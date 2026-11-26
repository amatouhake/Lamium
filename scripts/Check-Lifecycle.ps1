# Every feature says how it handles world exit, dimension change and focus
# loss (AGENTS.md rule 3, L-402): each row of the feature table in
# src/app/Runtime.cpp has a line in docs/LIFECYCLE.md with all three cells
# filled, and each line there names a row. Diagnostic rows (a name ending in
# "diagnostics" or "probe") are exempt. It checks that the answer is written,
# not that the code does it.
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path $PSScriptRoot -Parent
$runtime = Get-Content -LiteralPath (Join-Path $projectDirectory 'src/app/Runtime.cpp') -Raw
$table = [regex]::Match($runtime, '(?s)Feature const features\[\] = \{(.*?)\n\};')
if (-not $table.Success) { throw 'Lifecycle check: the feature table in Runtime.cpp was not found.' }
$features = @([regex]::Matches($table.Groups[1].Value, '(?m)^\s*\{"([^"]+)"') | ForEach-Object { $_.Groups[1].Value } |
    Where-Object { $_ -notmatch '(diagnostics|probe)$' })
$features += 'Periodic input' # started in Runtime::load(), outside the table
$documented = @{}
$problems = @()
foreach ($line in Get-Content -LiteralPath (Join-Path $projectDirectory 'docs/LIFECYCLE.md')) {
    if ($line -notmatch '^\|' -or $line -match '^\|\s*(Feature|---)') { continue }
    $cells = @($line.Trim('|').Split('|') | ForEach-Object { $_.Trim() })
    if ($cells.Count -ne 4 -or ($cells | Where-Object { $_ -eq '' })) { $problems += "LIFECYCLE.md line for '$($cells[0])' needs four filled cells"; continue }
    if ($documented.ContainsKey($cells[0])) { $problems += "LIFECYCLE.md lists '$($cells[0])' twice" }
    $documented[$cells[0]] = $true
}
foreach ($name in $features) {
    if (-not $documented.ContainsKey($name)) { $problems += "feature '$name' has no line in docs/LIFECYCLE.md: say how it handles world exit, dimension change and focus loss" }
}
foreach ($name in $documented.Keys) {
    if ($features -notcontains $name) { $problems += "docs/LIFECYCLE.md lists '$name', which is no feature row in Runtime.cpp" }
}
if ($problems) { $problems | ForEach-Object { Write-Host $_ }; throw "Lifecycle check failed ($($problems.Count))." }
Write-Host "Lifecycle table matches: $($features.Count) features."
