param (
    [string]$exe,
    [string]$dest,
    [string]$mingwBinDir
)

function Get-DllDependencies {
    param([string]$binary)
    & objdump.exe -p $binary 2>$null |
      Select-String 'DLL Name:' |
      ForEach-Object { ($_ -split ':')[1].Trim() }
}

if (-not $exe -or -not $dest -or -not $mingwBinDir) {
    Write-Error "Usage: $(Split-Path -Leaf $MyInvocation.MyCommand.Name) -exe exe -dest dest -mingwBinDir mingw-bin-dir"
    exit 2
}

New-Item -ItemType Directory -Path $dest -Force | Out-Null
$dlls = [System.Collections.Generic.Queue[string]]::new()
[void]$dlls.Enqueue($exe)
$seen = @{}
foreach ($dll in Get-DllDependencies $exe) {
    $dlls.Enqueue($dll)
}

while ($dlls.Count -gt 0) {
    $item = $dlls.Dequeue()
    $basename = if (Test-Path $item) { Split-Path $item -Leaf } else { $item }

    if ($seen[$basename]) {
        continue
    }
    $seen[$basename] = $true
    $full = Join-Path $mingwBinDir $basename
    if (Test-Path $full -PathType Leaf) {
        Copy-Item $full -Destination $dest -Force
        foreach ($dll in Get-DllDependencies $full) {
            $dlls.Enqueue($dll)
        }
    }
}
