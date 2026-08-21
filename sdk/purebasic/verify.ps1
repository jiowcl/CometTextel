#Requires -Version 5.1
<#
.SYNOPSIS
  Compile and run the PureBasic PDU self-check (Windows x64).
#>
param(
    [string]$PbCompiler = "",
    [string]$DllPath = ""
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot

function Find-PbCompiler {
    if ($PbCompiler) {
        if (-not (Test-Path $PbCompiler)) { throw "pbcompiler not found: $PbCompiler" }
        return (Resolve-Path $PbCompiler).Path
    }
    $cmd = Get-Command pbcompiler -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $default = "C:\Program Files\PureBasic\Compilers\pbcompiler.exe"
    if (Test-Path $default) { return $default }
    throw "PureBasic pbcompiler.exe not found. Pass -PbCompiler."
}

function Find-Dll {
    if ($DllPath) {
        if (-not (Test-Path $DllPath)) { throw "DLL not found: $DllPath" }
        return (Resolve-Path $DllPath).Path
    }
    if ($env:COMETTEXTEL_LIB) {
        $p = $env:COMETTEXTEL_LIB
        if (Test-Path $p -PathType Leaf) { return (Resolve-Path $p).Path }
        $cand = Join-Path $p "comettextel.dll"
        if (Test-Path $cand) { return (Resolve-Path $cand).Path }
    }
    $repo = Resolve-Path (Join-Path $Root "..\..")
    foreach ($c in @(
        (Join-Path $Root "comettextel.dll"),
        (Join-Path $repo "build-c-sdk\Release\comettextel.dll"),
        (Join-Path $repo "artifact\comettextel-c-sdk-windows-x64\bin\comettextel.dll"),
        (Join-Path $repo "build\Release\comettextel.dll")
    )) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    throw "comettextel.dll not found. Pass -DllPath or set COMETTEXTEL_LIB (must export ct_*)."
}

$pb = Find-PbCompiler
$dll = Find-Dll
Write-Host "pbcompiler: $pb"
Write-Host "dll: $dll"

$outDir = Join-Path $Root "Output"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Copy-Item $dll (Join-Path $outDir "comettextel.dll") -Force
Copy-Item $dll (Join-Path $Root "comettextel.dll") -Force

$exe = Join-Path $outDir "pdu_example.exe"
& $pb (Join-Path $Root "pdu_example.pb") /EXE $exe /CONSOLE
if ($LASTEXITCODE -ne 0) { throw "pbcompiler failed with exit $LASTEXITCODE" }

$modemExe = Join-Path $outDir "modem_example.exe"
& $pb (Join-Path $Root "modem_example.pb") /EXE $modemExe /CONSOLE
if ($LASTEXITCODE -ne 0) { throw "pbcompiler (modem_example) failed with exit $LASTEXITCODE" }

Write-Host "Running PDU self-check..."
& $exe
exit $LASTEXITCODE
