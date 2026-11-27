# Releases  

**1.7.3** packages CometTextel.NET **1.7.3** (net8.0–net11.0) with natives
that include vendor profile list/CPMS behavior. Use matching library and NuGet
versions; C ABI feature version remains **4**.

Push a version tag to publish a GitHub Release with binaries:  

```bash
git tag v1.0.0
git push origin v1.0.0
```

The **Release** workflow (`.github/workflows/release.yml`) builds, tests, and attaches:  

- `comettextel-<version>-windows-x64.zip`  
- `comettextel-<version>-linux-x64.tar.gz`  
- `comettextel-<version>-c-sdk-windows-x64.zip`  
- `comettextel-<version>-c-sdk-linux-x64.tar.gz`  
