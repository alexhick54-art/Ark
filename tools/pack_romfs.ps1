# Packs romfs folders into numbered zips of at most -MaxMB (uncompressed) each, keeping the bin\... paths, for
# sending parts of the RomFS when one zip would be too big. The game files stay yours: nothing is uploaded by this.
#
#   powershell -ExecutionPolicy Bypass -File tools\pack_romfs.ps1 -Romfs "F:\path\to\romfs" -Name romfs_field `
#       -Folders "bin\archive\field,bin\field\model,bin\effect\field"
#
# Writes romfs_field_1.zip, romfs_field_2.zip, ... to the Desktop (-Out to change; -MaxMB, default 180).
param(
    [Parameter(Mandatory = $true)][string]$Romfs,
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Folders,
    [int]$MaxMB = 180,
    [string]$Out = [Environment]::GetFolderPath('Desktop')
)
$ErrorActionPreference = 'Stop'
$limit = [int64]$MaxMB * 1MB
$root = (Resolve-Path -LiteralPath $Romfs).Path.TrimEnd('\')
$files = @()
foreach ($f in ($Folders -split ',')) {
    $f = $f.Trim()
    if (-not $f) { continue }
    $dir = Join-Path $root $f
    if (-not (Test-Path -LiteralPath $dir)) { Write-Warning "not found: $dir"; continue }
    $files += Get-ChildItem -LiteralPath $dir -Recurse -File
}
if ($files.Count -eq 0) { throw "no files found under $root for: $Folders" }

$part = 0
$batch = New-Object System.Collections.Generic.List[string]
$size = [int64]0
$list = Join-Path $env:TEMP "ark_pack_list.txt"
$made = @()
Push-Location -LiteralPath $root
try {
    for ($i = 0; $i -le $files.Count; $i++) {
        $last = $i -eq $files.Count
        if (-not $last) {
            $file = $files[$i]
            $rel = $file.FullName.Substring($root.Length + 1)
        }
        if ($batch.Count -gt 0 -and ($last -or $size + $file.Length -gt $limit)) {
            $part++
            $zip = Join-Path $Out ("{0}_{1}.zip" -f $Name, $part)
            if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip }
            [System.IO.File]::WriteAllLines($list, $batch)  # (UTF-8 without BOM: tar reads it as is)
            & tar.exe -a -cf $zip -C $root -T $list
            if ($LASTEXITCODE -ne 0) { throw "tar failed for $zip" }
            $made += "{0}  ({1} files, {2:N0} MB before compression)" -f (Split-Path $zip -Leaf), $batch.Count, ($size / 1MB)
            $batch.Clear()
            $size = 0
        }
        if (-not $last) {
            $batch.Add($rel)
            $size += $file.Length
        }
    }
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $list) { Remove-Item -LiteralPath $list }
}
Write-Host "Wrote to ${Out}:"
$made | ForEach-Object { Write-Host "  $_" }
