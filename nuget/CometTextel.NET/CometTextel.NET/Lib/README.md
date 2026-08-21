# Native RID assets for packing

| Path | RID | File |
|------|-----|------|
| `win-x64/comettextel.dll` | win-x64 | Required for pack |
| `linux-x64/libcomettextel.so` | linux-x64 | Required in CI full pack; optional for local Windows-only pack |

`pack.ps1` copies the Windows DLL into `win-x64/`.  
CI `pack-nuget` also stages the Linux shared library from the Linux C SDK artifact.

Do not commit binaries here.
