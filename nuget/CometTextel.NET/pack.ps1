#Requires -Version 5.1
<#
.SYNOPSIS
  Build comettextel.dll (C ABI) and pack CometTextel.NET (win-x64; optional linux-x64).
#>
param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$LinuxSoPath = "",
    [switch]$RequireLinuxNative
)

$ErrorActionPreference = "Stop"
$NugetRoot = $PSScriptRoot                                      # comettextel/nuget/CometTextel.NET
$Root = (Resolve-Path (Join-Path $NugetRoot "..\..")).Path      # comettextel/
$LibDir = Join-Path $NugetRoot "CometTextel.NET\Lib"
$WinLibDir = Join-Path $LibDir "win-x64"
$LinuxLibDir = Join-Path $LibDir "linux-x64"
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

New-Item -ItemType Directory -Force -Path $WinLibDir | Out-Null
Copy-Item $dll (Join-Path $WinLibDir "comettextel.dll") -Force
# Keep legacy path for older scripts / docs.
Copy-Item $dll (Join-Path $LibDir "comettextel.dll") -Force
Write-Host "==> Copied $dll -> Lib\win-x64\comettextel.dll"

if ($LinuxSoPath) {
    if (-not (Test-Path $LinuxSoPath)) {
        throw "Linux SO not found: $LinuxSoPath"
    }
    New-Item -ItemType Directory -Force -Path $LinuxLibDir | Out-Null
    Copy-Item $LinuxSoPath (Join-Path $LinuxLibDir "libcomettextel.so") -Force
    Write-Host "==> Copied $LinuxSoPath -> Lib\linux-x64\libcomettextel.so"
}

Write-Host "==> Test native library"
ctest --test-dir $BuildDir -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "ctest failed"
}

Write-Host "==> .NET PDU smoke tests"
dotnet test (Join-Path $NugetRoot "CometTextel.NET.Tests\CometTextel.NET.Tests.csproj") `
    -c $Configuration `
    -p:Platform=$Platform `
    --verbosity minimal
if ($LASTEXITCODE -ne 0) {
    throw "dotnet test failed"
}

# Smoke tests only build the TFM used by the test project; rebuild all TFMs before pack (NU5026).
$PackProj = Join-Path $NugetRoot "CometTextel.NET\CometTextel.NET.csproj"
Write-Host "==> Build all TargetFrameworks for pack"
dotnet build $PackProj -c $Configuration -p:Platform=$Platform
if ($LASTEXITCODE -ne 0) {
    throw "dotnet build (pack prep) failed"
}

$packArgs = @(
    "pack", $PackProj,
    "-c", $Configuration,
    "-p:Platform=$Platform",
    "-o", $Artifacts,
    "--no-build"
)
if ($RequireLinuxNative) {
    $packArgs += "-p:CometTextelRequireLinuxNative=true"
}

Write-Host "==> Pack NuGet"
New-Item -ItemType Directory -Force -Path $Artifacts | Out-Null
dotnet @packArgs
if ($LASTEXITCODE -ne 0) {
    throw "dotnet pack failed"
}

Write-Host "==> Done. Packages in $Artifacts"
Get-ChildItem $Artifacts -Filter *.nupkg | ForEach-Object { $_.FullName }
