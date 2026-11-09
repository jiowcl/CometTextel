# SDK  

## C SDK (optional)  

Stable C ABI: [`include/comettextel/c_api.h`](https://github.com/jiowcl/CometTextel/blob/main/include/comettextel/c_api.h).  
Sample: [`examples/c_api_example.c`](https://github.com/jiowcl/CometTextel/blob/main/examples/c_api_example.c) (built as `comettextel_c_api_example` when `COMETTEXTEL_BUILD_EXAMPLES=ON` and `COMETTEXTEL_BUILD_C_API=ON`).  

```bash
# After build (Release):
comettextel_c_api_example pdu 886912345678 "Hello" 886932000000
```

CI / Release also publish focused **C SDK** artifacts (`comettextel-c-sdk-*`) with only `c_api.h`, shared/static libs, and the example source — see [`sdk/c/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/c/README.md).  

Optional language samples (thin FFI, same shared library): [`sdk/python/`](https://github.com/jiowcl/CometTextel/blob/main/sdk/python) (ctypes, PDU + modem; **pytest + wheels in CI**), [`sdk/go/`](https://github.com/jiowcl/CometTextel/blob/main/sdk/go) (Windows amd64 `syscall.LoadDLL`, PDU + modem; local `verify.ps1`), [`sdk/purebasic/`](https://github.com/jiowcl/CometTextel/blob/main/sdk/purebasic) (PDU + modem; local `verify.ps1`), [`sdk/freebasic/`](https://github.com/jiowcl/CometTextel/blob/main/sdk/freebasic) (PDU + modem; local `verify.ps1`).  

## .NET NuGet (optional)  

A separate C# wrapper lives under [`nuget/CometTextel.NET`](https://github.com/jiowcl/CometTextel/blob/main/nuget/CometTextel.NET):  

```powershell
cd nuget/CometTextel.NET
.\pack.ps1
```

This builds `comettextel.dll` with the C ABI and produces `CometTextel.NET.*.nupkg` (CI also embeds **linux-x64** `libcomettextel.so`). See that folder’s README for Install-Package / Quick start.  
