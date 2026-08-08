#Requires -Version 5.1
<#
.SYNOPSIS
  Stage a Windows CometTextel C SDK directory (headers + libs + example source).
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$OutDir,

    [string]$BuildDir = "build",
    [string]$Configuration = "Release",
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

$binDll = Join-Path $BuildDir "$Configuration\comettextel.dll"
$impLib = Join-Path $BuildDir "$Configuration\comettextel.lib"
$staticLib = Join-Path $BuildDir "$Configuration\comettextel_static.lib"
$header = Join-Path $RepoRoot "include\comettextel\c_api.h"
$example = Join-Path $RepoRoot "examples\c_api_example.c"
$sdkReadme = Join-Path $RepoRoot "sdk\c\README.md"

foreach ($path in @($binDll, $impLib, $header, $example, $sdkReadme)) {
    if (-not (Test-Path $path)) {
        throw "Required path missing: $path"
    }
}

New-Item -ItemType Directory -Force -Path `
    (Join-Path $OutDir "bin"), `
    (Join-Path $OutDir "lib"), `
    (Join-Path $OutDir "include\comettextel"), `
    (Join-Path $OutDir "examples") | Out-Null

Copy-Item $binDll (Join-Path $OutDir "bin\comettextel.dll") -Force
Copy-Item $impLib (Join-Path $OutDir "lib\comettextel.lib") -Force
if (Test-Path $staticLib) {
    Copy-Item $staticLib (Join-Path $OutDir "lib\comettextel_static.lib") -Force
}
Copy-Item $header (Join-Path $OutDir "include\comettextel\c_api.h") -Force
Copy-Item $example (Join-Path $OutDir "examples\c_api_example.c") -Force
Copy-Item $sdkReadme (Join-Path $OutDir "README.md") -Force

$license = Join-Path $RepoRoot "LICENSE"
if (Test-Path $license) {
    Copy-Item $license (Join-Path $OutDir "LICENSE") -Force
}

Write-Host "Staged C SDK -> $OutDir"
Get-ChildItem -Recurse $OutDir | ForEach-Object { $_.FullName.Substring($OutDir.Length + 1) }
