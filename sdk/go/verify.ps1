#Requires -Version 5.1
<#
.SYNOPSIS
  Test and build the Go PDU sample (Windows amd64).

.DESCRIPTION
  Looks for go on PATH or C:\Program Files\Go (or -GoPath). Copies comettextel.dll
  from -DllPath / COMETTEXTEL_LIB / common build folders, runs go test, builds
  pdu.exe and modem.exe into Output\, and runs the PDU self-check.

.EXAMPLE
  .\verify.ps1
  .\verify.ps1 -DllPath ..\..\build\Release\comettextel.dll
#>
param(
    [string]$GoPath = "",
    [string]$DllPath = ""
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot

function Find-Go {
    if ($GoPath) {
        if (-not (Test-Path $GoPath)) { throw "go not found: $GoPath" }
        return (Resolve-Path $GoPath).Path
    }
    $cmd = Get-Command go -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $bundled = "C:\Program Files\Go\bin\go.exe"
    if (Test-Path $bundled) { return $bundled }
    throw "Go (windows/amd64) not found. Install Go 1.22+ or pass -GoPath."
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
        (Join-Path $repo "build\Release\comettextel.dll"),
        (Join-Path $repo "build-c-sdk\Release\comettextel.dll"),
        (Join-Path $repo "artifact\comettextel-c-sdk-windows-x64\bin\comettextel.dll")
    )) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    throw "comettextel.dll not found. Pass -DllPath or set COMETTEXTEL_LIB."
}

$go = Find-Go
$dll = Find-Dll
Write-Host "go: $go"
Write-Host "dll: $dll"

$env:COMETTEXTEL_LIB = $dll
$env:CGO_ENABLED = "0"

Push-Location $Root
try {
    & $go test -count=1 .
    if ($LASTEXITCODE -ne 0) { throw "go test failed with exit $LASTEXITCODE" }

    $outDir = Join-Path $Root "Output"
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    Copy-Item $dll (Join-Path $outDir "comettextel.dll") -Force

    $pdu = Join-Path $outDir "pdu.exe"
    $modem = Join-Path $outDir "modem.exe"
    & $go build -o $pdu .\cmd\pdu
    if ($LASTEXITCODE -ne 0) { throw "go build pdu failed with exit $LASTEXITCODE" }
    & $go build -o $modem .\cmd\modem
    if ($LASTEXITCODE -ne 0) { throw "go build modem failed with exit $LASTEXITCODE" }
} finally {
    Pop-Location
}

Write-Host "Running PDU self-check..."
& (Join-Path $Root "Output\pdu.exe")
exit $LASTEXITCODE
