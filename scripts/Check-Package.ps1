param(
    [string]$PackageDirectory = (Join-Path $PSScriptRoot '../bin/Lamium')
)
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path $PSScriptRoot -Parent
$package = (Resolve-Path -LiteralPath $PackageDirectory).Path
$manifest = Get-Content -LiteralPath (Join-Path $package 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.name -ne 'Lamium' -or $manifest.type -ne 'native' -or
    $manifest.platform -ne 'client' -or $manifest.entry -ne 'Lamium.dll') {
    throw 'Package manifest must describe the Lamium native client mod.'
}
if ((Get-Item -LiteralPath (Join-Path $package 'Lamium.dll')).Length -eq 0) {
    throw 'Lamium.dll is empty.'
}
foreach ($relative in @('COPYING', 'COPYING.LESSER', 'THIRD_PARTY_NOTICES.md')) {
    $sourceHash = (Get-FileHash -LiteralPath (Join-Path $projectDirectory $relative)).Hash
    $packageHash = (Get-FileHash -LiteralPath (Join-Path $package $relative)).Hash
    if ($sourceHash -ne $packageHash) { throw "Package notice differs from source: $relative" }
}
$licenseRoot = Join-Path $projectDirectory 'licenses'
foreach ($source in Get-ChildItem -LiteralPath $licenseRoot -File -Recurse) {
    $relative = [IO.Path]::GetRelativePath($projectDirectory, $source.FullName)
    $packageHash = (Get-FileHash -LiteralPath (Join-Path $package $relative)).Hash
    if ((Get-FileHash -LiteralPath $source.FullName).Hash -ne $packageHash) {
        throw "Package license differs from source: $relative"
    }
}
foreach ($privateDirectory in @('config', 'logs', '.xmake', '.git')) {
    if (Test-Path -LiteralPath (Join-Path $package $privateDirectory)) {
        throw "Development state must not be packaged: $privateDirectory"
    }
}
Write-Output 'Lamium client package and license copies verified.'
