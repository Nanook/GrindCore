# Usage: pwsh ./generate_exports.ps1 entrypoints.c GrindCore.def

param(
    [string]$InputFile,
    [string]$OutputFile
)

# Ensure the output directory exists
$dir = Split-Path -Parent $OutputFile
if (-not (Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir | Out-Null
}

$symbols = @()

# Read entrypoints.c and extract symbols from DllImportEntry(...)
Get-Content $InputFile | ForEach-Object {
    if ($_ -match 'DllImportEntry\(([^)]+)\)') {
        $symbols += $matches[1]
    }
}

# Write the .def file
"EXPORTS" | Set-Content $OutputFile
$symbols | ForEach-Object { $_ } | Add-Content $OutputFile