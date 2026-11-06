## Continuous Integration  

GitHub Actions (`.github/workflows/ci.yml`) runs on every push and pull request to `main`:  

- **Windows (MSVC x64)**: configure, build, run CTest, upload `comettextel-windows-x64`  
  (`bin/comettextel.dll`, import/static libs, headers)  
- **Windows C SDK**: upload `comettextel-c-sdk-windows-x64` (`c_api.h` + DLL/libs + `examples/c_api_example.c`)  
- **Windows Python**: pytest + wheel artifact `comettextel-python-wheel-windows`  
- **Windows .NET**: PDU smoke tests (`CometTextel.NET.Tests`)  
- **Linux (GCC 14)**: configure, build, run CTest, upload `comettextel-linux-x64`  
  (`libcomettextel.so*`, `libcomettextel.a`, headers)  
- **Linux C SDK**: upload `comettextel-c-sdk-linux-x64`  
- **Linux Python**: pytest + wheel artifact `comettextel-python-wheel-linux`  
- **Pack NuGet**: merges win-x64 + linux-x64 RID natives → `CometTextel.NET-nupkg`  

Download build packages from the workflow **Artifacts** tab (retained 14 days).  

For a step-by-step “download → include → link” walkthrough, see [doc/getting-started.md](https://github.com/jiowcl/CometTextel/blob/main/doc/getting-started.md).  
