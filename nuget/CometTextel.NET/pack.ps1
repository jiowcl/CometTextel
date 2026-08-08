#Requires -Version 5.1
<#
.SYNOPSIS
  Build comettextel.dll (C ABI) and pack CometTextel.NET.
#>
param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$NugetRoot = $PSScriptRoot                                      # comettextel/nuget/CometTextel.NET
$Root = (Resolve-Path (Join-Path $NugetRoot "..\..")).Path      # comettextel/
$LibDir = Join-Path $NugetRoot "CometTextel.NET\Lib"
$BuildDir = Join-Path $Root "build-nuget"
$Artifacts = Join-Path $NugetRoot "artifacts"

Write-Host "==> Configure C++ (C API ON)"
cmake -S $Root -B $BuildDir -A $Platform `
    -DCOMETTEXTEL_BUILD_C_API=ON `
    -DCOMETTEXTEL_BUILD_SHARED=ON `
    -DCOMETTEXTEL_BUILD_STATIC=ON `
    -DCOMETTEXTEL_BUILD_EXAMPLES=OFF `
    -DCOMETTEXTEL_BUILD_TESTS=ON `
    -DCOMETTEXTEL_BUILD_DOCS=OFF

Write-Host "==> Build native DLL + tests"
cmake --build $BuildDir --config $Configuration --parallel

$dll = Join-Path $BuildDir "$Configuration\comettextel.dll"
if (-not (Test-Path $dll)) {
    throw "Native DLL not found: $dll"
}

New-Item -ItemType Directory -Force -Path $LibDir | Out-Null
Copy-Item $dll (Join-Path $LibDir "comettextel.dll") -Force
Write-Host "==> Copied $dll -> Lib\comettextel.dll"

Write-Host "==> Test native library"
ctest --test-dir $BuildDir -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "ctest failed"
}

$libDll = Join-Path $LibDir "comettextel.dll"
if (-not (Test-Path $libDll)) {
    throw "Lib DLL missing before pack: $libDll"
}

Write-Host "==> .NET PDU smoke tests"
dotnet test (Join-Path $NugetRoot "CometTextel.NET.Tests\CometTextel.NET.Tests.csproj") `
    -c $Configuration `
    -p:Platform=$Platform `
    --verbosity minimal
if ($LASTEXITCODE -ne 0) {
    throw "dotnet test failed"
}

Write-Host "==> Pack NuGet"
New-Item -ItemType Directory -Force -Path $Artifacts | Out-Null
dotnet pack (Join-Path $NugetRoot "CometTextel.NET\CometTextel.NET.csproj") `
    -c $Configuration `
    -p:Platform=$Platform `
    -o $Artifacts
if ($LASTEXITCODE -ne 0) {
    throw "dotnet pack failed"
}

Write-Host "==> Done. Packages in $Artifacts"
Get-ChildItem $Artifacts -Filter *.nupkg | ForEach-Object { $_.FullName }
