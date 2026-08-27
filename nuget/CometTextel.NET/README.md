# CometTextel.NET

.NET wrapper for the [CometTextel](https://github.com/jiowcl/CometTextel) GSM SMS library.  
Send / list / delete short messages over a serial modem, or encode / decode PDU hex without hardware.

Native RID assets: **`runtimes/win-x64/native/comettextel.dll`** and **`runtimes/linux-x64/native/libcomettextel.so`**.

![GitHub](https://img.shields.io/github/license/jiowcl/CometTextel.svg)
![.NET](https://img.shields.io/badge/.NET-512BD4?style=flat-square&logo=dotnet&logoColor=white)
![Nuget](https://img.shields.io/nuget/v/CometTextel.NET)
![CI](https://github.com/jiowcl/CometTextel/actions/workflows/ci.yml/badge.svg)

## Requirements

| Item | Detail |
|------|--------|
| OS | Windows 10+ or Linux (x64) |
| Architecture | **x64 only** (`PlatformTarget=x64`) |
| .NET | **net8.0 / net9.0 / net10.0** |
| Native | Ships win-x64 DLL + linux-x64 `.so` (CometTextel C ABI) |
| Hardware | GSM modem / USB dongle that speaks AT commands in **PDU mode** (for send/list/delete) |

x86 / Arm64 processes cannot load the bundled 64-bit native library.

## NuGet

```powershell
PM> Install-Package CometTextel.NET
```

Target an **x64** process so the RID-native library can load (`win-x64` or `linux-x64`).

## Important notes

### Concatenated SMS

`Pdu.EncodeSubmit` is **single-segment** (fails if the payload is too long).  
`Pdu.EncodeSubmitSegments` and `GsmModem.Send` **auto-split** with concat UDH (IEI `0x00`, ≤ 255 parts).

Payload caps: single GSM 7-bit ≤ 160 septets (GSM 03.38 alphabet; ESC chars count as 2) / 8-bit·UCS-2 ≤ 140; concat GSM 7-bit ≤ 153 / 8-bit·UCS-2 ≤ 134.

GSM 7-bit `UserData` is UTF-8. Characters outside the default alphabet + ESC extension fail encode (use UCS-2).

On receive / list, if TP-UDHI is set, the UDH is **skipped** (`HasUdh == true`). Concat IEI `0x00` / `0x08` populate `IsConcatenated`, `ConcatRef`, `ConcatTotal`, `ConcatSeq`. `GsmModem.ListMessages` rejoins **complete** segment sets (`ConcatSeq == 0`, `IsReassembledConcat`). Incomplete parts remain separate.

### Validity Period / Status Report

Submit APIs omit TP-VP and TP-SRR by default. Set `relativeValidityPeriod` to
`0..255` to add a GSM relative TP-VP (`0` means five minutes), and set
`requestStatusReport: true` to set TP-SRR. These options are available on
`Pdu.EncodeSubmit`, `Pdu.EncodeSubmitSegments`, and `GsmModem.Send`.
This requests a report; delivery-report parsing is not yet implemented.

### Strings

All text fields are **UTF-8** at the native boundary (`LPUTF8Str` / UTF-8 byte fields). Prefer ASCII digits for SMSC / destination addresses.

### Native lifetime

`GsmModem` owns a native handle. Always `Dispose()` (or `using`) when finished.  
Do not use an instance after dispose.

### Errors

Failed native calls throw `CometTextelException` with a numeric `Status` (`ct_status`) and message from `ct_status_string`.

## Quick start

### PDU only (no modem)

```csharp
using CometTextel.NET.Core;

string hex = Pdu.EncodeSubmit(
    destination: "886912345678",
    text: "Hello from CometTextel.NET",
    serviceCenter: "886932000000",
    coding: DataCoding.Ucs2);

Console.WriteLine(hex);

SmsMessage decoded = Pdu.Decode(hex);
Console.WriteLine(decoded.PeerAddress + " => " + decoded.UserData);
```

### Send / list / delete

```csharp
using CometTextel.NET.Core;

using var modem = new GsmModem();
modem.Open("COM3", baudRate: 115200);   // Linux: "/dev/ttyUSB0"

modem.Send(
    destination: "886912345678",
    text: "Hello",
    serviceCenter: "886932000000",
    coding: DataCoding.Ucs2);

foreach (var message in modem.List())
{
    Console.WriteLine($"[{message.Index}] {message.PeerAddress}: {message.UserData}");
}

// modem.Delete(index: 1);
```

## Smoke tests

Requires `CometTextel.NET/Lib/win-x64/comettextel.dll` (same as packing):

```powershell
dotnet test CometTextel.NET.Tests\CometTextel.NET.Tests.csproj -c Release -p:Platform=x64
```

`pack.ps1` and CI run these before producing the NuGet package.

## Examples project

From `nuget/CometTextel.NET` (after `pack.ps1` or a native Release build that copied `Lib/win-x64/comettextel.dll`):

```powershell
dotnet run --project CometTextel.Example -c Release -p:Platform=x64 -- pdu 886912345678 "Hello" 886932000000
dotnet run --project CometTextel.Example -c Release -p:Platform=x64 -- list COM3
dotnet run --project CometTextel.Example -c Release -p:Platform=x64 -- send COM3 886932000000 886912345678 "Hello"
dotnet run --project CometTextel.Example -c Release -p:Platform=x64 -- delete COM3 1
```

## Build & pack

Requires CMake + MSVC (for `comettextel.dll`) and .NET SDK 8+ on Windows.  
Full multi-RID packages also need a Linux `libcomettextel.so` (CI `pack-nuget` job merges both).

```powershell
# From repository: comettextel/nuget/CometTextel.NET
.\pack.ps1
# Optional: include linux-x64 from a staged SO
# .\pack.ps1 -LinuxSoPath D:\path\to\libcomettextel.so -RequireLinuxNative
```

This script:

1. Configures/builds the C++ library (`COMETTEXTEL_BUILD_C_API=ON`, win-x64 Release)  
2. Copies `comettextel.dll` into `CometTextel.NET/Lib/win-x64/`  
3. Runs smoke tests, then `dotnet pack` → `artifacts/`  

CI uploads **`CometTextel.NET-nupkg`** with both `win-x64` and `linux-x64` RID folders.

## API surface (summary)

| Area | Highlights |
|------|------------|
| Modem | `GsmModem.Open`, `Send`, `List`, `Delete`, `Dispose` |
| PDU | `Pdu.EncodeSubmit`, `Pdu.EncodeSubmitSegments`, `Pdu.Decode` |
| Types | `SmsMessage`, `DataCoding`, `CometTextelException` |

Namespace: **`CometTextel.NET.Core`**.

Native C ABI: `ct_modem_*`, `ct_pdu_encode_submit`, `ct_pdu_decode` — see `include/comettextel/c_api.h`.

## License

Copyright (c) 2026 Ji-Feng Tsai.  
Code released under the MIT license.

## Donation

If this application help you reduce time to coding, you can give me a cup of coffee :)

[![paypal](https://www.paypalobjects.com/en_US/TW/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=3RNMD6Q3B495N&source=url)

[Paypal Me](https://paypal.me/jiowcl?locale.x=zh_TW)
