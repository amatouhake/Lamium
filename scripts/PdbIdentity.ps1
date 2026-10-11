# Which build a DLL and a PDB belong to (L-135): the debug GUID and age the
# linker writes into the DLL's CodeView record and into the PDB. Dot-source
# this file; the release and package checks compare the two.

function Get-DllDebugIdentity([byte[]]$Bytes) {
    $u16 = { param($at) [BitConverter]::ToUInt16($Bytes, $at) }
    $u32 = { param($at) [BitConverter]::ToUInt32($Bytes, $at) }
    $pe = & $u32 0x3c
    if ((& $u32 $pe) -ne 0x00004550) { throw 'Not a PE file.' }
    $sections = & $u16 ($pe + 6)
    $optionalSize = & $u16 ($pe + 20)
    $optional = $pe + 24
    $magic = & $u16 $optional
    $directories = if ($magic -eq 0x20b) { $optional + 112 } elseif ($magic -eq 0x10b) { $optional + 96 } else { throw 'Unknown PE optional header.' }
    $debugRva = & $u32 ($directories + 6 * 8)
    $debugSize = & $u32 ($directories + 6 * 8 + 4)
    if (-not $debugRva) { throw 'The DLL has no debug directory.' }
    $table = $optional + $optionalSize
    $debugOffset = $null
    for ($i = 0; $i -lt $sections; ++$i) {
        $header = $table + 40 * $i
        $virtualSize = & $u32 ($header + 8)
        $virtualAddress = & $u32 ($header + 12)
        $rawPointer = & $u32 ($header + 20)
        if ($debugRva -ge $virtualAddress -and $debugRva -lt $virtualAddress + $virtualSize) {
            $debugOffset = $rawPointer + ($debugRva - $virtualAddress)
        }
    }
    if ($null -eq $debugOffset) { throw 'The debug directory is outside every section.' }
    for ($entry = $debugOffset; $entry -lt $debugOffset + $debugSize; $entry += 28) {
        if ((& $u32 ($entry + 12)) -ne 2) { continue } # IMAGE_DEBUG_TYPE_CODEVIEW
        $record = & $u32 ($entry + 24)
        if ([Text.Encoding]::ASCII.GetString($Bytes, $record, 4) -ne 'RSDS') { continue }
        $guid = [Guid]::new([byte[]]$Bytes[($record + 4)..($record + 19)])
        return [pscustomobject]@{ Guid = $guid; Age = & $u32 ($record + 20) }
    }
    throw 'The DLL has no CodeView (RSDS) record.'
}

function Get-PdbDebugIdentity([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $signature = [Text.Encoding]::ASCII.GetString($bytes, 0, 24)
    if ($signature -ne 'Microsoft C/C++ MSF 7.00') { throw "Not an MSF 7.00 PDB: $Path" }
    $u32 = { param($at) [BitConverter]::ToUInt32($bytes, $at) }
    $blockSize = & $u32 32
    $directoryBytes = & $u32 44
    $blockMap = & $u32 52
    # The stream directory, gathered from the blocks the block map names.
    $directoryBlocks = [Math]::Ceiling($directoryBytes / $blockSize)
    $directory = New-Object byte[] ($directoryBlocks * $blockSize)
    for ($i = 0; $i -lt $directoryBlocks; ++$i) {
        $block = & $u32 ($blockMap * $blockSize + 4 * $i)
        [Array]::Copy($bytes, $block * $blockSize, $directory, $i * $blockSize, $blockSize)
    }
    $streams = [BitConverter]::ToUInt32($directory, 0)
    $sizes = for ($s = 0; $s -lt $streams; ++$s) { [BitConverter]::ToUInt32($directory, 4 + 4 * $s) }
    $cursor = 4 + 4 * $streams
    $firstBlocks = @{}
    for ($s = 0; $s -lt $streams; ++$s) {
        $size = $sizes[$s]
        $count = if ($size -eq [uint32]::MaxValue) { 0 } else { [Math]::Ceiling($size / $blockSize) }
        if ($count) { $firstBlocks[$s] = [BitConverter]::ToUInt32($directory, $cursor) }
        $cursor += 4 * $count
    }
    # Stream 1 (PDB info): version, signature, age, GUID; stream 3 (DBI) has the age again.
    $info = $firstBlocks[1] * $blockSize
    $guid = [Guid]::new([byte[]]$bytes[($info + 12)..($info + 27)])
    $dbiAge = if ($firstBlocks.ContainsKey(3)) { & $u32 ($firstBlocks[3] * $blockSize + 8) } else { $null }
    [pscustomobject]@{ Guid = $guid; Age = & $u32 ($info + 8); DbiAge = $dbiAge }
}

# Throws unless the PDB was written by the same link as the DLL bytes.
function Assert-PdbMatchesDll([byte[]]$DllBytes, [string]$PdbPath, [string]$What) {
    $dll = Get-DllDebugIdentity $DllBytes
    $pdb = Get-PdbDebugIdentity $PdbPath
    if ($dll.Guid -ne $pdb.Guid -or ($dll.Age -ne $pdb.Age -and $dll.Age -ne $pdb.DbiAge)) {
        throw "$What`: the PDB ($($pdb.Guid) age $($pdb.Age)) is not the DLL's ($($dll.Guid) age $($dll.Age))."
    }
    "$What`: PDB matches the DLL ($($dll.Guid) age $($dll.Age))."
}
