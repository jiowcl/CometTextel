#Requires -Version 5.1
<#
.SYNOPSIS
  Compile and run the FreeBASIC PDU self-check (Windows x64).

.DESCRIPTION
  Looks for fbc on PATH (or -FbcPath). Copies comettextel.dll from -DllPath /
  COMETTEXTEL_LIB / common build folders, builds pdu_example.exe into Output\,
  and runs it with no arguments.

.EXAMPLE
  .\verify.ps1
  .\verify.ps1 -DllPath ..\..\build-c-sdk\Release\comettextel.dll
#>
param(
    [string]$FbcPath = "",
    [string]$DllPath = ""
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot

function Find-Fbc {
    if ($FbcPath) {
        if (-not (Test-Path $FbcPath)) { throw "fbc not found: $FbcPath" }
        return (Resolve-Path $FbcPath).Path
    }
    $cmd = Get-Command fbc -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($c in @(
        "D:\IT\Developer\freebasic\fbc64.exe",
        "C:\FreeBASIC\fbc64.exe",
        "C:\FreeBASIC\fbc.exe",
        "C:\Program Files\FreeBASIC\fbc64.exe",
        "C:\Program Files\FreeBASIC\fbc.exe",
        "$env:LOCALAPPDATA\Programs\FreeBASIC\fbc64.exe"
    )) {
        if (Test-Path $c) { return $c }
    }
    throw @"
FreeBASIC compiler (fbc64.exe / fbc.exe, win64) not found.
Install FreeBASIC 1.10+ x64 and ensure fbc is on PATH, or pass -FbcPath
(e.g. -FbcPath D:\IT\Developer\freebasic\fbc64.exe).
"@
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
    throw "comettextel.dll not found. Pass -DllPath or set COMETTEXTEL_LIB."
}

$fbc = Find-Fbc
$dll = Find-Dll
Write-Host "fbc: $fbc"
Write-Host "dll: $dll"

$outDir = Join-Path $Root "Output"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Copy-Item $dll (Join-Path $outDir "comettextel.dll") -Force
Copy-Item $dll (Join-Path $Root "comettextel.dll") -Force

$exe = Join-Path $outDir "pdu_example.exe"
Push-Location $Root
try {
    & $fbc -x $exe "pdu_example.bas"
    if ($LASTEXITCODE -ne 0) { throw "fbc failed with exit $LASTEXITCODE" }
} finally {
    Pop-Location
}

Write-Host "Running self-check..."
& $exe
exit $LASTEXITCODE
