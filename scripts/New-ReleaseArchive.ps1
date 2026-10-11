param(
    [string]$PackageDirectory = (Join-Path $PSScriptRoot '../bin/Lamium'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../bin/release')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
. (Join-Path $PSScriptRoot 'PdbIdentity.ps1')
$projectDirectory = Split-Path $PSScriptRoot -Parent
$package = (Resolve-Path -LiteralPath $PackageDirectory).Path

# The version lives in xmake.lua, tooth.json and the built manifest; the LIP
# registry reads tooth.json at the tag, so any drift ships a broken listing.
$xmake = Get-Content -LiteralPath (Join-Path $projectDirectory 'xmake.lua') -Raw
$match = [regex]::Match($xmake, 'local\s+lamiumVersion\s*=\s*"([^"]+)"')
if (-not $match.Success) { throw 'lamiumVersion not found in xmake.lua.' }
$version = $match.Groups[1].Value
$tooth = Get-Content -LiteralPath (Join-Path $projectDirectory 'tooth.json') -Raw | ConvertFrom-Json
$manifest = Get-Content -LiteralPath (Join-Path $package 'manifest.json') -Raw | ConvertFrom-Json
if ($tooth.version -ne $version) { throw "tooth.json version $($tooth.version) differs from xmake.lua $version." }
if ($manifest.version -ne $version) { throw "Built manifest version $($manifest.version) differs from xmake.lua $version." }
if ($env:GITHUB_REF_TYPE -eq 'tag' -and $env:GITHUB_REF_NAME -ne "v$version") {
    throw "Tag $env:GITHUB_REF_NAME does not match version v$version."
}

$assetName = "Lamium-$version-client-windows-x64.zip"
# The symbols are a separate release asset (L-135): LIP and LeviLauncher
# install the ZIP only, and the PDB stays available for crash addresses.
$symbolsName = "Lamium-$version-client-windows-x64.pdb.zip"
if ($tooth.tooth -ne 'github.com/amatouhake/Lamium' -or $tooth.variants.Count -ne 1) {
    throw 'tooth.json must describe the single Lamium package variant.'
}
$variant = $tooth.variants[0]
if ($variant.label -ne 'client' -or $variant.platform -ne 'win-x64' -or
    -not $variant.dependencies.'github.com/LiteLDev/LeviLamina#client') {
    throw 'tooth.json variant must be the win-x64 client depending on LeviLamina#client.'
}
$asset = $variant.assets[0]
$expectedUrl = 'https://{{tooth}}/releases/download/v{{version}}/Lamium-{{version}}-client-windows-x64.zip'
if ($variant.assets.Count -ne 1 -or $asset.type -ne 'zip' -or $asset.urls.Count -ne 1 -or
    $asset.urls[0] -ne $expectedUrl -or $asset.placements.Count -ne 1 -or
    $asset.placements[0].type -ne 'dir' -or $asset.placements[0].src -ne 'Lamium/' -or
    $asset.placements[0].dest -ne 'mods/Lamium/') {
    throw "tooth.json asset must be $expectedUrl placing Lamium/ into mods/Lamium/."
}

# Built by hand before, archives stored '\' separators; the ZIP format requires '/'.
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$archivePath = Join-Path (Resolve-Path -LiteralPath $OutputDirectory).Path $assetName
if (Test-Path -LiteralPath $archivePath) { Remove-Item -LiteralPath $archivePath }
$stream = [IO.File]::Open($archivePath, [IO.FileMode]::CreateNew)
try {
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in Get-ChildItem -LiteralPath $package -File -Recurse | Sort-Object FullName) {
            if ($file.Extension -ieq '.pdb') { continue }
            $relative = [IO.Path]::GetRelativePath($package, $file.FullName).Replace('\', '/')
            $entry = $archive.CreateEntry("Lamium/$relative", [IO.Compression.CompressionLevel]::Optimal)
            $entryStream = $entry.Open()
            try {
                $source = [IO.File]::OpenRead($file.FullName)
                try { $source.CopyTo($entryStream) } finally { $source.Dispose() }
            } finally { $entryStream.Dispose() }
        }
    } finally { $archive.Dispose() }
} finally { $stream.Dispose() }

$read = [IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    $names = @($read.Entries | ForEach-Object FullName)
    # The DLL as shipped, to check the symbols against.
    $dllStream = $read.GetEntry('Lamium/Lamium.dll').Open()
    try {
        $buffer = [IO.MemoryStream]::new()
        $dllStream.CopyTo($buffer)
        $shippedDll = $buffer.ToArray()
    } finally { $dllStream.Dispose() }
} finally { $read.Dispose() }
foreach ($name in $names) {
    if ($name.Contains('\')) { throw "Archive entry uses '\': $name" }
    if (-not $name.StartsWith('Lamium/')) { throw "Archive entry outside Lamium/: $name" }
    if ($name -match '^Lamium/(config|logs)/') { throw "Runtime state must not be archived: $name" }
    if ($name -match '\.pdb$') { throw "Symbols ship as their own asset, not in the archive: $name" }
}
foreach ($required in @('Lamium/Lamium.dll', 'Lamium/manifest.json', 'Lamium/COPYING', 'Lamium/COPYING.LESSER')) {
    if ($names -notcontains $required) { throw "Archive is missing $required" }
}
Write-Output "Release archive $assetName verified ($($names.Count) entries, version $version)."

$pdb = Join-Path $package 'Lamium.pdb'
if (-not (Test-Path -LiteralPath $pdb)) { throw 'Lamium.pdb is missing from the package; it is released beside the archive.' }
Assert-PdbMatchesDll $shippedDll $pdb "Release symbols $symbolsName"
# Compressed: the PDB is about ten times the size of its ZIP.
$symbolsPath = Join-Path (Split-Path $archivePath -Parent) $symbolsName
if (Test-Path -LiteralPath $symbolsPath) { Remove-Item -LiteralPath $symbolsPath }
$stream = [IO.File]::Open($symbolsPath, [IO.FileMode]::CreateNew)
try {
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $entryStream = $archive.CreateEntry('Lamium.pdb', [IO.Compression.CompressionLevel]::Optimal).Open()
        try {
            $source = [IO.File]::OpenRead($pdb)
            try { $source.CopyTo($entryStream) } finally { $source.Dispose() }
        } finally { $entryStream.Dispose() }
    } finally { $archive.Dispose() }
} finally { $stream.Dispose() }
Write-Output "Release symbols $symbolsName written."
